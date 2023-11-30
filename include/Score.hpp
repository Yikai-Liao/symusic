#include <cmath>
#include <map>
#include <queue>
#include <algorithm>
#include <cmath>
#include <cfloat>
#include <exception>
#include <functional>
#include <pdqsort.h>
#include <Eigen/Core>
#include "MiniMidi.hpp"
namespace score {
#define DEFAULT_VELOCITY 100

#define MIDI_PITCH_NUMS 128
typedef Eigen::Array<bool, MIDI_PITCH_NUMS, Eigen::Dynamic> Pianoroll;

typedef Eigen::Array<float, Eigen::Dynamic, 1> F32Vector;
typedef Eigen::Array<uint8_t, Eigen::Dynamic, 1> U8Vector;

using minimidi::message::Message;

namespace utils {
inline int8_t safe_add(int8_t a, int8_t b) {
    int ans = a + b;
    if (ans > 127 || ans < 0)
        throw std::range_error("Overflow while adding " + std::to_string(a) + " and " + std::to_string(b));
    return (int8_t) ans;
}

// NOLINTBEGIN
std::string strip_non_utf_8(std::string *str) {
    int i, f_size = str->size();
    unsigned char c, c2, c3, c4;
    std::string to;
    to.reserve(f_size);

    for (i = 0; i < f_size; i++) {
        c = (unsigned char) (*str)[i];
        if (c < 32) {//control char
            if (c == 9 || c == 10 || c == 13) {//allow only \t \n \r
                to.append(1, c);
            }
            continue;
        } else if (c < 127) {//normal ASCII
            to.append(1, c);
            continue;
        } else if (c <
                   160) {//control char (nothing should be defined here either ASCII, ISO_8859-1 or UTF8, so skipping)
            if (c2 == 128) {//fix microsoft mess, add euro
                to.append(1, -30); // 226 for unsigned
                to.append(1, -126); // 130 for unsigned
                to.append(1, -84); // 172 for unsigned
            }
            if (c2 == 133) {//fix IBM mess, add NEL = \n\r
                to.append(1, 10);
                to.append(1, 13);
            }
            continue;
        } else if (c < 192) {//invalid for UTF8, converting ASCII
            to.append(1, (unsigned char) 194);
            to.append(1, c);
            continue;
        } else if (c < 194) {//invalid for UTF8, converting ASCII
            to.append(1, (unsigned char) 195);
            to.append(1, c - 64);
            continue;
        } else if (c < 224 && i + 1 < f_size) {//possibly 2byte UTF8
            c2 = (unsigned char) (*str)[i + 1];
            if (c2 > 127 && c2 < 192) {//valid 2byte UTF8
                if (c == 194 && c2 < 160) {//control char, skipping
                    ;
                } else {
                    to.append(1, c);
                    to.append(1, c2);
                }
                i++;
                continue;
            }
        } else if (c < 240 && i + 2 < f_size) {//possibly 3byte UTF8
            c2 = (unsigned char) (*str)[i + 1];
            c3 = (unsigned char) (*str)[i + 2];
            if (c2 > 127 && c2 < 192 && c3 > 127 && c3 < 192) {//valid 3byte UTF8
                to.append(1, c);
                to.append(1, c2);
                to.append(1, c3);
                i += 2;
                continue;
            }
        } else if (c < 245 && i + 3 < f_size) {//possibly 4byte UTF8
            c2 = (unsigned char) (*str)[i + 1];
            c3 = (unsigned char) (*str)[i + 2];
            c4 = (unsigned char) (*str)[i + 3];
            if (c2 > 127 && c2 < 192 && c3 > 127 && c3 < 192 && c4 > 127 && c4 < 192) {//valid 4byte UTF8
                to.append(1, c);
                to.append(1, c2);
                to.append(1, c3);
                to.append(1, c4);
                i += 3;
                continue;
            }
        }
        //invalid UTF8, converting ASCII (c>245 || string too short for multibyte))
        to.append(1, (unsigned char) 195);
        to.append(1, c - 64);
    }
    return to;
}
} // NOLINTEND

enum class TimeUnit : uint8_t {
    Tick,
    Quarter,
    Second
};

struct Time {
public:
    union {
        uint32_t tick;
        float quarter;
        float second;
    };

    Time() = default;
    Time(const Time &) = default;
    [[nodiscard]] inline Time copy() const { return {*this}; };

    inline Time(uint32_t tick, float qpm, TimeUnit time_unit) {
        switch (time_unit) {
            case TimeUnit::Tick:
                this->tick = tick;
                break;
            case TimeUnit::Quarter:
                this->quarter = (float) tick / qpm;
                break;
            case TimeUnit::Second:
                throw std::invalid_argument("Second time unit is not supported currently");
        }
    }

    inline Time& shift_inplace(float offset, TimeUnit time_unit) {
        switch (time_unit) {
            case TimeUnit::Tick:
                this->tick += (uint32_t) offset;
                break;
            case TimeUnit::Quarter:
                this->quarter += offset;
                break;
            case TimeUnit::Second:
                this->second += offset;
                break;
        } return *this;
    }

    [[nodiscard]] inline Time shift(float offset, TimeUnit time_unit) const {
        return copy().shift_inplace(offset, time_unit);
    }

    [[nodiscard]] inline uint32_t get_tick(TimeUnit time_unit, float tpq) const {
        switch (time_unit) {
            case TimeUnit::Tick:
                return tick;
            case TimeUnit::Quarter:
                return (uint32_t) std::round(quarter * tpq);
            case TimeUnit::Second:
                throw std::invalid_argument("Second time unit is not supported currently");
        }
    };

    [[nodiscard]] inline float get_quarter(TimeUnit time_unit, float tpq) const {
        switch (time_unit) {
            case TimeUnit::Tick:
                return (float) tick / tpq;
            case TimeUnit::Quarter:
                return quarter;
            case TimeUnit::Second:
                throw std::invalid_argument("Second time unit is not supported currently");
        }
    };

    float inline to_float(TimeUnit time_unit) const {
        switch (time_unit) {
            case TimeUnit::Tick:
                return (float) tick;
            case TimeUnit::Quarter:
                return quarter;
            case TimeUnit::Second:
                return second;
        }
    }

};

class TimeContainer {
protected:
    Time time;
    TimeUnit timeUnit;
public:
    TimeContainer() = default;

    TimeContainer(const TimeContainer &) = default;

    inline TimeContainer(uint32_t tick, float qpm, TimeUnit time_unit) :
        time(Time(tick, qpm, time_unit)), timeUnit(time_unit) {}

    inline TimeContainer(Time time, TimeUnit time_unit) :
        time(time), timeUnit(time_unit) {}

    [[nodiscard]] inline TimeUnit get_time_unit() const { return timeUnit; };

    [[nodiscard]] inline Time get_time_struct() const { return time; };

    [[nodiscard]] inline uint32_t get_tick(float tpq) const { return time.get_tick(timeUnit, tpq); };

    [[nodiscard]] inline float get_quarter(float tpq) const { return time.get_quarter(timeUnit, tpq); };

    [[nodiscard]] inline float to_float() const { return time.to_float(timeUnit); };

    [[nodiscard]] inline uint32_t get_tick_unchecked() const {return time.tick;}

    [[nodiscard]] inline float get_quarter_unchecked() const {return time.quarter;}

    [[nodiscard]] inline float get_second_unchecked() const {return time.second;}

    [[nodiscard]] inline TimeContainer shift_time(float offset) const {
        return {time.shift(offset, timeUnit), timeUnit};
    }

    inline TimeContainer& shift_time_inplace(float offset) {
        time.shift_inplace(offset, timeUnit);
        return *this;
    }
};

namespace utils {

template<typename T>
inline bool cmp_tick(T &a, T &b) { return a.get_tick_unchecked() < b.get_tick_unchecked(); }

template<typename T>
inline bool cmp_quarter(T &a, T &b) { return a.get_quarter_unchecked() < b.get_quarter_unchecked(); }

template<typename T>
inline bool cmp_second(T &a, T &b) { return a.get_second_unchecked() < b.get_second_unchecked(); }

template<typename T>
inline void sort_by_time(std::vector<T> &data) {
    if (!data.empty()) {
        switch (data[0].get_time_unit()) {
            case TimeUnit::Tick:
                pdqsort_branchless(data.begin(), data.end(), cmp_tick<T>);
                break;
            case TimeUnit::Quarter:
                pdqsort_branchless(data.begin(), data.end(), cmp_quarter<T> );
                break;
            case TimeUnit::Second:
                pdqsort_branchless(data.begin(), data.end(), cmp_second<T> );
                break;
        }
    }
}

template<typename T>
inline void sort_by_start(std::vector<T> &data) {
    if (!data.empty()) {
        switch (data[0].timeUnit) {
            case TimeUnit::Tick:
                pdqsort_branchless(data.begin(), data.end(), [](const T &a, const T &b) {
                    return a.start.tick < b.start.tick;
                });
                break;
            case TimeUnit::Quarter:
                pdqsort_branchless(data.begin(), data.end(), [](const T &a, const T &b) {
                    return a.start.quarter < b.start.quarter;
                });
                break;
            case TimeUnit::Second:
                pdqsort_branchless(data.begin(), data.end(), [](const T &a, const T &b) {
                    return a.start.second < b.start.second;
                });
                break;
        }
    }
}

template<typename T>
inline std::vector<T> filter(const std::vector<T> &data, const std::function<bool(const T &)> &filter) {
    std::vector<T> new_data;
    new_data.reserve(data.size());
    std::copy_if(
        data.begin(), data.end(), std::back_inserter(new_data), filter
    );
    new_data.shrink_to_fit();
    return new_data;
}

template<typename T>
inline std::vector<T> clip(const std::vector<T> &data, float start, float end) {
    if (data.empty()) return {};
    std::function<bool(const T &)> filter;
    switch (data[0].get_time_unit()) {
        case TimeUnit::Tick:
            filter = [start, end](const T &t) {
                auto tick = t.get_tick_unchecked();
                return tick >= (uint32_t) start && tick < (uint32_t) end;
            };
            break;
        case TimeUnit::Quarter:
            filter = [start, end](const T &t) {
                auto quarter = t.get_quarter_unchecked();
                return quarter >= start && quarter < end;
            };
            break;
        case TimeUnit::Second:
            filter = [start, end](const T &t) {
                auto second = t.get_second_unchecked();
                return second >= start && second < end;
            };
            break;
    } return utils::filter(data, filter);
}

std::function<uint64_t (const Time &)> get_quantize_func(TimeUnit time_unit, float tpq, uint16_t quantization){
    std::function<uint64_t (const Time &)> quantize;
    switch(time_unit){
        case TimeUnit::Tick:
            quantize = [tpq, quantization](const Time & time) {
                return (uint64_t) std::floor((float) time.tick * ((float) quantization / (4 * tpq)));
            };
            break;
        case TimeUnit::Quarter:
            quantize = [quantization](const Time & time) {
                return (uint64_t)std::floor(( time.quarter * ((float) quantization / 4)));
            };
        case TimeUnit::Second:
            throw std::invalid_argument("Second time unit is not supported currently");
    }
    return quantize;
}

}

class Track;

class NoteArray {
public: // use eigen 1d array to store start, duration, pitch and velocity
    std::string name;
    uint8_t program = 0;
    F32Vector start;
    F32Vector duration;
    U8Vector pitch;
    U8Vector velocity;

    NoteArray() = default;

    NoteArray(const NoteArray &) = default;

    inline explicit NoteArray(const Track &track);

    [[nodiscard]] inline auto copy() const { return NoteArray(*this); };
};
// Using int8_t to avoid possible errors while shifting

class Note {
public:
    Time start;
    Time duration;
    TimeUnit timeUnit;
    int8_t pitch;
    int8_t velocity;

public:
    inline Note(Time start, Time duration, TimeUnit time_unit, int8_t pitch, int8_t velocity = DEFAULT_VELOCITY) :
        start(start), duration(duration), timeUnit(time_unit), pitch(pitch), velocity(velocity) {};

    inline Note(const Note &) = default;

    [[nodiscard]] inline auto copy() const { return Note(*this); }

    [[nodiscard]] inline bool empty() const {
        bool answer = velocity <= 0;
        switch (timeUnit){
            case TimeUnit::Tick:
                return answer || duration.tick <= 0;
            case TimeUnit::Quarter:
                return answer || duration.quarter <= 0;
            case TimeUnit::Second:
                return answer || duration.second <= 0;
            default:
                return answer;
        }
    };

    [[nodiscard]] inline Time end_time() const {
        Time end = start;
        switch (timeUnit){
            case TimeUnit::Tick:
                end.tick += duration.tick;
                break;
            case TimeUnit::Quarter:
                end.quarter += duration.quarter;
                break;
            case TimeUnit::Second:
                end.second += duration.second;
                break;
        } return end;
    };

    [[nodiscard]] inline float end_time_float() const {
        return end_time().to_float(timeUnit);
    };

    [[nodiscard]] inline float start_float() const {
        return start.to_float(timeUnit);
    }

    [[nodiscard]] inline float duration_float() const {
        return duration.to_float(timeUnit);
    }

    [[nodiscard]] inline Note shift_pitch(int8_t offset) const {
        return {start, duration, timeUnit, utils::safe_add(pitch, offset), velocity};
    };

    [[nodiscard]] inline Note shift_velocity(int8_t offset) const {
        return {start, duration, timeUnit, pitch, utils::safe_add(velocity, offset)};
    };

    [[nodiscard]] inline Note shift_time(float offset) const {
        auto new_start = start.shift(offset, timeUnit);
        return {new_start, duration, timeUnit, pitch, velocity};
    }

    inline Note& shift_time_inplace(float offset) {
        start.shift_inplace(offset, timeUnit);
        return *this;
    }

    inline Note& shift_pitch_inplace(int8_t offset) {
        pitch = utils::safe_add(pitch, offset);
        return *this;
    }

    inline Note& shift_velocity_inplace(int8_t offset) {
        velocity = utils::safe_add(velocity, offset);
        return *this;
    }

    [[nodiscard]] inline Message note_on_msg(float tpq, uint8_t channel) const {
        return Message::NoteOn(start.get_tick(timeUnit, tpq), channel, pitch, velocity);
    }

    [[nodiscard]] inline Message note_off_msg(float tpq, uint8_t channel) const {
        return Message::NoteOff(end_time().get_tick(timeUnit, tpq), channel, pitch, velocity);
    }

};

class ControlChange: public TimeContainer {
public:
    uint8_t number;
    uint8_t value;

    inline ControlChange(TimeContainer time, uint8_t number, uint8_t value) :
        TimeContainer(time), number(number), value(value) {};

    inline ControlChange(const ControlChange &) = default;

    [[nodiscard]] inline ControlChange copy() const { return {*this}; }

    [[nodiscard]] inline Message to_msg(float tpq, uint8_t channel) const {
        return Message::ControlChange(get_tick(tpq), channel, number, value);
    }

};

class TimeSignature: public TimeContainer {
public:
    uint8_t numerator;
    uint8_t denominator;

    inline TimeSignature(TimeContainer time, uint8_t numerator, uint8_t denominator) :
        TimeContainer(time), numerator(numerator), denominator(denominator) {};

    inline TimeSignature(TimeContainer time, minimidi::message::TimeSignature msg) :
        TimeContainer(time), numerator(msg.numerator), denominator(msg.denominator) {};

    inline TimeSignature(const TimeSignature &) = default;

    [[nodiscard]] inline TimeSignature copy() const { return {*this}; };

    [[nodiscard]] inline Message to_msg(float tpq) const {
        return Message::TimeSignature(get_tick(tpq), numerator, denominator);
    };

};

class KeySignature: public TimeContainer {
public:
    int8_t key;
    uint8_t tonality;

    inline KeySignature(TimeContainer time, int8_t key, uint8_t tonality) :
        TimeContainer(time), key(key), tonality(tonality) {};

    inline KeySignature(TimeContainer time, minimidi::message::KeySignature msg) :
    TimeContainer(time), key(msg.key), tonality(msg.tonality) {};

    inline KeySignature(const KeySignature &) = default;

    [[nodiscard]] inline KeySignature copy() const { return {*this}; };

    [[nodiscard]] inline std::string to_string() const {
        static const std::string MINOR_KEYS[] = {
            "bc", "bg", "bd", "ba", "be", "bb", "f",
            "c", "g", "d", "a", "e", "b", "#f", "#c"
        };
        static const std::string MAJOR_KEYS[] = {
            "bC", "bG", "bD", "bA", "bE", "bB", "F", "C",
            "G", "D", "A", "E", "B", "#F", "#C"
        };

        return this->tonality ? MINOR_KEYS[this->key + 7] : MAJOR_KEYS[this->key + 7];
    };

    [[nodiscard]] inline Message to_msg(float tpq) const {
        return Message::KeySignature(get_tick(tpq), key, tonality);
    };

};

class Tempo: public TimeContainer {
public:
    float qpm;

    inline Tempo(TimeContainer time, float qpm) : TimeContainer(time), qpm(qpm) {};

    inline Tempo(const Tempo &) = default;

    [[nodiscard]] inline Tempo copy() const { return {*this}; };

    [[nodiscard]] inline Message to_msg(float tpq) const {
        return Message::SetTempo(get_tick(tpq), (uint32_t) (60000000. / qpm));
    };

};

class PitchBend: public TimeContainer{
public:
    int16_t value;

    inline PitchBend(TimeContainer time, int16_t value) : TimeContainer(time), value(value) {};

    inline PitchBend(const PitchBend &) = default;

    [[nodiscard]] inline PitchBend copy() const { return {*this}; };

    [[nodiscard]] inline Message to_msg(float tpq, uint8_t channel) const {
        return Message::PitchBend(get_tick(tpq), channel, value);
    };
};

class TextMeta: public TimeContainer {
public:
    std::string text;

    // copy string constructor
    inline TextMeta(TimeContainer time, std::string &text) : TimeContainer(time), text(text) {};

    // move string constructor
    inline TextMeta(TimeContainer time, std::string &&text) : TimeContainer(time), text(std::move(text)) {};

    inline TextMeta(const TextMeta &) = default;

    [[nodiscard]] inline TextMeta copy() const { return {*this}; };

    [[nodiscard]] inline Message to_msg(float tpq, minimidi::message::MetaType meta_type) const {
        return Message::Meta(get_tick(tpq), meta_type, text);
    };

    [[nodiscard]] inline Message to_lyric_msg(float tpq) const {
        return Message::Lyric(get_tick(tpq), text);
    }

    [[nodiscard]] inline Message to_marker_msg(float tpq) const {
        return Message::Marker(get_tick(tpq), text);
    }
};


typedef std::vector<Note> Notes;
typedef std::vector<ControlChange> ControlChanges;
typedef std::vector<PitchBend> PitchBends;

class Track {
public:
    std::string name;
    uint8_t program = 0;
    bool is_drum = false;
    Notes notes;
    ControlChanges controls;
    PitchBends pitch_bends;

    Track() = default;

    Track(const Track &) = default;

    [[nodiscard]] Track copy() const { return {*this}; }

    [[nodiscard]] bool empty() const {
        return notes.empty() && controls.empty() && pitch_bends.empty();
    }

    [[nodiscard]] Track shift_pitch(int8_t offset) const {
        Track newTrack(*this);
        for (auto &note: newTrack.notes) note = note.shift_pitch_inplace(offset);
        return newTrack;
    };

    [[nodiscard]] Track shift_time(float offset) const {
        Track newTrack(*this);
        for (auto &note: newTrack.notes) note.shift_time_inplace(offset);
        for (auto &controlChange: newTrack.controls) controlChange.shift_time_inplace(offset);
        for (auto &pitchBend: newTrack.pitch_bends) pitchBend.shift_time_inplace(offset);
        return newTrack;
    };

    [[nodiscard]] Track shift_velocity(int8_t offset) const {
        Track newTrack(*this);
        for (auto &note: newTrack.notes) note = note.shift_velocity_inplace(offset);
        return newTrack;
    };

    ControlChanges filter_controls(const std::function<bool(const ControlChange &)> &filter) const {
        return utils::filter(controls, filter);
    }

    Notes filter_notes(const std::function<bool(const Note &)> &filter) const {
        return utils::filter(notes, filter);
    };

    PitchBends filter_pitch_bends(const std::function<bool(const PitchBend &)> &filter) const {
        return utils::filter(pitch_bends, filter);
    };

    [[nodiscard]] ControlChanges get_control_changes(uint8_t control_number) const {
        return filter_controls([control_number](const ControlChange &c) { return c.number == control_number; });
    }

    [[nodiscard]] Notes clip_notes(float start, float end, bool clip_end) const {
        if(notes.empty()) return {};

        std::function<bool(const Note &)> filter;
        std::function<Time(const Note &)> get_end;

        if (clip_end)
            get_end = [](const Note &n) { return n.end_time(); };
        else
            get_end = [](const Note &n) { return n.start; };
        switch (notes[0].timeUnit) {
            case TimeUnit::Tick:
                filter = [start, end, get_end](const Note &n) {
                    return n.start.tick  >= (uint32_t) start && get_end(n).tick < (uint32_t) end;
                };
                break;
            case TimeUnit::Quarter:
                filter = [start, end, get_end](const Note &n) {
                    return n.start.quarter >= start && get_end(n).quarter < end;
                };
                break;
            case TimeUnit::Second:
                filter = [start, end, get_end](const Note &n) {
                    return n.start.second >= start && get_end(n).second < end;
                };
        }
        return filter_notes(filter);
    };

    [[nodiscard]] ControlChanges clip_control_changes(float start, float end) const {
       return utils::clip(controls, start, end);
    };

    [[nodiscard]] PitchBends clip_pitch_bends(float start, float end) const {
        return utils::clip(pitch_bends, start, end);
    };

    [[nodiscard]] Track clip(float start, float end, bool clip_end) const {
        Track newTrack(*this);
        newTrack.notes = clip_notes(start, end, clip_end);
        newTrack.controls = clip_control_changes(start, end);
        newTrack.pitch_bends = clip_pitch_bends(start, end);
        return newTrack;
    };

    Track &sort() {
        utils::sort_by_start(notes);
        utils::sort_by_time(controls);
        utils::sort_by_time(pitch_bends);
        return *this;
    };

    [[nodiscard]] inline size_t note_num() const {
        return this->notes.size();
    }

    [[nodiscard]] inline float end_time() const {
        float time = 0;
        for (auto const &note: this->notes)
            time = std::max(time, note.end_time_float());
        return time;
    };

    [[nodiscard]] inline float start_time() const {
        float time = FLT_MAX;
        for (auto const &note: this->notes)
            time = std::min(time, note.start_float());
        return time;
    }

    [[nodiscard]] Pianoroll frame_pianoroll(float tpq, uint16_t quantization = 16) const {
        Pianoroll pianoroll = empty_pianoroll(quantization);
        if(notes.empty()) return pianoroll;
        auto quantize = utils::get_quantize_func(notes[0].timeUnit, tpq, quantization);
        for (const auto &note: notes) {
            uint64_t start = quantize(note.start);
            uint64_t duration = quantize(note.duration);
            pianoroll.row(note.pitch).segment((long) start, duration) = true; // eigen index should be long
        }

        return pianoroll;
    };

    [[nodiscard]] Pianoroll onset_pianoroll(float tpq, uint16_t quantization = 16) const {
        Pianoroll pianoroll = empty_pianoroll(quantization);
        if(notes.empty()) return pianoroll;
        auto quantize = utils::get_quantize_func(notes[0].timeUnit, tpq, quantization);
        for (const auto &note: notes) {
            uint64_t start = quantize(note.start);
            pianoroll.row(note.pitch).col((long) start) = true; // eigen index should be long
        }

        return pianoroll;
    };

    [[nodiscard]] NoteArray note_array() const {
        return NoteArray(*this);
    };

    [[nodiscard]] size_t msg_num() const {
        return notes.size() * 2 + controls.size() + pitch_bends.size() + 2; // 2 for track name and program change
    };

    [[nodiscard]] std::vector<Message> to_msgs(float tpq, size_t additional_space = 0) {
        uint8_t channel = is_drum ? 9 : 0;
        std::vector<Message> msgs;
        msgs.reserve(msg_num() + additional_space);
        msgs.emplace_back(Message::TrackName(0, name));
        msgs.emplace_back(Message::ProgramChange(0, channel, program));
        for (auto &note: notes) {
            msgs.emplace_back(note.note_on_msg(tpq, channel));
            msgs.emplace_back(note.note_off_msg(tpq, channel));
        }
        for (auto &control: controls) {
            msgs.emplace_back(control.to_msg(tpq, channel));
        }
        for (auto &pitch_bend: pitch_bends) {
            msgs.emplace_back(pitch_bend.to_msg(tpq, channel));
        }
        pdqsort_branchless(
            msgs.begin(), msgs.end(),
            [](const Message &a, const Message &b) { return a.get_time() < b.get_time(); }
        );
        return msgs;
    };

protected:
    [[nodiscard]] inline Pianoroll empty_pianoroll(uint16_t quantization = 16) const {
        return Pianoroll::Zero(MIDI_PITCH_NUMS, std::ceil(this->end_time() * (float) quantization / 4));
    };
};

namespace utils {
typedef std::tuple<uint8_t, uint8_t> TrackIdx;

Track &get_track(
    std::map<TrackIdx, Track> &track_map,
    std::vector<Track> &stragglers,
    uint8_t channel, uint8_t program,
    size_t message_num, bool create_new) {
    // TrackIdx track_idx = {channel, program};
    auto track_idx = std::make_tuple(channel, program);
    if (track_map.find(track_idx) == track_map.end()) {
        auto &track = stragglers[channel];
        if (!create_new) return track;
        Track new_track;
        if (!track.empty()) {
            new_track = Track(stragglers[channel]); // copy from stragglers
        }
        new_track.program = program;
        new_track.is_drum = channel == 9;
        new_track.notes.reserve(message_num / 2 + 10);
        track_map[track_idx] = new_track;
    }
    return track_map[track_idx];
}


class NoteOn {
public:
    uint32_t start = 0;
    int8_t velocity = 0;

    NoteOn() = default;

    [[nodiscard]] inline bool empty() const { return velocity == 0; };

    inline void reset() {
        start = 0;
        velocity = 0;
    };
};

class NoteOnQueue {
private:
    std::queue<NoteOn> queue;
public:
    NoteOn front;

    NoteOnQueue() = default;

    inline size_t size() { return front.empty() ? 0 : queue.size() + 1; };

    [[nodiscard]] inline bool empty() const { return front.empty(); };

    inline void emplace(uint32_t start, int8_t velocity) {
        if (!front.empty()) queue.push(front);
        front.start = start;
        front.velocity = velocity;
    };

    inline void pop() {
        if (queue.empty()) front.reset();
        else {
            front = queue.front();
            queue.pop();
        }
    };

};
}

class Score {
public:
    uint16_t ticks_per_quarter;
    std::vector<Track> tracks;
    std::vector<Tempo> tempos;
    std::vector<TimeSignature> time_signatures;
    std::vector<KeySignature> key_signatures;
    std::vector<TextMeta> lyrics, markers;

    Score() = default;

    Score(const Score &) = default;

    [[nodiscard]] Score copy() const { return {*this}; }

    explicit Score(minimidi::file::MidiFile &midi, TimeUnit time_unit = TimeUnit::Tick) {
        size_t track_num = midi.track_num();
        ticks_per_quarter = midi.get_tick_per_quarter();
        auto tpq = (float) ticks_per_quarter;

        using std::cout, std::endl;
        for (size_t i = 0; i < track_num; ++i) {
            auto &midi_track = midi.track(i);
            std::map<utils::TrackIdx, Track> track_map; // (channel, program) -> Track
            std::vector<Track> stragglers(16); // channel -> Track
            uint8_t cur_instr[16] = {0}; // channel -> program
            std::string cur_name;

            utils::NoteOnQueue last_note_on[16][128]; // (channel, pitch) -> (start, velocity)
            // iter midi messages in the track
            size_t message_num = midi_track.message_num();
            for (size_t j = 0; j < message_num; ++j) {
                auto &msg = midi_track.message(j);
//                float start = (float) msg.get_time() / tpq;
                uint32_t cur_tick = msg.get_time();
                TimeContainer start(cur_tick, tpq, time_unit);
                switch (msg.get_type()) {
                    case (minimidi::message::MessageType::NoteOn): {
                        uint8_t velocity = msg.get_velocity();
                        if (velocity != 0) {
                            auto pitch = msg.get_pitch();
                            if (pitch >= 128)
                                throw std::range_error("Get pitch=" + std::to_string((int) pitch));
                            if (velocity >= 128)
                                throw std::range_error("Get velocity=" + std::to_string((int) velocity));
                            last_note_on[msg.get_channel()][pitch].emplace(cur_tick, (int8_t) velocity);
                            break;
                        } // if velocity is zero, turn to note off case
                    }
                    case (minimidi::message::MessageType::NoteOff): {
                        uint8_t channel = msg.get_channel();
                        uint8_t pitch = msg.get_pitch();
                        if (pitch >= 128)
                            throw std::range_error("Get pitch=" + std::to_string((int) pitch));
                        auto &note_on_queue = last_note_on[channel][pitch];
                        auto &track = utils::get_track(track_map, stragglers, channel, cur_instr[channel], message_num,
                                                       true);
                        while ((!note_on_queue.empty()) && (cur_tick > note_on_queue.front.start)) {
                            auto const &note_on = note_on_queue.front;
                            uint32_t duration = cur_tick - note_on.start;
                            track.notes.emplace_back(
                                Time(note_on.start, tpq, time_unit),
                                Time(duration, tpq, time_unit),
                                time_unit, (int8_t) pitch, note_on.velocity);
                            note_on_queue.pop();
                        }
                        break;
                    }
                    case (minimidi::message::MessageType::ProgramChange): {
                        uint8_t channel = msg.get_channel();
                        uint8_t program = msg.get_program();
                        if (program >= 128)
                            throw std::range_error("Get program=" + std::to_string((int) program));
                        cur_instr[channel] = program;
                        break;
                    }
                    case (minimidi::message::MessageType::ControlChange): {
                        uint8_t channel = msg.get_channel();
                        uint8_t program = cur_instr[channel];
                        auto &track = utils::get_track(track_map, stragglers, channel, program, message_num, false);
                        uint8_t control_number = msg.get_control_number();
                        uint8_t control_value = msg.get_control_value();
                        if (control_number >= 128)
                            throw std::range_error("Get control_number=" + std::to_string((int) control_number));
                        if (control_value >= 128)
                            throw std::range_error("Get control_value=" + std::to_string((int) control_value));
                        track.controls.emplace_back(start, msg.get_control_number(), msg.get_control_value());
                        break;
                    }
                    case (minimidi::message::MessageType::PitchBend): {
                        cout << "PitchBend" << endl;
                        uint8_t channel = msg.get_channel();
                        uint8_t program = cur_instr[channel];
                        auto &track = utils::get_track(track_map, stragglers, channel, program, message_num, false);
                        auto value = msg.get_pitch_bend();
                        if (value < minimidi::message::MIN_PITCHBEND || value > minimidi::message::MAX_PITCHBEND)
                            throw std::range_error("Get pitch_bend=" + std::to_string((int) value));
                        track.pitch_bends.emplace_back(start, value);
                    }
                        // Meta Message
                    case (minimidi::message::MessageType::Meta): {
                        switch (msg.get_meta_type()) {
                            case (minimidi::message::MetaType::TrackName): {
                                auto data = msg.get_meta_value();
                                auto tmp = std::string(data.begin(), data.end());
                                cur_name = utils::strip_non_utf_8(&tmp);
                                break;
                            }
                            case (minimidi::message::MetaType::TimeSignature): {
                                time_signatures.emplace_back(start, msg.get_time_signature());
                                break;
                            }
                            case (minimidi::message::MetaType::SetTempo): {
                                tempos.emplace_back(start, (float) ((double) 60000000. / (double) msg.get_tempo()));
                                break;
                            }
                            case (minimidi::message::MetaType::KeySignature): {
                                key_signatures.emplace_back(start, msg.get_key_signature());
                                break;
                            }
                            case (minimidi::message::MetaType::Lyric): {
                                auto data = msg.get_meta_value();
                                auto tmp = std::string(data.begin(), data.end());
                                lyrics.emplace_back(start, utils::strip_non_utf_8(&tmp));
                                break;
                            }
                            case (minimidi::message::MetaType::Marker): {
                                auto data = msg.get_meta_value();
                                auto tmp = std::string(data.begin(), data.end());
                                markers.emplace_back(start, utils::strip_non_utf_8(&tmp));
                                break;
                            }
                            default:
                                break;
                        }
                        break;
                    }
                    default:
                        break;
                }
            }
            for (auto &track_pair: std::move(track_map)) {
                Track track = track_pair.second;
                if (track.empty()) continue;
                track.name = cur_name;
                track.notes.shrink_to_fit();
                tracks.push_back(track);
            }
        }
        utils::sort_by_time(time_signatures);
        utils::sort_by_time(key_signatures);
        utils::sort_by_time(tempos);
        utils::sort_by_time(lyrics);
        utils::sort_by_time(markers);
    };

    inline static Score from_midi(minimidi::file::MidiFile &midi) {
        return Score(midi);
    };

    inline static Score from_file(const std::string &filepath) {
        auto midi = minimidi::file::MidiFile::from_file(filepath);
        return Score(midi);
    };

    Score &sort() {
        utils::sort_by_time(time_signatures);
        utils::sort_by_time(key_signatures);
        utils::sort_by_time(tempos);
        utils::sort_by_time(lyrics);
        utils::sort_by_time(markers);
        for (auto &track: tracks) { track.sort(); }
        return *this;
    };

    [[nodiscard]] Score shift_pitch(int8_t offset) const {
        Score new_score(*this);
        for (auto &track: new_score.tracks) track = track.shift_pitch(offset);
        return new_score;
    };

    [[nodiscard]] Score shift_time(float offset) const {
        Score new_score(*this);

        for (auto &track: new_score.tracks) track = track.shift_time(offset);
        for (auto &tempo: new_score.tempos) tempo.shift_time_inplace(offset);
        for (auto &time_signature: new_score.time_signatures) time_signature.shift_time_inplace(offset);
        for (auto &key_signature: new_score.key_signatures) key_signature.shift_time_inplace(offset);
        for (auto &lyric: new_score.lyrics) lyric.shift_time_inplace(offset);
        for (auto &marker: new_score.markers) marker.shift_time_inplace(offset);

        return new_score;
    };

    Score shift_velocity(int8_t offset) {
        Score new_score(*this);
        for (auto &track: new_score.tracks) track = track.shift_velocity(offset);
        return new_score;
    };

    Score filter_notes(std::function<bool(const Note &)> &filter) {
        Score new_score(*this);
        for (auto &track: new_score.tracks) track.notes = track.filter_notes(filter);
        return new_score;
    };

    Score clip(float start, float end, bool clip_end) {
        Score new_score(*this);
        for (auto &track: new_score.tracks) track = track.clip(start, end, clip_end);
        new_score.tempos = utils::clip(tempos, start, end);
        new_score.time_signatures = utils::clip(time_signatures, start, end);
        new_score.key_signatures = utils::clip(key_signatures, start, end);
        new_score.lyrics = utils::clip(lyrics, start, end);
        new_score.markers = utils::clip(markers, start, end);
        return new_score;
    };

    [[nodiscard]] inline size_t note_num() const {
        size_t num = 0;
        for (auto const &track: this->tracks)
            num += track.note_num();
        return num;
    };

    [[nodiscard]] inline float end_time() const {
        float time = 0.;
        for (auto const &track: this->tracks)
            time = std::max(time, track.end_time());
        return time;
    };

    [[nodiscard]] inline float start_time() const {
        float time = FLT_MAX;
        for (auto const &track: this->tracks)
            time = std::min(time, track.start_time());
        return time;
    };
};

NoteArray::NoteArray(const score::Track &track) {
    size_t note_num = track.note_num();
    name = track.name;
    program = track.program;
    start = F32Vector(note_num);
    duration = F32Vector(note_num);
    pitch = U8Vector(note_num);
    velocity = U8Vector(note_num);

    long i = 0;
    for (auto const &note: track.notes) {
        start[i] = note.start_float();
        duration[i] = note.duration_float();
        pitch[i] = note.pitch;
        velocity[i] = note.velocity;
        i++;
    }
}

}

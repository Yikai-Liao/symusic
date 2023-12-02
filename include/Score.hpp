#include <cmath>
#include <map>
#include <queue>
#include <algorithm>
#ifndef __SCORE_HPP__
#define __SCORE_HPP__

#include <cmath>
#include <cfloat>
#include <exception>
#include <functional>
#include <sstream>
#include <string>
#include <iostream>
#include <utility>
#include <pdqsort.h>
//#include <Eigen/Core>
#include "MiniMidi.hpp"
#include "MetaMacro.h"
namespace score {

typedef uint8_t     u8;
typedef int8_t      i8;
typedef uint32_t    u32;
typedef int32_t     i32;
typedef float       f32;
typedef double      f64;
#define vec std::vector

#define SS_PRECISION 2

namespace utils {
inline i8 safe_add(i8 a, i8 b) {
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

template<typename T>
struct TimeUnit {
    typedef T unit;
};

#define TIME_UNIT_TYPE(UNIT_NAME, UNIT_TYPE) \
    struct UNIT_NAME: public TimeUnit<UNIT_TYPE> {};

TIME_UNIT_TYPE(Tick,    i32)
TIME_UNIT_TYPE(Quarter, f32)
TIME_UNIT_TYPE(Second,  f32)

template<typename T>
struct TimeStamp {
    typedef typename T::unit unit;
    unit time;

    TimeStamp() = default;
    TimeStamp(const TimeStamp &) = default;
    explicit TimeStamp(const unit &time) : time(time) {};
};

#define TIME_EVENT                                                          \
    template<typename T>                                                    \
    struct CLASS_NAME : public TimeStamp<T>

#define DEFAULT_METHODS                                                     \
    typedef typename T::unit unit;                                          \
    CLASS_NAME() = default;                                                 \
    CLASS_NAME(const CLASS_NAME &) = default;                               \
    [[nodiscard]] inline CLASS_NAME copy() const { return {*this}; };       \
    CLASS_NAME & shift_time_inplace(unit offset) {                          \
        this->time += offset; return *this;                                 \
    }                                                                       \
    [[nodiscard]] CLASS_NAME shift_time(unit offset) const {                \
        return copy().shift_time_inplace(offset);                           \
    }

#define SHIFT_I8(ATTR)                                                      \
    CLASS_NAME & shift_##ATTR##_inplace (unit offset) {                     \
        ATTR = utils::safe_add(ATTR, offset); return *this;                 \
    }                                                                       \
    [[nodiscard]] CLASS_NAME shift_##ATTR (unit offset) {                  \
        return copy().shift_##ATTR##_inplace(offset);                       \
    }


#define CLASS_NAME Note
TIME_EVENT {
    DEFAULT_METHODS
    unit duration;
    i8 pitch{}, velocity{};

    Note(unit time, unit duration, i8 pitch, i8 velocity) :
        TimeStamp<T>(time), duration(duration), pitch(pitch), velocity(velocity) {};

    template<typename U>
    Note(unit time, const Note<U>& other):
        TimeStamp<T>(time), duration(other.duration), pitch(other.pitch), velocity(other.velocity) {};

    SHIFT_I8(pitch)

    SHIFT_I8(velocity)

    [[nodiscard]] bool empty() const {
        return duration <= 0 || velocity <= 0 || this->time <= 0;
    }

    [[nodiscard]] unit end() const { return this->time + duration; }

    [[nodiscard]] std::string to_string() const {
        std::stringstream ss;
        ss << std::fixed << std::setprecision(SS_PRECISION);
        ss << "Note"
           << "(time="  << this->time
           << ", duration=" << duration
           << ", pitch="    << (i32) pitch
           << ", velocity=" << (i32) velocity
           << ")";
        return ss.str();
    }
};
#undef CLASS_NAME // Note

#define CLASS_NAME ControlChange
TIME_EVENT{
    DEFAULT_METHODS
    u8 number{}, value{};

    ControlChange(unit time, u8 number, u8 value) :
        TimeStamp<T>(time), number(number), value(value) {};

    template<typename U>
    ControlChange(unit time, const ControlChange<U>& other):
        TimeStamp<T>(time), number(other.number), value(other.value) {};

    [[nodiscard]] std::string to_string() const {
        std::stringstream ss;
        ss << std::fixed << std::setprecision(SS_PRECISION);
        ss << "ControlChange"
           << "(time="  << this->time
           << ", number=" << (i32) number
           << ", value="    << (i32) value
           << ")";
        return ss.str();
    }
};
#undef CLASS_NAME // ControlChange

#define CLASS_NAME TimeSignature
TIME_EVENT{
    DEFAULT_METHODS
    u8 numerator{}, denominator{};

    TimeSignature(unit time, u8 numerator, u8 denominator) :
        TimeStamp<T>(time), numerator(numerator), denominator(denominator) {};

    TimeSignature(unit time, minimidi::message::TimeSignature msg) :
        TimeStamp<T>(time), numerator(msg.numerator), denominator(msg.denominator) {};

    template<typename U>
    TimeSignature(unit time, const TimeSignature<U>& other):
        TimeStamp<T>(time), numerator(other.numerator), denominator(other.denominator) {};

    [[nodiscard]] std::string to_string() const {
        std::stringstream ss;
        ss << std::fixed << std::setprecision(SS_PRECISION);
        ss << "TimeSignature"
           << "(time="  << this->time
           << ", numerator=" << (i32) numerator
           << ", denominator="    << (i32) denominator
           << ")";
        return ss.str();
    }
};
#undef CLASS_NAME // TimeSignature


#define CLASS_NAME KeySignature
TIME_EVENT{
    DEFAULT_METHODS
    i8 key{};
    u8 tonality{};

    KeySignature(unit time, i8 key, u8 tonality) :
        TimeStamp<T>(time), key(key), tonality(tonality) {};

    KeySignature(unit time, minimidi::message::KeySignature msg) :
        TimeStamp<T>(time), key(msg.key), tonality(msg.tonality) {};

    template<typename U>
    KeySignature(unit time, const KeySignature<U>& other):
        TimeStamp<T>(time), key(other.key), tonality(other.tonality) {};

    [[nodiscard]] std::string to_string() const {
        std::stringstream ss;
        ss << std::fixed << std::setprecision(SS_PRECISION);
        ss << "KeySignature"
           << "(time="  << this->time
           << ", key=" << (i32) key
           << ", tonality="    << (i32) tonality
           << ")";
        return ss.str();
    }
};
#undef CLASS_NAME // KeySignature

#define CLASS_NAME Tempo
TIME_EVENT{
    DEFAULT_METHODS
    f32 qpm{};

    Tempo(unit time, f32 qpm) : TimeStamp<T>(time), qpm(qpm) {};

    template<typename U>
    Tempo(unit time, const Tempo<U>& other): TimeStamp<T>(time), qpm(other.qpm) {};

    [[nodiscard]] std::string to_string() const {
        std::stringstream ss;
        ss << std::fixed << std::setprecision(SS_PRECISION);
        ss << "Tempo"
           << "(time="  << this->time
           << ", qpm=" << qpm
           << ")";
        return ss.str();
    }
};
#undef CLASS_NAME // Tempo

#define CLASS_NAME PitchBend
TIME_EVENT{
    DEFAULT_METHODS
    i32 value{};

    PitchBend(unit time, i32 value) : TimeStamp<T>(time), value(value) {};

    template<typename U>
    PitchBend(unit time, const PitchBend<U>& other): TimeStamp<T>(time), value(other.value) {};

    [[nodiscard]] std::string to_string() const {
        std::stringstream ss;
        ss << std::fixed << std::setprecision(SS_PRECISION);
        ss << "PitchBend"
           << "(time="  << this->time
           << ", value=" << value
           << ")";
        return ss.str();
    }
};
#undef CLASS_NAME // PitchBend

#define CLASS_NAME TextMeta
TIME_EVENT{
    DEFAULT_METHODS
    std::string text{};

    TextMeta(unit time, std::string text) : TimeStamp<T>(time), text(std::move(text)) {};

    template<typename U>
    TextMeta(unit time, const TextMeta<U>& other): TimeStamp<T>(time), text(other.text) {};

    [[nodiscard]] std::string to_string() const {
        std::stringstream ss;
        ss << std::fixed << std::setprecision(SS_PRECISION);
        ss << "TextMeta"
           << "(time="  << this->time
           << ", text=\"" << text
           << "\")";
        return ss.str();
    }
};
#undef CLASS_NAME // TextMeta
#undef TIME_EVENT
#undef DEFAULT_METHODS

namespace utils {
template<typename T>
void sort_by_time(vec<T> &events) {
    pdqsort(events.begin(), events.end(), [](const T &a, const T &b) {
        return a.time < b.time;
    });
}

template<typename T>
inline vec<T> filter(const vec<T> &data, const std::function<bool(const T &)> & _filter) {
    if (data.empty()) return {};
    vec<T> new_data;
    new_data.reserve(data.size());
    std::copy_if(
        data.begin(), data.end(), std::back_inserter(new_data), _filter
    );
    new_data.shrink_to_fit();
    return new_data;
}

template<typename T>
vec<T> clip_by_time(const vec<T> &data, typename T::unit start, typename T::unit end) {
    std::function<bool(const T &)> func = [start, end](const T &event) {
        return (event.time >= start) && (event.time < end);
    };
    return filter(data, func);
}

template<typename T>
vec<Note<T>> clip_notes(
    const vec<Note<T>> &notes,
    typename T::unit start, typename T::unit end, bool clip_end) {
    if (clip_end) {
        std::function<bool(const Note<T> &)> func = [start, end](const Note<T> &note) {
            return (note.time >= start) && (note.time + note.duration <= end);
        };
        return utils::filter(notes, func);
    }else
        return clip_by_time(notes, start, end);
}

template<typename T>
vec<T> &shift_time_inplace(vec<T> &data, typename T::unit offset) {
    for (auto &event: data)
        event.shift_time_inplace(offset);
    return data;
};

template<typename T>
vec<T> shift_time(const vec<T> &data, typename T::unit offset) {
    vec<T> new_data;
    new_data.reserve(data.size());
    for (auto &event: data)
        new_data.emplace_back(event.shift_time(offset));
    return new_data;
};
}

using std::to_string;
using std::ostream;
using namespace minimidi::message;

#define STRING_METHODS(i, CLASS_NAME)                                  \
    template <typename T>                                               \
    std::string to_string(const CLASS_NAME<T> & data) {                 \
        return data.to_string();                                        \
    }                                                                   \
    template <typename T>                                               \
    ostream& operator<<(ostream& out, const CLASS_NAME<T>& data) {      \
             out <<data.to_string(); return out;                        \
    }                                                                   \
    template <typename T>                                               \
    ostream& operator<<(ostream& out, const vec<CLASS_NAME<T>>& data) {     \
        if(data.empty()){                                               \
            out << "[]"; return out;                                    \
        } size_t j = 0; out << "[";                                     \
        for(; j<data.size()-1; ++j){                                    \
            out << data[j] << ", ";                                     \
        }   out << data[j] << "]";                                      \
        return out;                                                     \
    }                                                                   \
    template<typename T>                                                \
    std::string to_string(const vec<CLASS_NAME<T>> & data){               \
        std::stringstream ss;                                           \
        ss << data;                                                     \
        return ss.str();                                                \
    }

// avoid overload other types
REPEAT_ON(
    STRING_METHODS,
    Note, ControlChange, TimeSignature, KeySignature, Tempo, PitchBend, TextMeta
)


template <typename T>
struct Track{
    typedef typename T::unit unit;

    std::string name;
    u8 program = 0;
    bool is_drum = false;
    vec<Note<T>> notes;
    vec<ControlChange<T>> controls;
    vec<PitchBend<T>> pitch_bends;

    Track() = default;
    Track(const Track&) = default;
    Track(std::string name, u8 program, bool is_drum,
          vec<Note<T>> notes,
          vec<ControlChange<T>> controls,
          vec<PitchBend<T>> pitch_bends) :
        name(std::move(name)), program(program), is_drum(is_drum),
        notes(std::move(notes)), controls(std::move(controls)), pitch_bends(std::move(pitch_bends)) {};

    Track(std::string name, u8 program, bool is_drum) :
        name(std::move(name)), program(program), is_drum(is_drum) {};

    [[nodiscard]] Track copy() const { return {*this}; }

    Track& sort_inplace() {
        utils::sort_by_time(notes);
        utils::sort_by_time(controls);
        utils::sort_by_time(pitch_bends);
        return *this;
    }

    [[nodiscard]] Track sort() const { return copy().sort_inplace(); }

    [[nodiscard]] unit end() const {
        unit t = 0;
        for (auto const &note: notes)
            t = std::max(t, note.end());
        return t;
    }

    [[nodiscard]] unit start() const {
        unit t = 0; // using 0 to avoid get unit max
        for (auto const &note: notes)
            t = std::min(t, -note.time);
        return -t;
    }

    [[nodiscard]] size_t note_num() const { return notes.size(); }

    [[nodiscard]] bool empty() const {
        return notes.empty() && controls.empty() && pitch_bends.empty();
    }

    [[nodiscard]] Track clip(unit start, unit end, bool clip_end = false) const {
        Track new_track;
        // copy some attributes
        new_track.name = name;
        new_track.program = program;
        new_track.is_drum = is_drum;
        // clip notes
        new_track.notes = utils::clip_notes(notes, start, end, clip_end);
        // clip others
        new_track.controls = utils::clip_by_time(controls, start, end);
        new_track.pitch_bends = utils::clip_by_time(pitch_bends, start, end);
        return new_track;
    }

    [[nodiscard]] std::string to_string() const {
        std::stringstream ss;
        ss << std::fixed << std::setprecision(SS_PRECISION);
        ss << "Track"
           << "(name=\"" << name
           << "\", program=" << (i32) program
           << ", is_drum=" << (is_drum ? "True" : "False")
           << ", notes=" << notes
           << ", controls=" << controls
           << ", pitch_bends=" << pitch_bends
           << ")";
        return ss.str();
    }

    Track& shift_time_inplace(unit offset) {
        for(auto &note: notes)
            note.shift_time_inplace(offset);
        for(auto &control: controls)
            control.shift_time_inplace(offset);
        for(auto &pitch_bend: pitch_bends)
            pitch_bend.shift_time_inplace(offset);
        return *this;
    }

    [[nodiscard]] Track shift_time(unit offset) const {
        return copy().shift_time_inplace(offset);
    }

    Track& shift_pitch_inplace(i8 offset) {
        for(auto &note: notes)
            note.shift_pitch_inplace(offset);
        return *this;
    }

    [[nodiscard]] Track shift_pitch(i8 offset) const {
        return copy().shift_pitch_inplace(offset);
    }

    Track& shift_velocity_inplace(i8 offset) {
        for(auto &note: notes)
            note.shift_velocity_inplace(offset);
        return *this;
    }

    [[nodiscard]] Track shift_velocity(i8 offset) const {
        return copy().shift_velocity_inplace(offset);
    }
};

// get to_string and overload operator<< for Track and vec<Track>
STRING_METHODS(0, Track)
#undef STRING_METHODS

#undef STRING_METHODS

template<typename T>
struct Score{
protected:
    template<typename U>
    void from(U& other);

public:
    i32 ticks_per_quarter{};
    vec<Track<T>> tracks;
    vec<TimeSignature<T>> time_signatures;
    vec<KeySignature<T>> key_signatures;
    vec<Tempo<T>> tempos;
    vec<TextMeta<T>> lyrics, markers;

    Score() = default;
    Score(const Score&) = default;
    Score(Score&&)  noexcept = default;
    [[nodiscard]] Score copy() const { return {*this}; }

    template<typename U>
    explicit Score(const Score<U> &other) { from(other); }

    explicit Score(minimidi::file::MidiFile &midi);

    static Score from_file(const std::string &filename) {
        auto midi = minimidi::file::MidiFile::from_file(filename);
        return Score(midi);
    }

    static Score from_midi(minimidi::file::MidiFile &midi) {
        return Score(midi);
    }

    Score& sort_inplace() {
        utils::sort_by_time(time_signatures);
        utils::sort_by_time(key_signatures);
        utils::sort_by_time(tempos);
        utils::sort_by_time(lyrics);
        utils::sort_by_time(markers);
        for(auto &track: tracks) track.sort_inplace();
        return *this;
    }

    [[nodiscard]] Score sort() {
        return copy().sort_inplace();
    }

    [[nodiscard]] Score clip(typename T::unit start, typename T::unit end, bool clip_end = false) const {
        Score new_score;
        new_score.ticks_per_quarter = ticks_per_quarter;
        new_score.time_signatures = utils::clip_by_time(time_signatures, start, end);
        new_score.key_signatures = utils::clip_by_time(key_signatures, start, end);
        new_score.tempos = utils::clip_by_time(tempos, start, end);
        new_score.lyrics = utils::clip_by_time(lyrics, start, end);
        new_score.markers = utils::clip_by_time(markers, start, end);
        new_score.tracks.reserve(tracks.size());
        for(auto const &track: tracks)
            new_score.tracks.emplace_back(track.clip(start, end, clip_end));
        return new_score;
    }

    // shift time pitch velocity
    Score& shift_time_inplace(typename T::unit offset) {
        utils::shift_time_inplace(time_signatures, offset);
        utils::shift_time_inplace(key_signatures, offset);
        utils::shift_time_inplace(tempos, offset);
        utils::shift_time_inplace(lyrics, offset);
        utils::shift_time_inplace(markers, offset);
        for(auto &track: tracks) track.shift_time_inplace(offset);
        return *this;
    }

    [[nodiscard]] Score shift_time(typename T::unit offset) const {
        return copy().shift_time_inplace(offset);
    }

    Score& shift_pitch_inplace(i8 offset) {
        for(auto &track: tracks) track.shift_pitch_inplace(offset);
        return *this;
    }

    [[nodiscard]] Score shift_pitch(i8 offset) const {
        return copy().shift_pitch_inplace(offset);
    }

    Score& shift_velocity_inplace(i8 offset) {
        for(auto &track: tracks) track.shift_velocity_inplace(offset);
        return *this;
    }

    [[nodiscard]] Score shift_velocity(i8 offset) const {
        return copy().shift_velocity_inplace(offset);
    }

    [[nodiscard]] typename T::unit end() const {
        typename T::unit t = 0;
        for(auto const &track: tracks)
            t = std::max(t, track.end());
        return t;
    }

    [[nodiscard]] typename T::unit start() const {
        typename T::unit t = 0;
        for(auto const &track: tracks)
            t = std::min(t, -track.start());
        return -t;
    }

    [[nodiscard]] size_t note_num() const {
        size_t num = 0;
        for(auto const &track: tracks)
            num += track.note_num();
        return num;
    }

    [[nodiscard]] bool empty() const {
        bool ans = time_signatures.empty() && key_signatures.empty() &&
                   tempos.empty() && lyrics.empty() && markers.empty();
        for (auto const &track: tracks)
            ans = ans && track.empty();
        return ans;
    }

    [[nodiscard]] std::string to_string() const {
        std::stringstream ss;
        ss << std::fixed << std::setprecision(SS_PRECISION);
        ss << "Score"
           << "(ticks_per_quarter=" << ticks_per_quarter
           << ", time_signatures=" << time_signatures
           << ", key_signatures=" << key_signatures
           << ", tempos=" << tempos
           << ", lyrics=" << lyrics
           << ", markers=" << markers
           << ", tracks=" << tracks
           << ")";
        return ss.str();
    }
};

namespace utils {
template<typename T>
struct NoteOn: public TimeStamp<T> {
    typedef typename T::unit unit;
    i8 velocity = 0;
    NoteOn() = default;
    NoteOn(unit time, i8 velocity) : TimeStamp<T>(time), velocity(velocity) {};
    [[nodiscard]] inline bool empty() const {return velocity == 0;}
    inline void reset() { this->time=0; velocity=0; }
};

template<typename T>
struct NoteOnQueue {
    typedef typename T::unit unit;
    std::queue<NoteOn<T>> queue;
    NoteOn<T> front;

    NoteOnQueue() = default;
    [[nodiscard]] inline size_t size() const {
        return front.empty() ? 0 : queue.size() + 1;
    }

    [[nodiscard]] inline bool empty() const { return front.empty();}

    inline void emplace(unit time, i8 velocity) {
        if(!front.empty()) queue.emplace(front);
        front = NoteOn<T>{time, velocity};
    }

    inline void pop() {
        if (queue.empty()) front.reset();
        else {
            front = queue.front();
            queue.pop();
        }
    }
};

typedef std::tuple<u8, u8> TrackIdx;

template <typename T>
Track<T> &get_track(
    std::map<TrackIdx, Track<T>> &track_map,
    Track<T> (& stragglers)[16],
    uint8_t channel, uint8_t program,
    size_t message_num, bool create_new) {
    // TrackIdx track_idx = {channel, program};
    auto track_idx = std::make_tuple(channel, program);
    if (track_map.find(track_idx) == track_map.end()) {
        auto &track = stragglers[channel];
        if (!create_new) return track;
        Track<T> new_track;
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
}

// read midi file using Score<Tick>
template<>
inline Score<Tick>::Score(minimidi::file::MidiFile &midi) {
    size_t track_num = midi.track_num();
    ticks_per_quarter = midi.get_tick_per_quarter();
    auto tpq = (float) ticks_per_quarter;

    using std::cout, std::endl;
    for (size_t i = 0; i < track_num; ++i) {
        auto &midi_track = midi.track(i);
        std::map<utils::TrackIdx, Track<Tick>> track_map; // (channel, program) -> Track
        Track<Tick> stragglers[16]; // channel -> Track
        uint8_t cur_instr[16] = {0}; // channel -> program
        std::string cur_name;

        utils::NoteOnQueue<Tick> last_note_on[16][128]; // (channel, pitch) -> (start, velocity)
        // iter midi messages in the track
        size_t message_num = midi_track.message_num();
        for (size_t j = 0; j < message_num; ++j) {
            auto &msg = midi_track.message(j);
//                float start = (float) msg.get_time() / tpq;
            auto cur_tick = static_cast<i32>(msg.get_time());
            switch (msg.get_type()) {
                case (minimidi::message::MessageType::NoteOn): {
                    uint8_t velocity = msg.get_velocity();
                    if (velocity != 0) {
                        auto pitch = msg.get_pitch();
                        if (pitch >= 128)
                            throw std::range_error("Get pitch=" + std::to_string((int) pitch));
                        if (velocity >= 128)
                            throw std::range_error("Get velocity=" + std::to_string((int) velocity));
                        last_note_on[msg.get_channel()][pitch].emplace(cur_tick, (i8) velocity);
                        break;
                    } // if velocity is zero, turn to note off case
                }
                case (minimidi::message::MessageType::NoteOff): {
                    uint8_t channel = msg.get_channel();
                    uint8_t pitch = msg.get_pitch();
                    if (pitch >= 128)
                        throw std::range_error("Get pitch=" + std::to_string((int) pitch));
                    auto &note_on_queue = last_note_on[channel][pitch];
                    auto &track = utils::get_track(
                        track_map, stragglers, channel,
                        cur_instr[channel], message_num,true);
                    while ((!note_on_queue.empty()) && (cur_tick > note_on_queue.front.time)) {
                        auto const &note_on = note_on_queue.front;
                        uint32_t duration = cur_tick - note_on.time;
                        track.notes.emplace_back(
                            note_on.time, duration,
                            pitch, note_on.velocity
                        );
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
                    track.controls.emplace_back(cur_tick, msg.get_control_number(), msg.get_control_value());
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
                    track.pitch_bends.emplace_back(cur_tick, value);
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
                            time_signatures.emplace_back(cur_tick, msg.get_time_signature());
                            break;
                        }
                        case (minimidi::message::MetaType::SetTempo): {
                            tempos.emplace_back(cur_tick, (float) ((double) 60000000. / (double) msg.get_tempo()));
                            break;
                        }
                        case (minimidi::message::MetaType::KeySignature): {
                            key_signatures.emplace_back(cur_tick, msg.get_key_signature());
                            break;
                        }
                        case (minimidi::message::MetaType::Lyric): {
                            auto data = msg.get_meta_value();
                            auto tmp = std::string(data.begin(), data.end());
                            lyrics.emplace_back(cur_tick, utils::strip_non_utf_8(&tmp));
                            break;
                        }
                        case (minimidi::message::MetaType::Marker): {
                            auto data = msg.get_meta_value();
                            auto tmp = std::string(data.begin(), data.end());
                            markers.emplace_back(cur_tick, utils::strip_non_utf_8(&tmp));
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
}

#define TICK2QUARTER(i, CLASS_NAME)                                             \
    inline vec<CLASS_NAME<Quarter>> to_quarter(                                 \
        const vec<CLASS_NAME<Tick>>& data, i32 ticks_per_quarter) {             \
        vec<CLASS_NAME<Quarter>> new_data;                                      \
        new_data.reserve(data.size());                                          \
        f32 tpq = static_cast<f32>(ticks_per_quarter);                          \
        for(auto const &event: data)                                            \
            new_data.emplace_back(static_cast<f32>(event.time) / tpq, event);   \
        return new_data;                                                        \
    }

REPEAT_ON(
    TICK2QUARTER,
    Note, TimeSignature, KeySignature, ControlChange, Tempo, PitchBend, TextMeta
)

#undef TICK2QUARTER

// tick to quarter
template<> template<>
inline void Score<Quarter>::from(Score<Tick> &other){
    this->ticks_per_quarter = other.ticks_per_quarter;
    time_signatures = to_quarter(other.time_signatures, ticks_per_quarter);
    key_signatures = to_quarter(other.key_signatures, ticks_per_quarter);
    tempos = to_quarter(other.tempos, ticks_per_quarter);
    lyrics = to_quarter(other.lyrics, ticks_per_quarter);
    markers = to_quarter(other.markers, ticks_per_quarter);
    tracks = vec<Track<Quarter>>(other.tracks.size());
    for(size_t i=0; i<other.tracks.size(); ++i){
        tracks[i].name = other.tracks[i].name;
        tracks[i].program = other.tracks[i].program;
        tracks[i].is_drum = other.tracks[i].is_drum;
        tracks[i].notes = to_quarter(other.tracks[i].notes, ticks_per_quarter);
        tracks[i].controls = to_quarter(other.tracks[i].controls, ticks_per_quarter);
        tracks[i].pitch_bends = to_quarter(other.tracks[i].pitch_bends, ticks_per_quarter);
    }
}

template<>
inline Score<Quarter>::Score(minimidi::file::MidiFile &midi) {
    auto score_tick = Score<Tick>(midi);
    from(score_tick);
}
} // namespace score end
#endif //MUSIC_SCORE_H
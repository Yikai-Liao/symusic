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

inline uint32_t quantize(float time, float tpq) {
    return static_cast<uint32_t>(std::round(time * tpq));
}

template<typename T>
inline bool cmp_time(T &a, T &b) { return a.time < b.time; }

template<typename T>
inline void sort_by_time(std::vector<T> &data) {
    if (!data.empty())
        pdqsort_branchless(data.begin(), data.end(), cmp_time < T > );
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
    float start;
    float duration;
    int8_t pitch;
    int8_t velocity;

    inline Note(float start, float duration, int8_t pitch, int8_t velocity = DEFAULT_VELOCITY) :
        start(start), duration(duration), pitch(pitch), velocity(velocity) {};

    inline Note(const Note &) = default;

    [[nodiscard]] inline auto copy() const { return Note(*this); }

    [[nodiscard]] inline bool empty() const { return velocity <= 0 || duration <= 0; };

    [[nodiscard]] inline float end_time() const { return start + duration; };

    [[nodiscard]] inline Note shift_pitch(int8_t offset) const {
        return {start, duration, utils::safe_add(pitch, offset), velocity};
    };

    [[nodiscard]] inline Note shift_velocity(int8_t offset) const {
        return {start, duration, pitch, utils::safe_add(velocity, offset)};
    };

    [[nodiscard]] inline Message note_on_msg(float tpq, uint8_t channel) const {
        return Message::NoteOn(utils::quantize(start, tpq), channel, pitch, velocity);
    }

    [[nodiscard]] inline Message note_off_msg(float tpq, uint8_t channel) const {
        return Message::NoteOff(utils::quantize(end_time(), tpq), channel, pitch, velocity);
    }

};

class ControlChange {
public:
    float time;
    uint8_t number;
    uint8_t value;

    inline ControlChange(float time, uint8_t number, uint8_t value) : time(time), number(number), value(value) {};

    inline ControlChange(const ControlChange &) = default;

    [[nodiscard]] inline ControlChange copy() const { return {*this}; }

    [[nodiscard]] inline Message to_msg(float tpq, uint8_t channel) const {
        return Message::ControlChange(utils::quantize(time, tpq), channel, number, value);
    }

};

class TimeSignature {
public:
    float time;
    uint8_t numerator;
    uint8_t denominator;

    inline TimeSignature(float time, uint8_t numerator, uint8_t denominator) :
        time(time), numerator(numerator), denominator(denominator) {};

    inline TimeSignature(float time, minimidi::message::TimeSignature msg) :
        time(time), numerator(msg.numerator), denominator(msg.denominator) {};

    inline TimeSignature(const TimeSignature &) = default;

    [[nodiscard]] inline TimeSignature copy() const { return {*this}; };

    [[nodiscard]] inline Message to_msg(float tpq) const {
        return Message::TimeSignature(utils::quantize(time, tpq), numerator, denominator);
    };

};

class KeySignature {
public:
    float time;
    int8_t key;
    uint8_t tonality;

    inline KeySignature(float time, int8_t key, uint8_t tonality) :
        time(time), key(key), tonality(tonality) {};

    inline KeySignature(float time, minimidi::message::KeySignature msg) :
        time(time), key(msg.key), tonality(msg.tonality) {};

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
        return Message::KeySignature(utils::quantize(time, tpq), key, tonality);
    };

};

class Tempo {
public:
    float time;
    float qpm;

    inline Tempo(float time, float qpm) : time(time), qpm(qpm) {};

    inline Tempo(const Tempo &) = default;

    [[nodiscard]] inline Tempo copy() const { return {*this}; };

    [[nodiscard]] inline Message to_msg(float tpq) const {
        return Message::SetTempo(utils::quantize(time, tpq), (uint32_t) (60000000. / qpm));
    };

};

class PitchBend {
public:
    float time;
    int16_t value;

    inline PitchBend(float time, int16_t value) : time(time), value(value) {};

    inline PitchBend(const PitchBend &) = default;

    [[nodiscard]] inline PitchBend copy() const { return {*this}; };

    [[nodiscard]] inline Message to_msg(float tpq, uint8_t channel) const {
        return Message::PitchBend(utils::quantize(time, tpq), channel, value);
    };
};

class TextMeta {
public:
    float time;
    std::string text;

    // copy string constructor
    inline TextMeta(float time, std::string &text) : time(time), text(text) {};

    // move string constructor
    inline TextMeta(float time, std::string &&text) : time(time), text(std::move(text)) {};

    inline TextMeta(const TextMeta &) = default;

    [[nodiscard]] inline TextMeta copy() const { return {*this}; };

    [[nodiscard]] inline Message to_msg(float tpq, minimidi::message::MetaType meta_type) const {
        return Message::Meta(utils::quantize(time, tpq), meta_type, text);
    };

    [[nodiscard]] inline Message to_lyric_msg(float tpq) const {
        return Message::Lyric(utils::quantize(time, tpq), text);
    }

    [[nodiscard]] inline Message to_marker_msg(float tpq) const {
        return Message::Marker(utils::quantize(time, tpq), text);
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
        for (auto &note: newTrack.notes) note = note.shift_pitch(offset);
        return newTrack;
    };

    [[nodiscard]] Track shift_time(float offset) const {
        Track newTrack(*this);
        for (auto &note: newTrack.notes) note.start += offset;
        for (auto &controlChange: newTrack.controls) controlChange.time += offset;
        return newTrack;
    };

    [[nodiscard]] Track shift_velocity(int8_t offset) const {
        Track newTrack(*this);
        for (auto &note: newTrack.notes) note = note.shift_velocity(offset);
        return newTrack;
    };

    ControlChanges filter_controls(const std::function<bool(const ControlChange &)> &filter) const {
        ControlChanges new_controls;
        new_controls.reserve(controls.size());
        std::copy_if(
            controls.begin(), controls.end(), std::back_inserter(new_controls), filter
        );
        new_controls.shrink_to_fit();
        return new_controls;
    }

    Notes filter_notes(const std::function<bool(const Note &)> &filter) const {
        Notes new_notes;
        new_notes.reserve(notes.size());
        std::copy_if(
            notes.begin(), notes.end(), std::back_inserter(new_notes), filter
        );
        new_notes.shrink_to_fit();
        return new_notes;
    };

    [[nodiscard]] ControlChanges get_control_changes(uint8_t control_number) const {
        return filter_controls([control_number](const ControlChange &c) { return c.number == control_number; });
    }

    [[nodiscard]] Notes clip_notes(float start, float end, bool clip_end) const {
        if (clip_end)
            return this->filter_notes([start, end](const Note &n) { return (n.start >= start & n.end_time() < end); });
        else
            return this->filter_notes([start, end](const Note &n) { return (n.start >= start & n.start < end); });
    };

    [[nodiscard]] ControlChanges clip_control_changes(float start, float end) const {
        return this->filter_controls([start, end](const ControlChange &c) { return c.time >= start & c.time < end; });
    };

    [[nodiscard]] Track clip(float start, float end, bool clip_end) const {
        Track newTrack(*this);

        newTrack.notes = clip_notes(start, end, clip_end);
        newTrack.controls = clip_control_changes(start, end);

        return newTrack;
    };

    Track &sort() {
        if (!notes.empty())
            pdqsort_branchless(
                notes.begin(), notes.end(),
                [](const Note &a, const Note &b) { return a.start < b.start; }
            );
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
            time = std::max(time, note.end_time());
        return time;
    };

    [[nodiscard]] inline float start_time() const {
        float time = FLT_MAX;
        for (auto const &note: this->notes)
            time = std::min(time, note.start);
        return time;
    }

    [[nodiscard]] Pianoroll frame_pianoroll(uint16_t quantization = 16) const {
        Pianoroll pianoroll = empty_pianoroll(quantization);
        for (const auto &note: notes) {
            uint64_t start = std::floor(note.start * (float) quantization / 4);
            uint64_t duration = std::floor(note.duration * (float) quantization / 4);
            pianoroll.row(note.pitch).segment((long) start, duration) = true; // eigen index should be long
        }

        return pianoroll;
    };

    [[nodiscard]] Pianoroll onset_pianoroll(uint16_t quantization = 16) const {
        Pianoroll pianoroll = empty_pianoroll(quantization);

        for (const auto &note: notes) {
            uint64_t start = std::floor(note.start * (float) quantization / 4);
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
    float start = 0;
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

    inline void emplace(float start, int8_t velocity) {
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
    std::vector<Track> tracks;
    std::vector<Tempo> tempos;
    std::vector<TimeSignature> time_signatures;
    std::vector<KeySignature> key_signatures;
    std::vector<TextMeta> lyrics, markers;

    Score() = default;

    Score(const Score &) = default;

    [[nodiscard]] Score copy() const { return {*this}; }

    explicit Score(minimidi::file::MidiFile &midi) {
        size_t track_num = midi.track_num();
        float tpq = midi.get_tick_per_quarter();
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
                float start = (float) msg.get_time() / tpq;
                switch (msg.get_type()) {
                    case (minimidi::message::MessageType::NoteOn): {
                        uint8_t velocity = msg.get_velocity();
                        if (velocity != 0) {
                            auto pitch = msg.get_pitch();
                            if (pitch >= 128)
                                throw std::range_error("Get pitch=" + std::to_string((int) pitch));
                            if (velocity >= 128)
                                throw std::range_error("Get velocity=" + std::to_string((int) velocity));
                            last_note_on[msg.get_channel()][pitch].emplace(start, (int8_t) velocity);
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
                        while ((!note_on_queue.empty()) && (start > note_on_queue.front.start)) {
                            auto const &note_on = note_on_queue.front;
                            float duration = start - note_on.start;
                            track.notes.emplace_back(note_on.start, duration, (int8_t) pitch, note_on.velocity);
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
//        pdqsort_branchless(time_signatures.begin(), time_signatures.end(), utils::cmp_time<TimeSignature>);
//        pdqsort_branchless(key_signatures.begin(), key_signatures.end(), utils::cmp_time<KeySignature>);
//        pdqsort_branchless(tempos.begin(), tempos.end(), utils::cmp_time<Tempo>);
//        pdqsort_branchless(lyrics.begin(), lyrics.end(), utils::cmp_time<TextMeta>);
//        pdqsort_branchless(markers.begin(), markers.end(), utils::cmp_time<TextMeta>);
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
        for (auto &tempo: new_score.tempos) tempo.time += offset;
        for (auto &time_signature: new_score.time_signatures) time_signature.time += offset;
        for (auto &key_signature: new_score.key_signatures) key_signature.time = offset;

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
        // TODO: clip tempo, time sig, key sig
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
        start[i] = note.start;
        duration[i] = note.duration;
        pitch[i] = note.pitch;
        velocity[i] = note.velocity;
        i++;
    }
}
}

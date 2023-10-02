#include <unordered_map>
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

    inline auto copy() const { return NoteArray(*this); };
};

inline int8_t safe_add(int8_t a, int8_t b) {
    int ans = a + b;
    if (ans > 127 || ans < 0)
        throw std::range_error("Overflow while adding " + std::to_string(a) + " and " + std::to_string(b));
    return (int8_t) ans;
}

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

    inline auto copy() const { return Note(*this); }

    [[nodiscard]] inline bool empty() const { return velocity <= 0 || duration <= 0; };

    [[nodiscard]] inline float end_time() const { return start + duration; };

    inline Note& shift_pitch(int8_t offset) { pitch = safe_add(pitch, offset); return *this;};

    inline Note& shift_velocity(int8_t offset) { velocity = safe_add(velocity, offset); return *this;};

    inline Note& shift_time(float offset) { start += offset; return *this;};
};

class ControlChange {
public:
    float time;
    uint8_t value;

    inline ControlChange(float time, uint8_t value) : time(time), value(value) {};

    inline ControlChange(const ControlChange &) = default;

    inline auto copy() const { return ControlChange(*this); }
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

    inline auto copy() const { return TimeSignature(*this); }
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

    inline auto copy() const { return KeySignature(*this); };

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
};

class Tempo {
public:
    float time;
    float qpm;

    inline Tempo(float time, float qpm) : time(time), qpm(qpm) {};

    inline Tempo(const Tempo &) = default;

    inline auto copy() const { return Tempo(*this); };
};

template<typename T>
inline bool cmp_time(T &a, T &b) { return a.time < b.time; }

template<typename T, typename F>
inline void clip_sorted(std::vector<T> &vec, float start, float end, F &&get_key) {
    // using std::binary_search() to find the range
    auto start_it = std::lower_bound(
        vec.begin(), vec.end(), start,
        [&get_key](const T &item, float time) { return get_key(item) < time; }
    );
    auto end_it = std::upper_bound(
        vec.begin(), vec.end(), end,
        [&get_key](float time, const T &item) { return time <= get_key(item); }
    );
    vec = std::move(std::vector<T>(start_it, end_it));
}

template<typename T, typename F>
void clip_unsorted(std::vector<T> &vec, float start, float end, F &&get_key) {
    // using std::copy_if
    std::vector<T> new_vec;
    new_vec.reserve(vec.size());
    std::copy_if(
        vec.begin(), vec.end(), std::back_inserter(new_vec),
        [start, end, &get_key](const T &item) {
            float key = get_key(item);
            return start <= key && key < end;
        }
    );
    // move the new_vec to vec
    vec = std::move(new_vec);
}

class Track {
public:
    std::string name;
    uint8_t program = 0;
    bool is_drum = false;
    std::vector<Note> notes;
    std::unordered_map<uint8_t, std::vector<ControlChange>> controls;

    Track() = default;
    
    Track(const Track&) = default;

    auto copy() const { return Track(*this); }

    Track& shift_pitch(int8_t offset) {
        for (auto &note: notes) note.shift_pitch(offset);
        return *this;
    };

    Track& shift_time(float offset) {
        for (auto &note: notes) note.shift_time(offset);
        return *this;
    };

    Track& shift_velocity(int8_t offset) {
        for (auto &note: notes) note.shift_velocity(offset);
        return *this;
    };

    Track& clip_time_sorted(float start, float end, bool clip_end) {
        // using std::binary_search() to find the range
        clip_sorted(notes, start, end, [&clip_end](const Note &note) { return clip_end ? note.start + note.duration : note.start; });
        for (auto &control_vec: controls) {
            clip_sorted(control_vec.second, start, end, [](const ControlChange &control) { return control.time; });
        }
        return *this;
    };

    Track& clip_time_unsorted(float start, float end, bool clip_end) {
        clip_unsorted(notes, start, end, [&clip_end](const Note &note) { return clip_end ? note.start + note.duration : note.start; });
        for (auto &control_vec: controls) {
            clip_unsorted(control_vec.second, start, end, [](const ControlChange &control) { return control.time; });
        }
        return *this;
    };

    Track& filter_notes(std::function<bool(const Note &)> &filter){
        std::vector<Note> new_notes;
        new_notes.reserve(notes.size());
        std::copy_if(
            notes.begin(), notes.end(), std::back_inserter(new_notes), filter
        );
        notes = std::move(new_notes);
        return *this;
    };

    Track& sort() {
        pdqsort_branchless(
            notes.begin(), notes.end(),
            [](const Note &a, const Note &b) { return a.start < b.start; }
        );
        for (auto &control_pair: controls) {
            auto data = control_pair.second;
            pdqsort_branchless(data.begin(), data.end(), cmp_time<ControlChange>);
        }
        return *this;
    };

    inline size_t note_num() const {
        return this->notes.size();
    }

    inline float end_time() const {
        float time = 0;
        for (auto const &note: this->notes)
            time = std::max(time, note.end_time());
        return time;
    };

    inline float start_time() const {
        float time = FLT_MAX;
        for (auto const &note: this->notes)
            time = std::min(time, note.start);
        return time;
    }

    Pianoroll frame_pianoroll(uint16_t quantization = 16) const {
        Pianoroll pianoroll = empty_pianoroll(quantization);
        for (const auto &note: notes) {
            uint64_t start = floor(note.start * (float) quantization / 4);
            uint64_t duration = floor(note.duration * (float) quantization / 4);
            pianoroll.row(note.pitch).segment((long) start, duration) = true; // eigen index should be long
        }

        return pianoroll;
    };

    Pianoroll onset_pianoroll(uint16_t quantization = 16) const {
        Pianoroll pianoroll = empty_pianoroll(quantization);

        for (const auto &note: notes) {
            uint64_t start = floor(note.start * (float) quantization / 4);
            pianoroll.row(note.pitch).col((long) start) = true; // eigen index should be long
        }

        return pianoroll;
    };

    NoteArray note_array() const {
        return std::move(NoteArray(*this));
    };

protected:
    [[nodiscard]] inline Pianoroll empty_pianoroll(uint16_t quantization = 16) const {
        return Pianoroll::Zero(MIDI_PITCH_NUMS, ceil(this->end_time() * (float) quantization / 4));
    };
};

typedef std::tuple<uint8_t, uint8_t> TrackIdx;

Track &get_track(std::map<TrackIdx, Track> &track_map, uint8_t channel, uint8_t program) {
    // TrackIdx track_idx = {channel, program};
    auto track_idx = std::make_tuple(channel, program);
    if (track_map.find(track_idx) == track_map.end()) {
        Track track;
        track.program = program;
        track.is_drum = channel == 9;
        track_map[track_idx] = track;
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

class Score {
public:
    std::vector<Track> tracks;
    std::vector<Tempo> tempos;
    std::vector<TimeSignature> time_signatures;
    std::vector<KeySignature> key_signatures;

    Score() = default;

    Score(const Score&) = default;

    auto copy() const { return Score(*this); }

    explicit Score(minimidi::file::MidiFile &midi) {
        size_t track_num = midi.track_num();
        float tpq = midi.get_tick_per_quarter();
        using std::cout, std::endl;
        for (size_t i = 0; i < track_num; ++i) {
            auto &midi_track = midi.track(i);
            std::map<TrackIdx, Track> track_map; // (channel, program) -> Track
            uint8_t cur_instr[128] = {0}; // channel -> program
            std::string cur_name;

            NoteOnQueue last_note_on[16][128]; // (channel, pitch) -> (start, velocity)
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
                        auto &track = get_track(track_map, channel, cur_instr[channel]);
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
                        uint8_t program = msg.get_program();
                        auto &track = get_track(track_map, channel, program);
                        uint8_t control_number = msg.get_control_number();
                        uint8_t control_value = msg.get_control_value();
                        if (control_number >= 128)
                            throw std::range_error("Get control_number=" + std::to_string((int) control_number));
                        if (control_value >= 128)
                            throw std::range_error("Get control_value=" + std::to_string((int) control_value));
                        track.controls[msg.get_control_number()].emplace_back(start, msg.get_control_value());
                        break;
                    }
                        // Meta Message
                    case (minimidi::message::MessageType::Meta): {
                        switch (msg.get_meta_type()) {
                            case (minimidi::message::MetaType::TrackName): {
                                minimidi::container::Bytes data = msg.get_meta_value();
                                cur_name = std::string(data.begin(), data.end());
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
                if (track.notes.empty()) continue;
                track.name = cur_name;
                tracks.push_back(track);
            }
        }
        pdqsort_branchless(time_signatures.begin(), time_signatures.end(), cmp_time<TimeSignature>);
        pdqsort_branchless(key_signatures.begin(), key_signatures.end(), cmp_time<KeySignature>);
        pdqsort_branchless(tempos.begin(), tempos.end(), cmp_time<Tempo>);
    };

    inline static Score from_midi(minimidi::file::MidiFile &midi) {
        return Score(midi);
    };

    inline static Score from_file(const std::string &filepath) {
        auto midi = minimidi::file::MidiFile::from_file(filepath);
        return Score(midi);
    };

    Score& sort() {
        pdqsort_branchless(time_signatures.begin(), time_signatures.end(), cmp_time<TimeSignature>);
        pdqsort_branchless(key_signatures.begin(), key_signatures.end(), cmp_time<KeySignature>);
        pdqsort_branchless(tempos.begin(), tempos.end(), cmp_time<Tempo>);
        for (auto &track: tracks) { track.sort(); }
        return *this;
    };

    Score& shift_pitch(int8_t offset) {
        for (auto &track: tracks) track.shift_pitch(offset);
        return *this;
    };

    Score& shift_time(float offset) {
        for (auto &track: tracks) track.shift_time(offset);
        return *this;
    };

    Score& shift_velocity(int8_t offset) {
        for (auto &track: tracks) track.shift_velocity(offset);
        return *this;
    };

    Score& clip_time_sorted(float start, float end, bool clip_end) {
        for (auto &track: tracks) track.clip_time_sorted(start, end, clip_end);
        clip_sorted(time_signatures, start, end, [](const TimeSignature &time_signature) { return time_signature.time; });
        clip_sorted(key_signatures, start, end, [](const KeySignature &key_signature) { return key_signature.time; });
        clip_sorted(tempos, start, end, [](const Tempo &tempo) { return tempo.time; });
        return *this;
    };

    Score& clip_time_unsorted(float start, float end, bool clip_end) {
        for (auto &track: tracks) track.clip_time_unsorted(start, end, clip_end);
        clip_unsorted(time_signatures, start, end, [](const TimeSignature &time_signature) { return time_signature.time; });
        clip_unsorted(key_signatures, start, end, [](const KeySignature &key_signature) { return key_signature.time; });
        clip_unsorted(tempos, start, end, [](const Tempo &tempo) { return tempo.time; });
        return *this;
    };

    Score& filter_notes(std::function<bool(const Note &)> &filter){
        for (auto &track: tracks) track.filter_notes(filter);  
        return *this;
    };

    [[nodiscard]] inline size_t note_num() const {
        size_t num = 0;
        for (auto const &track: this->tracks)
            num += track.note_num();
        return num;
    }

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
    }
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

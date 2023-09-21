#include <unordered_map>
#include <map>
#include <queue>
#include <pdqsort.h>
#include "MiniMidi.hpp"

namespace score {
#define DEFAULT_VELOCITY 100

// Using int8_t to avoid possible errors while shifting

class Note {
public:
    float start;
    float duration;
    int8_t pitch;
    int8_t velocity;

    inline Note(float start, float duration, int8_t pitch, int8_t velocity = DEFAULT_VELOCITY) :
            start(start), duration(duration), pitch(pitch), velocity(velocity) {}
};

class ControlChange {
public:
    float time;
    uint8_t value;

    inline ControlChange(float time, uint8_t value) : time(time), value(value) {}
};

class TimeSignature {
public:
    float time;
    uint8_t numerator;
    uint8_t denominator;

    inline TimeSignature(float time, uint8_t numerator, uint8_t denominator) :
            time(time), numerator(numerator), denominator(denominator) {}

    inline TimeSignature(float time, minimidi::message::TimeSignature msg) :
            time(time), numerator(msg.numerator), denominator(msg.denominator) {}
};

class KeySignature {
public:
    float time;
    int8_t key;
    uint8_t tonality;

    inline KeySignature(float time, int8_t key, uint8_t tonality) :
            time(time), key(key), tonality(tonality) {}

    inline KeySignature(float time, minimidi::message::KeySignature msg) :
            time(time), key(msg.key), tonality(msg.tonality) {}

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

    inline Tempo(float time, float qpm) : time(time), qpm(qpm) {}
};

template<typename T>
inline bool cmp_time(T &a, T &b) { return a.time < b.time; }

class Track {
public:
    std::string name;
    uint8_t program = 0;
    bool is_drum = false;
    std::vector<Note> notes;
    std::unordered_map<uint8_t, std::vector<ControlChange>> controls;

    Track() = default;

    void shift_pitch(int8_t offset) {
        for (auto &note: notes) note.pitch += offset;
    }

    void shift_time(float offset) {
        for (auto &note: notes) note.start += offset;
    }

    void shift_velocity(int8_t offset) {
        for (auto &note: notes) note.velocity += offset;
    }

    void sort() {
        pdqsort_branchless(
                notes.begin(), notes.end(),
                [](const Note &a, const Note &b) { return a.start < b.start; }
        );
        for (auto &control_pair: controls) {
            auto data = control_pair.second;
            pdqsort_branchless(data.begin(), data.end(), cmp_time<ControlChange>);
        }
    }
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

    inline bool empty() const { return velocity == 0; }

    inline void reset() {
        start = 0;
        velocity = 0;
    }
};

class NoteOnQueue {
private:
    std::queue<NoteOn> queue;
public:
    NoteOn front;

    NoteOnQueue() = default;

    inline size_t size() { return front.empty() ? 0 : queue.size() + 1; }

    inline bool empty() { return front.empty(); }

    inline void emplace(float start, int8_t velocity) {
        if (!front.empty()) queue.push(front);
        front.start = start;
        front.velocity = velocity;
    }

    inline void pop() {
        if (queue.empty()) front.reset();
        else {
            front = queue.front();
            queue.pop();
        }
    }

};

class Score {
public:
    std::vector<Track> tracks;
    std::vector<Tempo> tempos;
    std::vector<TimeSignature> time_signatures;
    std::vector<KeySignature> key_signatures;

    Score() = default;

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
                        auto velocity = (int8_t) msg.get_velocity();
                        if (velocity != 0) {
                            last_note_on[msg.get_channel()][msg.get_pitch()].emplace(start, velocity);
                            break;
                        } // if velocity is zero, turn to note off case
                    }
                    case (minimidi::message::MessageType::NoteOff): {
                        uint8_t channel = msg.get_channel();
                        auto pitch = (int8_t) msg.get_pitch();
                        auto &note_on_queue = last_note_on[channel][pitch];
                        auto &track = get_track(track_map, channel, cur_instr[channel]);
                        while ((!note_on_queue.empty()) && (start > note_on_queue.front.start)) {
                            auto const &note_on = note_on_queue.front;
                            float duration = start - note_on.start;
                            track.notes.emplace_back(note_on.start, duration, pitch, note_on.velocity);
                            note_on_queue.pop();
                        }
                        break;
                    }
                    case (minimidi::message::MessageType::ProgramChange): {
                        uint8_t channel = msg.get_channel();
                        uint8_t program = msg.get_program();
                        cur_instr[channel] = program;
                        break;
                    }
                    case (minimidi::message::MessageType::ControlChange): {
                        uint8_t channel = msg.get_channel();
                        uint8_t program = msg.get_program();
                        auto &track = get_track(track_map, channel, program);
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
                                tempos.emplace_back(start, 60000000. / (float) msg.get_tempo());
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
    }

    inline static Score from_midi(minimidi::file::MidiFile &midi) {
        return Score(midi);
    }

    inline static Score from_file(const std::string &filepath) {
        auto midi = minimidi::file::MidiFile::from_file(filepath);
        return Score(midi);
    }

    void sort() {
        pdqsort_branchless(time_signatures.begin(), time_signatures.end(), cmp_time < TimeSignature > );
        pdqsort_branchless(key_signatures.begin(), key_signatures.end(), cmp_time < KeySignature > );
        pdqsort_branchless(tempos.begin(), tempos.end(), cmp_time < Tempo > );
        for (auto &track: tracks) { track.sort(); }
    }

    void shift_pitch(int8_t offset) {
        for (auto &track: tracks) track.shift_pitch(offset);
    }

    void shift_time(float offset) {
        for (auto &track: tracks) track.shift_time(offset);
    }

    void shift_velocity(int8_t offset) {
        for (auto &track: tracks) track.shift_velocity(offset);
    }
};
}
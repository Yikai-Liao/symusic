//
// Created by lyk on 23-12-25.
//

#include <map>
#include <queue>

#ifdef _MSC_VER
#pragma warning(disable:4996)
#endif

#include "minimidi/MiniMidi.hpp"
#include "pdqsort.h"

#include "symusic/score.h"
#include "symusic/ops.h"
#include "symusic/utils.h"
#include "symusic/conversion.h"
#include "symusic/io/common.h"


namespace symusic {

namespace details {

// define some utils
template<typename T>
struct NoteOn: TimeStamp<T> {
    typedef typename T::unit unit;
    i8 velocity;
    NoteOn(): TimeStamp<T>(0), velocity(0) {}
    NoteOn(const unit time, i8 const velocity) : TimeStamp<T>(time), velocity(velocity) {};
    [[nodiscard]] bool empty() const {return velocity == 0;}
    void reset() { this->time=0; velocity=0; }
};

template<typename T>
struct NoteOnQueue {
private:
    typedef typename T::unit unit;
    std::queue<NoteOn<T>> queue;
    NoteOn<T> _front;

public:
    NoteOnQueue() = default;
    [[nodiscard]] size_t size() const { return _front.empty() ? 0 : queue.size() + 1; }

    [[nodiscard]] bool empty() const { return _front.empty();}

    [[nodiscard]] const NoteOn<T> &front() const { return _front; }

    void emplace(unit time, i8 velocity) {
        if(_front.empty()) {
            // significantly faster than _front = NoteOn<T>{time, velocity};
            // what is the compiler doing?
            _front.time = time;
            _front.velocity = velocity;
        }
        else queue.emplace(time, velocity);
    }

    void pop() {
        if (queue.empty()) _front.reset();
        else {
            _front = queue.front();
            queue.pop();
        }
    }

    void reset() {
        if (empty()) return;
        while(!queue.empty()) queue.pop();
        _front.reset();
    }
};

struct TrackIdx {

    u8 channel, program;

    TrackIdx(const u8 channel, const u8 program): channel(channel), program(program) {}

    bool operator<(const TrackIdx &other) const {
        return std::tie(channel, program) < std::tie(other.channel, other.program);
    }

    bool operator==(const TrackIdx&) const = default;
};


template <typename T>
Track<T> &get_track(
    std::map<TrackIdx, Track<T>> &track_map,
    Track<T> (& stragglers)[16],
    const uint8_t channel, const uint8_t program,
    const size_t message_num, const bool create_new) {
    TrackIdx track_idx{channel, program};
    const auto & entry = track_map.find(track_idx);
    if (entry == track_map.end()) {
        auto &track = stragglers[channel];
        if (!create_new) return track;
        auto res = track_map.emplace(
            std::piecewise_construct,
            std::forward_as_tuple(track_idx),
            std::forward_as_tuple("", program, channel==9)
        );
        auto & iter = res.first;
        if(!res.second) throw std::runtime_error("Failed to insert track");
        // copy controls if not empty, don't move, don't clear original
        if (!track.controls.empty()) iter->second.controls = track.controls;
        // copy pitch_bends if not empty, don't move, don't clear original
        if (!track.pitch_bends.empty()) iter->second.pitch_bends = track.pitch_bends;
        // copy pedals if not empty, don't move, don't clear original
        if (!track.pedals.empty()) iter->second.pedals = track.pedals;
        // reserve space for notes
        iter->second.notes.reserve(message_num / 2 + 1);
        return iter->second;
    }
    return entry->second;
}


template<TType T, typename Conv>   // only works for Tick and Quarter
requires (std::is_same_v<T, Tick> || std::is_same_v<T, Quarter>)
[[nodiscard]] Score<T> parse_midi(const minimidi::file::MidiFile &midi, Conv tick2unit){
    typedef typename T::unit unit;
    // remove this redundant copy in the future
    const u16 tpq = midi.get_tick_per_quarter();
    Score<T> score(tpq); // create a score with the given ticks per quarter
    namespace message = minimidi::message; // alias
    // (channel, pitch) -> (start, velocity)
    NoteOnQueue<Tick> last_note_on[16][128] = {};

    for(const auto &midi_track: midi.tracks) {
        // (channel, program) -> Track
        std::map<TrackIdx, Track<T>> track_map;
        // channel -> Track
        Track<T> stragglers[16];
        Track<T>* last_track = nullptr;
        u8 last_channel = 255, last_program = 255;
        // channel -> program
        uint8_t cur_instr[16] = {0};
        std::string cur_name;
        // iter NoteOnQueue and reset
        for (auto &channel: last_note_on) {
            for (auto &note_on: channel) {
                note_on.reset();
            }
        }
        // channel -> pedal_on
        unit last_pedal_on[16] = {-1};
        // iter midi messages in the track
        size_t message_num = midi_track.message_num();
        for(const auto &msg: midi_track.messages) {
            const auto cur_tick = static_cast<Tick::unit>(msg.get_time());
            const auto cur_time = tick2unit(cur_tick);
            switch (msg.get_type()) {
                case (message::MessageType::NoteOn): {
                    if (uint8_t velocity = msg.get_velocity(); velocity != 0) {
                        auto pitch = msg.get_pitch();
                        if (pitch >= 128) throw std::range_error(
                            "Get pitch=" + std::to_string(static_cast<int>(pitch)));
                        if (velocity >= 128) throw std::range_error(
                            "Get velocity=" + std::to_string(static_cast<int>(velocity)));
                        last_note_on[msg.get_channel()][pitch].emplace(
                            cur_tick, static_cast<i8>(velocity));
                        break;
                    } // if velocity is zero, turn to note off case
                }
                case (message::MessageType::NoteOff): {
                    u8 channel = msg.get_channel();
                    u8 pitch = msg.get_pitch();
                    if (pitch >= 128) throw std::range_error(
                        "Get pitch=" + std::to_string(pitch));
                    auto &note_on_queue = last_note_on[channel][pitch];
                    u8 program  = cur_instr[channel];
                    Track<T>* track;
                    if (last_channel == channel && last_program == program) {
                        track = last_track;
                    } else {
                        track = &get_track(track_map, stragglers, channel, program, message_num, true);
                        last_track = track;
                        last_channel = channel;
                        last_program = program;
                    }
                    if ((!note_on_queue.empty()) && (cur_tick >= note_on_queue.front().time)) {
                        auto const &note_on = note_on_queue.front();
                        typename T::unit duration = tick2unit(cur_tick - note_on.time);
                        track->notes.emplace_back(
                            tick2unit(note_on.time), duration,
                            static_cast<i8>(pitch), note_on.velocity
                        );
                        note_on_queue.pop();
                    }
                    break;
                }
                case (message::MessageType::ProgramChange): {
                    uint8_t channel = msg.get_channel();
                    uint8_t program = msg.get_program();
                    if (program >= 128) throw std::range_error(
                        "Get program=" + std::to_string(program));
                    cur_instr[channel] = program;
                    break;
                }
                case (message::MessageType::ControlChange): {
                    uint8_t channel = msg.get_channel();
                    uint8_t program = cur_instr[channel];
                    auto &track = get_track(
                        track_map, stragglers, channel, program, message_num, false);
                    if(track.controls.capacity() < message_num / 2 ) [[unlikely]] {
                        track.controls.reserve(message_num / 2);
                    }
                    uint8_t control_number = msg.get_control_number();
                    uint8_t control_value = msg.get_control_value();
                    if (control_number >= 128) throw std::range_error(
                        "Get control_number=" + std::to_string(control_number));
                    if (control_value >= 128) throw std::range_error(
                        "Get control_value=" + std::to_string(control_value));
                    track.controls.emplace_back(
                        cur_time, msg.get_control_number(), msg.get_control_value());
                    // Pedal Part
                    if (control_number == 64) {
                        if (control_value >= 64) {
                            if (last_pedal_on[channel] < 0) last_pedal_on[channel] = cur_time;
                        } else {
                            if (last_pedal_on[channel] >= 0) {
                                track.pedals.emplace_back(
                                    last_pedal_on[channel], cur_time - last_pedal_on[channel]
                                );
                                last_pedal_on[channel] = -1;
                            }
                        }
                    }
                    break;
                }
                case (message::MessageType::PitchBend): {
                    uint8_t channel = msg.get_channel();
                    uint8_t program = cur_instr[channel];
                    auto &track = get_track(
                        track_map, stragglers, channel, program, message_num, false);
                    auto value = msg.get_pitch_bend();
                    if (value < message::MIN_PITCHBEND || value > message::MAX_PITCHBEND)
                        throw std::range_error("Get pitch_bend=" + std::to_string(value));
                    track.pitch_bends.emplace_back(
                        cur_time, value
                    );
                    break;
                }
                    // Meta Message
                case (message::MessageType::Meta): {
                    switch (msg.get_meta_type()) {
                        case (message::MetaType::TrackName): {
                            auto data = msg.get_meta_value();
                            auto tmp = std::string(data.begin(), data.end());
                            cur_name = strip_non_utf_8(tmp);
                            break;
                        }
                        case (message::MetaType::TimeSignature): {
                            const auto [numerator, denominator] = msg.get_time_signature();
                            score.time_signatures.emplace_back(cur_time, numerator, denominator);
                            break;
                        }
                        case (message::MetaType::SetTempo): {
                            // store the raw tempo value(mspq) directly
                            // qpm is calculated when needed
                            score.tempos.emplace_back(cur_time, msg.get_tempo());
                            break;
                        }
                        case (message::MetaType::KeySignature): {
                            const auto k_msg = msg.get_key_signature();
                            score.key_signatures.emplace_back(cur_time, k_msg.key, k_msg.tonality);
                            break;
                        }
                        case (message::MetaType::Lyric): {
                            auto data = msg.get_meta_value();
                            auto tmp = std::string(data.begin(), data.end());
                            auto text = strip_non_utf_8(tmp);
                            if (text.empty()) break;
                            score.lyrics.emplace_back(cur_time, text);
                            break;
                        }
                        case (message::MetaType::Marker): {
                            auto data = msg.get_meta_value();
                            auto tmp = std::string(data.begin(), data.end());
                            auto text = strip_non_utf_8(tmp);
                            if (text.empty()) break;
                            score.markers.emplace_back(cur_time, text);
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
        for (auto & [_, track]: track_map) {
            if (track.empty()) continue;
            track.name = cur_name;
            if(static_cast<double>(track.notes.size()) * 1.5 < static_cast<double>(track.notes.capacity()))
                track.notes.shrink_to_fit();
            if(static_cast<double>(track.controls.size()) * 1.5 < static_cast<double>(track.controls.capacity()))
                track.controls.shrink_to_fit();
//            ops::sort_notes(track.notes);
            pdqsort_detail::insertion_sort(track.notes.begin(), track.notes.end(), [](const auto &a, const auto &b) {
                return std::tie(a.time, a.pitch, a.duration) < std::tie(b.time, b.pitch, b.duration);
            });
            pdqsort_detail::insertion_sort(track.pedals.begin(), track.pedals.end(), [](const auto &a, const auto &b) {
                return std::tie(a.time, a.duration) < std::tie(b.time, b.duration);
            });
            score.tracks.emplace_back(std::move(track));
        }
    }
    ops::sort_by_time(score.time_signatures);
    ops::sort_by_time(score.key_signatures);
    ops::sort_by_time(score.tempos);
    ops::sort_by_time(score.lyrics);
    ops::sort_by_time(score.markers);
    return score;
}

minimidi::file::MidiFile to_midi(const Score<Tick> & score) {
    minimidi::file::MidiFile midi{
        minimidi::file::MidiFormat::MultiTrack, 0,
        static_cast<u16>(score.ticks_per_quarter)
    };
    namespace message = minimidi::message;
    midi.tracks.reserve(score.tracks.size() + 1);

    {   // add meta messages
        message::Messages msgs{};
        msgs.reserve(
            score.time_signatures.size()
            + score.key_signatures.size()
            + score.tempos.size()
            + score.lyrics.size()
            + score.markers.size()
            + 10
        );
        // add time signatures
        for(const auto &time_signature: score.time_signatures) {
            msgs.emplace_back(message::Message::TimeSignature(
                time_signature.time, time_signature.numerator, time_signature.denominator
            ));
        }
        // add key signatures
        for(const auto &key_signature: score.key_signatures) {
            msgs.emplace_back(message::Message::KeySignature(
                key_signature.time, key_signature.key, key_signature.tonality
            ));
        }
        // add tempos
        for(const auto &tempo: score.tempos) {
            msgs.emplace_back(message::Message::SetTempo(
                tempo.time, tempo.mspq
            ));
        }
        // add lyrics
        for(const auto &lyric: score.lyrics) {
            msgs.emplace_back(message::Message::Lyric(
                lyric.time, lyric.text
            ));
        }
        // add markers
        for(const auto &marker: score.markers) {
            msgs.emplace_back(message::Message::Marker(
                marker.time, marker.text
            ));
        }
        // messages will be sorted by time in minimidi
        if(!msgs.empty()) midi.tracks.emplace_back(std::move(msgs));
    }

    const u8 valid_channel[15] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 10 ,11, 12, 13, 14, 15};

    for(size_t idx=0; const auto &track: score.tracks) {
        message::Messages msgs{};
        msgs.reserve(track.note_num() * 2 + track.controls.size() + track.pitch_bends.size() + 10);
        const u8 channel = track.is_drum ? 9 : valid_channel[idx % 15];
        // add track name
        if(!track.name.empty())
            msgs.emplace_back(message::Message::TrackName(0, track.name));
        // add program change
        msgs.emplace_back(message::Message::ProgramChange(0, channel, track.program));
        // Todo add check for Pedal
        // add control change
        for(const auto &control: track.controls) {
            msgs.emplace_back(message::Message::ControlChange(
                control.time, channel, control.number, control.value
            ));
        }
        // add pitch bend
        for(const auto &pitch_bend: track.pitch_bends) {
            msgs.emplace_back(message::Message::PitchBend(
                pitch_bend.time, channel, static_cast<i16>(pitch_bend.value)
            ));
        }
        // add notes
        vec<Note<Tick>> notes {track.notes};
        ops::sort(notes.begin(), notes.end(), [](const auto &a, const auto &b) {
            return std::tie(a.time, a.duration) < std::tie(b.time, b.duration);
        });

        for(const auto &note: notes) {
            msgs.emplace_back(message::Message::NoteOn(
                note.time, channel, note.pitch, note.velocity
            ));
            msgs.emplace_back(message::Message::NoteOff(
                note.end(), channel, note.pitch, note.velocity
            ));
        }
        // messages will be sorted by time in minimidi
        if(!msgs.empty()) {
            midi.tracks.emplace_back(std::move(msgs));
            idx += 1;
        }
    }
    return midi;
}

template<TType T>
Score<T> parse_midi(const std::span<const u8> bytes) {
    const minimidi::file::MidiFile midi {bytes.data(), bytes.size()};
    if constexpr (std::is_same_v<T, Tick>) {
        return parse_midi<Tick>(midi, [](const Tick::unit x) { return x; });
    } else if constexpr (std::is_same_v<T, Quarter>) {
        const auto tpq = static_cast<float>(midi.get_tick_per_quarter());
        return parse_midi<Quarter>(midi, [tpq](const Tick::unit x) { return static_cast<float>(x) / tpq; });
    } else {
        // return parse_midi_sec(midi);
        return convert<Second>(parse_midi<Tick>(midi, [](const Tick::unit x) { return x; }));
    } return {};
}
}

template<> template<>
Score<Tick> Score<Tick>::parse<DataFormat::MIDI>(const std::span<const u8> bytes) {
    return details::parse_midi<Tick>(bytes);
}

template<> template<>
Score<Quarter> Score<Quarter>::parse<DataFormat::MIDI>(const std::span<const u8> bytes) {
    return details::parse_midi<Quarter>(bytes);
}

template<> template<>
Score<Second> Score<Second>::parse<DataFormat::MIDI>(const std::span<const u8> bytes) {
    return details::parse_midi<Second>(bytes);
}

template<> template<>
vec<u8> Score<Tick>::dumps<DataFormat::MIDI>() const {
    return details::to_midi(*this).to_bytes();
}

template<> template<>
vec<u8> Score<Quarter>::dumps<DataFormat::MIDI>() const {
    return details::to_midi(convert<Tick>(*this)).to_bytes();
}

template<> template<>
vec<u8> Score<Second>::dumps<DataFormat::MIDI>() const {
    throw std::runtime_error("Second is not supported yet");
}

}

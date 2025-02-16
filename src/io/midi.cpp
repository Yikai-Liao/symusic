//
// Created by lyk on 23-12-25.
//

#include <map>
#include <queue>

#ifdef _MSC_VER
#pragma warning(disable : 4996)
#endif

#include "minimidi/MiniMidi.hpp"
#include "pdqsort.h"
#include "MetaMacro.h"

#include "symusic/score.h"
#include "symusic/ops.h"
#include "symusic/utils.h"
#include "symusic/conversion.h"

namespace symusic {

namespace details {
// define some utils

template<typename T>
void sort_by_time(vec<T>& data) {
    pdqsort_branchless(data.begin(), data.end(), [](const auto& a, const auto& b) {
        return (a.time) < (b.time);
    });
}


template<typename T>
struct NoteOn : TimeStamp<T> {
    typedef typename T::unit unit;
    i8                       velocity;
    NoteOn() : TimeStamp<T>(0), velocity(0) {}
    NoteOn(const unit time, i8 const velocity) : TimeStamp<T>(time), velocity(velocity) {};
    [[nodiscard]] bool empty() const { return velocity == 0; }
    void               reset() {
        this->time = 0;
        velocity   = 0;
    }
};

template<typename T>
struct NoteOnQueue {
private:
    typedef typename T::unit unit;
    std::queue<NoteOn<T>>    queue;
    NoteOn<T>                _front;

public:
    NoteOnQueue() = default;

    [[nodiscard]] size_t size() const { return _front.empty() ? 0 : queue.size() + 1; }

    [[nodiscard]] bool empty() const { return _front.empty(); }

    [[nodiscard]] const NoteOn<T>& front() const { return _front; }

    void emplace(unit time, i8 velocity) {
        if (_front.empty()) {
            // significantly faster than _front = NoteOn<T>{time, velocity};
            // what is the compiler doing?
            _front.time     = time;
            _front.velocity = velocity;
        } else
            queue.emplace(time, velocity);
    }

    void pop() {
        if (queue.empty())
            _front.reset();
        else {
            _front = queue.front();
            queue.pop();
        }
    }

    void reset() {
        if (empty()) return;
        while (!queue.empty()) queue.pop();
        _front.reset();
    }
};

struct TrackIdx {

    u8 channel, program;

    TrackIdx(const u8 channel, const u8 program) : channel(channel), program(program) {}

    bool operator<(const TrackIdx& other) const {
        return std::tie(channel, program) < std::tie(other.channel, other.program);
    }

    bool operator==(const TrackIdx&) const = default;
};


template<typename T>
TrackNative<T>& get_track(
    std::map<TrackIdx, TrackNative<T>>& track_map,
    TrackNative<T> (&stragglers)[16],
    const uint8_t channel,
    const uint8_t program,
    const size_t  message_num,
    const bool    create_new
) {
    TrackIdx    track_idx{channel, program};
    const auto& entry = track_map.find(track_idx);
    if (entry == track_map.end()) {
        auto& track = stragglers[channel];
        if (!create_new) return track;
        auto res = track_map.emplace(
            std::piecewise_construct,
            std::forward_as_tuple(track_idx),
            std::forward_as_tuple("", program, channel == 9)
        );
        auto& iter = res.first;
        if (!res.second) throw std::runtime_error("Failed to insert track");
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


template<TType T, typename Conv, typename Container>   // only works for Tick and Quarter
    requires(std::is_same_v<T, Tick> || std::is_same_v<T, Quarter>)
[[nodiscard]] Score<T> parse_midi(const minimidi::MidiFileView<Container>& midi, Conv tick2unit) {
    typedef typename T::unit unit;
    // remove this redundant copy in the future
    const u16      tpq = midi.ticks_per_quarter();
    ScoreNative<T> score(tpq);   // create a score with the given ticks per quarter
    // (channel, pitch) -> (start, velocity)
    NoteOnQueue<Tick> last_note_on[16][128] = {};

    for (const minimidi::TrackView<Container>& midi_track : midi) {
        // (channel, program) -> Track
        std::map<TrackIdx, TrackNative<T>> track_map;
        // channel -> Track
        TrackNative<T>  stragglers[16];
        TrackNative<T>* last_track   = nullptr;
        u8              last_channel = 255, last_program = 255;
        // channel -> program
        uint8_t     cur_instr[16] = {0};
        std::string cur_name;
        // iter NoteOnQueue and reset
        for (auto& channel : last_note_on) {
            for (auto& note_on : channel) { note_on.reset(); }
        }
        // channel -> pedal_on
        unit last_pedal_on[16] = {-1};
        // iter midi messages in the track
        size_t message_num = midi_track.size / 3 + 100;
        for (const auto& msg : midi_track) {
            const auto cur_tick = static_cast<Tick::unit>(msg.time);
            const auto cur_time = tick2unit(cur_tick);
            switch (msg.type()) {
            case (minimidi::MessageType::NoteOn): {
                const auto& note_on = msg.template cast<minimidi::NoteOn>();
                if (const uint8_t velocity = note_on.velocity(); velocity != 0) {
                    auto pitch = note_on.pitch();
                    if (pitch >= 128)
                        throw std::range_error(
                            "Get pitch=" + std::to_string(static_cast<int>(pitch))
                        );
                    if (velocity >= 128)
                        throw std::range_error(
                            "Get velocity=" + std::to_string(static_cast<int>(velocity))
                        );
                    last_note_on[note_on.channel()][pitch].emplace(
                        cur_tick, static_cast<i8>(velocity)
                    );
                    break;
                }   // if velocity is zero, turn to note off case
            }
            case (minimidi::MessageType::NoteOff): {
                const auto& note_off = msg.template cast<minimidi::NoteOff>();
                const u8    channel  = note_off.channel();
                const u8    pitch    = note_off.pitch();
                if (pitch >= 128) throw std::range_error("Get pitch=" + std::to_string(pitch));
                auto&           note_on_queue = last_note_on[channel][pitch];
                u8              program       = cur_instr[channel];
                TrackNative<T>* track;
                if (last_channel == channel && last_program == program) {
                    track = last_track;
                } else {
                    track = &get_track(track_map, stragglers, channel, program, message_num, true);
                    last_track   = track;
                    last_channel = channel;
                    last_program = program;
                }
                if ((!note_on_queue.empty()) && (cur_tick >= note_on_queue.front().time)) {
                    auto const&      note_on  = note_on_queue.front();
                    typename T::unit duration = tick2unit(cur_tick - note_on.time);
                    track->notes.emplace_back(
                        tick2unit(note_on.time), duration, static_cast<i8>(pitch), note_on.velocity
                    );
                    note_on_queue.pop();
                }
                break;
            }
            case (minimidi::MessageType::ProgramChange): {
                const auto&   program_change = msg.template cast<minimidi::ProgramChange>();
                const uint8_t channel        = program_change.channel();
                const uint8_t program        = program_change.program();
                if (program >= 128)
                    throw std::range_error("Get program=" + std::to_string(program));
                cur_instr[channel] = program;
                break;
            }
            case (minimidi::MessageType::ControlChange): {
                const auto&   control_change = msg.template cast<minimidi::ControlChange>();
                const uint8_t channel        = control_change.channel();
                const uint8_t program        = cur_instr[channel];

                auto& track
                    = get_track(track_map, stragglers, channel, program, message_num, false);
                if (track.controls.capacity() < message_num / 2) [[unlikely]] {
                    track.controls.reserve(message_num / 2);
                }

                const uint8_t control_number = control_change.control_number();
                const uint8_t control_value  = control_change.control_value();

                if (control_number >= 128)
                    throw std::range_error("Get control_number=" + std::to_string(control_number));
                if (control_value >= 128)
                    throw std::range_error("Get control_value=" + std::to_string(control_value));
                track.controls.emplace_back(
                    cur_time, control_number, control_value
                );
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
            case (minimidi::MessageType::PitchBend): {
                const auto&   pitch_bend = msg.template cast<minimidi::PitchBend>();
                const uint8_t channel    = pitch_bend.channel();
                const uint8_t program    = cur_instr[channel];
                auto&         track
                    = get_track(track_map, stragglers, channel, program, message_num, false);
                auto value = pitch_bend.pitch_bend();
                if (value < minimidi::PitchBend<>::MAX_PITCH_BEND
                    || value > minimidi::PitchBend<>::MIN_PITCH_BEND)
                    throw std::range_error("Get pitch_bend=" + std::to_string(value));
                track.pitch_bends.emplace_back(cur_time, value);
                break;
            }
                // Meta Message
            case (minimidi::MessageType::Meta): {
                switch (const auto& meta = msg.template cast<minimidi::Meta>(); meta.meta_type()) {
                case (minimidi::MetaType::TrackName): {
                    auto data = meta.meta_value();
                    auto tmp  = std::string(data.begin(), data.end());
                    cur_name  = strip_non_utf_8(tmp);
                    break;
                }
                case (minimidi::MetaType::TimeSignature): {
                    const auto& time_sig = meta.template cast<minimidi::TimeSignature>();
                    score.time_signatures.emplace_back(
                        tick2unit(cur_tick), time_sig.numerator(), time_sig.denominator()
                    );
                    break;
                }
                case (minimidi::MetaType::SetTempo): {
                    // store the raw tempo value(mspq) directly
                    // qpm is calculated when needed
                    score.tempos.emplace_back(cur_time, meta.template cast<minimidi::SetTempo>().tempo());
                    break;
                }
                case (minimidi::MetaType::KeySignature): {
                    const auto& k_msg = meta.template cast<minimidi::KeySignature>();
                    score.key_signatures.emplace_back(cur_time, k_msg.key(), k_msg.tonality());
                    break;
                }
                case (minimidi::MetaType::Lyric): {
                    // lyrics should be placed in Track, not Score
                    auto    data    = meta.meta_value();
                    uint8_t channel = meta.channel();
                    uint8_t program = cur_instr[channel];
                    auto&   track   // all lyrics should be preserved, so we set create_new=true
                        = get_track(track_map, stragglers, channel, program, message_num, true);
                    auto text = strip_non_utf_8(std::string(data.begin(), data.end()));
                    if (text.empty()) break;
                    track.lyrics.emplace_back(cur_time, text);
                    break;
                }
                case (minimidi::MetaType::Marker): {
                    auto data = meta.meta_value();
                    auto tmp  = std::string(data.begin(), data.end());
                    auto text = strip_non_utf_8(tmp);
                    if (text.empty()) break;
                    score.markers.emplace_back(cur_time, text);
                    break;
                }
                default: break;
                }
                break;
            }
            default: break;
            }
        }
        for (auto& [_, track] : track_map) {
            if (track.empty()) continue;
            track.name = cur_name;
            if (static_cast<double>(track.notes.size()) * 1.5
                < static_cast<double>(track.notes.capacity()))
                track.notes.shrink_to_fit();
            if (static_cast<double>(track.controls.size()) * 1.5
                < static_cast<double>(track.controls.capacity()))
                track.controls.shrink_to_fit();
            pdqsort_detail::insertion_sort(
                track.notes.begin(),
                track.notes.end(),
                [](const auto& a, const auto& b) {
                    return std::tie(a.time, a.pitch, a.duration)
                           < std::tie(b.time, b.pitch, b.duration);
                }
            );
            pdqsort_detail::insertion_sort(
                track.pedals.begin(),
                track.pedals.end(),
                [](const auto& a, const auto& b) {
                    return std::tie(a.time, a.duration) < std::tie(b.time, b.duration);
                }
            );
            score.tracks.emplace_back(std::move(track));
        }
    }
    sort_by_time(score.time_signatures);
    sort_by_time(score.key_signatures);
    sort_by_time(score.tempos);
    // sort_by_time(score.lyrics);
    sort_by_time(score.markers);
    return to_shared(std::move(score));
}

minimidi::MidiFile<> to_midi(const Score<Tick>& score) {
    minimidi::MidiFile<> midi{
        minimidi::MidiFormat::MultiTrack, 0, static_cast<u16>(score.ticks_per_quarter)
    };

    midi.tracks.reserve(score.tracks->size() + 1);
    minimidi::Messages init_msgs{};
    {   // add meta messages
        auto&        msgs        = init_msgs;
        const size_t message_num = score.time_signatures->size() + score.key_signatures->size()
                                   + score.tempos->size() + score.markers->size();
        msgs.reserve(message_num);
        // add time signatures
        for (const auto& time_signature : *score.time_signatures) {
            msgs.emplace_back(
                minimidi::TimeSignature(
                    time_signature->time, time_signature->numerator, time_signature->denominator
                )
            );
        }
        // add key signatures
        for (const auto& key_signature : *score.key_signatures) {
            msgs.emplace_back(
                minimidi::KeySignature(
                    key_signature->time, key_signature->key, key_signature->tonality
                )
            );
        }
        // add tempos
        for (const auto& tempo : *score.tempos) {
            msgs.emplace_back(minimidi::SetTempo(tempo->time, tempo->mspq));
        }
        // add markers
        for (const auto& marker : *score.markers) {
            msgs.emplace_back(minimidi::Marker(marker->time, marker->text));
        }
    }
    const u8 valid_channel[15] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 10, 11, 12, 13, 14, 15};

    for (size_t idx = 0; const auto& track : *score.tracks) {
        minimidi::Messages msgs;
        if (!init_msgs.empty()) {
            msgs      = std::move(init_msgs);
            init_msgs = {};
        } else {
            msgs = {};
            msgs.reserve(
                msgs.size() + track->note_num() * 2 + track->controls->size()
                + track->pitch_bends->size() + track->lyrics->size() + 10
            );
        }
        const u8 channel = track->is_drum ? 9 : valid_channel[idx % 15];
        // add track name
        if (!track->name.empty()) msgs.emplace_back(minimidi::TrackName(0, track->name));
        // add program change
        msgs.emplace_back(minimidi::ProgramChange(0, channel, track->program));
        // Todo add check for Pedal
        // add control change
        for (const auto& control : *track->controls) {
            msgs.emplace_back(
                minimidi::ControlChange(
                    control->time, channel, control->number, control->value
                )
            );
        }
        // add pitch bend
        for (const auto& pitch_bend : *track->pitch_bends) {
            msgs.emplace_back(
                minimidi::PitchBend(
                    pitch_bend->time, channel, static_cast<i16>(pitch_bend->value)
                )
            );
        }
        // add lyrics
        for (const auto& lyric : *track->lyrics) {
            msgs.emplace_back(minimidi::Lyric(lyric->time, lyric->text));
        }

        gfx::timsort(msgs.begin(), msgs.end(), [](const auto& a, const auto& b) {
            return (a.time) < (b.time);
        });
        // add notes
        const size_t note_size  = track->notes->size();
        const auto   note_begin = static_cast<ptrdiff_t>(msgs.size());

        // collect notes and make sure they are sorted
        vec<Note<Tick>> notes(track->notes->size());
        bool            sorted    = true;
        Tick::unit      prev_time = 0;
        for (size_t j = 0; const auto& note : *track->notes) {
            notes[j++] = note;
            sorted &= note.time >= prev_time;
            prev_time = note.time;
        }
        if (!sorted) { sort_by_time(notes); }
        // add notes to messages
        msgs.resize(msgs.size() + note_size * 2);
        for (size_t i = note_begin; const auto& note : notes) {
            if (note.duration > 0) {
                msgs[i] = minimidi::NoteOff(note.end(), channel, note.pitch, note.velocity);
                msgs[i + note_size]
                    = minimidi::NoteOn(note.time, channel, note.pitch, note.velocity);
            } else {   // empty note
                msgs[i + note_size]
                    = minimidi::NoteOff(note.time, channel, note.pitch, note.velocity);
                msgs[i] = minimidi::NoteOn(note.time, channel, note.pitch, note.velocity);
            }
            i += 1;
        }
        // insertion sort note_off
        pdqsort_detail::insertion_sort(
            msgs.begin() + note_begin,
            msgs.begin() + (note_begin + static_cast<ptrdiff_t>(note_size)),
            [](const auto& a, const auto& b) { return (a.time) < (b.time); }
        );
        // merge note on and note off
        gfx::timmerge(
            msgs.begin() + note_begin,
            msgs.begin() + note_begin + static_cast<ptrdiff_t>(note_size),
            msgs.end(),
            [](const auto& a, const auto& b) { return (a.time) < (b.time); }
        );
        // merge prev to (note on and note off)
        gfx::timmerge(
            msgs.begin(),
            msgs.begin() + note_begin,
            msgs.end(),
            [](const auto& a, const auto& b) { return (a.time) < (b.time); }
        );
        // messages will be sorted by time in minimidi
        if (!msgs.empty()) {
            // sort msgs by time using timsort
            midi.tracks.emplace_back(std::move(msgs));
            idx += 1;
        }
    }
    if (!init_msgs.empty()) { midi.tracks.emplace_back(std::move(init_msgs)); }
    return std::move(midi);
}

template<TType T>
Score<T> parse_midi(const std::span<const u8> bytes) {
    const minimidi::MidiFileView<std::span<const uint8_t>> midi{bytes.data(), bytes.size()};

    if constexpr (std::is_same_v<T, Tick>) {
        return parse_midi<Tick>(midi, [](const Tick::unit x) { return x; });
    } else if constexpr (std::is_same_v<T, Quarter>) {
        const auto tpq = static_cast<float>(midi.ticks_per_quarter());
        return parse_midi<Quarter>(midi, [tpq](const Tick::unit x) {
            return static_cast<float>(x) / tpq;
        });
    } else {
        return convert<Second>(parse_midi<Tick>(midi, [](const Tick::unit x) { return x; }));
    }
}
}   // namespace details

template<>
template<>
Score<Tick> Score<Tick>::parse<DataFormat::MIDI>(const std::span<const u8> bytes) {
    return details::parse_midi<Tick>(bytes);
}

template<>
template<>
Score<Quarter> Score<Quarter>::parse<DataFormat::MIDI>(const std::span<const u8> bytes) {
    return details::parse_midi<Quarter>(bytes);
}

template<>
template<>
Score<Second> Score<Second>::parse<DataFormat::MIDI>(const std::span<const u8> bytes) {
    return details::parse_midi<Second>(bytes);
}

template<>
template<>
vec<u8> Score<Tick>::dumps<DataFormat::MIDI>() const {
    return details::to_midi(*this).to_bytes_sorted();
}

template<>
template<>
vec<u8> Score<Quarter>::dumps<DataFormat::MIDI>() const {
    return details::to_midi(convert<Tick>(*this)).to_bytes_sorted();
}

template<>
template<>
vec<u8> Score<Second>::dumps<DataFormat::MIDI>() const {
    return details::to_midi(convert<Tick>(*this)).to_bytes_sorted();
}

#define INSTANTIATE_GLOBAL_FUNC(__COUNT, T)                                 \
    template<>                                                              \
    Score<T> parse<DataFormat::MIDI, Score<T>>(std::span<const u8> bytes) { \
        return Score<T>::parse<DataFormat::MIDI>(bytes);                    \
    }                                                                       \
    template<>                                                              \
    vec<u8> dumps<DataFormat::MIDI, Score<T>>(const Score<T>& data) {       \
        return data.dumps<DataFormat::MIDI>();                              \
    }

REPEAT_ON(INSTANTIATE_GLOBAL_FUNC, Tick, Quarter, Second)
#undef INSTANTIATE_GLOBAL_FUNC

}   // namespace symusic

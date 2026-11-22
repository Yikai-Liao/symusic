#include <bit>
#include <limits>
#include <string_view>

#ifdef _MSC_VER
#pragma warning(disable : 4996)
#endif

#include <fmt/format.h>

#include "MetaMacro.h"

#include "symusic/score.h"
#include "symusic/ops.h"
#include "symusic/utils.h"
#include "symusic/conversion.h"

#include "midi_notes.h"
#include "score_builder.h"

namespace symusic {

namespace detail {

struct MidiTrackKey {
    u8 channel;
    u8 program;

    auto operator<=>(const MidiTrackKey&) const = default;
};

template<TType T>
class MidiTrackSink {
    using unit = typename T::unit;
    ScoreBuilder<T, MidiTrackKey>& builder;
    size_t                         message_count;
    std::string                    name;
    // Pending program segments retain arrival order when a note promotes them.
    std::array<vec<TrackNative<T>>, 16> pending_tracks;
    std::array<u8, 16>             programs{};
    std::array<unit, 16>           pedal_start;

    static void append_pending(TrackNative<T>& target, TrackNative<T>& source) {
        const auto append = [](auto& dest, auto& events) {
            if (dest.empty()) {
                dest = std::move(events);
            } else {
                dest.insert(dest.end(), events.begin(), events.end());
            }
        };
        append(target.controls, source.controls);
        append(target.pitch_bends, source.pitch_bends);
        append(target.pedals, source.pedals);
    }

    TrackNative<T>& track(u8 channel, bool promote) {
        const MidiTrackKey key{channel, programs[channel]};
        if (auto* existing = builder.find_track(key)) return *existing;
        auto& pending = pending_tracks[channel];
        if (!promote) {
            if (pending.empty() || pending.back().program != key.program) {
                pending.emplace_back("", key.program, channel == 9);
            }
            return pending.back();
        }

        auto& result = builder.create_track(key, TrackNative<T>("", key.program, channel == 9));
        result.notes.reserve(message_count / 2 + 1);
        for (auto& segment : pending) append_pending(result, segment);
        pending.clear();
        return result;
    }

public:
    using NoteHandle = typename ScoreBuilder<T, MidiTrackKey>::NoteHandle;

    MidiTrackSink(ScoreBuilder<T, MidiTrackKey>& builder, size_t message_count) :
        builder(builder), message_count(message_count) {
        pedal_start.fill(-1);
    }

    void set_program(u8 channel, u8 program) { programs[channel] = program; }
    void set_name(std::string value) { name = std::move(value); }

    NoteHandle begin_note(u8 channel, u8 pitch, unit time, u8 velocity) {
        // Pitch and velocity fit signed eight-bit fields after MIDI validation.
        return builder.begin_note(
            track(channel, true), time, static_cast<i8>(pitch), static_cast<i8>(velocity)
        );
    }

    void end_note(NoteHandle handle, unit time) { builder.end_note(handle, time); }

    void add_control(u8 channel, unit time, u8 number, u8 value) {
        auto& target = track(channel, false);
        target.controls.emplace_back(time, number, value);
        if (number != 64) return;

        auto& start = pedal_start[channel];
        if (value >= 64) {
            if (start < 0) start = time;
        } else if (start >= 0) {
            target.pedals.emplace_back(start, details::sub_time(time, start));
            start = -1;
        }
    }

    void add_pitch_bend(u8 channel, unit time, i32 value) {
        track(channel, false).pitch_bends.emplace_back(time, value);
    }

    void add_lyric(u8 channel, unit time, std::string text) {
        const u8 program = programs[channel];
        builder.add_lyric(
            MidiTrackKey{channel, program},
            program,
            channel == 9,
            TextMeta<T>(time, std::move(text))
        );
    }

    template<TimeEvent E>
    void add(E event) {
        builder.add(std::move(event));
    }

    void finish() {
        for (size_t channel = 0; channel < pending_tracks.size(); ++channel) {
            for (auto& segment : pending_tracks[channel]) {
                const MidiTrackKey key{static_cast<u8>(channel), segment.program};
                if (auto* existing = builder.find_track(key)) {
                    append_pending(*existing, segment);
                } else {
                    builder.create_track(key, std::move(segment));
                }
            }
        }
        builder.finish_group(name);
    }
};

template<TType T, typename Convert, typename Container>
[[nodiscard]] Score<T> parse_midi(
    const minimidi::MidiFileView<Container>& midi, Convert tick_to_unit, bool sanitize_data
) {
    ScoreBuilder<T, MidiTrackKey> builder(midi.ticks_per_quarter());
    for (const auto& track : midi) {
        MidiTrackSink<T> sink(builder, track.size / 3 + 100);
        decode_midi_track<T>(track, tick_to_unit, sanitize_data, sink);
        sink.finish();
    }
    return to_shared(std::move(builder).finish());
}

template<TType T>
void validate_midi_times(const Score<T>& score) {
    const auto validate = [](const auto& events, const char* kind) {
        for (const auto& event : *events) {
            if (event.time < 0) {
                throw std::invalid_argument(
                    fmt::format("MIDI {} time must be non-negative, got {}", kind, event.time)
                );
            }
        }
    };
    validate(score.time_signatures, "time signature");
    validate(score.key_signatures, "key signature");
    validate(score.tempos, "tempo");
    validate(score.markers, "marker");
    for (const auto& track : *score.tracks) {
        validate(track->controls, "control change");
        validate(track->pitch_bends, "pitch bend");
        validate(track->lyrics, "lyric");
        validate(track->notes, "note");
        validate(track->pedals, "pedal");
    }
}

constexpr u32 kMidiVlqMax = 0x0FFFFFFF;

void validate_midi_text(std::string_view text, std::string_view kind) {
    if (text.size() > kMidiVlqMax) {
        throw std::invalid_argument(
            fmt::format("MIDI {} text is too long, got {} bytes", kind, text.size())
        );
    }
}

u16 midi_division(i32 ticks_per_quarter) {
    if (ticks_per_quarter < 1 || ticks_per_quarter > 32767) {
        throw std::invalid_argument(
            fmt::format("MIDI ticks_per_quarter must be in 1..32767, got {}", ticks_per_quarter)
        );
    }
    // The range check leaves the SMPTE flag clear.
    return static_cast<u16>(ticks_per_quarter);
}

minimidi::MidiFile<> to_midi(const Score<Tick>& score, u16 division) {
    // Every event start fits u32. Positive note durations use the checked tick end.
    validate_midi_times(score);
    // Each score track emits a program change, so every track counts toward the header limit.
    if (score.tracks->size() > std::numeric_limits<u16>::max()) {
        throw std::invalid_argument("The number of MIDI tracks must fit in 16 bits");
    }
    minimidi::MidiFile<> midi{minimidi::MidiFormat::MultiTrack, 0, division};

    midi.tracks.reserve(score.tracks->size() + 1);
    minimidi::Messages<> init_msgs{};
    {   // add meta messages
        auto&        msgs        = init_msgs;
        const size_t message_num = score.time_signatures->size() + score.key_signatures->size()
                                   + score.tempos->size() + score.markers->size();
        msgs.reserve(message_num);
        // add time signatures
        for (const auto& time_signature : *score.time_signatures) {
            if (!std::has_single_bit(time_signature->denominator)) {
                throw std::invalid_argument(fmt::format(
                    "MIDI time signature denominator must be a power of two, got {}",
                    time_signature->denominator
                ));
            }
            msgs.emplace_back(minimidi::TimeSignature(
                time_signature->time, time_signature->numerator, time_signature->denominator
            ));
        }
        // add key signatures
        for (const auto& key_signature : *score.key_signatures) {
            msgs.emplace_back(minimidi::KeySignature(
                key_signature->time, key_signature->key, key_signature->tonality
            ));
        }
        // add tempos
        for (const auto& tempo : *score.tempos) {
            if (tempo->mspq < 0 || tempo->mspq > 0xFFFFFF) {
                throw std::invalid_argument(
                    fmt::format("MIDI tempo mspq must fit in 24 bits, got {}", tempo->mspq)
                );
            }
            msgs.emplace_back(minimidi::SetTempo(tempo->time, tempo->mspq));
        }
        // add markers
        for (const auto& marker : *score.markers) {
            validate_midi_text(marker->text, "marker");
            msgs.emplace_back(minimidi::Marker(marker->time, marker->text));
        }
    }
    const std::array<u8, 15> valid_channel{0, 1, 2, 3, 4, 5, 6, 7, 8, 10, 11, 12, 13, 14, 15};

    for (size_t idx = 0; const auto& track : *score.tracks) {
        minimidi::Messages<> msgs;
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
        validate_midi_text(track->name, "track name");
        if (!track->name.empty()) msgs.emplace_back(minimidi::TrackName(0, track->name));
        // add program change
        msgs.emplace_back(minimidi::ProgramChange(0, channel, track->program));
        // Todo add check for Pedal
        // add control change
        for (const auto& control : *track->controls) {
            msgs.emplace_back(
                minimidi::ControlChange(control->time, channel, control->number, control->value)
            );
        }
        // add pitch bend
        for (const auto& pitch_bend : *track->pitch_bends) {
            if (pitch_bend->value < minimidi::PitchBend<>::MIN_PITCH_BEND
                || pitch_bend->value > minimidi::PitchBend<>::MAX_PITCH_BEND) {
                throw std::invalid_argument(
                    fmt::format("MIDI pitch bend must be in -8192..8191, got {}", pitch_bend->value)
                );
            }
            // The validated MIDI interval fits in i16.
            msgs.emplace_back(
                minimidi::PitchBend(pitch_bend->time, channel, static_cast<i16>(pitch_bend->value))
            );
        }
        // add lyrics
        for (const auto& lyric : *track->lyrics) {
            validate_midi_text(lyric->text, "lyric");
            msgs.emplace_back(minimidi::Lyric(lyric->time, lyric->text));
        }

        gfx::timsort(msgs.begin(), msgs.end(), [](const auto& a, const auto& b) {
            return (a.time) < (b.time);
        });
        append_midi_notes(msgs, *track->notes, channel);
        // messages will be sorted by time in minimidi
        if (!msgs.empty()) {
            // sort msgs by time using timsort
            midi.tracks.emplace_back(std::move(msgs));
            idx += 1;
        }
    }
    if (!init_msgs.empty()) {
        gfx::timsort(init_msgs.begin(), init_msgs.end(), [](const auto& a, const auto& b) {
            return a.time < b.time;
        });
        midi.tracks.emplace_back(std::move(init_msgs));
    }
    for (const auto& track : midi.tracks) {
        u32 previous_time   = 0;
        u64 payload_size    = 4;   // End-of-track event.
        u8  previous_status = 0;
        for (const auto& message : track.messages) {
            // Events are sorted by time, so the unsigned difference cannot underflow.
            const u32 delta = message.time - previous_time;
            if (delta > kMidiVlqMax) {
                throw std::invalid_argument(
                    fmt::format("MIDI event delta must fit in 28 bits, got {}", delta)
                );
            }
            payload_size += minimidi::utils::calc_variable_length(delta);
            // This writer emits channel messages and meta events only.
            if (message.statusByte != previous_status || message.statusByte == 0xFF) {
                ++payload_size;
            }
            payload_size += message.data().size();
            if (payload_size > std::numeric_limits<u32>::max()) {
                throw std::invalid_argument("MIDI track payload must fit in 32 bits");
            }
            previous_time   = message.time;
            previous_status = message.statusByte;
        }
    }
    return midi;
}

/**
 * Parse raw MIDI bytes to a score, optionally sanitizing payloads before decoding.
 *
 * @param bytes Raw MIDI bytes to parse.
 * @param sanitize_data Clamp payload bytes to the 7-bit MIDI range.
 */
template<TType T>
Score<T> parse_midi(const std::span<const u8> bytes, bool sanitize_data = false) {
    const auto parse_view = [&](const auto& midi_view) -> Score<T> {
        if constexpr (std::is_same_v<T, Tick>) {
            return parse_midi<Tick>(midi_view, [](const Tick::unit x) { return x; }, sanitize_data);
        } else if constexpr (std::is_same_v<T, Quarter>) {
            const auto tpq = static_cast<float>(midi_view.ticks_per_quarter());
            return parse_midi<Quarter>(
                midi_view,
                [tpq](const Tick::unit x) { return static_cast<float>(x) / tpq; },
                sanitize_data
            );
        } else {
            return convert<Second>(parse_midi<Tick>(
                midi_view, [](const Tick::unit x) { return x; }, sanitize_data
            ));
        }
    };

    if (sanitize_data) {
        const minimidi::MidiFileView<minimidi::container::SmallBytes> midi{
            bytes.data(),
            bytes.size(),
            true   // sanitize data payloads to enforce strict MIDI range
        };
        return parse_view(midi);
    }

    const minimidi::MidiFileView<std::span<const uint8_t>> midi{bytes.data(), bytes.size()};
    return parse_view(midi);
}
}   // namespace detail

template<>
template<>
Score<Tick> Score<Tick>::parse<DataFormat::MIDI>(const std::span<const u8> bytes) {
    return detail::parse_midi<Tick>(bytes);
}

template<>
template<>
Score<Quarter> Score<Quarter>::parse<DataFormat::MIDI>(const std::span<const u8> bytes) {
    return detail::parse_midi<Quarter>(bytes);
}

template<>
template<>
Score<Second> Score<Second>::parse<DataFormat::MIDI>(const std::span<const u8> bytes) {
    return detail::parse_midi<Second>(bytes);
}

template<>
template<>
vec<u8> Score<Tick>::dumps<DataFormat::MIDI>() const {
    return detail::to_midi(*this, detail::midi_division(ticks_per_quarter)).to_bytes_sorted();
}

template<>
template<>
vec<u8> Score<Quarter>::dumps<DataFormat::MIDI>() const {
    const auto division = detail::midi_division(ticks_per_quarter);
    detail::validate_midi_times(*this);
    return detail::to_midi(convert<Tick>(*this), division).to_bytes_sorted();
}

template<>
template<>
vec<u8> Score<Second>::dumps<DataFormat::MIDI>() const {
    const auto division = detail::midi_division(ticks_per_quarter);
    detail::validate_midi_times(*this);
    return detail::to_midi(convert<Tick>(*this), division).to_bytes_sorted();
}

#define INSTANTIATE_GLOBAL_FUNC(__COUNT, T)                                                     \
    template<>                                                                                  \
    Score<T> parse<DataFormat::MIDI, Score<T>>(std::span<const u8> bytes) {                     \
        return Score<T>::parse<DataFormat::MIDI>(bytes);                                        \
    }                                                                                           \
    template<>                                                                                  \
    Score<T> parse<DataFormat::MIDI, Score<T>>(std::span<const u8> bytes, bool sanitize_data) { \
        return detail::parse_midi<T>(bytes, sanitize_data);                                     \
    }                                                                                           \
    template<>                                                                                  \
    vec<u8> dumps<DataFormat::MIDI, Score<T>>(const Score<T>& data) {                           \
        return data.dumps<DataFormat::MIDI>();                                                  \
    }

REPEAT_ON(INSTANTIATE_GLOBAL_FUNC, Tick, Quarter, Second)
#undef INSTANTIATE_GLOBAL_FUNC

}   // namespace symusic

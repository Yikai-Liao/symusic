#pragma once

#include <algorithm>
#include <array>
#include <concepts>
#include <iterator>
#include <limits>
#include <queue>
#include <unordered_map>
#include <utility>

#include <fmt/format.h>
#include <minimidi/MiniMidi.hpp>

#include <pdqsort.h>

#include "symusic/event.h"
#include "symusic/utils.h"

namespace symusic::detail {

[[nodiscard]] inline bool is_valid_midi_data_byte(const u8 value) { return value <= 0x7F; }

inline void ensure_valid_midi_data_byte(
    const char* const field_name, const u8 value, const bool sanitize_data
) {
    if (!sanitize_data && !is_valid_midi_data_byte(value)) {
        throw std::runtime_error("Get " + std::string(field_name) + "=" + std::to_string(value));
    }
}

[[noreturn]] SYMUSIC_COLD inline void throw_midi_time_overflow(const u32 time) {
    throw std::runtime_error(
        "MIDI event time " + std::to_string(time) + " exceeds the 32-bit tick range"
    );
}

// The sink maps decoded events to its own tracks. A note handle remains valid
// until the end of this MIDI track, even if its program changes.
template<typename Sink, typename T>
concept MidiNoteSink = TType<T>
                       && requires(
                           Sink&                     sink,
                           typename Sink::NoteHandle handle,
                           typename T::unit          time,
                           u8                        channel,
                           u8                        data,
                           std::string               text
                       ) {
                              {
                                  sink.begin_note(channel, data, time, data)
                              } -> std::same_as<typename Sink::NoteHandle>;
                              sink.end_note(handle, time);
                              sink.set_program(channel, data);
                              sink.add_control(channel, time, data, data);
                              sink.add_pitch_bend(channel, time, i32{});
                              sink.set_name(std::move(text));
                              sink.add_lyric(channel, time, std::move(text));
                              sink.add(TimeSignature<T>{});
                              sink.add(KeySignature<T>{});
                              sink.add(Tempo<T>{});
                              sink.add(TextMeta<T>{});
                          };

template<TType T, MidiNoteSink<T> Sink>
class PendingMidiNotes {
    using Handle = typename Sink::NoteHandle;

    struct Slot {
        size_t count = 0;
        Handle handle{};
    };

    std::array<Slot, 16 * 128>                  slots{};
    std::unordered_map<u16, std::queue<Handle>> overflow;

public:
    void begin(Sink& sink, u8 channel, u8 pitch, typename T::unit time, u8 velocity) {
        const auto key    = static_cast<u16>(channel * 128 + pitch);
        auto       handle = sink.begin_note(channel, pitch, time, velocity);
        if (auto& slot = slots[key]; slot.count++ == 0) {
            slot.handle = handle;
        } else {
            overflow.try_emplace(key).first->second.push(handle);
        }
    }

    void end(Sink& sink, u8 channel, u8 pitch, typename T::unit time) {
        const auto key  = static_cast<u16>(channel * 128 + pitch);
        auto&      slot = slots[key];
        if (slot.count == 0) return;

        sink.end_note(slot.handle, time);
        if (--slot.count > 0) {
            auto& queue = overflow.find(key)->second;
            slot.handle = queue.front();
            queue.pop();
        }
    }
};

template<TType T, MidiNoteSink<T> Sink, typename Container, typename Convert>
void decode_midi_track(
    const minimidi::TrackView<Container>& track,
    Convert                               tick_to_unit,
    bool                                  sanitize_data,
    Sink&                                 sink
) {
    PendingMidiNotes<T, Sink> notes;
    for (const auto& msg : track) {
        // Event times accumulate delta times in u32; anything above INT32_MAX (including a
        // wrapped sum, which must pass through that range first) cannot be a valid tick.
        if (msg.time > static_cast<u32>(std::numeric_limits<Tick::unit>::max())) [[unlikely]] {
            throw_midi_time_overflow(msg.time);
        }
        const auto cur_tick = detail::narrow_i32(msg.time);
        const auto cur_time = tick_to_unit(cur_tick);
        switch (msg.type()) {
        case minimidi::MessageType::NoteOn: {
            const auto& note_on = msg.template cast<minimidi::NoteOn>();
            ensure_valid_midi_data_byte("pitch", note_on.pitch(), sanitize_data);
            ensure_valid_midi_data_byte("velocity", note_on.velocity(), sanitize_data);
            if (note_on.velocity() != 0) {
                notes.begin(sink, note_on.channel(), note_on.pitch(), cur_time, note_on.velocity());
                break;
            }
        }
            [[fallthrough]];
        case minimidi::MessageType::NoteOff: {
            const auto& note_off = msg.template cast<minimidi::NoteOff>();
            ensure_valid_midi_data_byte("pitch", note_off.pitch(), sanitize_data);
            ensure_valid_midi_data_byte("velocity", note_off.velocity(), sanitize_data);
            notes.end(sink, note_off.channel(), note_off.pitch(), cur_time);
            break;
        }
        case minimidi::MessageType::ProgramChange: {
            const auto& program_change = msg.template cast<minimidi::ProgramChange>();
            const u8    channel        = program_change.channel();
            const u8    program        = program_change.program();
            ensure_valid_midi_data_byte("program", program, sanitize_data);
            sink.set_program(channel, program);
            break;
        }
        case minimidi::MessageType::ControlChange: {
            const auto& control_change = msg.template cast<minimidi::ControlChange>();
            const u8    channel        = control_change.channel();

            const u8 control_number = control_change.control_number();
            const u8 control_value  = control_change.control_value();

            ensure_valid_midi_data_byte("control_number", control_number, sanitize_data);
            ensure_valid_midi_data_byte("control_value", control_value, sanitize_data);
            sink.add_control(channel, cur_time, control_number, control_value);
            break;
        }
        case minimidi::MessageType::PitchBend: {
            const auto& pitch_bend = msg.template cast<minimidi::PitchBend>();
            auto        value      = pitch_bend.pitch_bend();
            if (!sanitize_data
                && (value < minimidi::PitchBend<>::MIN_PITCH_BEND
                    || value > minimidi::PitchBend<>::MAX_PITCH_BEND))
                throw std::runtime_error("Get pitch_bend=" + std::to_string(value));
            sink.add_pitch_bend(pitch_bend.channel(), cur_time, value);
            break;
        }
            // Meta Message
        case minimidi::MessageType::Meta: {
            switch (const auto& meta = msg.template cast<minimidi::Meta>(); meta.meta_type()) {
            case (minimidi::MetaType::TrackName): {
                auto data = meta.meta_value();
                auto tmp  = std::string(data.begin(), data.end());
                sink.set_name(detail::strip_non_utf_8(tmp));
                break;
            }
            case (minimidi::MetaType::TimeSignature): {
                const auto data = meta.meta_value();
                if (data.size() != 4 || data[1] > 7) {
                    throw std::runtime_error("Invalid MIDI time signature denominator or size");
                }
                // The exponent is at most seven, so the power of two fits in u8.
                sink.add(
                    symusic::detail::make_event<TimeSignature<T>>(
                        cur_time, data[0], static_cast<u8>(1u << data[1])
                    )
                );
                break;
            }
            case (minimidi::MetaType::SetTempo): {
                // store the raw tempo value(mspq) directly
                // qpm is calculated when needed
                sink.add(
                    symusic::detail::make_event<Tempo<T>>(
                        cur_time, meta.template cast<minimidi::SetTempo>().tempo()
                    )
                );
                break;
            }
            case (minimidi::MetaType::KeySignature): {
                const auto& k_msg = meta.template cast<minimidi::KeySignature>();
                sink.add(
                    symusic::detail::make_event<KeySignature<T>>(
                        cur_time, k_msg.key(), k_msg.tonality()
                    )
                );
                break;
            }
            case (minimidi::MetaType::Lyric): {
                const auto data = meta.meta_value();
                auto       text = detail::strip_non_utf_8(std::string(data.begin(), data.end()));
                if (!text.empty()) { sink.add_lyric(meta.channel(), cur_time, std::move(text)); }
                break;
            }
            case (minimidi::MetaType::Marker): {
                auto data = meta.meta_value();
                auto tmp  = std::string(data.begin(), data.end());
                auto text = detail::strip_non_utf_8(tmp);
                if (text.empty()) break;
                sink.add(symusic::detail::make_event<TextMeta<T>>(cur_time, std::move(text)));
                break;
            }
            default: break;
            }
            break;
        }
        default: break;
        }
    }
}

// Merge ordered metadata, note starts and note ends directly into the output.
template<typename Notes>
minimidi::Messages<> merge_midi_notes(
    minimidi::Messages<> messages, const Notes& source, u8 channel
) {
    if (source.empty()) return messages;
    // collect notes and make sure they are sorted
    vec<Note<Tick>> notes(source.size());
    bool            sorted    = true;
    Tick::unit      prev_time = 0;
    for (size_t j = 0; const auto& note : source) {
        notes[j++] = note;
        sorted &= note.time >= prev_time;
        prev_time = note.time;
    }
    if (!sorted) {
        pdqsort_branchless(notes.begin(), notes.end(), [](const auto& a, const auto& b) {
            return a.time < b.time;
        });
    }
    minimidi::Messages<> earlier_note_events;
    earlier_note_events.reserve(notes.size());
    for (const auto& note : notes) {
        if (note.pitch < 0 || note.velocity < 0) {
            throw std::invalid_argument(
                fmt::format(
                    "MIDI note pitch and velocity must be non-negative, got {} and {}",
                    note.pitch,
                    note.velocity
                )
            );
        }
        // Non-negative i8 values fit in seven MIDI data bits.
        const auto pitch    = static_cast<u8>(note.pitch);
        const auto velocity = static_cast<u8>(note.velocity);
        if (note.duration > 0) {
            earlier_note_events.emplace_back(
                minimidi::NoteOff(note.end(), channel, pitch, velocity)
            );
        } else {
            earlier_note_events.emplace_back(minimidi::NoteOn(note.time, channel, pitch, velocity));
        }
    }
    pdqsort_detail::insertion_sort(
        earlier_note_events.begin(), earlier_note_events.end(), [](const auto& a, const auto& b) {
            return a.time < b.time;
        }
    );

    minimidi::Messages<> output;
    output.reserve(messages.size() + notes.size() * 2);
    auto       message = messages.begin();
    const auto emit    = [&](auto&& event) {
        // Metadata comes first at the same tick.
        while (message != messages.end() && message->time <= event.time) {
            output.push_back(std::move(*message++));
        }
        output.push_back(std::move(event));
    };
    auto end = earlier_note_events.begin();
    for (const auto& note : notes) {
        const auto tick = static_cast<u32>(note.time);
        // Note-offs for sustained notes and note-ons for empty notes come first.
        while (end != earlier_note_events.end() && end->time <= tick) { emit(std::move(*end++)); }
        // The first pass validated both data bytes and the caller checked note times.
        const auto pitch    = static_cast<u8>(note.pitch);
        const auto velocity = static_cast<u8>(note.velocity);
        if (note.duration > 0) {
            emit(minimidi::NoteOn(tick, channel, pitch, velocity));
        } else {
            emit(minimidi::NoteOff(tick, channel, pitch, velocity));
        }
    }
    for (; end != earlier_note_events.end(); ++end) { emit(std::move(*end)); }
    output.insert(
        output.end(), std::make_move_iterator(message), std::make_move_iterator(messages.end())
    );
    return output;
}

}   // namespace symusic::detail

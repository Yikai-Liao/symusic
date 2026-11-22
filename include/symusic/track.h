#pragma once

#include <span>

#include "symusic/event.h"
#include "symusic/storage.h"

namespace symusic {

template<TType T, typename S = ValueStorage>
struct Track {
    using ttype = T;
    using unit = typename T::unit;
    using storage_type = S;

    std::string name;
    u8 program = 0;
    bool is_drum = false;
    typename S::template List<Note<T>> notes = S::adopt(vec<Note<T>>{});
    typename S::template List<ControlChange<T>> controls = S::adopt(vec<ControlChange<T>>{});
    typename S::template List<PitchBend<T>> pitch_bends = S::adopt(vec<PitchBend<T>>{});
    typename S::template List<TextMeta<T>> lyrics = S::adopt(vec<TextMeta<T>>{});

    Track() = default;
    Track(std::string name, i64 program, bool is_drum)
        : name(std::move(name)), program(checked_program(program)), is_drum(is_drum) {}
    Track(std::string name, i64 program, bool is_drum,
          typename S::template List<Note<T>> notes,
          typename S::template List<ControlChange<T>> controls,
          typename S::template List<PitchBend<T>> pitch_bends,
          typename S::template List<TextMeta<T>> lyrics)
        : name(std::move(name)), program(checked_program(program)), is_drum(is_drum), notes(std::move(notes)), controls(std::move(controls)), pitch_bends(std::move(pitch_bends)), lyrics(std::move(lyrics)) {}

    void set_program(i64 value) { program = checked_program(value); }

    // Derive completed sustain regions from the current CC64 events.
    [[nodiscard]] vec<Pedal<T>> pedals() const;

    [[nodiscard]] Track copy() const;
    bool operator==(const Track& other) const;

    auto default_key() const { return std::tuple(is_drum, program, std::string_view(name), S::get(notes).size()); }

    [[nodiscard]] static Track deserialize(std::span<const u8> bytes);
    [[nodiscard]] vec<u8> serialize() const;

    // return the start time of the track
    [[nodiscard]] unit start() const;

    // return the end time of the track
    [[nodiscard]] unit end() const;

    // return true if the track is empty
    [[nodiscard]] bool empty() const;

    // return a string representation of the track, same as summary
    [[nodiscard]] std::string to_string() const;

    // summary info of the track
    [[nodiscard]] std::string summary() const;

    void sort(bool reverse = false);

    // For events with duration, clip_end is used to determine whether to clip based on end time.
    [[nodiscard]] Track clip(unit start, unit end, bool clip_end = false) const;

    [[nodiscard]] Track trim(
        unit               start,
        unit               end,
        unit               min_overlap = 0,
        const std::string& start_mode  = "remove",
        const std::string& end_mode    = "remove"
    ) const;

    void shift_time(unit offset);

    void shift_pitch(i32 offset);

    void shift_velocity(i32 offset);

private:
    static u8 checked_program(i64 value) {
        if (value < 0 || value > 127) {
            throw std::invalid_argument("program must be in 0..127, got " + std::to_string(value));
        }
        return static_cast<u8>(value);
    }
};

extern template struct Track<Tick>;
extern template struct Track<Quarter>;
extern template struct Track<Second>;

}  // namespace symusic

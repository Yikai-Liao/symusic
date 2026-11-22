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
    typename S::template List<Pedal<T>> pedals = S::adopt(vec<Pedal<T>>{});
    typename S::template List<TextMeta<T>> lyrics = S::adopt(vec<TextMeta<T>>{});

    Track() = default;
    Track(std::string name, u8 program, bool is_drum)
        : name(std::move(name)), program(program), is_drum(is_drum) {}
    Track(std::string name, u8 program, bool is_drum,
          typename S::template List<Note<T>> notes,
          typename S::template List<ControlChange<T>> controls,
          typename S::template List<PitchBend<T>> pitch_bends,
          typename S::template List<Pedal<T>> pedals,
          typename S::template List<TextMeta<T>> lyrics)
        : name(std::move(name)), program(program), is_drum(is_drum), notes(std::move(notes)), controls(std::move(controls)), pitch_bends(std::move(pitch_bends)), pedals(std::move(pedals)), lyrics(std::move(lyrics)) {}

    [[nodiscard]] Track copy() const { return *this; }
    [[nodiscard]] Track deepcopy() const;
    bool operator==(const Track& other) const;

    auto default_key() const { return std::tuple(is_drum, program, std::string_view(name), note_num()); }

    template<DataFormat F>
    [[nodiscard]] static Track parse(std::span<const u8> bytes) {
        static_assert(F == DataFormat::ZPP);
        return parse_zpp_bytes(bytes);
    }

    template<DataFormat F>
    [[nodiscard]] vec<u8> dumps() const {
        static_assert(F == DataFormat::ZPP);
        return dump_zpp_bytes();
    }

    [[nodiscard]] static Track parse_zpp_bytes(std::span<const u8> bytes);
    [[nodiscard]] vec<u8> dump_zpp_bytes() const;


    // return the start time of the track
    [[nodiscard]] unit start() const;

    // return the end time of the track
    [[nodiscard]] unit end() const;

    // return the number of notes in the track
    [[nodiscard]] size_t note_num() const;

    // return true if the track is empty
    [[nodiscard]] bool empty() const;

    // return a string representation of the track, same as summary
    [[nodiscard]] std::string to_string() const;

    // summary info of the track
    [[nodiscard]] std::string summary() const;

    // non-inplace sort, return a new Track
    [[nodiscard]] Track sort(bool reverse = false);

    void sort_inplace(bool reverse = false);

    // Clip all the events in the track, non-inplace, return a new Track
    // For events with duration, clip_end is used to determine whether to clip based on end time.
    void clip_inplace(unit start, unit end, bool clip_end = false);

    [[nodiscard]] Track clip(unit start, unit end, bool clip_end = false) const;

    void trim_inplace(
        unit               start,
        unit               end,
        unit               min_overlap = 0,
        const std::string& start_mode  = "remove",
        const std::string& end_mode    = "remove"
    );

    [[nodiscard]] Track trim(
        unit               start,
        unit               end,
        unit               min_overlap = 0,
        const std::string& start_mode  = "remove",
        const std::string& end_mode    = "remove"
    ) const;

    // shift the time of all the events in the track, non-inplace, return a new Track
    [[nodiscard]] Track shift_time(unit offset) const;

    void shift_time_inplace(unit offset);

    // shift the pitch of all notes in the track, non-inplace, return a new Track
    [[nodiscard]] Track shift_pitch(i8 offset) const;

    void shift_pitch_inplace(i8 offset);

    // shift the velocity of all notes in the track, non-inplace, return a new Track
    [[nodiscard]] Track shift_velocity(i8 offset) const;

    void shift_velocity_inplace(i8 offset);
};

extern template struct Track<Tick>;
extern template struct Track<Quarter>;
extern template struct Track<Second>;

}  // namespace symusic

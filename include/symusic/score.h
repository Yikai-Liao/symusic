#pragma once

#include <span>

#include "symusic/event.h"
#include "symusic/storage.h"
#include "symusic/track.h"

namespace symusic {

template<TType T, typename S = ValueStorage>
struct Score {
    using ttype = T;
    using unit = typename T::unit;
    using storage_type = S;

    using track_type = Track<T, S>;
    using track_handle = typename S::template TrackHandle<track_type>;

    i32 ticks_per_quarter = 0;
    typename S::template Tracks<track_type> tracks = S::adopt_tracks(vec<track_handle>{});
    typename S::template List<TimeSignature<T>> time_signatures = S::adopt(vec<TimeSignature<T>>{});
    typename S::template List<KeySignature<T>> key_signatures = S::adopt(vec<KeySignature<T>>{});
    typename S::template List<Tempo<T>> tempos = S::adopt(vec<Tempo<T>>{});
    typename S::template List<TextMeta<T>> markers = S::adopt(vec<TextMeta<T>>{});

    Score() = default;
    explicit Score(i32 tpq) : ticks_per_quarter(tpq) {}
    Score(i32 tpq, typename S::template Tracks<track_type> tracks,
          typename S::template List<TimeSignature<T>> time_signatures,
          typename S::template List<KeySignature<T>> key_signatures,
          typename S::template List<Tempo<T>> tempos,
          typename S::template List<TextMeta<T>> markers)
        : ticks_per_quarter(tpq), tracks(std::move(tracks)), time_signatures(std::move(time_signatures)), key_signatures(std::move(key_signatures)), tempos(std::move(tempos)), markers(std::move(markers)) {}

    [[nodiscard]] Score copy() const { return *this; }
    [[nodiscard]] Score deepcopy() const;
    bool operator==(const Score& other) const;

    template<DataFormat F>
    [[nodiscard]] static Score parse(std::span<const u8> bytes) {
        if constexpr (F == DataFormat::MIDI) return parse_midi_bytes(bytes, false);
        if constexpr (F == DataFormat::ZPP) return parse_zpp_bytes(bytes);
    }

    template<DataFormat F>
    [[nodiscard]] vec<u8> dumps() const {
        if constexpr (F == DataFormat::MIDI) return dump_midi_bytes();
        if constexpr (F == DataFormat::ZPP) return dump_zpp_bytes();
    }

    [[nodiscard]] static Score parse_midi_bytes(std::span<const u8> bytes, bool sanitize_data);
    [[nodiscard]] vec<u8> dump_midi_bytes() const;
    [[nodiscard]] static Score parse_zpp_bytes(std::span<const u8> bytes);
    [[nodiscard]] vec<u8> dump_zpp_bytes() const;

    // return the start time of the score
    [[nodiscard]] unit start() const;

    // return the end time of the score
    [[nodiscard]] unit end() const;

    // return the number of notes in the score
    [[nodiscard]] size_t note_num() const;

    // return the number of tracks in the score
    [[nodiscard]] size_t track_num() const;

    // return true if the score is empty
    [[nodiscard]] bool empty() const;

    // return beat locations in the current time unit
    [[nodiscard]] vec<unit> get_beats(unit start_time = 0) const;

    // return downbeat locations in the current time unit
    [[nodiscard]] vec<unit> get_downbeats(unit start_time = 0) const;

    // return a string representation of the score, same as summary
    [[nodiscard]] std::string to_string() const;

    // summary info of the score
    [[nodiscard]] std::string summary() const;

    // non-inplace sort, return a new score
    [[nodiscard]] Score sort(bool reverse = false) const;

    void sort_inplace(bool reverse = false);

    // Clip all the events in the score, non-inplace, return a new Score
    // For events with duration, clip_end is used to determine whether to clip based on end time.
    [[nodiscard]] Score clip(unit start, unit end, bool clip_end = false) const;
    void                clip_inplace(unit start, unit end, bool clip_end = false);

    [[nodiscard]] Score trim(
        unit               start,
        unit               end,
        unit               min_overlap = 0,
        const std::string& start_mode  = "remove",
        const std::string& end_mode    = "remove"
    ) const;
    void trim_inplace(
        unit               start,
        unit               end,
        unit               min_overlap = 0,
        const std::string& start_mode  = "remove",
        const std::string& end_mode    = "remove"
    );

    // shift the time of all the events in the score, non-inplace, return a new Score
    [[nodiscard]] Score shift_time(unit offset) const;

    void shift_time_inplace(unit offset);

    // shift the pitch of all notes in the score, non-inplace, return a new Score
    [[nodiscard]] Score shift_pitch(i8 offset) const;

    void shift_pitch_inplace(i8 offset);

    // shift the velocity of all notes in the score, non-inplace, return a new Score
    [[nodiscard]] Score shift_velocity(i8 offset) const;

    void shift_velocity_inplace(i8 offset);
};

extern template struct Score<Tick>;
extern template struct Score<Quarter>;
extern template struct Score<Second>;

}  // namespace symusic

namespace symusic::detail {
template<TType T, typename S>
vec<u8> dump_track_list(const typename S::template Tracks<Track<T, S>>& tracks);
}  // namespace symusic::detail

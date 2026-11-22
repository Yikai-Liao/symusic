#pragma once

#include <span>

#include "symusic/event.h"
#include "symusic/storage.h"
#include "symusic/track.h"

namespace symusic {

enum class InvalidDataBytes { Raise, Clamp };

struct MidiReadOptions {
    InvalidDataBytes invalid_data_bytes = InvalidDataBytes::Raise;
};


template<TType T, typename S = ValueStorage>
struct Score {
    using ttype        = T;
    using unit         = typename T::unit;
    using storage_type = S;

    using track_type   = Track<T, S>;
    using track_handle = typename S::template TrackHandle<track_type>;

    i32                                         ticks_per_quarter = 960;
    typename S::template Tracks<track_type>     tracks = S::adopt_tracks(vec<track_handle>{});
    typename S::template List<TimeSignature<T>> time_signatures = S::adopt(vec<TimeSignature<T>>{});
    typename S::template List<KeySignature<T>>  key_signatures  = S::adopt(vec<KeySignature<T>>{});
    typename S::template List<Tempo<T>>         tempos          = S::adopt(vec<Tempo<T>>{});
    typename S::template List<TextMeta<T>>      markers         = S::adopt(vec<TextMeta<T>>{});

    Score() = default;
    explicit Score(i64 resolution) { set_ticks_per_quarter(resolution); }

    void set_ticks_per_quarter(i64 resolution) {
        if (resolution <= 0 || !std::in_range<i32>(resolution)) {
            throw std::invalid_argument(
                "ticks_per_quarter must be in 1..2147483647, got " + std::to_string(resolution)
            );
        }
        ticks_per_quarter = static_cast<i32>(resolution);
    }

    Score(
        i32                                         ticks_per_quarter,
        typename S::template Tracks<track_type>     tracks,
        typename S::template List<TimeSignature<T>> time_signatures,
        typename S::template List<KeySignature<T>>  key_signatures,
        typename S::template List<Tempo<T>>         tempos,
        typename S::template List<TextMeta<T>>      markers
    ) :
        ticks_per_quarter(ticks_per_quarter), tracks(std::move(tracks)),
        time_signatures(std::move(time_signatures)), key_signatures(std::move(key_signatures)),
        tempos(std::move(tempos)), markers(std::move(markers)) {}

    [[nodiscard]] Score copy() const;
    bool                operator==(const Score& other) const;

    // Read Standard MIDI File bytes, with optional recovery of out-of-range data bytes.
    [[nodiscard]] static Score from_midi(std::span<const u8> bytes, MidiReadOptions options = {});

    // Encode the score as a Standard MIDI File.
    [[nodiscard]] vec<u8> to_midi() const;

    [[nodiscard]] static Score deserialize(std::span<const u8> bytes);
    [[nodiscard]] vec<u8>      serialize() const;

    // return the start time of the score
    [[nodiscard]] unit start() const;

    // return the end time of the score
    [[nodiscard]] unit end() const;

    // return the number of notes in the score
    [[nodiscard]] size_t note_count() const;

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

    void sort(bool reverse = false);

    // For events with duration, clip_end is used to determine whether to clip based on end time.
    [[nodiscard]] Score clip(unit start, unit end, bool clip_end = false) const;

    [[nodiscard]] Score trim(
        unit               start,
        unit               end,
        unit               min_overlap = 0,
        const std::string& start_mode  = "remove",
        const std::string& end_mode    = "remove"
    ) const;

    void shift_time(unit offset);

    void shift_pitch(i32 offset);

    void shift_velocity(i32 offset);
};

extern template struct Score<Tick>;
extern template struct Score<Quarter>;
extern template struct Score<Second>;

}   // namespace symusic

namespace symusic::detail {
template<TType T, typename S>
vec<u8> dump_track_list(const typename S::template Tracks<Track<T, S>>& tracks);
}   // namespace symusic::detail

#pragma once

#include "symusic/pianoroll.h"

namespace symusic {

namespace detail {

/// Multiply dimensions, throwing std::length_error (ValueError in Python) when the element count
/// would not fit in a signed object size.
inline size_t checked_volume(std::initializer_list<size_t> dims) {
    size_t volume = 1;
    for (const size_t dim : dims) {
        if (dim != 0 && volume > static_cast<size_t>(std::numeric_limits<ptrdiff_t>::max()) / dim) {
            throw std::length_error("Pianoroll dimensions are too large.");
        }
        volume *= dim;
    }
    return volume;
}

/// Time-axis length for a track or score ending at `end` (exclusive end plus one slot).
/// The sum is done in 64 bits so `end == INT32_MAX` cannot overflow.
inline size_t time_dim_from_end(const i32 end) {
    if (end < 0) throw std::invalid_argument("Cannot build a pianoroll: the end time is negative.");
    // A non-negative i32 plus one fits in the size_t of supported targets.
    return static_cast<size_t>(end) + 1;
}

/// Validate one note against a time axis of `time_dim` slots and return its start and end.
/// Throws std::invalid_argument for a negative time or duration, or a note that runs past the axis.
/// A zero duration is valid: frame mode writes nothing for it.
inline std::pair<size_t, size_t> checked_note_span(
    const int64_t time, const int64_t duration, const size_t time_dim
) {
    if (time < 0) {
        throw std::invalid_argument(
            "Cannot build a pianoroll: note time " + std::to_string(time) + " is negative."
        );
    }
    if (duration < 0) {
        throw std::invalid_argument(
            "Cannot build a pianoroll: note duration " + std::to_string(duration) + " is negative."
        );
    }
    const int64_t end = time + duration;   // both are non-negative i32 values, so no overflow
    if (static_cast<uint64_t>(end) >= time_dim) {
        throw std::invalid_argument(
            "Cannot build a pianoroll: note end " + std::to_string(end)
            + " lies beyond the track end."
        );
    }
    // The checks above establish 0 <= time <= end < time_dim.
    return {static_cast<size_t>(time), static_cast<size_t>(end)};
}

inline pianoroll_t encode_velocity(i8 velocity, bool enabled) {
    if (!enabled) return 1;
    if (velocity < 0) {
        throw std::invalid_argument(
            "Pianoroll note velocity must be non-negative, got " + std::to_string(velocity)
        );
    }
    return static_cast<pianoroll_t>(velocity);
}

inline void check_index(const char* name, const size_t index, const size_t dim) {
    if (index >= dim) {
        throw std::out_of_range(
            std::string("Pianoroll ") + name + " index " + std::to_string(index)
            + " is out of range for size " + std::to_string(dim) + "."
        );
    }
}

/// Check that `duration` slots starting at `start` fit inside `time_dim` without overflowing.
inline void check_time_span(const size_t start, const size_t duration, const size_t time_dim) {
    if (start > time_dim || duration > time_dim - start) {
        throw std::out_of_range(
            "Pianoroll time span [" + std::to_string(start) + ", +" + std::to_string(duration)
            + ") is out of range for size " + std::to_string(time_dim) + "."
        );
    }
}

}   // namespace detail

template<typename S>
TrackPianoroll TrackPianoroll::from_track(
    const Track<Tick, S>&             track,
    const std::vector<PianorollMode>& modes,
    const std::pair<uint8_t, uint8_t> pitch_range,
    const bool                        encode_note_velocity
) {
    if (pitch_range.second < pitch_range.first) {
        throw std::invalid_argument("Pianoroll pitch range is reversed.");
    }
    const size_t   time_dim = detail::time_dim_from_end(track.end());
    TrackPianoroll pianoroll(modes.size(), pitch_range.second - pitch_range.first, time_dim);

    for (const auto& note : S::get(track.notes)) {
        if (note.pitch >= pitch_range.first && note.pitch < pitch_range.second) {
            const auto [note_start, note_end]
                = detail::checked_note_span(note.time, note.duration, time_dim);
            for (size_t mode_index = 0; mode_index < modes.size(); ++mode_index) {
                // Invariant: mode_index < modes.size(), the pitch offset is below the pitch range
                // size, and checked_note_span proved the span lies inside the time axis.
                pianoroll.fill_unchecked(
                    mode_index,
                    note.pitch - pitch_range.first,
                    modes[mode_index] != PianorollMode::Offset ? note_start : note_end,
                    modes[mode_index] == PianorollMode::Frame ? note_end - note_start : 1,
                    detail::encode_velocity(note.velocity, encode_note_velocity)
                );
            }
        }
    }

    return pianoroll;
};

template<typename S>
ScorePianoroll ScorePianoroll::from_score(
    const Score<Tick, S>&             score,
    const std::vector<PianorollMode>& modes,
    const std::pair<uint8_t, uint8_t> pitch_range,
    const bool                        encode_note_velocity
) {
    if (pitch_range.second < pitch_range.first) {
        throw std::invalid_argument("Pianoroll pitch range is reversed.");
    }
    const auto&    tracks   = S::get(score.tracks);
    const size_t   time_dim = detail::time_dim_from_end(score.end());
    ScorePianoroll pianoroll(
        modes.size(), tracks.size(), pitch_range.second - pitch_range.first, time_dim
    );

    for (size_t track_index = 0; track_index < tracks.size(); ++track_index) {
        for (const auto& track = S::get(tracks[track_index]);
             const auto& note : S::get(track.notes)) {
            if (note.pitch >= pitch_range.first && note.pitch < pitch_range.second) {
                const auto [note_start, note_end]
                    = detail::checked_note_span(note.time, note.duration, time_dim);
                for (size_t mode_index = 0; mode_index < modes.size(); ++mode_index) {
                    // Invariant: mode_index < modes.size(), track_index < tracks.size(), the pitch
                    // offset is below the pitch range size, and checked_note_span proved the
                    // span lies inside the time axis.
                    pianoroll.fill_unchecked(
                        mode_index,
                        track_index,
                        note.pitch - pitch_range.first,
                        modes[mode_index] != PianorollMode::Offset ? note_start : note_end,
                        modes[mode_index] == PianorollMode::Frame ? note_end - note_start : 1,
                        detail::encode_velocity(note.velocity, encode_note_velocity)
                    );
                }
            }
        }
    }

    return pianoroll;
}

}   // namespace symusic

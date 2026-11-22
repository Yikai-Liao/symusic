#pragma once

#include <cstdint>
#include <memory>
#include <stdexcept>
#include <tuple>
#include <utility>

#include "symusic/score.h"
#include "symusic/track.h"

namespace symusic {

using pianoroll_t = uint8_t;

enum class PianorollMode : u8 { Onset, Frame, Offset };

inline PianorollMode str_to_pianoroll_mode(std::string_view mode) {
    if (mode == "onset") return PianorollMode::Onset;
    if (mode == "frame") return PianorollMode::Frame;
    if (mode == "offset") return PianorollMode::Offset;
    throw std::invalid_argument("Unknown pianoroll mode: " + std::string(mode));
}

class TrackPianoroll {
private:
    size_t                         mode_dim;
    size_t                         pitch_dim;
    size_t                         time_dim;
    std::unique_ptr<pianoroll_t[]> data_ptr;

    /// Unchecked fill used by `from_track` after it has validated every index.
    void fill_unchecked(
        size_t mode, size_t pitch, size_t start, size_t duration, pianoroll_t value
    ) noexcept;

public:
    TrackPianoroll(size_t mode_dim, size_t pitch_dim, size_t time_dim);
    TrackPianoroll(const TrackPianoroll&)                = delete;
    TrackPianoroll& operator=(const TrackPianoroll&)     = delete;
    TrackPianoroll(TrackPianoroll&&) noexcept            = default;
    TrackPianoroll& operator=(TrackPianoroll&&) noexcept = default;
    ~TrackPianoroll()                                    = default;

    template<typename S>
    static TrackPianoroll from_track(
        const Track<Tick, S>&                track,
        const std::vector<PianorollMode>& modes,
        std::pair<uint8_t, uint8_t>       pitch_range,
        bool                              encode_note_velocity
    );

    pianoroll_t* data();

    [[nodiscard]] const pianoroll_t* data() const;

    pianoroll_t* release();

    pianoroll_t* operator()(size_t mode, size_t pitch, size_t time);

    pianoroll_t get(size_t mode, size_t pitch, size_t time);
    void        set(size_t mode, size_t pitch, size_t start, size_t duration, pianoroll_t value);
    void        clear();

    [[nodiscard]] std::tuple<size_t, size_t, size_t> dims() const;
};

class ScorePianoroll {
private:
    size_t                         mode_dim;
    size_t                         track_dim;
    size_t                         pitch_dim;
    size_t                         time_dim;
    std::unique_ptr<pianoroll_t[]> data_ptr;

    /// Unchecked fill used by `from_score` after it has validated every index.
    void fill_unchecked(
        size_t mode, size_t track, size_t pitch, size_t start, size_t duration, pianoroll_t value
    ) noexcept;

public:
    ScorePianoroll(size_t mode_dim, size_t track_dim, size_t pitch_dim, size_t time_dim);
    ScorePianoroll(const ScorePianoroll&)                = delete;
    ScorePianoroll& operator=(const ScorePianoroll&)     = delete;
    ScorePianoroll(ScorePianoroll&&) noexcept            = default;
    ScorePianoroll& operator=(ScorePianoroll&&) noexcept = default;
    ~ScorePianoroll()                                    = default;

    template<typename S>
    static ScorePianoroll from_score(
        const Score<Tick, S>&                score,
        const std::vector<PianorollMode>& modes,
        std::pair<uint8_t, uint8_t>       pitch_range,
        bool                              encode_note_velocity
    );

    pianoroll_t* data();

    [[nodiscard]] const pianoroll_t* data() const;

    pianoroll_t* release();

    pianoroll_t* operator()(size_t mode, size_t track, size_t pitch, size_t time);

    pianoroll_t get(size_t mode, size_t track, size_t pitch, size_t time);

    void set(
        size_t mode, size_t track, size_t pitch, size_t start, size_t duration, pianoroll_t value
    );

    void clear();

    [[nodiscard]] std::tuple<size_t, size_t, size_t, size_t> dims() const;
};

}   // namespace symusic

#pragma once

#ifndef LIBSYMUSIC_PIANOROLL_H
#define LIBSYMUSIC_PIANOROLL_H

#include <cstdint>
#include <memory>
#include <stdexcept>
#include <tuple>
#include <utility>

#include "symusic/score.h"
#include "symusic/track.h"

namespace symusic {

typedef uint8_t pianoroll_t;

#define PIANOROLL_MODES      \
    PIANOROLL_MODE(onset, 0) \
    PIANOROLL_MODE(frame, 1) \
    PIANOROLL_MODE(offset, 2)

enum class PianorollMode : uint8_t {
#define PIANOROLL_MODE(name, flag) name = flag,
    PIANOROLL_MODES
#undef PIANOROLL_MODE
};

static PianorollMode str_to_pianoroll_mode(const std::string& modeStr) {
#define PIANOROLL_MODE(name, flag) \
    if (modeStr == (#name)) return PianorollMode::name;
    PIANOROLL_MODES
#undef PIANOROLL_MODE
    throw std::invalid_argument("No such pianoroll mode called " + modeStr + ".");
};

class TrackPianoroll {
private:
    size_t                      modeDim;
    size_t                      pitchDim;
    size_t                      timeDim;
    std::unique_ptr<pianoroll_t[]> dataPtr;

public:
    TrackPianoroll(size_t modeDim, size_t pitchDim, size_t timeDim);
    TrackPianoroll(const TrackPianoroll&) = delete;
    TrackPianoroll& operator=(const TrackPianoroll&) = delete;
    TrackPianoroll(TrackPianoroll&&) noexcept = default;
    TrackPianoroll& operator=(TrackPianoroll&&) noexcept = default;
    ~TrackPianoroll() = default;

    static TrackPianoroll from_track(
        const Track<Tick>&                track,
        const std::vector<PianorollMode>& modes,
        std::pair<uint8_t, uint8_t>       pitchRange,
        bool                              encodeVelocity
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
    size_t                      modeDim;
    size_t                      trackDim;
    size_t                      pitchDim;
    size_t                      timeDim;
    std::unique_ptr<pianoroll_t[]> dataPtr;

public:
    ScorePianoroll(size_t modeDim, size_t trackDim, size_t pitchDim, size_t timeDim);
    ScorePianoroll(const ScorePianoroll&) = delete;
    ScorePianoroll& operator=(const ScorePianoroll&) = delete;
    ScorePianoroll(ScorePianoroll&&) noexcept = default;
    ScorePianoroll& operator=(ScorePianoroll&&) noexcept = default;
    ~ScorePianoroll() = default;

    static ScorePianoroll from_score(
        const Score<Tick>&                   score,
        const std::vector<PianorollMode>& modes,
        std::pair<uint8_t, uint8_t>       pitchRange,
        bool                              encodeVelocity
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


#endif   // LIBSYMUSIC_PIANOROLL_H

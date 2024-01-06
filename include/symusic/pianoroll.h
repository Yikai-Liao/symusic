#pragma once

#ifndef LIBSYMUSIC_PIANOROLL_H
#define LIBSYMUSIC_PIANOROLL_H

#include <cstdint>
#include <cstddef>
#include <exception>
#include <algorithm>
#include <utility>
#include <tuple>
#include <assert.h>

#include "symusic/track.h"
#include "symusic/score.h"

namespace symusic {

typedef uint8_t pianoroll_t;

#define PIANOROLL_MODES                            \
    PIANOROLL_MODE(Onset, 0)                       \
    PIANOROLL_MODE(Frame, 1)                       \
    PIANOROLL_MODE(Offset, 2)

enum class PianorollMode : uint8_t {
#define PIANOROLL_MODE(name, flag) name=flag,
    PIANOROLL_MODES
#undef PIANOROLL_MODE
};

class TrackPianoroll
{
private:
    size_t modeDim;
    size_t pitchDim;
    size_t timeDim;
    pianoroll_t* data;

public:
    TrackPianoroll(size_t modeDim, size_t pitchDim, size_t timeDim);
    ~TrackPianoroll();

    static TrackPianoroll from_track(const Track<Tick>& track,
        const std::vector<PianorollMode>& modes,
        std::pair<uint8_t, uint8_t> pitchRange,
        bool encodeVelocity,
        bool deOverlap);

    pianoroll_t* get_data();
    pianoroll_t* release_data();

    pianoroll_t* operator()(size_t mode, size_t pitch, size_t time);

    pianoroll_t get(size_t mode, size_t pitch, size_t time);
    void set(size_t mode, size_t pitch, size_t start, size_t duration, pianoroll_t value);
    void clear();

    const std::tuple<size_t, size_t, size_t> dims() const;
};



class ScorePianoroll
{
private:
    size_t modeDim;
    size_t trackDim;
    size_t pitchDim;
    size_t timeDim;
    pianoroll_t* data;

public:
    ScorePianoroll(size_t modeDim, size_t trackDim, size_t pitchDim, size_t timeDim);
    ~ScorePianoroll();
    static ScorePianoroll from_track(const Score<Tick>& score,
        const std::vector<PianorollMode>& modes,
        std::pair<uint8_t, uint8_t> pitchRange,
        bool encodeVelocity,
        bool deOverlap);

    pianoroll_t* get_data();
    pianoroll_t* release_data();

    pianoroll_t* operator()(size_t mode, size_t track, size_t pitch, size_t time);

    pianoroll_t get(size_t mode, size_t track, size_t pitch, size_t time);
    void set(size_t mode, size_t track, size_t pitch, size_t start, size_t duration, pianoroll_t value);
    void clear();

    const std::tuple<size_t, size_t, size_t, size_t> dims() const;
};

}


#endif // LIBSYMUSIC_PIANOROLL_H

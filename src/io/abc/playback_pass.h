#pragma once

#include "syntax.h"
#include "symusic/track.h"

namespace symusic::abc::detail {

// A declared lane in MIDI track order. Its melody tracks precede its backing.
// All pitches are resolved at attack time.
struct PlaybackPass {
    Location               declaration;
    vec<TrackNative<Tick>> tracks;
};

}   // namespace symusic::abc::detail

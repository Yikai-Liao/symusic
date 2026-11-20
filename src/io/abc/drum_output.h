#pragma once

#include "settings.h"
#include "symusic/score.h"

namespace symusic::abc::detail {

// An attack whose source pitch has not been remapped in this pass uses the
// map inherited from preceding MIDI tracks. Keep only those attacks pending.
struct RenderedTrack {
    TrackNative<Tick> track;
    vec<size_t>       inherited_drums;

    void append_drum(Note<Tick> note, const DrumMap& changes);
    void discard_silent_notes();
    void resolve_drums(const DrumMap& initial);
};

// A melody or backing pass in MIDI track order. Publishing resolves its
// inherited pitches and leaves its last map changes for the next pass.
struct PlaybackPass {
    Location           declaration;
    vec<RenderedTrack> tracks;
    DrumMap            drum_changes;

    void publish(vec<TrackNative<Tick>>& output, DrumMap& drum_map);
};

}   // namespace symusic::abc::detail

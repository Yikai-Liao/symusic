#pragma once

#include <map>

#include "expression.h"
#include "navigation.h"
#include "pitch_tuning.h"
#include "symusic/score.h"

namespace symusic::abc::detail {

struct TuneSettings {
    Meter                             meter;
    Key                               key;
    PitchTuning                       tuning;
    AccidentalPolicy                  accidentals;
    i32                               tempo      = 500000;
    long double                       unit       = whole / 8;
    int                               midi_shift = 0;
    RhythmStyle                       rhythm     = RhythmStyle::straight;
    std::string                       title;
    std::array<std::string_view, 128> symbols{};
};

struct LyricVerse {
    Location    at;
    std::string text;
};
using VoiceLyrics = std::map<size_t, vec<LyricVerse>>;

// One voice owns its clock, instrument changes, ties and backing patterns.
// Metadata is collected with its source location for tune-level reconciliation.
template<class T>
using Located = std::pair<T, Location>;

struct Performance {
    // Lyrics, overlays and generated backing can need a conductor track
    // even when the tune declares only one voice.
    bool                              needs_conductor = false;
    vec<TrackNative<Tick>>            tracks;
    vec<Located<Tempo<Tick>>>         tempos;
    vec<Located<TimeSignature<Tick>>> meters;
    vec<Located<KeySignature<Tick>>>  keys;
    vec<TextMeta<Tick>>               markers;
};

Performance perform(
    const TuneSettings& settings,
    const vec<Token>&   preamble,
    const PlayOrder&    order,
    const VoiceLyrics&  lyrics,
    std::string         name,
    DrumMap&            drum_map,
    Diagnostics&        diagnostics
);

}   // namespace symusic::abc::detail

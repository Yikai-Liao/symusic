#pragma once

#include "navigation.h"
#include "settings.h"
#include "symusic/score.h"

namespace symusic::abc::detail {

struct TuneSettings {
    Meter                             meter;
    Key                               key;
    i32                               tempo = 500000;
    long double                       unit  = whole / 8;
    std::string                       title;
    std::array<std::string_view, 128> symbols{};
};

// One voice owns its clock, instrument changes, ties and backing patterns.
// Metadata is collected with its source location for tune-level reconciliation.
template<class T>
using Located = std::pair<T, Location>;

struct Performance {
    vec<TrackNative<Tick>>            tracks;
    vec<Located<Tempo<Tick>>>         tempos;
    vec<Located<TimeSignature<Tick>>> meters;
    vec<Located<KeySignature<Tick>>>  keys;
    vec<TextMeta<Tick>>               markers;
};

Performance perform(
    const TuneSettings& settings, const PlayOrder& order, std::string name, Diagnostics& diagnostics
);

}   // namespace symusic::abc::detail

#pragma once

#include "syntax.h"

namespace symusic::abc::detail {

// Playback visits immutable source tokens. Expanded repeats and ordered parts
// keep the location of their original token and are bounded before rendering.
using PlayOrder = vec<const Token*>;
PlayOrder play_order(
    const vec<Token>& source, std::string_view parts, Location at, Diagnostics& diagnostics
);

}   // namespace symusic::abc::detail

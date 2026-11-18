#pragma once

#include "syntax.h"

namespace symusic::abc::detail {

// The score clock only moves forward. Repeat jumps restore written state, not
// emitted notes, the accent clock, or running MIDI effects.
enum class Flow { token, enter_repeat, jump_repeat, soft_bar, hard_bar };
struct PlayStep {
    const Token* token;
    Flow         flow = Flow::token;
};
using PlayOrder = vec<PlayStep>;

PlayOrder play_order(
    const vec<Token>& source, std::string_view parts, Location at, Diagnostics& diagnostics
);

}   // namespace symusic::abc::detail

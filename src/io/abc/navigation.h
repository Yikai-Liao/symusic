#pragma once

#include "syntax.h"

namespace symusic::abc::detail {

// The score clock only moves forward. Repeat jumps restore written state, not
// emitted notes, the accent clock, or running MIDI effects.
enum class Flow { token, enter_part, enter_repeat, jump_repeat, soft_bar, hard_bar };
struct PlayStep {
    const Token* token;
    Flow         flow = Flow::token;
};
using PlayOrder = vec<PlayStep>;

// Compile the tune-level P: order once. Each voice supplies its own music;
// missing sections warn, and expansion never exceeds the navigation limit.
class PartItinerary {
public:
    PartItinerary(std::string_view order, Location at, Diagnostics& diagnostics);
    [[nodiscard]] PlayOrder arrange(const vec<Token>& voice) const;

private:
    Location                   at;
    Diagnostics&               diagnostics;
    std::optional<std::string> labels;
};

}   // namespace symusic::abc::detail

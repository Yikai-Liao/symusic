#pragma once

#include "syntax.h"

namespace symusic::abc::detail {

// Navigation is a schedule, not a clock. A token can appear several times
// with the same source location. A flow marker tells playback when to apply
// a boundary or restore repeat-entry state without rewinding emitted time.
enum class Flow : u8 {
    token,
    enter_part,
    enter_repeat,
    enter_implicit_repeat,
    jump_repeat,
    soft_bar,
    hard_bar,
    inline_ending
};
struct PlayStep {
    const Token* token;
    Flow         flow = Flow::token;
    // The jump bypasses the written music-line entry, so the singer keeps the verse.
    bool continues_lyrics = false;   // only at jump_repeat
    u32  part_occurrence  = 0;       // only at enter_part
};
using PlayOrder = vec<PlayStep>;

// Compile the tune-level P: order once, then expand it and each voice's repeats
// independently. The returned schedule borrows tokens from voice, which must
// outlive it and not be modified during use. Missing sections warn; exceeding
// the navigation limit raises rather than returning a partial schedule.
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

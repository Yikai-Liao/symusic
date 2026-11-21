#pragma once

#include <array>
#include <optional>
#include <span>
#include <string>
#include <string_view>

#include "syntax.h"

namespace symusic::abc::detail {

enum class Flow : u8 {
    Token,
    EnterPart,
    EnterRepeat,
    EnterImplicitRepeat,
    JumpRepeat,
    SoftBar,
    HardBar,
    InlineEnding
};
// Half-open positions in the finalized written voice. A token range plays all its
// tokens. Each other flow is a singleton action anchored at the indicated token.
// Apply the written snapshot at first before dispatch when restore_written is set.
struct PlayRange {
    size_t first, last;
    Flow   flow             = Flow::Token;
    bool   restore_written  = false;
    bool   continues_lyrics = false;   // A repeat jump bypasses the lyric line entry.
    u32    part_occurrence  = 0;       // Zero-based verse selection on part entry.

    template<class T>
    [[nodiscard]] std::span<T> tokens(std::span<T> voice) const {
        return voice.subspan(first, last - first);
    }
};
using PlayOrder = vec<PlayRange>;

// Expand the tune's P: order and each voice's repeats within the playback token
// limit. Missing sections warn. Exceeding the limit raises before playback.
class PartItinerary {
public:
    PartItinerary(std::string_view order, Location at, Diagnostics& diagnostics);
    [[nodiscard]] PlayOrder arrange(const vec<Token>& voice) const;

private:
    Location                   at;
    Diagnostics&               diagnostics;
    std::optional<std::string> labels;
    std::array<size_t, 26>     visits{};
};

}   // namespace symusic::abc::detail

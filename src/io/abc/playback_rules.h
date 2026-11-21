#pragma once

#include <cstddef>
#include <string_view>

#include "symusic/mtype.h"

namespace symusic::abc::detail {

// The ABC playback clock is distinct from written time. The exporter inverts
// these offsets when constructing its timeline; it does not infer them from
// the spelling of a note or MIDI directive.
struct PlaybackGrid {
    static constexpr i32 kAttackOffset = 1;
    static constexpr i32 kReleaseTrim  = 1;
    // Separate conductor tracks advance without the melodic pickup tick.
    static constexpr i32 tempo_offset(bool conductor) { return conductor ? 0 : kAttackOffset; }

    // Untied notes are shortened on export's target grid unless compensated.
    static constexpr i32 kExportReleaseExtension = kReleaseTrim;
    static constexpr i32 kExportTail             = kReleaseTrim;
};

// Charge work at dispatch and emission, independently of navigation's cap on
// the number of materialized steps. Export budgets a conservative upper bound
// using the same costs, before it publishes the resulting text.
struct PlaybackWork {
    static constexpr std::size_t kLimit  = 1'000'000;
    static constexpr std::size_t kToken  = 1;
    static constexpr std::size_t kAttack = 1;

    static constexpr std::size_t text(std::string_view value) { return value.size() / 64; }
    static constexpr std::size_t chord(std::size_t members, std::size_t ties) {
        return members + ties;
    }
    static constexpr std::size_t written_chord(std::size_t members) {
        // A written interval can attack all its members, or sustain as many
        // ties as were previously active, including notes now ending. Charge
        // each outgoing tie to the interval that created it.
        return chord(members, members) + members * kAttack;
    }
};

}   // namespace symusic::abc::detail

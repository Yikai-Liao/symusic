#pragma once

#include <cstddef>
#include <string_view>

namespace symusic::abc::detail {

// The ABC playback clock is distinct from written time. The exporter inverts
// these offsets when constructing its timeline; it does not infer them from
// the spelling of a note or MIDI directive.
struct PlaybackGrid {
    static constexpr int attack_offset = 1;
    static constexpr int release_trim  = 1;
    // Separate conductor tracks advance without the melodic pickup tick.
    static constexpr int tempo_offset(bool conductor) { return conductor ? 0 : attack_offset; }

    // Untied notes are shortened on export's target grid unless compensated.
    static constexpr int export_release_extension = release_trim;
    static constexpr int export_tail              = release_trim;
};

// Charge work at dispatch and emission, independently of navigation's cap on
// the number of materialized steps. Export budgets a conservative upper bound
// using the same costs, before it publishes the resulting text.
struct PlaybackWork {
    static constexpr std::size_t limit  = 1'000'000;
    static constexpr std::size_t token  = 1;
    static constexpr std::size_t attack = 1;

    static constexpr std::size_t text(std::string_view value) { return value.size() / 64; }
    static constexpr std::size_t chord(std::size_t members, std::size_t ties) {
        return members + ties;
    }
    static constexpr std::size_t written_chord(std::size_t members) {
        // A written interval can attack all its members, or sustain as many
        // ties as were previously active, including notes now ending. Charge
        // each outgoing tie to the interval that created it.
        return chord(members, members) + members * attack;
    }
};

}   // namespace symusic::abc::detail

#pragma once

#include <random>
#include <string>

#include "settings.h"

namespace symusic::abc::detail {

using BeatLevels = std::array<i32, 3>;              // Downbeat, other accented beats, weak beats.
using Dynamic    = std::variant<BeatLevels, i32>;   // Absolute levels or a signed step.
inline constexpr BeatLevels kMezzoForte{90, 80, 65};
inline constexpr i32        kDynamicStep = 15;

// A hairpin's step is fixed where the mark is written, even when a different
// voice sets the step or navigation plays the mark more than once.
class WrittenDynamics {
public:
    bool                                 consume(const Token& token, Diagnostics& diagnostics);
    [[nodiscard]] std::optional<Dynamic> mark(std::string_view name, Location at) const;

private:
    WrittenSetting<i32> steps{kDynamicStep};
};

// Resolve rhythm instructions in source order, before splitting the tune into
// independently rendered voices and before expanding repeats or parts.
class WrittenRhythm {
public:
    bool                       consume(const Token& token, Diagnostics& diagnostics);
    void                       finish_header(Meter meter, Diagnostics& diagnostics);
    [[nodiscard]] AttackRhythm at_attack() const { return rhythm; }

private:
    AttackRhythm rhythm;
    Location     hornpipe_at;
};

bool starts_hornpipe_pair(const Meter& opening_meter, f64 offset, f64 duration);

enum class GateArticulation { Normal, Staccato, Unshaped };
enum class FermataMode { Proportional, Fixed };

struct GraceSettings {
    i32 divider = 4;
    f64 ratio   = 0;

    [[nodiscard]] bool proportional() const { return ratio > 0; }
    [[nodiscard]] f64  step(f64 host, f64 unit, f64 weights) const;
};

struct NoteLengthSettings {
    GraceSettings grace;
    FermataMode   fermata = FermataMode::Proportional;
};

// These commands shape written notes before navigation, across voices.
class WrittenNoteLengths {
public:
    void                             consume(const Token& token, Diagnostics& diagnostics);
    [[nodiscard]] NoteLengthSettings at(Location at) const { return lengths.at(at.written); }
    [[nodiscard]] static bool        handles(std::string_view name);

private:
    WrittenSetting<NoteLengthSettings> lengths{};
};

f64    fermata_length(f64 duration, f64 unit, FermataMode mode);
Length fermata_length(Length length, FermataMode mode);

// Snapshot the ordinary beat velocity at the same attack, after one-shot dynamics.
struct AttackVelocity {
    i8                value;
    std::optional<i8> without_stress{};
};

// A resolved policy for one chord: a fixed gap, or a draw in [0, ticks).
struct ChordSpread {
    i32  ticks  = 0;
    bool random = false;
};

// One sequence per tune. Fixed gaps do not consume the random sequence.
class ChordAttackSequence {
public:
    i32 next_gap(ChordSpread spread);

private:
    // Each rendering of the tune must reproduce the same chord gaps.
    std::minstd_rand random;
};

// Preserve the exact unit-relative ratio after tuplets. Harp mode also uses
// the unreduced numerator for its historical subdivision.
struct CutNotation {
    Length length;
    bool   harp;
};

struct HarpAttacks {
    f64 first;
    f64 last;
};

HarpAttacks harp_attacks(Length length);

// The active lane's dynamics, beat accents and articulation. Positions,
// durations and unit lengths are fractional ticks; stress gate ratios are
// dimensionless. An articulation gate is independent of cursor advance.
class Expression {
public:
    bool directive(
        std::string_view name,
        std::string_view args,
        f64              unit,
        Meter            meter,
        Location         at,
        Diagnostics&     diagnostics
    );
    void               apply(Dynamic mark);
    void               reset_beat_spacing() { beat_step = 0; }
    static ChordSpread take_chord_spread(ChordAttackSettings attack, bool marked = false);
    void               enable_stress() { stress_enabled = true; }
    // Resolve velocity at a position relative to the accent origin, consuming
    // one-shot volume settings. A stress velocity of zero keeps the beat profile.
    AttackVelocity    attack_velocity(f64 position, Meter meter, bool accented, i32 stress);
    [[nodiscard]] f64 gate(
        f64 duration, bool slur, GateArticulation style, std::optional<f64> stress
    ) const;
    [[nodiscard]] f64 tied_gate(f64 duration, GateArticulation style) const;

private:
    BeatLevels  beat = kMezzoForte;
    std::string beat_string;
    // An absolute dynamic uses every beat unless the meter has explicit groups.
    // Zero restores the meter's ordinary accents after a meter change.
    std::optional<i32> beat_step;
    bool               accents = true;
    std::optional<i8>  next_volume;
    i32                next_volume_delta = 0;
    std::optional<f64> trim_ticks;   // An explicit zero disables stress trim too.
    f64                expand_ticks   = 0;
    bool               stress_enabled = true;
    void              set_beat_string(std::string_view text, Location at, Diagnostics& diagnostics);
    [[nodiscard]] i32 beat_velocity(f64 position, Meter meter) const;
    [[nodiscard]] i32 string_velocity(f64 position, f64 measure) const;
};

// Subdivide the notated duration until each alternating attack fits the
// upstream 100 ms trill pulse, bounded by the tune's expansion limit.
i32 trill_attacks(
    f64 duration, f64 unit, i32 usec_per_quarter, Location at, Diagnostics& diagnostics
);

struct Ornament {
    bool staccato = false;
    bool tenuto   = false;
    bool accent   = false;
    bool fermata  = false;
    bool trill    = false;
    bool cut      = false;
    bool roll     = false;
    bool arpeggio = false;
};

// Resolve the harp setting at a cut's written position. Harp mode uses three
// attacks of the host pitch, whose total can differ from the written length.
class HarpCuts {
public:
    bool                                     consume(const Token& token, Diagnostics& diagnostics);
    [[nodiscard]] std::optional<CutNotation> for_atom(
        const Atom& atom, Ornament ornament, Location written, Length scale
    ) const;

private:
    WrittenSetting<bool> harp{false};
};

}   // namespace symusic::abc::detail

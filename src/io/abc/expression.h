#pragma once

#include <random>

#include "settings.h"

namespace symusic::abc::detail {

using Dynamic = std::variant<std::array<int, 3>, int>;   // Absolute beat levels or a signed step.

// A hairpin's step is fixed where the mark is written, even when a different
// voice sets the step or navigation plays the mark more than once.
class WrittenDynamics {
public:
    bool                                 consume(const Token& token, Diagnostics& diagnostics);
    [[nodiscard]] std::optional<Dynamic> mark(std::string_view name, Location at) const;

private:
    vec<std::pair<Location, int>> steps;
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

bool starts_hornpipe_pair(const Meter& opening_meter, long double offset, long double duration);

enum class GateArticulation { normal, staccato, unshaped };
enum class FermataMode { proportional, fixed };

struct GraceSettings {
    int         divider = 4;
    long double ratio   = 0;

    [[nodiscard]] bool        proportional() const { return ratio > 0; }
    [[nodiscard]] long double step(long double host, long double unit, long double weights) const;
};

struct NoteLengthSettings {
    GraceSettings grace;
    FermataMode   fermata = FermataMode::proportional;
};

// These commands shape written notes before navigation, across voices.
class WrittenNoteLengths {
public:
    void                             consume(const Token& token, Diagnostics& diagnostics);
    [[nodiscard]] NoteLengthSettings at(Location written) const;
    [[nodiscard]] static bool        handles(std::string_view name);

private:
    vec<std::pair<Location, NoteLengthSettings>> changes;
};

long double fermata_length(long double duration, long double unit, FermataMode mode);
Length      fermata_length(Length length, FermataMode mode);

// Snapshot the ordinary beat velocity at the same attack, after one-shot dynamics.
struct AttackVelocity {
    i8                value;
    std::optional<i8> without_stress{};
};

// A resolved policy for one chord: a fixed gap, or a draw in [0, ticks).
struct ChordSpread {
    int  ticks  = 0;
    bool random = false;
};

// One sequence per tune. Fixed gaps do not consume the random sequence.
class ChordAttackSequence {
public:
    int next_gap(ChordSpread spread);

private:
    std::minstd_rand random;
};

// Preserve the exact unit-relative ratio after tuplets. Harp mode also uses
// the unreduced numerator for its historical subdivision.
struct CutNotation {
    Length length;
    bool   harp;
};

struct HarpAttacks {
    long double first;
    long double last;
};

HarpAttacks harp_attacks(Length length);

// The active lane's dynamics, beat accents and articulation. Positions,
// durations and unit lengths are fractional ticks; stress gate ratios are
// dimensionless. An articulation gate is independent of cursor advance.
struct Expression {
    Expression();

    std::array<int, 3> beat{105, 95, 80};
    std::string_view   beat_string;
    bool               accents = true;

    bool directive(
        std::string_view name,
        std::string_view args,
        long double      unit,
        Meter            meter,
        Location         at,
        Diagnostics&     diagnostics
    );
    void        apply(Dynamic mark);
    void        reset_beat_spacing() { beat_step = 0; }
    ChordSpread take_chord_spread(ChordAttackSettings attack, bool marked = false);
    void        enable_stress() { stress_enabled = true; }
    // Resolve velocity at a position relative to the accent origin, consuming
    // one-shot volume settings. A stress velocity of zero keeps the beat profile.
    AttackVelocity attack_velocity(long double position, Meter meter, bool accented, int stress);
    long double    gate(
        long double duration, bool slur, GateArticulation style, std::optional<long double> stress
    ) const;
    long double tied_gate(long double duration, GateArticulation style) const;

private:
    static constexpr int dynamic_beat_step = -1;
    int                  beat_step         = 0;
    bool                 note_volume(
        std::string_view name, std::string_view args, Location at, Diagnostics& diagnostics
    );
    std::optional<int>         next_volume;
    int                        next_volume_delta = 0;
    std::optional<long double> trim_ticks;   // An explicit zero disables stress trim too.
    long double                expand_ticks    = 0;
    bool                       stress_enabled  = true;
    void                       shift(int amount);
    void set_beat_string(std::string_view text, Location at, Diagnostics& diagnostics);
    int  beat_velocity(long double position, Meter meter) const;
    int  string_velocity(long double position, long double pulse) const;
    bool timing(
        std::string_view name,
        std::string_view args,
        long double      unit,
        Location         at,
        Diagnostics&     diagnostics
    );
};

// Subdivide the notated duration until each alternating attack fits the
// upstream 100 ms trill pulse, bounded by the tune's expansion limit.
int trill_attacks(
    long double  duration,
    long double  unit,
    i32          usec_per_quarter,
    Location     at,
    Diagnostics& diagnostics
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
    vec<std::pair<Location, bool>> changes;
};


}   // namespace symusic::abc::detail

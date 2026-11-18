#pragma once

#include "settings.h"

namespace symusic::abc::detail {

enum class LoudnessShift { louder, softer };
using Dynamic = std::variant<std::array<int, 3>, LoudnessShift>;
std::optional<Dynamic> dynamic_mark(std::string_view name);

// MIDI expression is evaluated on a voice's performed clock. It never changes
// pitch or meter, and its articulation changes the gate, not the next onset.
struct Expression {
    std::array<int, 3> beat{105, 95, 80};
    std::string_view   beat_string;
    int                beat_step     = 0;
    int                delta         = 15;
    int                grace_divider = 4;
    bool               accents       = true;
    bool               fixed_fermata = false;
    long double        broken_short  = 0.5;
    long double        grace_ratio   = 0;
    long double        trim          = 0;
    long double        expand        = 0;

    bool directive(
        std::string_view name, std::string_view args, Location at, Diagnostics& diagnostics
    );
    void        apply(Dynamic mark);
    void        arpeggiate_next_chord() { arpeggio = true; }
    int         take_chord_spread();
    long double broken_factor(Broken mark) const;
    long double fermata(long double duration, long double unit) const;
    int         velocity(long double position, Meter meter) const;
    long double gate(long double duration, long double unit, bool slur, bool staccato) const;

private:
    int  chord_attack = 10;
    bool arpeggio     = false;
    void shift(int amount);
    void set_beat_string(std::string_view text, Location at, Diagnostics& diagnostics);
    int  string_velocity(long double position, long double pulse) const;
    bool timing(
        std::string_view name, std::string_view args, Location at, Diagnostics& diagnostics
    );
    bool articulation(
        std::string_view name, std::string_view args, Location at, Diagnostics& diagnostics
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
    bool fermata  = false;
    bool trill    = false;
    bool cut      = false;
    bool roll     = false;
};

}   // namespace symusic::abc::detail

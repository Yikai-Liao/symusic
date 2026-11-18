#pragma once

#include "settings.h"

namespace symusic::abc::detail {

int midi_integer(std::string_view& args, int low, int high, Diagnostics& diagnostics, Location at);

enum class LoudnessShift { louder, softer };
using Dynamic = std::variant<std::array<int, 3>, LoudnessShift>;
std::optional<Dynamic> dynamic_mark(std::string_view name);

// MIDI expression is evaluated on a voice's performed clock. It never changes
// pitch or meter, and its articulation changes the gate, not the next onset.
struct Expression {
    std::array<int, 3> beat{105, 95, 80};
    std::string_view   beat_string;
    int                beat_step     = 0;
    int                chord_attack  = 10;
    int                delta         = 15;
    int                grace_divider = 4;
    bool               accents       = true;
    long double        grace_ratio   = 0;
    long double        trim          = 0;
    long double        expand        = 0;

    bool directive(
        std::string_view name, std::string_view args, Location at, Diagnostics& diagnostics
    );
    void        apply(Dynamic mark);
    int         velocity(long double position, Meter meter) const;
    long double gate(long double duration, long double unit, bool slur, bool staccato) const;

private:
    void shift(int amount);
    int  string_velocity(long double position, long double pulse) const;
    bool articulation(
        std::string_view name, std::string_view args, Location at, Diagnostics& diagnostics
    );
};

struct Ornament {
    bool staccato = false;
    bool fermata  = false;
    bool trill    = false;
    bool roll     = false;
};

}   // namespace symusic::abc::detail

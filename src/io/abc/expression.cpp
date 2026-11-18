#include "expression.h"

#include <cmath>

namespace symusic::abc::detail {

int midi_integer(std::string_view& args, int low, int high, Diagnostics& diagnostics, Location at) {
    const auto number = integer(word(args));
    if (number && *number >= low && *number <= high) return *number;
    diagnostics.error(
        at, "invalid-midi-value", fmt::format("expected a MIDI value between {} and {}", low, high)
    );
}

namespace {

using namespace std::literals;
struct Mark {
    std::string_view   name;
    std::array<int, 3> beat;
};
constexpr std::array marks{
    Mark{"ppp", {30, 20, 10}},
    Mark{"pp", {45, 35, 20}},
    Mark{"p", {60, 50, 35}},
    Mark{"mp", {75, 65, 50}},
    Mark{"mf", {90, 80, 65}},
    Mark{"f", {105, 95, 80}},
    Mark{"ff", {120, 110, 95}},
    Mark{"fff", {127, 125, 110}},
};

bool aligned(long double position, long double every) {
    return std::fabs(std::remainder(position, every)) < 0.0000001L;
}

}   // namespace

void Expression::shift(int amount) {
    for (auto& level : beat) level = std::clamp(level + amount, 0, midi_max);
}

std::optional<Dynamic> dynamic_mark(std::string_view name) {
    const auto mark = std::ranges::find(marks, name, &Mark::name);
    if (mark != marks.end()) return mark->beat;
    constexpr std::array louder{"crescendo("sv, "crescendo)"sv, "<("sv, "<)"sv};
    constexpr std::array softer{"diminuendo("sv, "diminuendo)"sv, ">("sv, ">)"sv};
    if (std::ranges::find(louder, name) != louder.end()) return LoudnessShift::louder;
    if (std::ranges::find(softer, name) != softer.end()) return LoudnessShift::softer;
    return {};
}

void Expression::apply(Dynamic mark) {
    if (const auto* levels = std::get_if<std::array<int, 3>>(&mark)) {
        beat      = *levels;
        beat_step = 1;
    } else {
        shift(std::get<LoudnessShift>(mark) == LoudnessShift::louder ? delta : -delta);
    }
}

int Expression::string_velocity(long double position, long double pulse) const {
    if (!aligned(position, pulse)) return beat[2];
    const auto index = static_cast<size_t>(std::floor(position / pulse + 0.0000001L));
    const char mark  = beat_string[index % beat_string.size()];
    return beat[mark == 'f' ? 0 : (mark == 'm' ? 1 : 2)];
}

int Expression::velocity(long double position, Meter meter) const {
    if (!accents) return beat[1];
    if (position < 0.0000001L) return beat[0];
    if (!beat_string.empty()) return string_velocity(position, meter.beat());
    long double period = meter.beat() * (beat_step == 0 ? meter.accent_beats() : beat_step);
    if (meter.denominator == 2 && beat_step == 0) period *= 2;
    return beat[aligned(position, period) ? 1 : 2];
}

long double Expression::gate(
    long double duration, long double unit, bool slur, bool staccato
) const {
    if (staccato) return duration / 2;
    if (slur) return duration;
    const auto gap = unit * trim;
    if (gap > 0) return gap >= duration ? duration / 2 : duration - gap;
    return duration + unit * expand;
}

bool Expression::directive(
    std::string_view name, std::string_view args, Location at, Diagnostics& diagnostics
) {
    const auto value
        = [&](int min, int max) { return midi_integer(args, min, max, diagnostics, at); };
    if (name == "beat") {
        for (auto& level : beat) level = value(0, midi_max);
        beat_step = value(1, 128);
    } else if (name == "beatmod")
        shift(value(-midi_max, midi_max));
    else if (name == "beatstring") {
        beat_string = word(args);
        if (beat_string.empty() || beat_string.find_first_not_of("fmp") != beat_string.npos) {
            diagnostics.error(at, "invalid-beatstring", "beatstring must consist of f, m and p");
        }
    } else if (name == "deltaloudness")
        delta = value(0, midi_max);
    else if (name == "nobeataccents")
        accents = false;
    else if (name == "beataccents")
        accents = true;
    else
        return articulation(name, args, at, diagnostics);
    return true;
}

bool Expression::articulation(
    std::string_view name, std::string_view args, Location at, Diagnostics& diagnostics
) {
    const auto value
        = [&](int min, int max) { return midi_integer(args, min, max, diagnostics, at); };
    if (name == "chordattack")
        chord_attack = value(0, tpq);
    else if (name == "grace")
        grace_ratio = ratio(args, diagnostics, at, "grace").fraction();
    else if (name == "gracedivider") {
        grace_divider = value(1, 1024);
        grace_ratio   = 0;
    } else if (name == "trim")
        trim = ratio(args, diagnostics, at, "trim").fraction();
    else if (name == "expand")
        expand = ratio(args, diagnostics, at, "expand").fraction();
    else
        return false;
    return true;
}

}   // namespace symusic::abc::detail

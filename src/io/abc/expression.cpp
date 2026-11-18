#include "expression.h"

#include <cmath>
#include <limits>

namespace symusic::abc::detail {

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

constexpr int max_trill_attacks = static_cast<int>(PlaybackWork::limit);

[[noreturn]] void trill_limit(Location at, Diagnostics& diagnostics) {
    diagnostics.error(at, "expansion-limit", "trill exceeds the playback work limit");
}

int notated_trill_attacks(long double units, Location at, Diagnostics& diagnostics) {
    // Rhythm transforms leave a rational number of reference units, e.g.
    // 2/3 for a triplet. Use the reduced numerator, not a rounded tick count.
    const auto tolerance = 8 * std::numeric_limits<long double>::epsilon() * std::max(1.L, units);
    if (units <= tolerance) return 1;
    long double previous_n = 0, n = 1;
    long double previous_d = 1, d = 0;
    auto        remainder = units;
    for (;;) {
        const auto term   = std::floor(remainder);
        const auto next_n = term * n + previous_n;
        const auto next_d = term * d + previous_d;
        if (next_n > max_trill_attacks) trill_limit(at, diagnostics);
        const auto fraction = remainder - term;
        if (next_n >= 1 && (fraction == 0 || std::abs(next_n / next_d - units) <= tolerance))
            return static_cast<int>(next_n);
        previous_n = std::exchange(n, next_n);
        previous_d = std::exchange(d, next_d);
        remainder  = 1 / fraction;
    }
}

bool aligned(long double position, long double every) {
    return std::fabs(std::remainder(position, every)) < 0.0000001L;
}

}   // namespace

int trill_attacks(
    long double  duration,
    long double  unit,
    i32          usec_per_quarter,
    Location     at,
    Diagnostics& diagnostics
) {
    if (duration == 0) return 0;
    auto count = notated_trill_attacks(duration / unit, at, diagnostics);
    while (std::floor(duration * usec_per_quarter / (count * static_cast<long double>(tpq)))
           > 100'000) {
        if (count > max_trill_attacks / 2) trill_limit(at, diagnostics);
        count *= 2;
    }
    return count;
}

RhythmStyle rhythm_style(std::string_view name) {
    return name.starts_with("hornpipe") || name.starts_with("Hornpipe") ? RhythmStyle::hornpipe :
                                                                          RhythmStyle::straight;
}

bool hornpipe_meter(Meter meter) {
    return !meter.free && meter.denominator == 4 && (meter.numerator == 2 || meter.numerator == 4);
}

Expression::Expression(RhythmStyle rhythm) :
    rhythm(rhythm), broken_short(rhythm == RhythmStyle::hornpipe ? 2.L / 3 : 0.5L) {}

bool Expression::starts_swing_pair(Meter meter, long double offset, long double duration) const {
    if (rhythm != RhythmStyle::hornpipe || !hornpipe_meter(meter)) return false;
    const auto short_note = static_cast<long double>(whole) * meter.numerator / 32;
    return std::fabs(duration - short_note) < 0.0000001L && aligned(offset, 2 * short_note);
}

int Expression::take_chord_spread() {
    return std::exchange(arpeggio, false) ? 3 * chord_attack : chord_attack;
}

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
    const char mark  = upper(beat_string[index % beat_string.size()]);
    return beat[mark == 'F' ? 0 : (mark == 'M' ? 1 : 2)];
}

int Expression::velocity(long double position, Meter meter) const {
    if (!accents) return beat[1];
    if (!beat_string.empty())
        return string_velocity(position, meter.measure() / beat_string.size());
    if (position < 0.0000001L) return beat[0];
    const auto period = meter.beat() * (beat_step == 0 ? meter.accent_beats() : beat_step);
    return beat[aligned(position, period) ? 1 : 2];
}

long double Expression::gate(
    long double duration, long double unit, bool slur, bool staccato
) const {
    const auto sounding = staccato ? duration / 2 : duration;
    const auto gap      = unit * trim;
    if (!slur && gap > 0 && gap < sounding) return sounding - gap;
    return tied_gate(sounding, unit);
}

long double Expression::tied_gate(long double duration, long double unit) const {
    // Ties sustain through trim. Native export still relies on expand to
    // compensate for the one-tick release trim, including across ties.
    return duration + unit * expand;
}

void Expression::set_beat_string(std::string_view text, Location at, Diagnostics& diagnostics) {
    if (text.empty() || text.find_first_not_of("fFmMpP") != text.npos)
        diagnostics.error(at, "invalid-beatstring", "beatstring must consist of f, m and p");
    beat_string = text;
}

long double Expression::broken_factor(Broken mark) const {
    const auto short_factor = mark.count == 1 ? broken_short : std::ldexp(1.L, -mark.count);
    return mark.direction > 0 ? 2 - short_factor : short_factor;
}

long double Expression::fermata(long double duration, long double unit) const {
    return duration + (fixed_fermata ? unit : duration);
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
    else if (name == "beatstring")
        set_beat_string(word(args), at, diagnostics);
    else if (name == "deltaloudness")
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
    } else
        return timing(name, args, at, diagnostics);
    return true;
}

bool Expression::timing(
    std::string_view name, std::string_view args, Location at, Diagnostics& diagnostics
) {
    if (name == "trim")
        trim = ratio(args, diagnostics, at, "trim").fraction();
    else if (name == "expand") {
        expand = ratio(args, diagnostics, at, "expand").fraction();
        if (expand > 0) trim = 0;
    } else if (name == "fermatafixed" || name == "fermataproportional")
        fixed_fermata = name == "fermatafixed";
    else if (name == "ratio") {
        const auto longer  = midi_integer(args, 1, 100'000, diagnostics, at);
        const auto shorter = midi_integer(args, 1, 100'000, diagnostics, at);
        broken_short       = 2.L * shorter / (longer + shorter);
    } else
        return false;
    return true;
}

}   // namespace symusic::abc::detail

#include "expression.h"

#include <cmath>
#include <limits>
#include <numeric>

namespace symusic::abc::detail {

namespace {

using namespace std::literals;
struct Mark {
    std::string_view   name;
    std::array<int, 3> beat;
};
constexpr std::array mezzo_forte{90, 80, 65};
constexpr std::array marks{
    Mark{"ppp", {30, 20, 10}},
    Mark{"pp", {45, 35, 20}},
    Mark{"p", {60, 50, 35}},
    Mark{"mp", {75, 65, 50}},
    Mark{"mf", mezzo_forte},
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

static int hornpipe_divisor(int opening_numerator) {
    if (opening_numerator == 4) return 8;
    if (opening_numerator == 2) return 16;
    return 0;
}

bool WrittenRhythm::consume(const Token& token, Diagnostics& diagnostics) {
    if (const auto* field = std::get_if<Field>(&token.value); field && field->kind == 'R') {
        // ABC 2.1 3.1.15 leaves playback open. abc2midi enables hornpipe
        // timing until the end of the tune; other rhythm names are labels.
        if (field->value.starts_with("hornpipe") || field->value.starts_with("Hornpipe")) {
            rhythm      = {.longer = 2, .shorter = 1, .swing = true};
            hornpipe_at = token.at;
        }
        return true;
    }
    const auto command = token.information();
    if (!command) return false;
    auto args = *command;
    if (word(args) != "MIDI" || word(args) != "ratio") return false;
    rhythm.longer  = midi_integer(args, 1, 100'000, diagnostics, token.at);
    rhythm.shorter = midi_integer(args, 1, 100'000, diagnostics, token.at);
    return true;
}

void WrittenRhythm::finish_header(Meter meter, Diagnostics& diagnostics) {
    if (!rhythm.swing || (meter.denominator == 4 && hornpipe_divisor(meter.numerator))) return;
    diagnostics.warn(
        hornpipe_at,
        "hornpipe-meter",
        "disabled automatic hornpipe swing: the opening meter must be 2/4 or 4/4"
    );
    rhythm.swing = false;
}

bool starts_hornpipe_pair(const Meter& opening_meter, long double offset, long double duration) {
    const auto divisor = hornpipe_divisor(opening_meter.numerator);
    if (!divisor) return false;
    const auto short_note = static_cast<long double>(whole) / divisor;
    // abc2midi checks the reduced denominator at the right note, in opening
    // meter beats. In cut time a beat is twice as long as a quarter note.
    const auto bar_unit = short_note * 4 / opening_meter.denominator;
    return std::fabs(duration - short_note) < 0.0000001L
           && aligned(offset + short_note - bar_unit, 2 * bar_unit);
}

Expression::Expression() { apply(mezzo_forte); }

int ChordAttackSequence::next_gap(ChordSpread spread) {
    if (!spread.random) return spread.ticks;
    // Scale one portable draw per gap, regardless of the requested range.
    constexpr u64 sample_count = std::minstd_rand::max() - std::minstd_rand::min() + 1;
    const u64     sample       = random() - std::minstd_rand::min();
    return static_cast<int>(sample * spread.ticks / sample_count);
}

ChordSpread Expression::take_chord_spread(ChordAttackSettings attack, bool marked) {
    const auto factor = marked ? 3 : 1;
    return attack.random_limit > 0 ? ChordSpread{factor * attack.random_limit, true} :
                                     ChordSpread{factor * attack.gap, false};
}

void Expression::shift(int amount) {
    for (auto& level : beat) level = std::clamp(level + amount, 0, midi_max);
}

bool WrittenDynamics::consume(const Token& token, Diagnostics& diagnostics) {
    auto args = token.information().value_or("");
    if (word(args) != "MIDI" || word(args) != "deltaloudness") return false;
    steps.emplace_back(token.at, midi_integer(args, 0, midi_max, diagnostics, token.at));
    return true;
}

std::optional<Dynamic> WrittenDynamics::mark(std::string_view name, Location at) const {
    if (name == "pppp" || name == "ffff") name.remove_suffix(1);
    const auto level = std::ranges::find(marks, name, &Mark::name);
    if (level != marks.end()) return level->beat;
    constexpr std::array louder{"crescendo("sv, "crescendo)"sv, "<("sv, "<)"sv};
    constexpr std::array softer{"diminuendo("sv, "diminuendo)"sv, ">("sv, ">)"sv};
    const bool           increase = std::ranges::find(louder, name) != louder.end();
    if (!increase && std::ranges::find(softer, name) == softer.end()) return {};
    const auto next = std::ranges::upper_bound(steps, at, {}, &decltype(steps)::value_type::first);
    const int  step = next == steps.begin() ? 15 : std::prev(next)->second;
    return increase ? step : -step;
}

void Expression::apply(Dynamic mark) {
    if (const auto* levels = std::get_if<std::array<int, 3>>(&mark)) {
        beat      = *levels;
        beat_step = dynamic_beat_step;
    } else {
        shift(std::get<int>(mark));
    }
}

int Expression::string_velocity(long double position, long double pulse) const {
    if (!aligned(position, pulse)) return beat[2];
    const auto index = static_cast<size_t>(std::floor(position / pulse + 0.0000001L));
    const char mark  = upper(beat_string[index % beat_string.size()]);
    return beat[mark == 'F' ? 0 : (mark == 'M' ? 1 : 2)];
}

AttackVelocity Expression::attack_velocity(
    long double position, Meter meter, bool accented, int stress
) {
    if (next_volume) return {static_cast<i8>(*std::exchange(next_volume, {}))};
    const auto change      = std::exchange(next_volume_delta, 0);
    const auto apply_marks = [&](int base) {
        const auto volume = std::clamp(base + change, 0, midi_max);
        // Use one dynamic step without unmuting a silent attack.
        return static_cast<i8>(accented && volume > 0 ? std::min(volume + 15, midi_max) : volume);
    };
    const auto normal = apply_marks(beat_velocity(position, meter));
    if (!stress_enabled || stress == 0) return {normal};
    const auto stressed = apply_marks(stress);
    return {stressed, stressed == normal ? std::nullopt : std::optional{normal}};
}

int Expression::beat_velocity(long double position, Meter meter) const {
    if (!accents) return beat[1];
    if (!beat_string.empty())
        return string_velocity(position, meter.measure() / beat_string.size());
    if (position < 0.0000001L) return beat[0];
    // A dynamic changes levels but does not replace explicitly written grouping.
    const auto step
        = beat_step == dynamic_beat_step ? (meter.group_starts.none() ? 1 : 0) : beat_step;
    const auto accent
        = step ? aligned(position, meter.beat() * step) : meter.default_accent(position);
    return beat[accent ? 1 : 2];
}

long double Expression::gate(
    long double duration, bool slur, GateArticulation style, std::optional<long double> stress
) const {
    if (style == GateArticulation::unshaped) return duration;
    if (!slur) {
        const auto trim = trim_ticks.value_or(0);
        if (stress_enabled && stress && (!trim_ticks || trim > 0)) return duration * *stress;
        if (trim > 0) return trim < duration ? duration - trim : duration;
    }
    return duration + expand_ticks;
}

long double Expression::tied_gate(long double duration, GateArticulation style) const {
    // Export uses expand to compensate for the one-tick release trim on a
    // native round trip.
    if (style == GateArticulation::unshaped) return duration;
    return duration + (trim_ticks.value_or(0) == 0 ? expand_ticks : 0);
}

long double GraceSettings::step(long double host, long double unit, long double weights) const {
    return proportional() ? host * ratio / weights : unit / divider;
}

void Expression::set_beat_string(std::string_view text, Location at, Diagnostics& diagnostics) {
    if (text.empty() || text.find_first_not_of("fFmMpP") != text.npos)
        diagnostics.error(at, "invalid-beatstring", "beatstring must consist of f, m and p");
    beat_string = text;
}

long double fermata_length(long double duration, long double unit, FermataMode mode) {
    return duration + (mode == FermataMode::fixed ? unit : duration);
}

static std::optional<FermataMode> read_fermata_mode(std::string_view name) {
    if (name == "fermatafixed") return FermataMode::fixed;
    if (name == "fermataproportional") return FermataMode::proportional;
    return {};
}

bool WrittenNoteLengths::handles(std::string_view name) {
    return name == "grace" || name == "gracedivider" || read_fermata_mode(name).has_value();
}

void WrittenNoteLengths::consume(const Token& token, Diagnostics& diagnostics) {
    auto args = token.information().value_or("");
    if (word(args) != "MIDI") return;
    const auto name = word(args);
    if (!handles(name)) return;
    auto next = changes.empty() ? NoteLengthSettings{} : changes.back().second;
    if (name == "grace")
        next.grace.ratio = ratio(args, diagnostics, token.at, "grace").fraction();
    else if (name == "gracedivider")
        next.grace = {midi_integer(args, 1, 1024, diagnostics, token.at), 0};
    else
        next.fermata = *read_fermata_mode(name);
    changes.emplace_back(token.at, next);
}

NoteLengthSettings WrittenNoteLengths::at(Location written) const {
    const auto next
        = std::ranges::upper_bound(changes, written, {}, &decltype(changes)::value_type::first);
    return next == changes.begin() ? NoteLengthSettings{} : std::prev(next)->second;
}

bool Expression::note_volume(
    std::string_view name, std::string_view args, Location at, Diagnostics& diagnostics
) {
    if (name != "vol" && name != "volinc") return false;
    if (name == "vol")
        next_volume = midi_integer(args, 0, midi_max, diagnostics, at);
    else
        next_volume_delta = midi_integer(args, -midi_max, midi_max, diagnostics, at);
    return true;
}

bool Expression::directive(
    std::string_view name,
    std::string_view args,
    long double      unit,
    Meter            meter,
    Location         at,
    Diagnostics&     diagnostics
) {
    const auto value
        = [&](int min, int max) { return midi_integer(args, min, max, diagnostics, at); };
    if (name == "beat") {
        for (auto& level : beat) level = value(0, midi_max);
        const int spacing = value(0, std::numeric_limits<int>::max());
        // abc2midi takes zero to mean the current meter's numerator.
        beat_step = spacing == 0 ? meter.numerator : spacing;
    } else if (name == "beatmod")
        shift(value(-midi_max, midi_max));
    else if (name == "beatstring")
        set_beat_string(word(args), at, diagnostics);
    else if (name == "nobeataccents")
        accents = false;
    else if (name == "beataccents") {
        accents        = true;
        stress_enabled = false;
    } else
        return note_volume(name, args, at, diagnostics)
               || timing(name, args, unit, at, diagnostics);
    return true;
}

bool Expression::timing(
    std::string_view name,
    std::string_view args,
    long double      unit,
    Location         at,
    Diagnostics&     diagnostics
) {
    if (name == "trim")
        trim_ticks = unit * ratio(args, diagnostics, at, "trim").fraction();
    else if (name == "expand") {
        expand_ticks = unit * ratio(args, diagnostics, at, "expand").fraction();
        if (expand_ticks > 0) trim_ticks = 0;
    } else
        return false;
    return true;
}

bool HarpCuts::consume(const Token& token, Diagnostics& diagnostics) {
    auto args = token.information().value_or("");
    if (word(args) != "MIDI" || word(args) != "harpmode") return false;
    const auto value = integer(word(args));
    if (!value || *value < 0 || !trim(args).empty()) {
        diagnostics.warn(
            token.at, "invalid-harpmode", "expected a nonnegative integer for MIDI harpmode"
        );
        return true;
    }
    changes.emplace_back(token.at, *value != 0);
    return true;
}

Length fermata_length(Length length, FermataMode mode) {
    if (mode == FermataMode::proportional) return {2 * length.numerator, length.denominator};
    const auto numerator = length.numerator + length.denominator;
    const auto divisor   = std::gcd(numerator, length.denominator);
    return {numerator / divisor, length.denominator / divisor};
}

HarpAttacks harp_attacks(Length length) {
    const auto [numerator, denominator] = length;
    const auto unit                     = static_cast<long double>(denominator);
    if (numerator == 3 * denominator)
        return {(numerator - 1) / (4 * unit), (numerator / 2 + 1) / unit};
    const auto duration = (numerator == 0 ? 1.L : numerator) / unit;
    return {duration / 4, duration / 2};
}

std::optional<CutNotation> HarpCuts::for_atom(
    const Atom& atom, Ornament ornament, Location written, Length scale
) const {
    if (!ornament.cut || ornament.trill || atom.chord || atom.pitches.empty()) return {};
    const auto next
        = std::ranges::upper_bound(changes, written, {}, &decltype(changes)::value_type::first);
    return CutNotation{
        {atom.length.numerator * scale.numerator, atom.length.denominator * scale.denominator},
        next != changes.begin() && std::prev(next)->second
    };
}

}   // namespace symusic::abc::detail

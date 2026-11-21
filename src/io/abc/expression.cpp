#include "expression.h"

#include <cmath>
#include <limits>
#include <numeric>

namespace symusic::abc::detail {

namespace {

struct Mark {
    std::string_view name;
    BeatLevels       beat;
};
constexpr std::array kMarks{
    Mark{"ppp", {30, 20, 10}},
    Mark{"pp", {45, 35, 20}},
    Mark{"p", {60, 50, 35}},
    Mark{"mp", {75, 65, 50}},
    Mark{"mf", kMezzoForte},
    Mark{"f", {105, 95, 80}},
    Mark{"ff", {120, 110, 95}},
    Mark{"fff", {127, 125, 110}},
};

constexpr i32 kMaxTrillAttacks = static_cast<i32>(PlaybackWork::kLimit);

[[noreturn]] void trill_limit(Location at, Diagnostics& diagnostics) {
    diagnostics.error(at, "expansion-limit", "trill exceeds the playback work limit");
}

i32 notated_trill_attacks(f64 units, Location at, Diagnostics& diagnostics) {
    // Rhythm transforms leave a rational number of reference units, e.g.
    // 2/3 for a triplet. Use the reduced numerator, not a rounded tick count.
    const auto tolerance = 8 * std::numeric_limits<f64>::epsilon() * std::max(1.0, units);
    if (units <= tolerance) return 1;
    f64  previous_n = 0, n = 1;
    f64  previous_d = 1, d = 0;
    auto remainder = units;
    for (;;) {
        const auto term   = std::floor(remainder);
        const auto next_n = term * n + previous_n;
        const auto next_d = term * d + previous_d;
        if (next_n > kMaxTrillAttacks) trill_limit(at, diagnostics);
        const auto fraction = remainder - term;
        if (next_n >= 1 && (fraction == 0 || std::abs(next_n / next_d - units) <= tolerance))
            return static_cast<i32>(next_n);
        previous_n = std::exchange(n, next_n);
        previous_d = std::exchange(d, next_d);
        remainder  = 1 / fraction;
    }
}

bool aligned(f64 position, f64 every) {
    return std::abs(std::remainder(position, every)) < kTickTolerance;
}

}   // namespace

i32 trill_attacks(
    f64 duration, f64 unit, i32 usec_per_quarter, Location at, Diagnostics& diagnostics
) {
    if (duration == 0) return 0;
    auto count = notated_trill_attacks(duration / unit, at, diagnostics);
    while (std::floor(duration * usec_per_quarter / (count * static_cast<f64>(kTpq))) > 100'000) {
        if (count > kMaxTrillAttacks / 2) trill_limit(at, diagnostics);
        count *= 2;
    }
    return count;
}

static i32 hornpipe_divisor(i32 opening_numerator) {
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
    Instruction command(token);
    if (command.midi != "ratio") return false;
    rhythm.longer  = midi_integer(command.args, 1, 100'000, diagnostics, token.at);
    rhythm.shorter = midi_integer(command.args, 1, 100'000, diagnostics, token.at);
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

bool starts_hornpipe_pair(const Meter& opening_meter, f64 offset, f64 duration) {
    const auto divisor = hornpipe_divisor(opening_meter.numerator);
    if (!divisor) return false;
    const auto short_note = static_cast<f64>(kWhole) / divisor;
    // abc2midi checks the reduced denominator at the right note, in opening
    // meter beats. In cut time a beat is twice as long as a quarter note.
    const auto bar_unit = short_note * 4 / opening_meter.denominator;
    return std::abs(duration - short_note) < kTickTolerance
           && aligned(offset + short_note - bar_unit, 2 * bar_unit);
}

i32 ChordAttackSequence::next_gap(ChordSpread spread) {
    if (!spread.random) return spread.ticks;
    // One 31-bit draw per random gap. Even a marked gap (3 * tpq) fits the u64 product.
    constexpr u64 kSampleCount = std::minstd_rand::max() - std::minstd_rand::min() + 1;
    const u64     sample       = random() - std::minstd_rand::min();
    return static_cast<i32>(sample * spread.ticks / kSampleCount);
}

ChordSpread Expression::take_chord_spread(ChordAttackSettings attack, bool marked) {
    const auto factor = marked ? 3 : 1;
    return attack.random_limit > 0 ? ChordSpread{factor * attack.random_limit, true} :
                                     ChordSpread{factor * attack.gap, false};
}

bool WrittenDynamics::consume(const Token& token, Diagnostics& diagnostics) {
    Instruction command(token);
    if (command.midi != "deltaloudness") return false;
    steps.record(token.at.written, midi_integer(command.args, 0, kMidiMax, diagnostics, token.at));
    return true;
}

std::optional<Dynamic> WrittenDynamics::mark(std::string_view name, Location at) const {
    if (name == "pppp" || name == "ffff") name.remove_suffix(1);
    const auto level = std::ranges::find(kMarks, name, &Mark::name);
    if (level != kMarks.end()) return level->beat;
    if (!name.ends_with('(') && !name.ends_with(')')) return {};
    name.remove_suffix(1);
    if (name == "<" || name == "crescendo") return steps.at(at.written);
    if (name == ">" || name == "diminuendo") return -steps.at(at.written);
    return {};
}

void Expression::apply(Dynamic mark) {
    if (const auto* levels = std::get_if<BeatLevels>(&mark)) {
        beat = *levels;
        beat_step.reset();
    } else {
        const i32 step = std::get<i32>(mark);
        for (auto& level : beat) level = std::clamp(level + step, 0, kMidiMax);
    }
}

i32 Expression::string_velocity(f64 position, f64 measure) const {
    const auto count = static_cast<f64>(beat_string.size());
    const auto pulse = measure / count;
    if (!aligned(position, pulse)) return beat[2];
    // Reduce the pulse count before narrowing it to a string index.
    auto index = std::fmod(std::floor(position / pulse + kTickTolerance), count);
    if (index < 0) index += count;
    const char mark = upper(beat_string[static_cast<size_t>(index)]);
    return beat[mark == 'F' ? 0 : (mark == 'M' ? 1 : 2)];
}

AttackVelocity Expression::attack_velocity(f64 position, Meter meter, bool accented, i32 stress) {
    if (const auto volume = std::exchange(next_volume, std::nullopt)) return {*volume};
    const auto change      = std::exchange(next_volume_delta, 0);
    const auto apply_marks = [&](i32 base) {
        const auto volume = std::clamp(base + change, 0, kMidiMax);
        // Use one dynamic step without unmuting a silent attack.
        return static_cast<i8>(
            accented && volume > 0 ? std::min(volume + kDynamicStep, kMidiMax) : volume
        );
    };
    const auto normal = apply_marks(beat_velocity(position, meter));
    if (!stress_enabled || stress == 0) return {normal};
    const auto stressed = apply_marks(stress);
    return {stressed, stressed == normal ? std::nullopt : std::optional{normal}};
}

i32 Expression::beat_velocity(f64 position, Meter meter) const {
    if (!accents) return beat[1];
    if (!beat_string.empty()) return string_velocity(position, meter.measure());
    if (position < kTickTolerance) return beat[0];
    const auto step = beat_step.value_or(meter.group_starts.none() ? 1 : 0);
    const auto accent
        = step ? aligned(position, meter.beat() * step) : meter.default_accent(position);
    return beat[accent ? 1 : 2];
}

f64 Expression::gate(
    f64 duration, bool slur, GateArticulation style, std::optional<f64> stress
) const {
    if (style == GateArticulation::Unshaped) return duration;
    if (!slur) {
        const auto trim = trim_ticks.value_or(0);
        if (stress_enabled && stress && (!trim_ticks || trim > 0)) return duration * *stress;
        if (trim > 0) return trim < duration ? duration - trim : duration;
    }
    return duration + expand_ticks;
}

f64 Expression::tied_gate(f64 duration, GateArticulation style) const {
    // Export uses expand to compensate for the one-tick release trim on a
    // native round trip.
    if (style == GateArticulation::Unshaped) return duration;
    return duration + (trim_ticks.value_or(0) == 0 ? expand_ticks : 0);
}

f64 GraceSettings::step(f64 host, f64 unit, f64 weights) const {
    return proportional() ? host * ratio / weights : unit / divider;
}

void Expression::set_beat_string(std::string_view text, Location at, Diagnostics& diagnostics) {
    if (text.empty() || text.find_first_not_of("fFmMpP") != text.npos)
        diagnostics.error(at, "invalid-beatstring", "beatstring must consist of f, m and p");
    beat_string = text;
}

f64 fermata_length(f64 duration, f64 unit, FermataMode mode) {
    return duration + (mode == FermataMode::Fixed ? unit : duration);
}

static std::optional<FermataMode> read_fermata_mode(std::string_view name) {
    if (name == "fermatafixed") return FermataMode::Fixed;
    if (name == "fermataproportional") return FermataMode::Proportional;
    return {};
}

bool WrittenNoteLengths::handles(std::string_view name) {
    return name == "grace" || name == "gracedivider" || read_fermata_mode(name).has_value();
}

void WrittenNoteLengths::consume(const Token& token, Diagnostics& diagnostics) {
    Instruction command(token);
    auto        next = lengths.current();
    if (command.midi == "grace")
        next.grace.ratio = ratio(command.args, diagnostics, token.at, "grace").fraction();
    else if (command.midi == "gracedivider")
        next.grace = {midi_integer(command.args, 1, 1024, diagnostics, token.at), 0};
    else if (const auto mode = read_fermata_mode(command.midi))
        next.fermata = *mode;
    else
        return;
    lengths.record(token.at.written, next);
}

bool Expression::directive(
    std::string_view name,
    std::string_view args,
    f64              unit,
    Meter            meter,
    Location         at,
    Diagnostics&     diagnostics
) {
    const auto value
        = [&](i32 min, i32 max) { return midi_integer(args, min, max, diagnostics, at); };
    if (name == "beat") {
        BeatLevels levels;
        for (auto& level : levels) level = value(0, kMidiMax);
        const i32 spacing = value(0, std::numeric_limits<i32>::max());
        // abc2midi takes zero to mean the current meter's numerator.
        beat      = levels;
        beat_step = spacing == 0 ? meter.numerator : spacing;
    } else if (name == "beatmod")
        apply(value(-kMidiMax, kMidiMax));
    else if (name == "beatstring")
        set_beat_string(word(args), at, diagnostics);
    else if (name == "nobeataccents")
        accents = false;
    else if (name == "beataccents") {
        accents        = true;
        stress_enabled = false;
    } else if (name == "vol")
        next_volume = static_cast<i8>(value(0, kMidiMax));
    else if (name == "volinc")
        next_volume_delta = value(-kMidiMax, kMidiMax);
    else if (name == "trim")
        trim_ticks = unit * ratio(args, diagnostics, at, name).fraction();
    else if (name == "expand") {
        expand_ticks = unit * ratio(args, diagnostics, at, name).fraction();
        if (expand_ticks > 0) trim_ticks = 0;
    } else
        return false;
    return true;
}

bool HarpCuts::consume(const Token& token, Diagnostics& diagnostics) {
    Instruction command(token);
    if (command.midi != "harpmode") return false;
    const auto value = integer(word(command.args));
    if (!value || *value < 0 || !trim(command.args).empty()) {
        diagnostics.warn(
            token.at, "invalid-harpmode", "expected a nonnegative integer for MIDI harpmode"
        );
    } else {
        harp.record(token.at.written, *value != 0);
    }
    return true;
}

Length fermata_length(Length length, FermataMode mode) {
    if (mode == FermataMode::Proportional) return {2 * length.numerator, length.denominator};
    const auto numerator = length.numerator + length.denominator;
    const auto divisor   = std::gcd(numerator, length.denominator);
    return {numerator / divisor, length.denominator / divisor};
}

HarpAttacks harp_attacks(Length length) {
    const auto [numerator, denominator] = length;
    const auto unit                     = static_cast<f64>(denominator);
    if (numerator == 3 * denominator) {
        // A three-unit cut halves the written numerator in whole parts, even when it is odd.
        const i64 last_parts = numerator / 2 + 1;
        return {static_cast<f64>(numerator - 1) / (4 * unit), static_cast<f64>(last_parts) / unit};
    }
    const auto duration = static_cast<f64>(numerator == 0 ? 1 : numerator) / unit;
    return {duration / 4, duration / 2};
}

std::optional<CutNotation> HarpCuts::for_atom(
    const Atom& atom, Ornament ornament, Location at, Length scale
) const {
    if (!ornament.cut || ornament.trill || atom.chord || atom.notes.empty()) return {};
    // Single-note components are at most INT_MAX and tuplet factors at most 1024.
    // The unreduced products and their fermata and harp subdivisions all fit i64.
    return CutNotation{
        {atom.length.numerator * scale.numerator, atom.length.denominator * scale.denominator},
        harp.at(at.written)
    };
}

}   // namespace symusic::abc::detail

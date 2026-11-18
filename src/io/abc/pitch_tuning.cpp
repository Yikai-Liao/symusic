#include "pitch_tuning.h"

#include <cmath>
#include <locale>
#include <sstream>

namespace symusic::abc::detail {
namespace {

MidiPitch fractional_pitch(int base, Alteration alteration) {
    const auto [numerator, denominator] = alteration;
    const auto divisor                  = static_cast<int64_t>(denominator);
    // Put any shift outside the wheel's range in the MIDI note number.
    const bool wide     = numerator >= 2 * divisor || numerator < -2 * divisor;
    const auto fraction = wide ? numerator % divisor : numerator;
    const auto steps    = (midi_bend_center * std::abs(fraction) + divisor) / (2 * divisor);
    const auto wheel    = std::clamp<int64_t>(
        fraction < 0 ? -steps : steps, -midi_bend_center, midi_bend_center - 1
    );
    return {base + (wide ? numerator / denominator : 0), static_cast<int>(wheel)};
}

// Keep values small enough for bounded pitch and wheel arithmetic even with
// exotic scales and the largest octave and accidental accepted by the reader.
constexpr float  max_scale_cents      = 9600;
constexpr double third_harmonic_cents = 1901.9550008653874;

std::istringstream numbers(std::string_view args) {
    std::istringstream values{std::string(args)};
    values.imbue(std::locale::classic());
    return values;
}

template<class T>
bool optional_number(std::istream& values, std::optional<T>& result) {
    values >> std::ws;
    return values.eof() || static_cast<bool>(values >> result.emplace());
}

bool valid_octave(float cents) { return cents >= 1 && cents <= max_scale_cents; }
bool valid_interval(float cents) {
    return std::isfinite(cents) && std::abs(cents) <= max_scale_cents;
}

float quantized_fifth(float octave, int divisions, int fifth_steps) {
    if (fifth_steps > 0) return static_cast<float>(1.0 * fifth_steps * octave / divisions);
    const auto steps = std::trunc(0.5 + (third_harmonic_cents - octave) * divisions / octave);
    return static_cast<float>(steps) * octave / divisions;
}

}   // namespace

PitchTuning::PitchTuning(const Tune& tune, Diagnostics& diagnostics) {
    for (const auto& token : tune.header) read(token, diagnostics);
    for (const auto& token : tune.body) read(token, diagnostics);
}

std::optional<PitchTuning::Detune> PitchTuning::detune(std::string_view args) {
    auto   values = numbers(args);
    Detune offsets;
    for (auto& offset : offsets) {
        float cents;
        if (!(values >> cents) || !std::isfinite(cents) || std::abs(cents) > 200) return {};
        // abc2midi truncates after adding 0.5, including for negative offsets.
        offset = std::clamp(
            static_cast<int>(0.5 + 40.96 * cents), -midi_bend_center, midi_bend_center - 1
        );
    }
    return offsets;
}

std::optional<PitchTuning::Scale> PitchTuning::scale(
    float octave, float fifth, float sharp, float step
) {
    if (!valid_interval(fifth) || !valid_interval(sharp)) return {};
    return Scale{octave, fifth, sharp, step};
}

std::optional<PitchTuning::Scale> PitchTuning::linear(std::string_view args) {
    auto  values = numbers(args);
    float octave, fifth;
    if (!(values >> octave >> fifth) || !valid_octave(octave)) return {};
    const auto sharp = 7 * fifth - 4 * octave;
    return scale(octave, fifth, sharp, sharp);
}

std::optional<PitchTuning::Scale> PitchTuning::equal(std::string_view args) {
    auto values = numbers(args);
    int  divisions;
    if (!(values >> divisions) || divisions <= 0) return {};
    std::optional<float> octave_parameter;
    std::optional<int>   fifth_steps, sharp_steps;
    if (!optional_number(values, octave_parameter) || !optional_number(values, fifth_steps)
        || !optional_number(values, sharp_steps))
        return {};
    const float octave = octave_parameter.value_or(1200);
    if (!valid_octave(octave)) return {};
    const auto fifth = quantized_fifth(octave, divisions, fifth_steps.value_or(0));
    const auto step  = octave / divisions;
    const auto sharp = sharp_steps ? *sharp_steps * octave / divisions : 7 * fifth - 4 * octave;
    return scale(octave, fifth, sharp, step);
}

std::optional<PitchTuning::Midi> PitchTuning::midi(std::string_view name) {
    if (name == "C") return Midi::middle_c;
    if (name == "temperamentnormal") return Midi::normal;
    if (name == "temperamentlinear") return Midi::linear;
    if (name == "temperamentequal") return Midi::equal;
    return {};
}

bool PitchTuning::handles_midi(std::string_view name) { return midi(name).has_value(); }

const PitchTuning::State& PitchTuning::current() const {
    static const State standard;
    return changes.empty() ? standard : changes.back().state;
}

void PitchTuning::read_detune(std::string_view args, Location at, Diagnostics& diagnostics) {
    if (const auto offsets = detune(args))
        changes.push_back({at, {current().middle_c, *offsets}});
    else
        diagnostics.warn(
            at,
            "invalid-temperament",
            "ignored temperament: expected 12 detunings from -200 to 200 cents"
        );
}

void PitchTuning::read_midi(std::string_view args, Location at, Diagnostics& diagnostics) {
    if (word(args) != "MIDI") return;
    const auto name    = word(args);
    const auto command = midi(name);
    if (!command) return;
    if (*command == Midi::middle_c) {
        auto next     = current();
        next.middle_c = midi_integer(args, 0, midi_max, diagnostics, at);
        changes.push_back({at, next});
        return;
    }
    if (*command == Midi::normal) {
        changes.push_back({at, {}});
        return;
    }
    const auto scale = *command == Midi::linear ? linear(args) : equal(args);
    if (!scale) {
        diagnostics.warn(at, "invalid-temperament", fmt::format("ignored invalid MIDI {}", name));
        return;
    }
    if (scale->sharp < 0)
        diagnostics.warn(at, "inverted-accidentals", "temperament makes sharps lower the pitch");
    // All three MIDI temperament commands reset the tuning anchor to C4.
    changes.push_back({at, {60, *scale}});
}

void PitchTuning::read(const Token& token, Diagnostics& diagnostics) {
    if (const auto* directive = std::get_if<Directive>(&token.value)) {
        auto args = directive->value;
        if (word(args) == "temperament")
            read_detune(args, token.at, diagnostics);
        else
            read_midi(directive->value, token.at, diagnostics);
    } else if (const auto* field = std::get_if<Field>(&token.value)) {
        if (field->kind == 'I') read_midi(field->value, token.at, diagnostics);
        // X: resets a file-level temperament, but not an earlier MIDI C.
        if (field->kind == 'X' && !changes.empty())
            changes.push_back({token.at, {current().middle_c, Detune{}}});
    }
}

MidiPitch PitchTuning::Scale::pitch(
    char letter, int register_, int middle_c, Alteration alteration
) const {
    constexpr std::array fifths{3, 5, 0, 2, 4, -1, 1};
    constexpr std::array octaves{-1, -2, 0, -1, -2, 1, 0};
    const auto           index           = letter_index(letter);
    const float          sharp_semitones = sharp / 100.0;
    const auto [numerator, denominator]  = alteration;
    float semitones = (fifths[index] * fifth + octaves[index] * octave) / 100.0;
    if (denominator == 0) semitones += numerator * sharp_semitones;
    semitones = semitones + register_ * octave / 100.0 + middle_c;
    // The fifths and octaves are relative to a stable A4 at the center wheel.
    semitones += 9.0 - (3.0 * fifth - octave) / 100.0;
    if (denominator != 0) {
        const float fraction = denominator == 1 ? numerator * microstep / 100.0 :
                                                  static_cast<float>(numerator) / denominator;
        semitones += fraction * sharp_semitones;
    }
    const int note  = static_cast<int>(semitones + 0.5);
    const int wheel = static_cast<int>(0.5 + midi_bend_center + 4096.0 * (semitones - note));
    return {note, std::clamp(wheel, 0, 2 * midi_bend_center - 1) - midi_bend_center};
}

MidiPitch PitchTuning::pitch(char letter, int octave, Alteration alteration, Location at) const {
    static const State standard;
    const auto         change = std::ranges::upper_bound(changes, at, {}, &Change::at);
    const auto&        state  = change == changes.begin() ? standard : std::prev(change)->state;
    if (const auto* scale = std::get_if<Scale>(&state.temper))
        return scale->pitch(letter, octave, state.middle_c, alteration);
    const int natural = natural_pitch[letter_index(letter)];
    const int base    = state.middle_c + 12 * octave + natural;
    if (alteration.denominator != 0 && alteration.numerator != 0)
        return fractional_pitch(base, alteration);
    const int   semitone = natural + alteration.numerator;
    const auto& offsets  = std::get<Detune>(state.temper);
    return {base + alteration.numerator, offsets[(semitone % 12 + 12) % 12]};
}

}   // namespace symusic::abc::detail

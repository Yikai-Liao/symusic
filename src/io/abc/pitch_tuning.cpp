#include "pitch_tuning.h"

#include <algorithm>
#include <array>
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

MidiPitch quantize_comma53(MidiPitch pitch) {
    constexpr int  kWheelStepsPerSemitone = midi_bend_center / 2;
    constexpr auto kSplitSlots            = std::array{4, 12, 24, 32, 40};
    constexpr auto kSemitonesPerComma     = 0.22641509f;
    const auto     bend      = static_cast<float>(pitch.wheel) / kWheelStepsPerSemitone;
    const auto     semitones = static_cast<float>(pitch.note) + bend;
    const auto     octave    = static_cast<int>(std::floor(semitones / 12.0));
    const auto     slot      = static_cast<int>(std::floor(semitones * 4.0)) - 48 * octave;
    const auto     split     = std::ranges::lower_bound(kSplitSlots, slot);
    // abc2midi leaves a second comma in each black-key slot for alternate spellings.
    auto commas = 53 * octave + slot + static_cast<int>(split - kSplitSlots.begin());
    if (split != kSplitSlots.end() && *split == slot && (bend > 1.1 || bend < -0.8)) ++commas;

    // Use the converter's single-precision step and a lower MIDI note with an upward bend.
    const auto quantized = static_cast<float>(commas) * kSemitonesPerComma;
    const auto note      = static_cast<int>(std::floor(quantized));
    return {note, static_cast<int>((quantized - note) * kWheelStepsPerSemitone)};
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
    for (const auto& token : tune.header) read(token, tune.reference, diagnostics);
    for (const auto& token : tune.body) read(token, tune.reference, diagnostics);
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
    if (name == "C") return Midi::MiddleC;
    if (name == "temperamentnormal") return Midi::Normal;
    if (name == "temperamentlinear") return Midi::Linear;
    if (name == "temperamentequal") return Midi::Equal;
    if (name == "tuningsystem") return Midi::TuningSystem;
    return {};
}

bool PitchTuning::handles_midi(std::string_view name) { return midi(name).has_value(); }

const PitchTuning::State& PitchTuning::current() const {
    static const State standard;
    return changes.empty() ? standard : changes.back().state;
}

void PitchTuning::read_detune(std::string_view args, Location at, Diagnostics& diagnostics) {
    if (const auto offsets = detune(args)) {
        auto next   = current();
        next.temper = *offsets;
        changes.push_back({at, next});
    } else
        diagnostics.warn(
            at,
            "invalid-temperament",
            "ignored temperament: expected 12 detunings from -200 to 200 cents"
        );
}

void PitchTuning::read_midi(
    std::string_view args, Location at, Location tune_start, Diagnostics& diagnostics
) {
    if (word(args) != "MIDI") return;
    const auto name    = word(args);
    const auto command = midi(name);
    if (!command) return;
    auto next = current();
    switch (*command) {
    case Midi::MiddleC: next.middle_c = midi_integer(args, 0, midi_max, diagnostics, at); break;
    case Midi::Normal:
        next.middle_c = 60;
        next.temper   = Detune{};
        break;
    case Midi::Linear:
    case Midi::Equal: {
        const auto tuning = *command == Midi::Linear ? linear(args) : equal(args);
        if (!tuning) {
            diagnostics.warn(
                at, "invalid-temperament", fmt::format("ignored invalid MIDI {}", name)
            );
            return;
        }
        if (tuning->sharp < 0)
            diagnostics.warn(
                at, "inverted-accidentals", "temperament makes sharps lower the pitch"
            );
        // The MIDI temperament commands reset the anchor to C4, not the comma53 quantizer.
        next.middle_c = 60;
        next.temper   = *tuning;
        break;
    }
    case Midi::TuningSystem:
        if (at < tune_start) {
            diagnostics.warn(at, "tuning-system-placement", "ignored tuningsystem before X:");
            return;
        }
        if (trim(args) != "comma53") {
            diagnostics.warn(at, "invalid-tuning-system", "expected comma53 after tuningsystem");
            return;
        }
        next.comma53 = true;
        break;
    }
    changes.push_back({at, next});
}

void PitchTuning::read(const Token& token, Location tune_start, Diagnostics& diagnostics) {
    if (const auto* directive = std::get_if<Directive>(&token.value)) {
        auto args = directive->value;
        if (word(args) == "temperament")
            read_detune(args, token.at, diagnostics);
        else
            read_midi(directive->value, token.at, tune_start, diagnostics);
    } else if (const auto* field = std::get_if<Field>(&token.value)) {
        if (field->kind == 'I') read_midi(field->value, token.at, tune_start, diagnostics);
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

MidiPitch PitchTuning::State::tempered_pitch(char letter, int octave, Alteration alteration) const {
    if (const auto* scale = std::get_if<Scale>(&temper))
        return scale->pitch(letter, octave, middle_c, alteration);
    const int natural = natural_pitch[letter_index(letter)];
    const int base    = middle_c + 12 * octave + natural;
    if (alteration.denominator != 0 && alteration.numerator != 0)
        return fractional_pitch(base, alteration);
    const int   semitone = natural + alteration.numerator;
    const auto& offsets  = std::get<Detune>(temper);
    return {base + alteration.numerator, offsets[(semitone % 12 + 12) % 12]};
}

MidiPitch PitchTuning::pitch(char letter, int octave, Alteration alteration, Location at) const {
    static const State standard;
    const auto         change = std::ranges::upper_bound(changes, at, {}, &Change::at);
    const auto&        state  = change == changes.begin() ? standard : std::prev(change)->state;
    const auto         tuned  = state.tempered_pitch(letter, octave, alteration);
    return state.comma53 ? quantize_comma53(tuned) : tuned;
}

}   // namespace symusic::abc::detail

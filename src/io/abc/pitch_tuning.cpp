#include "pitch_tuning.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <locale>
#include <sstream>

namespace symusic::abc::detail {
namespace {

constexpr i32 kWheelStepsPerSemitone = kMidiBendCenter / 2;

// abc2midi adds half a unit and truncates toward zero, including for negative pitches.
i32 tuning_integer(f64 value) { return static_cast<i32>(std::trunc(value + 0.5)); }

MidiPitch fractional_pitch(i32 base, Alteration alteration) {
    const auto [numerator, denominator] = alteration;
    const auto divisor                  = static_cast<i64>(denominator);
    // Put any shift outside the wheel's range in the MIDI note number.
    const bool wide     = numerator >= 2 * divisor || numerator < -2 * divisor;
    const auto fraction = wide ? numerator % divisor : numerator;
    const auto steps    = (kMidiBendCenter * std::abs(fraction) + divisor) / (2 * divisor);
    const auto wheel
        = std::clamp<i64>(fraction < 0 ? -steps : steps, -kMidiBendCenter, kMidiBendCenter - 1);
    return {base + (wide ? numerator / denominator : 0), static_cast<i32>(wheel)};
}

MidiPitch quantize_comma53(MidiPitch pitch) {
    constexpr auto kSplitSlots        = std::array{4, 12, 24, 32, 40};
    constexpr auto kSemitonesPerComma = 0.22641509f;
    const auto     bend               = static_cast<f32>(pitch.wheel) / kWheelStepsPerSemitone;
    const auto     semitones          = static_cast<f32>(pitch.note) + bend;
    const auto     octave             = static_cast<i32>(std::floor(semitones / 12.0));
    const auto     slot               = static_cast<i32>(std::floor(semitones * 4.0)) - 48 * octave;
    const auto     split              = std::ranges::lower_bound(kSplitSlots, slot);
    // abc2midi leaves a second comma in each black-key slot for alternate spellings.
    auto commas = 53 * octave + slot + static_cast<i32>(split - kSplitSlots.begin());
    if (split != kSplitSlots.end() && *split == slot && (bend > 1.1 || bend < -0.8)) ++commas;

    // Use the converter's single-precision step and a lower MIDI note with an upward bend.
    const auto quantized = static_cast<f32>(commas) * kSemitonesPerComma;
    const auto note      = std::floor(quantized);
    return {static_cast<i32>(note), static_cast<i32>((quantized - note) * kWheelStepsPerSemitone)};
}

// Keep values small enough for bounded pitch and wheel arithmetic even with
// exotic scales and the largest octave and accidental accepted by the reader.
constexpr f32 kMaxScaleCents      = 9600;
constexpr f64 kThirdHarmonicCents = 1901.9550008653874;

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

bool valid_octave(f32 cents) { return cents >= 1 && cents <= kMaxScaleCents; }
bool valid_interval(f32 cents) { return std::isfinite(cents) && std::abs(cents) <= kMaxScaleCents; }

f32 quantized_fifth(f32 octave, i32 divisions, i32 fifth_steps) {
    if (fifth_steps > 0)
        return static_cast<f32>(static_cast<f64>(fifth_steps) * octave / divisions);
    const auto steps = std::trunc(0.5 + (kThirdHarmonicCents - octave) * divisions / octave);
    return static_cast<f32>(steps) * octave / static_cast<f32>(divisions);
}

enum class Midi { MiddleC, Normal, Linear, Equal, TuningSystem };

std::optional<Midi> midi(std::string_view name) {
    if (name == "C") return Midi::MiddleC;
    if (name == "temperamentnormal") return Midi::Normal;
    if (name == "temperamentlinear") return Midi::Linear;
    if (name == "temperamentequal") return Midi::Equal;
    if (name == "tuningsystem") return Midi::TuningSystem;
    return {};
}

}   // namespace

PitchTuning::PitchTuning(const Tune& tune, Diagnostics& diagnostics) {
    const auto collect = [&](const Token& token) {
        if (const auto next = changes.current().change(token, tune.reference, diagnostics))
            changes.record(token.at.written, *next);
    };
    std::ranges::for_each(tune.header, collect);
    std::ranges::for_each(tune.body, collect);
}

std::optional<PitchTuning::Detune> PitchTuning::detune(std::string_view args) {
    auto   values = numbers(args);
    Detune offsets;
    for (auto& offset : offsets) {
        f32 cents;
        if (!(values >> cents) || !std::isfinite(cents) || std::abs(cents) > 200) return {};
        constexpr auto kWheelStepsPerCent = kWheelStepsPerSemitone / 100.0;
        offset                            = std::clamp(
            tuning_integer(kWheelStepsPerCent * cents), -kMidiBendCenter, kMidiBendCenter - 1
        );
    }
    return offsets;
}

std::optional<PitchTuning::Scale> PitchTuning::scale(f32 octave, f32 fifth, f32 sharp, f32 step) {
    if (!valid_interval(fifth) || !valid_interval(sharp)) return {};
    return Scale{octave, fifth, sharp, step};
}

std::optional<PitchTuning::Scale> PitchTuning::linear(std::string_view args) {
    auto values = numbers(args);
    f32  octave, fifth;
    if (!(values >> octave >> fifth) || !valid_octave(octave)) return {};
    const auto sharp = 7 * fifth - 4 * octave;
    return scale(octave, fifth, sharp, sharp);
}

std::optional<PitchTuning::Scale> PitchTuning::equal(std::string_view args) {
    auto values = numbers(args);
    i32  divisions;
    if (!(values >> divisions) || divisions <= 0) return {};
    std::optional<f32> octave_parameter;
    std::optional<i32> fifth_steps, sharp_steps;
    if (!optional_number(values, octave_parameter) || !optional_number(values, fifth_steps)
        || !optional_number(values, sharp_steps))
        return {};
    const f32 octave = octave_parameter.value_or(1200);
    if (!valid_octave(octave)) return {};
    const auto fifth = quantized_fifth(octave, divisions, fifth_steps.value_or(0));
    const auto count = static_cast<f32>(divisions);
    const auto step  = octave / count;
    const auto sharp
        = sharp_steps ? static_cast<f32>(*sharp_steps) * octave / count : 7 * fifth - 4 * octave;
    return scale(octave, fifth, sharp, step);
}

bool PitchTuning::handles_midi(std::string_view name) { return midi(name).has_value(); }

std::optional<PitchTuning::State> PitchTuning::State::change(
    const Token& token, Location tune_start, Diagnostics& diagnostics
) const {
    const auto at = token.at;
    // X: carries only the file-level middle C into the tune.
    if (const auto* field = std::get_if<Field>(&token.value); field && field->kind == 'X')
        return State{.middle_c = middle_c};

    Instruction instruction(token);
    if (instruction.name == "temperament" && std::holds_alternative<Directive>(token.value)) {
        if (const auto offsets = detune(instruction.args))
            return State{middle_c, *offsets, comma53};
        diagnostics.warn(
            at,
            "invalid-temperament",
            "ignored temperament: expected 12 detunings from -200 to 200 cents"
        );
        return {};
    }
    const auto command = midi(instruction.midi);
    if (!command) return {};
    auto& args = instruction.args;
    switch (*command) {
    case Midi::MiddleC:
        return State{midi_integer(args, 0, kMidiMax, diagnostics, at), temper, comma53};
    case Midi::Normal: return State{.comma53 = comma53};
    case Midi::Linear:
    case Midi::Equal: {
        const auto tuning = *command == Midi::Linear ? linear(args) : equal(args);
        if (!tuning) {
            diagnostics.warn(
                at, "invalid-temperament", fmt::format("ignored invalid MIDI {}", instruction.midi)
            );
            return {};
        }
        if (tuning->sharp < 0)
            diagnostics.warn(
                at, "inverted-accidentals", "temperament makes sharps lower the pitch"
            );
        // MIDI temperament changes reset the anchor to C4 and keep the comma53 quantizer.
        return State{.temper = *tuning, .comma53 = comma53};
    }
    case Midi::TuningSystem:
        if (at < tune_start) {
            diagnostics.warn(at, "tuning-system-placement", "ignored tuningsystem before X:");
            return {};
        }
        if (trim(args) != "comma53") {
            diagnostics.warn(at, "invalid-tuning-system", "expected comma53 after tuningsystem");
            return {};
        }
        return State{middle_c, temper, true};
    }
    return {};
}

MidiPitch PitchTuning::Scale::pitch(
    char letter, i32 register_index, i32 middle_c, Alteration alteration
) const {
    constexpr std::array<f32, 7> kOctaves{-1, -2, 0, -1, -2, 1, 0};
    const auto                   index           = letter_index(letter);
    const auto                   sharp_semitones = sharp / 100.0f;
    const auto                   numerator       = static_cast<f32>(alteration.numerator);
    const auto                   denominator     = alteration.denominator;
    // The reference rounds the spelling, register and A4 correction separately to f32.
    f32 semitones
        = (static_cast<f32>(kTonicFifths[index]) * fifth + kOctaves[index] * octave) / 100.0f;
    if (denominator == 0) semitones += numerator * sharp_semitones;
    const auto register_semitones = static_cast<f32>(register_index) * octave / 100.0;
    semitones                     = static_cast<f32>(semitones + register_semitones + middle_c);
    // The fifths and octaves are relative to a stable A4 at the center wheel.
    const auto anchor = 9.0 - (3.0 * fifth - octave) / 100.0;
    semitones         = static_cast<f32>(semitones + anchor);
    if (denominator != 0) {
        const auto fraction = denominator == 1 ? numerator * microstep / 100.0f :
                                                 numerator / static_cast<f32>(denominator);
        semitones += fraction * sharp_semitones;
    }
    const i32 note  = tuning_integer(semitones);
    const f64 bend  = semitones - static_cast<f32>(note);
    const i32 wheel = tuning_integer(kMidiBendCenter + kWheelStepsPerSemitone * bend);
    return {note, std::clamp(wheel, 0, 2 * kMidiBendCenter - 1) - kMidiBendCenter};
}

MidiPitch PitchTuning::State::tempered_pitch(char letter, i32 octave, Alteration alteration) const {
    if (const auto* scale = std::get_if<Scale>(&temper))
        return scale->pitch(letter, octave, middle_c, alteration);
    const i32 natural = kNaturalPitch[letter_index(letter)];
    const i32 base    = middle_c + 12 * octave + natural;
    if (alteration.denominator != 0 && alteration.numerator != 0)
        return fractional_pitch(base, alteration);
    const i32   semitone = natural + alteration.numerator;
    const auto& offsets  = std::get<Detune>(temper);
    return {base + alteration.numerator, offsets[(semitone % 12 + 12) % 12]};
}

MidiPitch PitchTuning::pitch(char letter, i32 octave, Alteration alteration, Location at) const {
    const auto& state = changes.at(at.written);
    const auto  tuned = state.tempered_pitch(letter, octave, alteration);
    return state.comma53 ? quantize_comma53(tuned) : tuned;
}

}   // namespace symusic::abc::detail

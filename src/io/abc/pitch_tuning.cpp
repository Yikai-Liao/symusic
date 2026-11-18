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

}   // namespace

PitchTuning::PitchTuning(const Tune& tune, Diagnostics& diagnostics) {
    // abc2midi resets temperament at X:, even after a file-level directive.
    for (const auto& token : tune.header) {
        const auto* field = std::get_if<Field>(&token.value);
        if (field && field->kind == 'X') changes.clear();
        read(token, diagnostics);
    }
    for (const auto& token : tune.body) read(token, diagnostics);
}

std::optional<PitchTuning::Detune> PitchTuning::detune(std::string_view args) {
    std::istringstream values{std::string(args)};
    values.imbue(std::locale::classic());
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

void PitchTuning::read(const Token& token, Diagnostics& diagnostics) {
    std::string_view args;
    if (const auto* directive = std::get_if<Directive>(&token.value)) {
        args           = directive->value;
        auto remainder = args;
        if (word(remainder) == "temperament") {
            if (const auto offsets = detune(remainder))
                changes.push_back({token.at, *offsets});
            else
                diagnostics.warn(
                    token.at,
                    "invalid-temperament",
                    "ignored temperament: expected 12 detunings from -200 to 200 cents"
                );
            return;
        }
    } else if (const auto* field = std::get_if<Field>(&token.value); field && field->kind == 'I')
        args = field->value;
    if (word(args) == "MIDI" && word(args) == reset_command) changes.push_back({token.at, {}});
}

MidiPitch PitchTuning::pitch(
    char letter, int octave, int middle_c, Alteration alteration, Location at
) const {
    const int natural = natural_pitch[letter_index(letter)];
    const int base    = middle_c + 12 * octave + natural;
    if (alteration.denominator != 0) return fractional_pitch(base, alteration);
    const int  semitone = natural + alteration.numerator;
    const auto change   = std::ranges::upper_bound(changes, at, {}, &Change::at);
    const int  wheel    = change == changes.begin() ?
                              0 :
                              std::prev(change)->wheel_offsets[(semitone % 12 + 12) % 12];
    return {base + alteration.numerator, wheel};
}

}   // namespace symusic::abc::detail

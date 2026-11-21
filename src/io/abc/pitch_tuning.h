#pragma once

#include "settings.h"

namespace symusic::abc::detail {

// MIDI note number and signed bend offset (zero is the centered wheel).
// The selected output route validates the final note before narrowing it.
struct MidiPitch {
    i32 note;
    i32 wheel;
};

// Tuning follows written source order across voices, not the performed order
// of repeats and parts. A malformed change warns and leaves the prior tuning.
class PitchTuning {
public:
    PitchTuning() = default;
    PitchTuning(const Tune& tune, Diagnostics& diagnostics);

    [[nodiscard]] static bool handles_midi(std::string_view name);
    // Resolve written spelling using the state's source position. The caller
    // supplies the effective octave and accidental; channel transposition and
    // drum mapping are subsequent playback decisions.
    [[nodiscard]] MidiPitch pitch(
        char letter, i32 octave, Alteration alteration, Location at
    ) const;

private:
    using Detune = std::array<i32, 12>;
    struct Scale {
        f32 octave;
        f32 fifth;
        f32 sharp;
        f32 microstep;

        [[nodiscard]] MidiPitch pitch(
            char letter, i32 octave, i32 middle_c, Alteration alteration
        ) const;
    };
    struct State {
        i32                         middle_c = 60;
        std::variant<Detune, Scale> temper{Detune{}};
        bool                        comma53 = false;

        [[nodiscard]] std::optional<State> change(
            const Token& token, Location tune_start, Diagnostics& diagnostics
        ) const;
        [[nodiscard]] MidiPitch tempered_pitch(
            char letter, i32 octave, Alteration alteration
        ) const;
    };
    WrittenSetting<State> changes;

    [[nodiscard]] static std::optional<Detune> detune(std::string_view args);
    [[nodiscard]] static std::optional<Scale>  scale(f32 octave, f32 fifth, f32 sharp, f32 step);
    [[nodiscard]] static std::optional<Scale>  linear(std::string_view args);
    [[nodiscard]] static std::optional<Scale>  equal(std::string_view args);
};

}   // namespace symusic::abc::detail

#pragma once

#include "settings.h"

namespace symusic::abc::detail {

// MIDI note number and signed bend offset (zero is the centered wheel).
// The selected output route validates the final note before narrowing it.
struct MidiPitch {
    int note;
    int wheel;
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
        char letter, int octave, Alteration alteration, Location at
    ) const;

private:
    enum class Midi { MiddleC, Normal, Linear, Equal, TuningSystem };
    using Detune = std::array<int, 12>;
    struct Scale {
        float octave;
        float fifth;
        float sharp;
        float microstep;

        [[nodiscard]] MidiPitch pitch(
            char letter, int octave, int middle_c, Alteration alteration
        ) const;
    };
    struct State {
        int                         middle_c = 60;
        std::variant<Detune, Scale> temper{Detune{}};
        bool                        comma53 = false;

        [[nodiscard]] MidiPitch tempered_pitch(
            char letter, int octave, Alteration alteration
        ) const;
    };
    struct Change {
        Location at;
        State    state;
    };
    vec<Change> changes;

    [[nodiscard]] static std::optional<Midi>   midi(std::string_view name);
    [[nodiscard]] static std::optional<Detune> detune(std::string_view args);
    [[nodiscard]] static std::optional<Scale>  scale(
        float octave, float fifth, float sharp, float step
    );
    [[nodiscard]] static std::optional<Scale> linear(std::string_view args);
    [[nodiscard]] static std::optional<Scale> equal(std::string_view args);
    void read(const Token& token, Location tune_start, Diagnostics& diagnostics);
    void read_midi(
        std::string_view args, Location at, Location tune_start, Diagnostics& diagnostics
    );
    void read_detune(std::string_view args, Location at, Diagnostics& diagnostics);
    [[nodiscard]] const State& current() const;
};

}   // namespace symusic::abc::detail

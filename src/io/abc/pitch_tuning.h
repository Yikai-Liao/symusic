#pragma once

#include "settings.h"

namespace symusic::abc::detail {

struct MidiPitch {
    int note;
    int wheel;
};

// Tuning follows written source order across voices, not the performed order
// of repeats and parts. A malformed change warns and leaves the prior tuning.
class PitchTuning {
public:
    static constexpr std::string_view reset_command = "temperamentnormal";

    PitchTuning() = default;
    PitchTuning(const Tune& tune, Diagnostics& diagnostics);

    [[nodiscard]] MidiPitch pitch(
        char letter, int octave, int middle_c, Alteration alteration, Location at
    ) const;

private:
    using Detune = std::array<int, 12>;
    struct Change {
        Location at;
        Detune   wheel_offsets;
    };
    vec<Change> changes;

    void                                       read(const Token& token, Diagnostics& diagnostics);
    [[nodiscard]] static std::optional<Detune> detune(std::string_view args);
};

}   // namespace symusic::abc::detail

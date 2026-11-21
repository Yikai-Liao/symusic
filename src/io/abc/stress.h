#pragma once

#include "settings.h"

namespace symusic::abc::detail {

// A slice of the performed stress grid. duration is in ticks; a zero velocity
// leaves the normal accent profile in charge. gate_ratio is dimensionless.
struct StressNote {
    f64                duration;
    i32                velocity = 0;
    std::optional<f64> gate_ratio;
};

// The first Phil Taylor profile supplies the tune default. Later changes apply
// in written order; beataccents can still disable expression in performed order.
// Invalid settings warn and leave the previous valid setting in effect.
class TuneStress {
public:
    TuneStress() = default;
    // Replace stress instructions with their performed commands while resolving written settings.
    TuneStress(Tune& tune, const TextLoader& files, Diagnostics& diagnostics);

    [[nodiscard]] bool configured() const { return !profiles.empty(); }
    // Only model 1 samples the attacks of an expanded ornament individually.
    [[nodiscard]] bool articulates(Location at, Meter meter) const;
    // Map an interval in unwarped ticks from the current stress origin.
    // Model 1 considers the endpoint when deciding whether to set a gate.
    // With no applicable profile, duration is unchanged and there is no gate
    // override. Location selects the profile in written, not repeat order.
    [[nodiscard]] StressNote note(Location at, f64 start, f64 length, Meter meter) const;

private:
    enum class Model { Articulation, Timing };
    struct Segment {
        i32 velocity;
        f64 stretch;
        f64 warped_start = 0;
    };

    struct Profile {
        vec<Segment> segments;
        f64          widest = 0;

        [[nodiscard]] f64 warped_tick(f64 position, f64 measure) const;
        [[nodiscard]] f64 gate_ratio(f64 start, f64 end, f64 length) const;
    };
    struct Settings {
        size_t profile = 0;
        Model  model   = Model::Timing;
    };

    WrittenSetting<Settings> settings;
    vec<Profile>             profiles;

    static std::optional<Profile> read_profile(
        std::string_view args, Location at, const TextLoader& files, Diagnostics& diagnostics
    );
    [[nodiscard]] static std::optional<Profile> parse(std::string_view text);
};

}   // namespace symusic::abc::detail

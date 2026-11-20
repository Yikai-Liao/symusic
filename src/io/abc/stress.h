#pragma once

#include <map>

#include "settings.h"

namespace symusic::abc::detail {

// A slice of the performed stress grid. duration is in ticks; a zero velocity
// leaves the normal accent profile in charge. gate_ratio is dimensionless.
struct StressNote {
    long double                duration;
    int                        velocity = 0;
    std::optional<long double> gate_ratio;
};

// The first Phil Taylor profile supplies the tune default. Later changes apply
// in written order; beataccents can still disable expression in performed order.
// Invalid settings warn and leave the previous valid setting in effect.
class TuneStress {
public:
    TuneStress() = default;
    TuneStress(const Tune& tune, const TextLoader& files, Diagnostics& diagnostics);

    [[nodiscard]] static bool handles(std::string_view midi_command);
    [[nodiscard]] bool        configured() const { return !profiles.empty(); }
    [[nodiscard]] bool        loads(Location at, std::string_view command) const;
    // Only model 1 samples the attacks of an expanded ornament individually.
    [[nodiscard]] bool articulates(Location at, Meter meter) const;
    // Map an interval in unwarped ticks from the current stress origin.
    // Model 1 considers the endpoint when deciding whether to set a gate.
    // With no applicable profile, duration is unchanged and there is no gate
    // override. Location selects the profile in written, not repeat order.
    [[nodiscard]] StressNote note(
        Location at, long double start, long double length, Meter meter
    ) const;

private:
    enum class Model { articulation, timing };
    struct Segment {
        int         velocity;
        long double stretch;
        long double warped_start = 0;
    };

    struct Profile {
        vec<Segment> segments;
        long double  widest = 0;

        [[nodiscard]] long double warped_tick(long double position, Meter meter) const;
        [[nodiscard]] long double gate_ratio(
            long double start, long double end, long double length
        ) const;
    };
    struct Settings {
        size_t profile = 0;
        Model  model   = Model::timing;
    };

    Settings                                                opening;
    vec<Profile>                                            profiles;
    vec<std::pair<Location, Settings>>                      changes;
    std::map<std::pair<Location, std::string_view>, size_t> loaded_profiles;

    [[nodiscard]] const Settings& settings_at(Location at) const;
    bool                          configure(
        const Token& token, Settings& state, const TextLoader& files, Diagnostics& diagnostics
    );
    [[nodiscard]] static std::optional<Profile> parse(std::string_view text);
};

}   // namespace symusic::abc::detail

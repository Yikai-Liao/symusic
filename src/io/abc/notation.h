#pragma once

#include <map>
#include <string_view>

#include "expression.h"
#include "navigation.h"

namespace symusic::abc::detail {

struct TuneSettings;
struct VoicePlan;

inline constexpr f64 kEpsilon = kTickTolerance;

[[nodiscard]] std::string_view read_mark(std::string_view spelling);
[[nodiscard]] bool Ornament::* ornament_flag(std::string_view spelling);

// The measure remembers values, not the policy that wrote them. A pitch-wide
// write replaces older octave overrides for that letter; a local write does not.
class Accidentals {
public:
    Alteration resolve(Pitch pitch, const Key& key, AccidentalScope scope);
    void       reset() { *this = {}; }

private:
    std::array<std::optional<Alteration>, 7>   by_letter{};
    std::map<std::pair<char, i32>, Alteration> by_octave;
};

struct WrittenFields {
    Meter meter;
    Key   tone;
    f64   unit;
};

// A K: field that sets a signature clears this branch's carried accidentals
// and returns true so playback can publish the signature at its current time.
struct WrittenState {
    WrittenFields fields;
    Accidentals   accidentals;

    bool change_key(std::string_view value, Diagnostics& diagnostics, Location at);
};

enum class SlurPosition { Outside, Inside, Closing };

// One written slot supplies the same timing on every visit, even if navigation
// enters between the two operands of a broken rhythm.
struct Prepared {
    const Atom*                atom;
    Location                   at;
    size_t                     music_line;
    f64                        duration;
    f64                        unit;
    f64                        tuplet_unit;
    Ornament                   ornament;
    NoteLengthSettings         lengths;
    std::optional<CutNotation> cut;
    bool                       tied;
    SlurPosition               slur;
    f64                        rhythm_scale = 1;
    f64                        last_scale   = 1;
    [[nodiscard]] f64          played_duration() const { return duration * rhythm_scale; }
    [[nodiscard]] bool         harp_cut() const { return cut && cut->harp; }
    [[nodiscard]] bool         neighbor_cut() const {
        return ornament.cut && !ornament.roll && !ornament.trill && atom->notes.size() == 1;
    }
    [[nodiscard]] bool staccato() const { return ornament.staccato && !harp_cut(); }
};

struct WrittenNavigation {
    std::map<const Token*, WrittenState> entries;
    std::map<const Token*, Prepared>     notes;
    std::map<const Token*, f64>          grace_units;
    // Operands of visited M: and L: fields, including those deferred past grace attacks.
    std::map<const Field*, std::variant<Meter, f64>> metric_fields;
};

// Validate the whole written voice. Keep timing and articulation once per visited
// note or grace group, and the notation in force just before each jump destination.
// All token keys borrow from plan.source.
WrittenNavigation written_navigation(
    const VoicePlan&    plan,
    WrittenFields       opening,
    const TuneSettings& settings,
    Diagnostics&        diagnostics
);
std::optional<HarpAttacks> harp_attacks(const Prepared& note);

}   // namespace symusic::abc::detail

#pragma once

#include <utility>

#include "notation.h"
#include "playback.h"

namespace symusic::abc::detail {

// An overlay selects consecutive lanes. An implicit group ends at a measure or
// part boundary, an explicit group at &). Invalid commands diagnose at their token.
class OverlaySelector {
public:
    explicit OverlaySelector(Diagnostics& diagnostics) : diagnostics(diagnostics) {}
    [[nodiscard]] size_t id() const { return selected; }
    [[nodiscard]] bool   in_implicit_overlay() const { return selected != 0 && !explicit_group; }
    // Returns false for an ignored end. Nesting and exceeding the lane limit throw.
    bool control(Overlay command, Location at);
    bool close_implicit();

private:
    Diagnostics& diagnostics;
    size_t       selected       = 0;
    bool         explicit_group = false;
};

// Each split starts a fresh secondary measure with the preceding branch's
// fields. A join selects the suspended primary. Only a split followed by an
// explicit end counts as a barline for its carried accidentals.
class OverlayNotation : private OverlaySelector {
public:
    OverlayNotation(WrittenFields opening, Diagnostics& diagnostics) :
        OverlaySelector(diagnostics), primary{opening, {}} {}
    using OverlaySelector::close_implicit;
    using OverlaySelector::id;
    using OverlaySelector::in_implicit_overlay;
    WrittenState& notation() { return id() == 0 ? primary : secondary; }
    bool          control(Overlay command, Location at);

private:
    WrittenState primary, secondary{};
};

struct TieStress {
    Location at;
    Meter    meter;
    f64      start;
    f64      next_offset;
    f64      release_offset;
};

struct TiedOrnament;

struct TieNote {
    i32                                     untransposed_pitch;
    i32                                     letter;
    i32                                     octave;
    std::variant<AttackGate, TiedOrnament*> note;
    Location                                at;
    std::optional<TieStress>                stress;
    i32                                     overlap_release = 0;
};

// Event times advance through repeats. Stress phase tracks the unwarped
// position in the performed measure. Both positions are in fractional ticks.
struct PlaybackLane {
    f64  time             = 0;
    f64  bar              = 0;
    bool before_first_bar = true;
    bool part_pickup      = false;
    // Soft barlines use the last performed M: and the accumulated beat count.
    Meter             played_meter{.free = true};
    f64               bar_check_origin = 0;
    f64               stress_phase     = 0;
    f64               accent_origin    = 0;
    vec<TieNote>      ties;
    MidiTrackSettings midi;
    Expression        expression{};
    // Re-entry inherits the primary measure phase at the last synchronization.
    f64                reentry_bar = 0;
    std::optional<f64> primary_until;

    // Compensated summation carries fractions smaller than the current f64 tick spacing.
    // Three one-third-tick steps still advance a whole tick after a long rest.
    f64  clock_error = 0;
    void advance(f64 duration) {
        const auto step = duration - clock_error;
        const auto end  = time + step;
        clock_error     = (end - time) - step;
        time            = end;
    }

    void reset_played_bar(f64 origin) { bar = accent_origin = bar_check_origin = origin; }
    void reset_written_bar() { stress_phase = 0; }

    void reenter() {
        part_pickup = true;
        reset_played_bar(reentry_bar);
        reset_written_bar();
    }

    void rebase_accents(Meter previous, Meter next) {
        accent_origin = rebase(accent_origin, previous, next);
    }

    void set_played_meter(Meter next) {
        bar_check_origin = rebase(bar_check_origin, played_meter, next);
        played_meter     = next;
    }

    [[nodiscard]] bool closes_soft_bar() const {
        return !played_meter.free
               && (before_first_bar || part_pickup
                   || time - bar_check_origin + kEpsilon >= played_meter.measure());
    }

private:
    [[nodiscard]] f64 rebase(f64 origin, Meter previous, Meter next) const {
        // Preserve the number of beats counted before a denominator change.
        return time - (time - origin) / previous.beat() * next.beat();
    }
};

void report_ties(vec<TieNote>& ties, Diagnostics& diagnostics);

// Each performed lane owns its clock, ties and MIDI settings for the voice.
// Idle lanes synchronize with the primary branch and deliver bars to backing.
class OverlayLanes : private OverlayNotation {
public:
    OverlayLanes(
        WrittenFields   opening,
        Diagnostics&    diagnostics,
        vec<LanePlan>&& plans,
        Accompaniment&  backing
    ) :
        OverlayNotation(opening, diagnostics), diagnostics(diagnostics), plans(std::move(plans)),
        backing(backing), lanes{new_lane(0, 0, 0)} {}
    using OverlayNotation::id;
    using OverlayNotation::in_implicit_overlay;
    using OverlayNotation::notation;
    PlaybackLane&                               active() { return lanes[id()]; }
    [[nodiscard]] LanePlan&                     plan(size_t index) { return plans[index]; }
    [[nodiscard]] size_t                        plan_count() const { return plans.size(); }
    [[nodiscard]] std::span<const PlaybackLane> clocks() const { return lanes; }
    void                                        control(Overlay command, Location at);
    bool                                        close_implicit();
    void                                        enter_part();
    void                                        finish_voice(Location at);
    void primary_boundary(std::optional<Location> closing_bar = {});
    void break_ties();

private:
    struct PrimaryBoundary {
        f64                     time;
        f64                     phase;
        std::optional<Location> closing_bar;
    };

    struct ExplicitGroup {
        f64                  start_bar;
        vec<PrimaryBoundary> deferred_boundaries;
    };

    Diagnostics&                 diagnostics;
    vec<LanePlan>                plans;
    Accompaniment&               backing;
    vec<PlaybackLane>            lanes;
    f64                          last_primary_boundary = 0;
    std::optional<ExplicitGroup> group;

    [[nodiscard]] PlaybackLane new_lane(size_t index, f64 time, f64 bar) const;
    void                       enter_secondary();
    void                       synchronize_join(size_t last_lane);
    void                       advance_idle_lanes(PrimaryBoundary boundary, size_t first_lane);
};

}   // namespace symusic::abc::detail

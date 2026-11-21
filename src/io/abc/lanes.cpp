#include "lanes.h"

namespace symusic::abc::detail {

bool OverlaySelector::control(Overlay command, Location at) {
    switch (command) {
    case Overlay::Split:
        if (selected == kMaxVoiceLanes - 1)
            diagnostics.error(at, "overlay-limit", "too many overlay lanes");
        ++selected;
        break;
    case Overlay::Start:
    case Overlay::StartAtCursor:
        if (explicit_group || selected != 0)
            diagnostics.error(at, "nested-overlay", "nested overlays have no unambiguous playback");
        explicit_group = true;
        break;
    case Overlay::End:
        if (!explicit_group && selected == 0) {
            diagnostics.warn(at, "unexpected-overlay-end", "ignored '&)' without an overlay");
            return false;
        }
        explicit_group = false;
        selected       = 0;
        break;
    case Overlay::PreviousTwo:
        diagnostics.error(
            at, "unresolved-overlay", "two-bar overlay was not resolved before playback"
        );
    }
    return true;
}

bool OverlaySelector::close_implicit() {
    if (!in_implicit_overlay()) return false;
    selected = 0;
    return true;
}

bool OverlayNotation::control(Overlay command, Location at) {
    const auto previous = id();
    if (!OverlaySelector::control(command, at)) return false;
    if (command == Overlay::Split) {
        if (previous == 0) secondary.fields = primary.fields;
        secondary.accidentals.reset();
    } else if (command == Overlay::End && previous != 0) {
        primary.accidentals.reset();
    }
    return true;
}

void report_ties(vec<TieNote>& ties, Diagnostics& diagnostics) {
    for (const auto& tie : ties)
        diagnostics.warn(tie.at, "unmatched-tie", "tie has no matching next note");
    ties.clear();
}

void OverlayLanes::control(Overlay command, Location at) {
    const auto previous = id();
    if (!OverlayNotation::control(command, at)) return;
    switch (command) {
    case Overlay::Split: enter_secondary(); break;
    case Overlay::StartAtCursor:
        // A two-bar overlay can follow a repeat that left the performed measure open.
        primary_boundary();
        [[fallthrough]];
    case Overlay::Start: group.emplace(ExplicitGroup{.start_bar = active().bar}); break;
    case Overlay::End:
        synchronize_join(previous);
        active().reset_played_bar(active().time);
        active().reset_written_bar();
        break;
    case Overlay::PreviousTwo: break;   // Rejected by the selector.
    }
}

PlaybackLane OverlayLanes::new_lane(size_t index, f64 time, f64 bar) const {
    PlaybackLane lane{.time = time, .reentry_bar = bar};
    if (index < plans.size()) lane.midi = plans[index].note_settings;
    return lane;
}

void OverlayLanes::enter_secondary() {
    const auto selected = id();
    if (selected == lanes.size()) {
        const auto bar    = group ? group->start_bar : lanes.front().bar;
        auto       lane   = new_lane(selected, last_primary_boundary, bar);
        lane.played_meter = lanes.back().played_meter;
        lanes.push_back(std::move(lane));
    }
    active().reenter();
}

void OverlayLanes::synchronize_join(size_t last_lane) {
    if (group) {
        // The primary split acts as a virtual bar. Only lanes absent from this
        // group need its intervening boundaries to restart their backing.
        if (last_lane != 0) {
            for (auto& idle : std::span(lanes).subspan(last_lane + 1))
                if (lanes.front().time > idle.primary_until.value_or(last_primary_boundary))
                    report_ties(idle.ties, diagnostics);
        }
        for (const auto& boundary : group->deferred_boundaries)
            advance_idle_lanes(boundary, last_lane + 1);
        if (last_lane != 0) advance_idle_lanes({lanes.front().time, 0, {}}, last_lane + 1);
    }
    for (auto& lane : std::span(lanes).subspan(1, last_lane)) {
        lane.primary_until = lanes.front().time;
        if (group) lane.reentry_bar = lane.bar;
    }
    group.reset();
}

bool OverlayLanes::close_implicit() {
    const auto previous = id();
    if (!OverlayNotation::close_implicit()) return false;
    synchronize_join(previous);
    return true;
}

void OverlayLanes::enter_part() {
    if (!close_implicit()) return;
    // The melody resumes on a downbeat. Secondary lanes continue their
    // existing measure rather than receiving the melody's reset.
    primary_boundary();
    active().reset_played_bar(active().time);
    active().reset_written_bar();
}

void OverlayLanes::finish_voice(Location at) {
    if (close_implicit() || !group) return;
    diagnostics.warn(at, "unterminated-overlay", "closed overlay at the end of the voice");
    const auto previous = id();
    OverlayNotation::control(Overlay::End, at);
    synchronize_join(previous);
}

void OverlayLanes::primary_boundary(std::optional<Location> closing_bar) {
    const auto            now = lanes.front().time;
    const PrimaryBoundary boundary{now, now - lanes.front().bar, closing_bar};
    // Bars in the first branch must not advance the clocks of branches we have yet to read.
    if (group)
        group->deferred_boundaries.push_back(boundary);
    else
        advance_idle_lanes(boundary, 1);
}

void OverlayLanes::advance_idle_lanes(PrimaryBoundary boundary, size_t first_lane) {
    for (size_t i = first_lane; i < lanes.size(); ++i) {
        auto& lane = lanes[i];
        if (!lane.primary_until && boundary.time > last_primary_boundary)
            report_ties(lane.ties, diagnostics);
        lane.advance(boundary.time - lane.primary_until.value_or(last_primary_boundary));
        lane.reentry_bar = lane.time - boundary.phase;
        lane.primary_until.reset();
        if (boundary.closing_bar) backing.bar(i, lane.time, *boundary.closing_bar);
    }
    last_primary_boundary = boundary.time;
}

void OverlayLanes::break_ties() {
    // A suspended primary lane is outside a secondary lane's repeat jump.
    for (size_t index = id() == 0 ? 0 : 1; index < lanes.size(); ++index)
        report_ties(lanes[index].ties, diagnostics);
}

}   // namespace symusic::abc::detail

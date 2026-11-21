#pragma once

#include <deque>

#include "playback.h"

namespace symusic::abc::detail {

// A spread chord inserts its releases before some already queued releases.
// abc2midi deducts the current gap from the interval to the next release,
// then adds the full spread to releases sent while stepping past that chord.
class ExpandedReleaseSchedule {
public:
    ExpandedReleaseSchedule(const VoicePlan& plan, Diagnostics& diagnostics) :
        enabled(plan.expands_releases), diagnostics(diagnostics) {}

    // All new attacks at an onset occupy one slot, even when a continuation
    // between two chord members emits no attack. Keep their gates contiguous.
    template<class Play>
    void chord(size_t lane, f64 start, f64 duration, Location at, Play play) {
        const auto first = attacks.size();
        const auto end   = enabled ? tick(start + duration, diagnostics, at) : 0;
        play([&](AttackGate gate, i32 delay, i32 gap) {
            if (!enabled) return;
            attacks.push_back({gate, delay, gap});
            staggered |= delay != 0;
        });
        if (first < attacks.size())
            slots.push_back({lane, start, end, first, attacks.size() - first, at});
    }

    void pulse(size_t lane, f64 start, f64 step, Location at, AttackGate gate, i32 gap) {
        if (enabled) chord(lane, start, step, at, [&](auto record) { record(gate, 0, gap); });
    }

    void apply() {
        if (!staggered) return;
        // A tied ornament's slots are recorded when its final duration is known.
        if (!std::ranges::is_sorted(slots, {}, &Slot::start))
            std::ranges::stable_sort(slots, {}, &Slot::start);
        std::map<size_t, Queue> lanes;
        for (const auto& slot : slots) {
            auto& pending = lanes[slot.lane];
            release_before(pending, tick(slot.start, diagnostics, slot.at), 0);
            const auto notes = std::span(attacks).subspan(slot.first, slot.count);
            for (const auto& note : notes) insert(pending, note, slot.at);
            release_before(pending, slot.end, notes.back().delay);
        }
        for (auto& [lane, pending] : lanes)
            release_before(pending, std::numeric_limits<i64>::max(), 0);
    }

private:
    struct Slot {
        size_t   lane;
        f64      start;
        i32      end;
        size_t   first;
        size_t   count;
        Location at;
    };
    struct ScheduledAttack {
        AttackGate gate;
        i32        delay;
        i32        gap;
    };
    struct Release {
        i64        logical;
        AttackGate gate;
    };
    using Queue = std::deque<Release>;

    bool                 enabled;
    Diagnostics&         diagnostics;
    vec<Slot>            slots;
    vec<ScheduledAttack> attacks;
    bool                 staggered = false;

    void insert(Queue& pending, const ScheduledAttack& note, Location at) {
        const i64  at_tick = static_cast<i64>(note.gate.end()) - note.delay;
        const auto next    = std::ranges::upper_bound(pending, at_tick, {}, &Release::logical);
        if (next != pending.end()) {
            const auto shift = std::min<i64>(note.gap, next->logical - at_tick);
            diagnostics.spend(static_cast<size_t>(pending.end() - next), at);
            for (auto later = next; later != pending.end(); ++later) later->logical -= shift;
        }
        pending.insert(next, {at_tick, note.gate});
    }

    static void release_before(Queue& pending, i64 end, i32 delay) {
        // Logical release ticks here include the one-tick MIDI onset offset.
        while (!pending.empty() && pending.front().logical <= end) {
            const auto release = pending.front();
            release.gate.release_at(
                static_cast<i32>(
                    std::clamp<i64>(release.logical + delay, 0, std::numeric_limits<i32>::max())
                )
            );
            pending.pop_front();
        }
    }
};

}   // namespace symusic::abc::detail

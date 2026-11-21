#pragma once

#include <algorithm>
#include <cmath>
#include <ranges>
#include <utility>
#include <variant>

#include "settings.h"
#include "symusic/track.h"

namespace symusic::abc::detail {

struct BackingStep {
    char kind;
    i32  units;
    i32  pitch    = 0;
    i32  velocity = 0;
};

// A guitar or drum pattern on one MIDI track's clock. Slots and note releases
// share a bounded queue. Inserting an earlier event can shorten sounding notes.
class BackingPattern {
public:
    enum class Part { Guitar, Drums };
    explicit BackingPattern(Part part) : part(part) {}

    // Consecutive drum tracks inherit the fitted steps. Each starts a fresh
    // one-bar cycle with the gate closed and its own queue and gap.
    void start_track(f64 time, i32 opening_gap) {
        pending.clear();
        cursor  = 0;
        bars    = 1;
        enabled = part == Part::Guitar;
        gap     = opening_gap;
        slot(time, cursor);
    }

    void install(vec<BackingStep> replacement, Meter meter) {
        steps     = std::move(replacement);
        i32 units = 0;
        for (const auto& step : steps) units += step.units;
        unit_ticks = meter.measure() * bars / units;
        if (part == Part::Drums && cursor >= steps.size()) cursor = 0;
    }

    // A span change restarts the cursor. Only future installs use the new span.
    void set_span(i32 span, f64 time) {
        bars   = span;
        cursor = 0;
        slot(time, cursor);
    }

    void bar(f64 time) {
        if (bars == 1) {
            cursor = 0;
            // abc2midi requests both a restart and a downbeat on one-bar drum patterns.
            if (part == Part::Drums) slot(time, cursor);
        }
        slot(time, cursor);
    }

    // The track stays in place until finish(). Its notes can reallocate between attacks.
    void release_last(f64 time, TrackNative<Tick>& track) {
        enqueue({time, Release{&track, track.notes.size() - 1}});
    }

    template<class Play>
    void advance(f64 end, Location at, Diagnostics& diagnostics, Play play) {
        while (!pending.empty() && pending.front().time < end - kTickTolerance) {
            const auto item = pending.front();
            pending.erase(pending.begin());
            diagnostics.spend(1, at);
            if (const auto* release = std::get_if<Release>(&item.action)) {
                release->stop(tick(item.time + PlaybackGrid::kAttackOffset, diagnostics, at));
                continue;
            }
            if (cursor >= steps.size()) cursor = 0;
            if (steps.empty() || std::get<size_t>(item.action) != cursor) continue;
            const auto& step   = steps[cursor];
            const auto  length = std::floor(unit_ticks * step.units + kTickTolerance);
            if (enabled && step.kind != 'z') play(step, item.time, length, at);
            slot(item.time + length, ++cursor);
            if (cursor == steps.size()) cursor = 0;
        }
    }

    void finish(i32 stop) {
        for (const auto& item : pending)
            if (const auto* release = std::get_if<Release>(&item.action)) release->stop(stop);
        pending.clear();
    }

    i32  gap     = ChordAttackSettings::kDefaultGap;
    bool enabled = true;

private:
    struct Release {
        TrackNative<Tick>* track;
        size_t             index;

        void stop(i32 tick) const {
            auto& note    = track->notes[index];
            note.duration = std::max(0, tick - note.time);
        }
    };
    struct Pending {
        f64                           time;
        std::variant<size_t, Release> action;
    };

    static constexpr size_t kMaxPending = 50;
    Part                    part;
    vec<BackingStep>        steps;
    f64                     unit_ticks = 0;
    i32                     bars       = 1;
    size_t                  cursor     = 0;
    vec<Pending>            pending;

    void slot(f64 time, size_t index) {
        if (part == Part::Drums && steps.size() <= 1) {
            // A duplicate downbeat would strike twice in a one-slot pattern.
            const auto same_time = std::ranges::equal_range(pending, time, {}, &Pending::time);
            if (std::ranges::any_of(same_time, [index](const Pending& entry) {
                    const auto* slot = std::get_if<size_t>(&entry.action);
                    return slot && *slot == index;
                }))
                return;
        }
        enqueue({time, index});
    }

    void enqueue(Pending item) {
        if (pending.size() >= kMaxPending) return;
        const auto after = std::ranges::upper_bound(pending, item.time, {}, &Pending::time);
        if (after != pending.end()) {
            const auto pull = std::min<f64>(after->time - item.time, gap);
            for (auto& entry : std::ranges::subrange(after, pending.end())) entry.time -= pull;
        }
        pending.insert(after, item);
    }
};

}   // namespace symusic::abc::detail

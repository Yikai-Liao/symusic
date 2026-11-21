#pragma once

#include <algorithm>
#include <array>
#include <bitset>
#include <span>
#include <tuple>
#include <utility>

#include "settings.h"
#include "write_timeline.h"

namespace symusic::abc::detail {

// abc2midi accepts 30 voices. Channel 10 is shared by drums. Leave half
// of the 50-slot MIDI release queue available for the next chord's attacks.
constexpr size_t kMaxExportVoices = 30;
constexpr size_t kMelodicChannels = kMidiChannels - 1;
constexpr size_t kMaxActiveNotes  = 25;
constexpr size_t kRetryLimit      = 100'000;

struct VoiceLimit {
    size_t      count;
    const char* name;
    size_t&     retries_left;   // All onsets across the score share this allowance.

    [[noreturn]] void unplaceable(const ExportLocation& at, i32 tick) const {
        at.invalid(
            fmt::format(
                "[voice-placement] cannot place attacks at playback tick {} ({} TPQ) in {} "
                "available "
                "voices (limit: {}); earlier onsets are not reassigned. Use MIDI export for this "
                "score",
                tick,
                kTpq,
                count,
                name
            )
        );
    }

    void try_candidate(const ExportLocation& at, i32 tick) const {
        if (!retries_left)
            at.invalid(
                fmt::format(
                    "[voice-search-limit] stopped searching at playback tick {} ({} TPQ) with {} "
                    "available voices after {} retry candidates across the score; use MIDI export "
                    "for "
                    "this score",
                    tick,
                    kTpq,
                    count,
                    kRetryLimit
                )
            );
        --retries_left;
    }
};

struct VoiceAllocation {
    size_t total        = 0;
    size_t melodic      = 0;
    size_t retries_left = kRetryLimit;

    VoiceLimit available(bool drums) {
        const auto voices   = kMaxExportVoices - total;
        const auto channels = kMelodicChannels - melodic;
        if (drums || voices <= channels) return {voices, "30 ABC voices", retries_left};
        return {channels, "15 melodic ABC voices", retries_left};
    }
};

using PitchSet = std::bitset<kMidiMax + 1>;

struct OnsetSlot {
    PitchSet occupied;       // Sustains from earlier onsets plus this onset's assigned attacks.
    i8       velocity = 0;   // Audible attacks always have positive velocity.

    [[nodiscard]] bool accepts(const WrittenNote& n) const {
        return !occupied[n.pitch] && occupied.count() < kMaxActiveNotes
               && (velocity == 0 || velocity == n.velocity);
    }
    void add(const WrittenNote& n) {
        occupied.set(n.pitch);
        velocity = n.velocity;
    }
};

// Try the earliest compatible lane, preferring one already taking this velocity.
inline bool greedy_onset(
    std::span<const WrittenNote> chord, std::span<OnsetSlot> slots, std::span<size_t> choices
) {
    for (size_t n = 0; n < chord.size(); ++n) {
        size_t best = slots.size();
        for (size_t i = 0; i < slots.size(); ++i) {
            if (!slots[i].accepts(chord[n])) continue;
            if (best == slots.size()) best = i;
            if (slots[i].velocity == chord[n].velocity) {
                best = i;
                break;
            }
        }
        if (best == slots.size()) return false;
        choices[n] = best;
        slots[best].add(chord[n]);
    }
    return true;
}

inline bool retry_onset(
    std::span<const WrittenNote> chord,
    std::span<OnsetSlot>         slots,
    std::span<size_t>            choices,
    const VoiceLimit&            limit,
    const ExportLocation&        at
) {
    if (chord.empty()) return true;
    const auto& note       = chord.front();
    bool        tried_idle = false;
    for (size_t i = 0; i < slots.size(); ++i) {
        auto& slot = slots[i];
        // Completely idle lanes are interchangeable until the next onset.
        if (slot.occupied.none() && std::exchange(tried_idle, true)) continue;
        limit.try_candidate(at, note.start + PlaybackGrid::kAttackOffset);
        if (!slot.accepts(note)) continue;
        const auto before = slot;
        slot.add(note);
        choices.front() = i;
        if (retry_onset(chord.subspan(1), slots, choices.subspan(1), limit, at)) return true;
        slot = before;
    }
    return false;
}

// Assign an entire onset before committing any notes. Earlier onsets stay fixed.
inline vec<Timeline> partition_lanes(
    vec<WrittenNote> notes, const VoiceLimit& limit, const ExportLocation& at
) {
    std::ranges::sort(notes, {}, &WrittenNote::start);
    vec<Timeline> lanes(1);   // A channel-only track can also use this first lane.
    std::array<vec<WrittenNote>, kMaxExportVoices>         held;
    std::array<size_t, kMaxActiveNotes * kMaxExportVoices> choices;
    auto                                                   remaining = std::span(notes);
    while (!remaining.empty()) {
        const auto start = remaining.front().start;
        const auto tick  = start + PlaybackGrid::kAttackOffset;
        const auto next
            = std::ranges::find_if(remaining, [start](const auto& n) { return n.start != start; });
        std::span chord(remaining.begin(), next);
        if (chord.size() > kMaxActiveNotes * limit.count) limit.unplaceable(at, tick);

        std::array<OnsetSlot, kMaxExportVoices> slots{};
        std::array<i32, kMidiMax + 1>           pressure{};
        for (size_t i = 0; i < lanes.size(); ++i) {
            std::erase_if(held[i], [start](const auto& n) { return n.end <= start; });
            for (const auto& n : held[i]) {
                slots[i].occupied.set(n.pitch);
                ++pressure[n.pitch];
            }
        }
        if (chord.size() > 1) {
            PitchSet velocities;
            for (const auto& n : chord) {
                velocities.set(n.velocity);
                ++pressure[n.pitch];
            }
            if (velocities.count() > limit.count) limit.unplaceable(at, tick);
            // Give pitches already held in other lanes and simultaneous copies first choice.
            std::ranges::sort(chord, {}, [&](const auto& n) {
                return std::tuple{-pressure[n.pitch], n.pitch, n.velocity, n.end};
            });
        }
        auto trial = slots;
        if (!greedy_onset(chord, std::span(trial).first(limit.count), choices)
            && !retry_onset(chord, std::span(slots).first(limit.count), choices, limit, at))
            limit.unplaceable(at, tick);
        for (size_t n = 0; n < chord.size(); ++n) {
            const auto lane = choices[n];
            if (lane == lanes.size()) lanes.emplace_back();
            held[lane].push_back(chord[n]);
            lanes[lane][start].attacks.push_back(chord[n]);
            ++lanes[lane][chord[n].end].releases;
        }
        remaining = remaining.subspan(chord.size());
    }
    return lanes;
}

}   // namespace symusic::abc::detail

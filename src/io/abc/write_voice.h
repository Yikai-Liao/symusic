#pragma once

#include <algorithm>
#include <cmath>
#include <limits>
#include <numeric>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>

#include <fmt/format.h>

#include "settings.h"
#include "write_timeline.h"

namespace symusic::abc::detail {

// At L:1/4 a unit is 960 half ticks, or 128 inside a one-note 15:2 tuplet.
constexpr std::string_view kTickTuplet      = "(15:2:1";
constexpr i32              kTupletHalfTicks = 128;

inline std::string length_suffix(i32 half_ticks, i32 units) {
    const auto  divisor = std::gcd(half_ticks, units);
    const auto  num     = half_ticks / divisor;
    const auto  den     = units / divisor;
    std::string suffix  = num == 1 ? "" : std::to_string(num);
    if (den != 1) {
        suffix += '/';
        if (den != 2) suffix += std::to_string(den);
    }
    return suffix;
}

inline std::string pitch(i8 note) {
    constexpr std::string_view kChromatic[]
        = {"=C", "^C", "=D", "^D", "=E", "=F", "^F", "=G", "^G", "=A", "^A", "=B"};
    std::string result(kChromatic[note % 12]);
    i32         octave = note / 12 - 5;
    if (octave > 0) {
        result.back() = static_cast<char>(result.back() - 'A' + 'a');
        --octave;
    }
    result.append(static_cast<size_t>(std::abs(octave)), octave < 0 ? ',' : '\'');
    return result;
}

// A music line and its following w:/+: lines wrap independently. Each sounding
// atom claims one lyric slot, including continuations of tied notes.
struct VoiceLines {
    std::string&            output;
    ExportBudget&           budget;
    const ExportLocation&   at;
    bool                    has_lyrics;
    size_t                  columns = 0;
    vec<std::string>        words;
    static constexpr size_t kLineWidth = 76;

    void finish_line() {
        output += '\n';
        columns = 0;
        if (words.empty()) return;
        // An empty w: claims an unsung music line. Trailing skips are implicit.
        while (!words.empty() && words.back().empty()) words.pop_back();
        std::string verse = "w:";
        const auto  flush = [&] {
            budget.spend(1 + 2 * PlaybackWork::text(verse), at);
            output += verse;
            output += '\n';
        };
        for (const auto& word : words) {
            const std::string_view syllable = word.empty() ? std::string_view("*") : word;
            if (verse.size() > 2 && verse.size() + 1 + syllable.size() > kLineWidth) {
                flush();
                verse = "+:";
            }
            verse += ' ';
            verse += syllable;
        }
        flush();
        words.clear();
    }

    void token(std::string_view text, size_t work = PlaybackWork::kToken) {
        budget.spend(work + PlaybackWork::text(text), at);
        if (columns > kLineWidth) finish_line();
        output += text;
        output += ' ';
        columns += text.size() + 1;
    }

    void fields(const vec<std::string>* changes) {
        if (changes)
            for (const auto& field : *changes) token(field);
    }

    void lyric_slot(const Moment* onset, i32 tick) {
        if (!has_lyrics) return;
        auto& word = words.emplace_back();
        if (!onset || !onset->lyric) return;
        budget.spend(PlaybackWork::text(*onset->lyric), at);
        word = encode_text(*onset->lyric, TextContext::Lyric);
        // abc2midi's 199-byte payload includes its karaoke marker and inter-word space.
        if (word.size() > 197)
            at.warn(
                "midi-lyric-width",
                "abc2midi may put part of the lyric at tick {} on later attacks",
                tick
            );
        if (onset->attacks.empty())
            at.warn(
                "midi-tied-lyric",
                "abc2midi may move the lyric at tick {} to the start of its tie and shorten the "
                "held note",
                tick
            );
    }
};

struct ExportChordMember {
    i8   pitch;
    i32  half_ticks;
    bool tied;
};

inline void write_atom(
    VoiceLines&                        lines,
    std::span<const ExportChordMember> members,
    i32                                step,
    i32                                start,
    const Moment*                      onset
) {
    const auto mixed
        = std::ranges::any_of(members, [step](const auto& m) { return m.half_ticks != step; });
    const auto  tuplet = mixed || step % 15 != 0;
    const auto  units  = tuplet ? kTupletHalfTicks : 2 * kTpq;
    std::string text(tuplet ? kTickTuplet : "");
    const auto  steps = PlaybackWork::kToken * (tuplet ? 2 : 1);
    if (members.empty()) {
        lines.token(text + 'z' + length_suffix(step, units), steps);
        return;
    }
    if (members.size() > 1) text += '[';
    for (const auto& member : members) {
        text += pitch(member.pitch);
        if (members.size() == 1 || mixed) text += length_suffix(member.half_ticks, units);
        if (member.tied) text += '-';
    }
    if (members.size() > 1) {
        text += ']';
        if (!mixed) text += length_suffix(step, units);
    }
    lines.token(text, steps + PlaybackWork::written_chord(members.size()));
    lines.lyric_slot(onset, start + PlaybackGrid::kAttackOffset);
}

// Native ties release at floor(written end). abc2midi schedules the release
// at the attack plus floor(total tied length) - 1. Start the voice's ties on
// half ticks and carry each release half a tick into the following span. For
// a note of D ticks starting at written S + 1/2, that gives end S + D + 1
// and length D + 1/2. Attacks, fields and lyrics still floor to their own ticks.
struct VoiceRhythm {
    VoiceLines&            lines;
    bool                   drums;
    bool                   on_half_ticks = false;
    vec<i8>                releases;
    vec<ExportChordMember> members;

    void atom(std::span<const WrittenNote> chord, i32 start, i32 end, const Moment* onset) {
        // write_interval bounds atom lengths before conversion to half ticks.
        const auto span      = 2 * (end - start);
        const auto needs_tie = [&](const WrittenNote& note) {
            return drums || note.start < start || note.end > end;
        };
        if (!on_half_ticks && std::ranges::any_of(chord, needs_tie)) {
            write_atom(lines, {}, 1, start, nullptr);
            on_half_ticks = true;
        }
        const bool has_releases = !releases.empty();
        if (has_releases) {
            members.clear();
            // The retiring pitches come first so a unison reattack follows
            // its continuation. The first member sets the half-tick step.
            for (const auto p : releases) members.push_back({p, 1, false});
            for (const auto& note : chord) {
                const bool tied = needs_tie(note);
                members.push_back({note.pitch, tied ? 1 : span, tied});
            }
            write_atom(lines, members, 1, start, std::exchange(onset, nullptr));
        }
        releases.clear();
        members.clear();
        const auto step = span - (has_releases ? 1 : 0);
        for (const auto& note : chord) {
            const bool tied = needs_tie(note);
            // Untied members already sounded for their full span in the release chord.
            if (!has_releases || tied) members.push_back({note.pitch, step, tied});
            if (tied && note.end == end) releases.push_back(note.pitch);
        }
        write_atom(lines, members, step, start, onset);
    }
};

inline void warn_tied_release(const ExportLocation& at, const WrittenNote& note) {
    at.warn(
        "midi-tied-release",
        "abc2midi may release a tied pitch {} one tick before playback tick {} "
        "({} TPQ). Use MIDI export for exact releases",
        note.pitch,
        note.end + PlaybackGrid::kAttackOffset,
        kTpq
    );
}

// abc2midi drops the release extension on a tie. Tie an otherwise untied note
// into its last tick to make its release precede a same-pitch melodic reattack.
inline bool tie_reattack(
    std::span<const WrittenNote> chord,
    i32                          start,
    i32                          end,
    const Moment&                next,
    const ExportLocation&        at
) {
    bool              repeats = false;
    std::optional<i8> other_release;
    for (const auto& note : chord) {
        if (note.start != start || note.end != end) continue;
        if (std::ranges::find(next.attacks, note.pitch, &WrittenNote::pitch) != next.attacks.end())
            repeats = true;
        else
            other_release = note.pitch;
    }
    if (!repeats) return false;
    if (end - start == 1) {
        at.warn(
            "midi-short-reattack",
            "abc2midi may cut off a same-pitch reattack after a one-tick note; use MIDI export to "
            "preserve the release order"
        );
        return false;
    }
    if (other_release)
        at.warn(
            "midi-chord-release",
            "the reattack planned at playback tick {} ({} TPQ) uses a tie that can shorten "
            "chord pitch {} by one tick in abc2midi; use MIDI export for exact releases",
            end + PlaybackGrid::kAttackOffset,
            kTpq,
            *other_release
        );
    return true;
}

inline void write_interval(
    VoiceRhythm&                 rhythm,
    std::span<const WrittenNote> chord,
    i32                          start,
    i32                          end,
    const Moment*                onset,
    const Moment*                next
) {
    // Bound numerators before abc2midi multiplies by the tuplet ratio.
    constexpr i32 kMaxAtomTicks = kTpq * 1024;
    auto          stop          = end;
    if (end - start > kMaxAtomTicks) {
        if (!chord.empty()) warn_tied_release(rhythm.lines.at, chord.front());
    } else if (
        !rhythm.drums && next && !next->attacks.empty()
        && tie_reattack(chord, start, end, *next, rhythm.lines.at)
    ) {
        --stop;
    }
    while (start < stop) {
        const auto next_tick = start + std::min(stop - start, kMaxAtomTicks);
        rhythm.atom(chord, start, next_tick, std::exchange(onset, nullptr));
        start = next_tick;
    }
    if (stop < end) rhythm.atom(chord, stop, end, nullptr);
}

// All event ticks leave room for the release extension below INT32_MAX.
constexpr i32 kNoEvent = std::numeric_limits<i32>::max();

template<class Schedule>
struct EventCursor {
    typename Schedule::const_iterator next, end;

    [[nodiscard]] i32 tick() const { return next == end ? kNoEvent : next->first; }
    const typename Schedule::mapped_type* take(i32 time) {
        return tick() == time ? &(next++)->second : nullptr;
    }
};

inline void write_declaration(VoiceLines& lines, i32 id, const PlannedTrack& track) {
    auto declaration = fmt::format("V:{}", id);
    if (!track.name.empty())
        declaration += fmt::format(" name=\"{}\"", encode_text(track.name, TextContext::VoiceName));
    lines.budget.spend(PlaybackWork::kToken + PlaybackWork::text(declaration), track.at);
    lines.output += declaration;
    lines.output += '\n';
    lines.token("[I:MIDI gchordoff]");
    if (track.drums) lines.token(fmt::format("[I:MIDI channel {}]", kMidiDrumChannel));
    lines.token(fmt::format("[I:MIDI program {}]", track.program));
    lines.token("[I:MIDI chordattack 0]");
    lines.token(fmt::format("[I:MIDI expand {}/{}]", PlaybackGrid::kExportReleaseExtension, kTpq));
}

inline void write_voice(
    std::string&        output,
    ExportBudget&       budget,
    i32                 id,
    const PlannedTrack& track,
    const Timeline&     lane,
    const Fields&       global
) {
    const auto has_lyrics
        = std::ranges::any_of(lane, [](const auto& entry) { return entry.second.lyric; });
    VoiceLines lines{output, budget, track.at, has_lyrics, 0, {}};
    write_declaration(lines, id, track);
    VoiceRhythm rhythm{lines, track.drums, false, {}, {}};

    // ABC 2.1 section 7.3 repeats fields in each affected voice. The first
    // voice carries all changes, including those after its own music ends.
    const auto last_tick
        = [](const auto& events) { return events.empty() ? 0 : events.rbegin()->first; };
    const auto            end = std::max(last_tick(lane), last_tick(track.channel));
    EventCursor<Fields>   fields{global.begin(), id == 1 ? global.end() : global.lower_bound(end)};
    EventCursor<Fields>   channel{track.channel.begin(), track.channel.end()};
    EventCursor<Timeline> notes{lane.begin(), lane.end()};
    vec<WrittenNote>      chord;   // Sorted by pitch, including notes tied across the current span.
    i8                    velocity = 0;
    const Moment*         onset    = nullptr;
    i32                   start    = 0;
    for (;;) {
        const auto time = std::min({fields.tick(), channel.tick(), notes.tick()});
        if (time == kNoEvent) break;
        const auto* music   = fields.take(time);
        const auto* effects = channel.take(time);
        const auto* change  = notes.take(time);
        auto held = std::ranges::find_if(chord, [time](const auto& n) { return n.end > time; });
        if (held != chord.end()) {
            if (music && !effects && !change)
                track.at.warn(
                    "midi-field-release",
                    "the music field at tick {} splits a held note with a tie that abc2midi may "
                    "shorten. Use MIDI export for exact releases",
                    time
                );
            else if (!change || !change->attacks.empty() || change->releases)
                warn_tied_release(track.at, *held);   // A lyric-only split has its own warning.
        }
        write_interval(rhythm, chord, start, time, onset, change);
        start = time;
        onset = change;
        lines.fields(music);
        lines.fields(effects);
        if (!change) continue;
        if (change->releases) std::erase_if(chord, [time](const auto& n) { return n.end == time; });
        if (change->attacks.empty()) continue;
        if (velocity != change->attacks.front().velocity) {
            velocity = change->attacks.front().velocity;
            lines.token(fmt::format("[I:MIDI beat {0} {0} {0} 1]", velocity));
        }
        chord.insert(chord.end(), change->attacks.begin(), change->attacks.end());
        std::ranges::sort(chord, {}, &WrittenNote::pitch);
    }
    if (!lane.empty() && last_tick(lane) == start)
        rhythm.atom({}, start, start + PlaybackGrid::kExportTail, nullptr);
    lines.token("|]");
    lines.finish_line();
}

}   // namespace symusic::abc::detail

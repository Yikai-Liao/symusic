#include "symusic/io/abc.h"

#include <algorithm>
#include <array>
#include <bit>
#include <bitset>
#include <cmath>
#include <limits>
#include <map>
#include <numeric>
#include <optional>
#include <ranges>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <utility>
#include <variant>

#include <fmt/format.h>
#include <utf8.h>

#include "settings.h"
#include "../note_tuning.h"
#include "symusic/detail/time_conversion.h"
#include "symusic/utils.h"

namespace symusic::abc {
namespace {

using detail::encode_text;
using detail::midi_max;
using detail::PlaybackGrid;
using detail::PlaybackWork;
using detail::TextContext;
using detail::tpq;
using detail::whole;

constexpr i32 kMidiTempoMax = (1U << 24) - 1U;
constexpr i32 kMidiDeltaMax = (1U << 28) - 1U;
// abc2midi accepts 30 voice declarations. Melodic voices use distinct MIDI
// channels; percussion voices explicitly select channel 10.
constexpr size_t kMaxExportVoices = 30;
constexpr size_t kMelodicChannels = detail::midi_channels - 1;
// Leave room in the 50-slot MIDI release queue for the next chord's attacks.
constexpr int kMaxActiveNotes = 25;

// A write either returns complete text with recoverable diagnostics or throws.
// Keep the first diagnostic for each score/track label and code; the caller
// chooses whether to report these losses before writing the text to a file.
class ExportIssues {
public:
    [[noreturn]] static void invalid(std::string_view where, std::string_view message) {
        throw std::invalid_argument(fmt::format("ABC export {}: {}", where, message));
    }
    template<class... Args>
    void warn(
        std::string_view            where,
        std::string_view            code,
        fmt::format_string<Args...> message,
        Args&&... args
    ) {
        auto label             = fmt::format("ABC export {} [{}]", where, code);
        auto [found, inserted] = warnings.try_emplace(std::move(label));
        if (inserted) found->second = fmt::format(message, std::forward<Args>(args)...);
    }
    vec<std::string> take() const {
        vec<std::string> result;
        for (const auto& [label, message] : warnings)
            result.push_back(fmt::format("{}: {}", label, message));
        return result;
    }

private:
    // Report the first example of each problem per location, not one warning per note.
    std::map<std::string, std::string> warnings;
};

// L:1/4 gives 480 ticks per unit. Binary lengths cover multiples of 15;
// an explicit one-note 15:2 tuplet supplies the other integer tick lengths.
struct NoteLength {
    std::string prefix;
    std::string suffix;
};

NoteLength length(i32 duration) {
    const auto  tuplet  = duration % 15 != 0;
    const auto  units   = tuplet ? 64 : tpq;
    const auto  divisor = std::gcd(duration, units);
    const auto  num     = duration / divisor;
    const auto  den     = units / divisor;
    std::string suffix  = num == 1 ? "" : std::to_string(num);
    if (den != 1) {
        suffix += '/';
        if (den != 2) suffix += std::to_string(den);
    }
    return {tuplet ? "(15:2:1" : "", std::move(suffix)};
}

std::string pitch(i8 note) {
    constexpr std::string_view kChromatic[]
        = {"=C", "^C", "=D", "^D", "=E", "=F", "^F", "=G", "^G", "=A", "^A", "=B"};
    std::string result(kChromatic[note % 12]);
    int         octave = note / 12 - 5;
    if (octave > 0) {
        result.back() = static_cast<char>(result.back() - 'A' + 'a');
        --octave;
    }
    result.append(static_cast<size_t>(std::abs(octave)), octave < 0 ? ',' : '\'');
    return result;
}

void check_tempo(i32 mspq) {
    if (mspq < 1 || mspq > kMidiTempoMax)
        ExportIssues::invalid("tempo", "microseconds per quarter must fit the 24-bit MIDI range");
}

std::string tempo_text(i32 mspq) {
    check_tempo(mspq);
    // ABC 2.1 section 3.1.8 permits fractional beats. Try a quarter first,
    // then halve the beat until the integer interval rate/(mspq+1) < bpm <= rate/mspq
    // contains a BPM. At denominator 2^25 the interval is wider than one
    // for every 24-bit tempo, and the first match fits abc2midi's BPM * 4.
    i64 rate = 60'000'000;
    for (int denominator = 4;; denominator *= 2, rate *= 2) {
        const auto bpm = rate / mspq;
        if (rate / bpm == mspq) return fmt::format("1/{}={}", denominator, bpm);
    }
}

struct WrittenNote {
    i32 start;
    i32 end;
    i8  pitch;
    i8  velocity;
};

// One lane's changes at one written tick, before the reader adds its attack
// offset. Releases remove held pitches; attacks supply their replacements.
struct Moment {
    vec<WrittenNote> attacks;
    vec<int>         releases;
    // The input score owns lyric text throughout planning and rendering.
    const std::string* lyric = nullptr;
};

using Timeline   = std::map<i32, Moment>;
using Fields     = std::map<i32, vec<std::string>>;
using FieldRange = std::ranges::subrange<Fields::const_iterator>;
using Lyrics     = std::multimap<i32, const std::string*>;

struct VoiceLane {
    Timeline                      timeline;
    std::array<i32, midi_max + 1> ends{};

    bool holds(int pitch, i32 time) const { return ends[pitch] > time; }
    int  room_at(i32 time) const {
        return kMaxActiveNotes - static_cast<int>(std::ranges::count_if(ends, [time](i32 end) {
                   return end > time;
               }));
    }
    void add(WrittenNote note) {
        ends[note.pitch] = note.end;
        timeline[note.start].attacks.push_back(note);
        timeline[note.end].releases.push_back(note.pitch);
    }
};

// At an onset, give scarce pitches first choice of lane. This is greedy,
// not a proof of the minimum voice count.
void prioritize_attacks(std::span<WrittenNote> chord, const vec<VoiceLane>& lanes) {
    if (chord.size() < 2) return;
    std::array<int, midi_max + 1> pressure{};
    for (const auto& note : chord) {
        auto& rank = pressure[note.pitch];
        if (rank == 0)
            rank = std::ranges::count_if(lanes, [&](const VoiceLane& lane) {
                return lane.holds(note.pitch, note.start);
            });
        ++rank;   // simultaneous copies also compete for lanes
    }
    std::ranges::sort(chord, {}, [&](const WrittenNote& note) {
        return std::tuple{-pressure[note.pitch], note.pitch, note.velocity, note.end};
    });
}

struct VoiceLimit {
    size_t           available;
    std::string_view maximum;

    [[noreturn]] void fail(std::string_view where) const {
        ExportIssues::invalid(where, fmt::format("export requires more than {}", maximum));
    }

    [[noreturn]] void fail_onset(std::string_view where, i32 tick) const {
        ExportIssues::invalid(
            where,
            fmt::format(
                "[voice-placement] cannot place attacks at playback tick {} ({} TPQ) in {} "
                "available voices "
                "(limit: {}); earlier onsets are not reassigned. Use MIDI export for this score",
                tick,
                tpq,
                available,
                maximum
            )
        );
    }
};

class PlacementBudget {
public:
    bool try_candidate() {
        if (!remaining) return false;
        --remaining;
        return true;
    }

    [[noreturn]] static void exhausted(std::string_view where, i32 tick, size_t available) {
        ExportIssues::invalid(
            where,
            fmt::format(
                "[voice-search-limit] stopped searching at playback tick {} ({} TPQ) with {} "
                "available voices "
                "after {} retry candidates across the score; use MIDI export for this score",
                tick,
                tpq,
                available,
                limit
            )
        );
    }

private:
    // All onsets share this budget, so a file cannot trigger a large retry at every chord.
    static constexpr size_t limit     = 100'000;
    size_t                  remaining = limit;
};

// Allocate all attacks of one onset before touching the timeline. Tentative
// lanes share one acceptance rule for greedy placement and bounded retry.
class OnsetPlacement {
public:
    OnsetPlacement(
        vec<VoiceLane>& lanes, VoiceLimit limit, PlacementBudget& budget, std::string_view where
    ) : lanes(lanes), limit(limit), budget(budget), where(where) {}

    void add(std::span<WrittenNote> attacks);

private:
    struct Candidate {
        const VoiceLane*          held = nullptr;
        std::bitset<midi_max + 1> pitches;
        int                       room     = kMaxActiveNotes;
        int                       velocity = -1;

        bool accepts(WrittenNote note) const;
        void add(WrittenNote note);
        void undo(WrittenNote note);
    };

    vec<VoiceLane>&                                        lanes;
    VoiceLimit                                             limit;
    PlacementBudget&                                       budget;
    std::string_view                                       where;
    std::span<const WrittenNote>                           attacks;
    std::array<Candidate, kMaxExportVoices>                candidates;
    std::array<size_t, kMaxExportVoices * kMaxActiveNotes> choice;

    void   reset(size_t count);
    bool   greedy();
    size_t first_fit(WrittenNote note, size_t used) const;
    bool   retry(size_t next);
};

bool OnsetPlacement::Candidate::accepts(WrittenNote note) const {
    return room > 0 && (velocity < 0 || velocity == note.velocity) && !pitches[note.pitch]
           && (!held || !held->holds(note.pitch, note.start));
}

void OnsetPlacement::Candidate::add(WrittenNote note) {
    pitches.set(note.pitch);
    --room;
    velocity = note.velocity;
}

void OnsetPlacement::Candidate::undo(WrittenNote note) {
    pitches.reset(note.pitch);
    ++room;
    if (pitches.none()) velocity = -1;
}

void OnsetPlacement::reset(size_t count) {
    const auto onset = attacks.front().start;
    for (size_t i = 0; i < lanes.size(); ++i)
        candidates[i] = {&lanes[i], {}, lanes[i].room_at(onset)};
    std::fill(candidates.begin() + lanes.size(), candidates.begin() + count, Candidate{});
}

// Prefer a lane already committed to this velocity over an unclaimed lane.
size_t OnsetPlacement::first_fit(WrittenNote note, size_t used) const {
    size_t spare = used;
    for (size_t i = 0; i < used; ++i) {
        const auto& lane = candidates[i];
        if (!lane.accepts(note)) continue;
        if (lane.velocity == note.velocity) return i;
        spare = std::min(spare, i);
    }
    return spare;
}

bool OnsetPlacement::greedy() {
    size_t used = lanes.size();
    reset(used);
    for (size_t i = 0; i < attacks.size(); ++i) {
        const auto lane = first_fit(attacks[i], used);
        if (lane == limit.available) return false;
        if (lane == used) candidates[used++] = {};
        candidates[lane].add(attacks[i]);
        choice[i] = lane;
    }
    return true;
}

bool OnsetPlacement::retry(size_t next) {
    if (next == attacks.size()) return true;
    const auto note       = attacks[next];
    bool       tried_idle = false;
    for (size_t i = 0; i < limit.available; ++i) {
        // Completely idle lanes are interchangeable for one onset.
        auto& lane = candidates[i];
        if (lane.room == kMaxActiveNotes && std::exchange(tried_idle, true)) continue;
        if (!budget.try_candidate())
            PlacementBudget::exhausted(
                where, note.start + PlaybackGrid::attack_offset, limit.available
            );
        if (!lane.accepts(note)) continue;
        lane.add(note);
        choice[next] = i;
        if (retry(next + 1)) return true;
        lane.undo(note);
    }
    return false;
}

void OnsetPlacement::add(std::span<WrittenNote> chord) {
    const auto tick = chord.front().start + PlaybackGrid::attack_offset;
    if (chord.size() > limit.available * kMaxActiveNotes) limit.fail_onset(where, tick);
    prioritize_attacks(chord, lanes);
    attacks = chord;
    if (!greedy()) {
        std::bitset<midi_max + 1> velocities;
        for (const auto& note : attacks) velocities.set(note.velocity);
        if (velocities.count() > limit.available) limit.fail_onset(where, tick);
        reset(limit.available);
        if (!retry(0)) limit.fail_onset(where, tick);
    }
    for (size_t i = 0; i < attacks.size(); ++i) {
        const auto lane = choice[i];
        if (lane >= lanes.size()) lanes.resize(lane + 1);
        lanes[lane].add(attacks[i]);
    }
}

vec<VoiceLane> partition(
    vec<WrittenNote> notes, VoiceLimit limit, PlacementBudget& budget, std::string_view where
) {
    std::ranges::sort(notes, {}, &WrittenNote::start);
    vec<VoiceLane> lanes;
    OnsetPlacement placement(lanes, limit, budget, where);
    auto           remaining = std::span(notes);
    while (!remaining.empty()) {
        const auto next  = std::ranges::find_if(remaining, [&](WrittenNote note) {
            return note.start != remaining.front().start;
        });
        const auto onset = remaining.first(static_cast<size_t>(next - remaining.begin()));
        placement.add(onset);
        remaining = remaining.subspan(onset.size());
    }
    if (lanes.empty()) lanes.emplace_back();   // a control-only track still needs a voice
    return lanes;
}

class ExportBudget {
public:
    void spend_repeated(size_t work, size_t copies, std::string_view where) {
        if (copies && work > remaining / copies) exhausted(where);
        spend(work * copies, where);
    }

    void spend(size_t work, std::string_view where) {
        if (work > remaining) exhausted(where);
        remaining -= work;
    }

private:
    size_t                   remaining = PlaybackWork::limit;
    [[noreturn]] static void exhausted(std::string_view where) {
        ExportIssues::invalid(
            where, "[expansion-limit] output exceeds the ABC playback work limit"
        );
    }
};

// Render an already allocated lane. The active pitches are the notes held
// across its current span; emit their tied continuations before advancing to
// the next event boundary. Lyrics count emitted slots, not input notes.
class VoiceWriter {
public:
    VoiceWriter(
        std::string&     output,
        ExportBudget&    budget,
        ExportIssues&    issues,
        std::string_view where,
        bool             drums
    ) : output(output), budget(budget), issues(issues), where(where), drums(drums) {}
    void write(
        int              id,
        std::string_view name,
        int              program,
        const Timeline&  notes,
        const Fields&    channel,
        FieldRange       global,
        i32              explicit_end = 0
    );

private:
    std::string&                        output;
    ExportBudget&                       budget;
    ExportIssues&                       issues;
    std::string_view                    where;
    const bool                          drums;
    std::map<int, WrittenNote>          active;
    int                                 velocity    = 0;
    size_t                              columns     = 0;
    size_t                              lyric_slots = 0;
    vec<std::pair<size_t, std::string>> words;
    const std::string*                  next_syllable = nullptr;
    bool                                has_lyrics    = false;

    void token(std::string_view text, size_t steps = PlaybackWork::token);
    void finish_line();
    void lyric_slot(i32 start);
    void write_lyric_line();
    // Bound numerators before abc2midi multiplies by the tuplet ratio.
    static constexpr i32 kMaxAtomTicks = tpq * 1024;
    void                 interval(i32 start, i32 end, const Moment* next);
    void                 write_span(i32 start, i32 end);
    void                 warn_tied_release(const WrittenNote& note);
    struct Reattack {
        bool              repeats_pitch = false;
        std::optional<i8> other_release;
    };
    Reattack reattack(i32 start, i32 end, const Moment& next) const;
    void     atom(i32 duration, i32 end);
    void     moment(const Moment& change);
    void play(const Timeline& notes, const Fields& channel, FieldRange global, i32 explicit_end);
};

void VoiceWriter::token(std::string_view text, size_t steps) {
    budget.spend(steps + PlaybackWork::text(text), where);
    if (columns > 76) finish_line();
    output += text;
    output += ' ';
    columns += text.size() + 1;
}

void VoiceWriter::finish_line() {
    output += '\n';
    columns = 0;
    if (!lyric_slots) return;
    // An empty field claims any otherwise unsung music line in this voice.
    write_lyric_line();
    lyric_slots = 0;
    words.clear();
}

void VoiceWriter::lyric_slot(i32 start) {
    if (!has_lyrics) return;
    const auto  slot     = lyric_slots++;
    const auto* syllable = std::exchange(next_syllable, nullptr);
    if (!syllable) return;
    // Bound work before allocating the escaped syllable.
    budget.spend(PlaybackWork::text(*syllable), where);
    auto text = encode_text(*syllable, TextContext::Lyric);
    // abc2midi getword() has 199 payload bytes, including up to two bytes
    // for the karaoke marker and inter-word space. Measure the encoded text.
    constexpr size_t kAbc2midiLyricBytes = 197;
    if (text.size() > kAbc2midiLyricBytes)
        issues.warn(
            where,
            "midi-lyric-width",
            "abc2midi may put part of the lyric at tick {} on later attacks",
            details::add_time(start, PlaybackGrid::attack_offset)
        );
    words.emplace_back(slot, std::move(text));
    if (std::ranges::none_of(active, [start](const auto& note) {
            return note.second.start == start;
        }))
        issues.warn(
            where,
            "midi-tied-lyric",
            "abc2midi may move the lyric at tick {} to the start of its tie and shorten the held "
            "note",
            details::add_time(start, PlaybackGrid::attack_offset)
        );
}

void VoiceWriter::write_lyric_line() {
    constexpr size_t kLineWidth = 76;
    std::string      verse      = "w:";
    const auto       flush      = [&] {
        budget.spend(1 + 2 * PlaybackWork::text(verse), where);
        output += verse;
        output += '\n';
    };
    auto       word  = words.begin();
    const auto slots = words.empty() ? 0 : words.back().first + 1;
    for (size_t slot = 0; slot < slots; ++slot) {
        std::string_view syllable = "*";
        if (word != words.end() && word->first == slot) {
            syllable = word->second;
            ++word;
        }
        if (verse.size() > 2 && verse.size() + 1 + syllable.size() > kLineWidth) {
            flush();
            verse = "+:";
        }
        verse += ' ';
        verse += syllable;
    }
    flush();
}

VoiceWriter::Reattack VoiceWriter::reattack(i32 start, i32 end, const Moment& next) const {
    Reattack result;
    if (next.attacks.empty() || end - start > kMaxAtomTicks) return result;
    std::bitset<midi_max + 1> next_pitches;
    for (const auto& hit : next.attacks) next_pitches.set(hit.pitch);
    for (const auto& [pitch, note] : active) {
        // A note that started earlier is already tied. Check only notes
        // whose otherwise untied release coincides with the next attack.
        if (note.start != start || note.end != end) continue;
        if (next_pitches[pitch])
            result.repeats_pitch = true;
        else
            result.other_release = pitch;
    }
    return result;
}

void VoiceWriter::interval(i32 start, i32 end, const Moment* next) {
    if (start == end) return;
    const auto release = !drums && next ? reattack(start, end, *next) : Reattack{};
    if (!release.repeats_pitch) {
        write_span(start, end);
        return;
    }
    if (end - start == 1) {
        issues.warn(
            where,
            "midi-short-reattack",
            "abc2midi may cut off a same-pitch reattack after a one-tick note; use MIDI export to "
            "preserve the release order"
        );
        atom(1, end);
        return;
    }
    // abc2midi ignores expand on a tie. Release one tick before the next
    // attack there, rather than emit Note Off after the reattack at equal
    // time. The native reader still performs the entire tied duration.
    if (release.other_release)
        issues.warn(
            where,
            "midi-chord-release",
            "the reattack planned at playback tick {} ({} TPQ) uses a tie that can shorten "
            "chord pitch {} by one tick in abc2midi; use MIDI export for exact releases",
            end + PlaybackGrid::attack_offset,
            tpq,
            *release.other_release
        );
    write_span(start, end - 1);
    atom(1, end);
}

void VoiceWriter::write_span(i32 start, i32 end) {
    if (end - start > kMaxAtomTicks && !active.empty()) warn_tied_release(active.begin()->second);
    while (end - start > kMaxAtomTicks) {
        start += kMaxAtomTicks;
        atom(kMaxAtomTicks, start);
    }
    atom(end - start, end);
}

void VoiceWriter::warn_tied_release(const WrittenNote& note) {
    // abc2midi drops the export release extension on an ordinary tie.
    issues.warn(
        where,
        "midi-tied-release",
        "abc2midi may release a tied pitch {} one tick before playback tick {} "
        "({} TPQ). Use MIDI export for exact releases",
        note.pitch,
        note.end + PlaybackGrid::attack_offset,
        tpq
    );
}

void VoiceWriter::atom(i32 duration, i32 end) {
    const auto [prefix, suffix] = length(duration);
    std::string text            = prefix;
    const auto  steps           = PlaybackWork::token * (prefix.empty() ? 1 : 2);
    if (active.empty()) {
        token(text + 'z' + suffix, steps);
        return;
    }
    const auto chord = active.size() > 1;
    if (chord) text += '[';
    for (const auto& [key, note] : active) {
        text += pitch(note.pitch);
        if (!chord) text += suffix;
        if (note.end > end) text += '-';
    }
    if (chord) text += ']' + suffix;
    token(text, steps + PlaybackWork::written_chord(active.size()));
    lyric_slot(end - duration);
}

void VoiceWriter::moment(const Moment& change) {
    for (const auto pitch : change.releases) active.erase(pitch);
    if (!change.attacks.empty() && velocity != change.attacks.front().velocity) {
        velocity = change.attacks.front().velocity;
        token(fmt::format("[I:MIDI beat {0} {0} {0} 1]", velocity));
    }
    for (const auto note : change.attacks) active.emplace(note.pitch, note);
    next_syllable = change.lyric;
}

void VoiceWriter::write(
    int              id,
    std::string_view name,
    int              program,
    const Timeline&  notes,
    const Fields&    channel,
    FieldRange       global,
    i32              explicit_end
) {
    has_lyrics       = std::ranges::any_of(notes, [](const auto& entry) {
        return entry.second.lyric != nullptr;
    });
    auto declaration = fmt::format("V:{}", id);
    if (!name.empty())
        declaration += fmt::format(" name=\"{}\"", encode_text(name, TextContext::VoiceName));
    budget.spend(PlaybackWork::token + PlaybackWork::text(declaration), where);
    output += declaration;
    output += '\n';
    token("[I:MIDI gchordoff]");
    if (drums) token(fmt::format("[I:MIDI channel {}]", detail::midi_drum_channel));
    token(fmt::format("[I:MIDI program {}]", program));
    token("[I:MIDI chordattack 0]");
    token(fmt::format("[I:MIDI expand {}/{}]", PlaybackGrid::export_release_extension, tpq));
    play(notes, channel, global, explicit_end);
    token("|]");
    finish_line();
}

// Merge schedules without copying fields into every lane. At a shared tick,
// global fields precede channel effects, which precede note attacks.
void VoiceWriter::play(
    const Timeline& notes, const Fields& channel, FieldRange global, i32 explicit_end
) {
    constexpr auto end     = std::numeric_limits<i32>::max();
    const auto     tick_of = [end](auto iter, const auto& sequence) {
        return iter == sequence.end() ? end : iter->first;
    };
    auto next_note    = notes.begin();
    auto next_channel = channel.begin();
    auto next_global  = global.begin();
    i32  previous     = 0;
    for (;;) {
        const auto time = std::min(
            {tick_of(next_note, notes),
             tick_of(next_channel, channel),
             tick_of(next_global, global)}
        );
        if (time == end) break;
        const auto* change = tick_of(next_note, notes) == time ? &(next_note++)->second : nullptr;
        const auto  held   = std::ranges::find_if(active, [time](const auto& item) {
            return item.second.end > time;
        });
        if (held != active.end()) {
            if (tick_of(next_global, global) == time && !change
                && tick_of(next_channel, channel) != time) {
                issues.warn(
                    where,
                    "midi-field-release",
                    "the music field at tick {} splits a held note with a tie that abc2midi may "
                    "shorten. Use MIDI export for exact releases",
                    time
                );
            } else if (!change || !change->attacks.empty() || !change->releases.empty()) {
                // A lyric-only split already has its own diagnostic in lyric_slot().
                warn_tied_release(held->second);
            }
        }
        interval(previous, time, change);
        const auto emit_fields = [&](auto& next, const auto& schedule) {
            if (tick_of(next, schedule) != time) return;
            for (const auto& field : next->second) token(field);
            ++next;
        };
        emit_fields(next_global, global);
        emit_fields(next_channel, channel);
        if (change) moment(*change);
        previous = time;
    }
    // Pad by a tick when the last release still needs its gate extension.
    if (!notes.empty() && notes.rbegin()->first == previous)
        explicit_end = std::max(explicit_end, previous + PlaybackGrid::export_tail);
    if (explicit_end > previous) write_span(previous, explicit_end);
}

// Validate times in the input unit, then round each endpoint to the ABC grid.
// Second-based scores use the input tempo map. For an interval, convert the
// original end rather than rounding a standalone duration; the caller decides
// what to do if an originally positive interval collapses to one grid point.
template<TType T>
class ExportClock {
public:
    explicit ExportClock(const Score<T>& score) : source_tpq(score.ticks_per_quarter) {
        if (source_tpq <= 0) ExportIssues::invalid("score", "ticks per quarter must be positive");
        if constexpr (std::is_same_v<T, Second>) {
            for (const auto& tempo : *score.tempos) {
                if (!std::isfinite(tempo.time) || tempo.time < 0)
                    ExportIssues::invalid("tempo", "time must be finite and nonnegative");
                check_tempo(tempo.mspq);
            }
            seconds.emplace(score);
        }
    }

    // This is only a placement probe. The event's normal projection validates
    // it and reports any rounding with its track context.
    bool rounds_to_origin(long double time) const {
        return std::isfinite(time) && time >= 0 && std::round(on_grid(time)) == 0;
    }

    i32 scale(long double time, std::string_view where, ExportIssues& issues) const {
        if (!std::isfinite(time) || time < 0)
            ExportIssues::invalid(where, "event time must be finite and nonnegative");
        if constexpr (std::is_same_v<T, Tick>) {
            if (time > std::numeric_limits<i32>::max())
                ExportIssues::invalid(where, "event time is outside the score's tick range");
        }
        const auto exact   = on_grid(time);
        const auto rounded = std::round(exact);
        if (!(rounded >= 0 && rounded <= std::numeric_limits<i32>::max() - 2))
            ExportIssues::invalid(where, "rescaled event leaves no room for the ABC note release");
        if (rounded > kMidiDeltaMax)
            issues.warn(
                where, "midi-delta-range", "very late events may exceed the MIDI delta-time range"
            );
        if (std::abs(rounded - exact) > grid_noise(exact))
            issues.warn(
                where,
                "resampled-time",
                "rounded {:.8g} ABC ticks to {:.0f} on the 480-TPQ grid",
                exact,
                rounded
            );
        // The bounded, integral result is exact in f64.
        return details::f64_to_i32(static_cast<f64>(rounded));
    }

private:
    i32 source_tpq;
    std::conditional_t<
        std::is_same_v<T, Second>,
        std::optional<details::SecondToQuarter<f64>>,
        std::monostate>
        seconds;

    long double on_grid(long double time) const {
        if constexpr (std::is_same_v<T, Tick>) {
            return time * tpq / source_tpq;
        } else if constexpr (std::is_same_v<T, Quarter>) {
            return time * tpq;
        } else {
            return static_cast<long double>(seconds.value().time_value(static_cast<f64>(time)))
                   * tpq;
        }
    }

    static long double grid_noise(long double ticks) {
        if constexpr (std::is_same_v<T, Tick>) return 0;
        // Ignore floating storage noise near ordinary grid positions, without
        // hiding larger rounding errors in long floating-point scores.
        return std::min(0.001L, std::numeric_limits<typename T::unit>::epsilon() * ticks);
    }
};

template<TType T>
struct PlannedTrack {
    vec<VoiceLane>  lanes;
    Fields          channel;
    std::string     where;
    const Track<T>* track;
};

// Plan track splits, channel events and lyrics before rendering a tune. Global
// timed fields are shared by all participating lanes; channel fields are
// replayed on each split lane of the source track. The input score owns all
// borrowed lyric text.
template<TType T>
class ScoreWriter {
public:
    explicit ScoreWriter(const Score<T>& score) : score(score), clock(score) {}
    Result<std::string> write();

private:
    using unit = typename T::unit;

    const Score<T>&       score;
    ExportClock<T>        clock;
    ExportIssues          issues;
    ExportBudget          budget;
    ExportBudget          planning_budget;
    PlacementBudget       placement_budget;
    std::string           output;
    Fields                global;
    std::optional<Fields> first_voice_fields;
    vec<PlannedTrack<T>>  tracks;
    size_t                total_voices   = 0;
    size_t                melodic_voices = 0;
    int                   start_offset   = 0;

    i32 scale(long double time, std::string_view where) { return clock.scale(time, where, issues); }
    vec<WrittenNote> notes(const Track<T>& track, std::string_view where);
    Lyrics           lyrics(const Track<T>& track, std::string_view where);
    void   place_lyrics(const Track<T>& track, vec<VoiceLane>& lanes, std::string_view where);
    void   add_note_tunings(Fields& fields);
    Fields channel_events(const Track<T>& track, std::string_view where, size_t copies);
    void   add_control(Fields& fields, i32 at, int number, int value, std::string_view where);
    void   release_limits(i32 start, i32 duration, bool drums, std::string_view where);
    vec<std::pair<i32, i32>> pedals(const Track<T>& track, std::string_view where);
    void                     add_pedals(
        Fields& fields, vec<std::pair<i32, i32>> pedals, std::string_view where, size_t copies
    );
    void add_bends(Fields& fields, const Track<T>& track, std::string_view where);
    void reconcile_pedals(
        Fields&                  fields,
        vec<std::pair<i32, i32>> pedals,
        vec<ControlChange<Tick>> controls,
        std::string_view         where,
        size_t                   copies
    );
    VoiceLimit voice_limit(bool drums) const;
    void       reserve_voices(size_t lanes, bool drums, std::string_view where);
    void       limitations(const Track<T>& track, std::string_view where);
    void       header();
    void       render_voices();
    void       plan_track(const Track<T>& track, size_t index);
    int        onset_offset();

    template<class Event, class Format>
    std::string fields(char key, const pyvec<Event>& events, std::string fallback, Format format);
};

template<TType T>
bool is_audible(const Note<T>& note) {
    return note.duration > 0 && note.velocity > 0;
}

void check_duration(long double duration, std::string_view where, std::string_view event) {
    if (!std::isfinite(duration))
        ExportIssues::invalid(where, fmt::format("{} duration must be finite", event));
    if (duration < 0)
        ExportIssues::invalid(where, fmt::format("{} duration cannot be negative", event));
}

bool legacy_sustain_fits(i32 duration) {
    // abc2midi joins tied atoms before conversion. Its MIDI release queue
    // multiplies the reduced quarter-note numerator by its tick division.
    const auto numerator = duration / std::gcd(duration, tpq);
    return numerator <= std::numeric_limits<i32>::max() / tpq;
}

template<TType T>
void ScoreWriter<T>::release_limits(i32 start, i32 duration, bool drums, std::string_view where) {
    if (!legacy_sustain_fits(duration))
        issues.warn(
            where, "midi-sustain-range", "abc2midi may shorten an unusually long held note"
        );
    if (drums && duration == 1)
        issues.warn(
            where,
            "midi-short-drum",
            "abc2midi can play a one-tick drum hit at tick {} with zero duration; use MIDI export "
            "to keep the release",
            start
        );
}

template<TType T>
vec<WrittenNote> ScoreWriter<T>::notes(const Track<T>& track, std::string_view where) {
    vec<WrittenNote> result;
    result.reserve(track.notes->size());
    for (const auto& note : *track.notes) {
        if (note.pitch < 0 || note.velocity < 0)
            ExportIssues::invalid(where, "note pitch and velocity must be in MIDI range 0–127");
        check_duration(note.duration, where, "note");
        const auto start = scale(note.time, where);
        auto       end   = scale(static_cast<long double>(note.time) + note.duration, where);
        if (!is_audible(note)) {
            issues.warn(where, "silent-note", "omitted a zero-velocity or zero-duration note");
            continue;
        }
        if (end == start) {
            ++end;
            issues.warn(where, "short-note", "stretched a resampled note to one ABC tick");
        }
        release_limits(
            start - start_offset + PlaybackGrid::attack_offset, end - start, track.is_drum, where
        );
        result.push_back({start - start_offset, end - start_offset, note.pitch, note.velocity});
    }
    return result;
}

template<TType T>
std::string meter_text(const TimeSignature<T>& event) {
    if (!event.valid_beat_groups())
        ExportIssues::invalid("meter", "beat groups must be positive and sum to the numerator");
    if (event.is_free_meter()) return "none";
    if (!event.numerator || !event.denominator || !std::has_single_bit(event.denominator)) {
        ExportIssues::invalid(
            "meter", "expected a positive numerator and a power-of-two denominator"
        );
    }
    if (event.beat_groups.empty()) return fmt::format("{}/{}", event.numerator, event.denominator);
    std::string groups = "(";
    for (const auto group : event.beat_groups) {
        if (groups.size() > 1) groups += '+';
        groups += std::to_string(group);
    }
    return fmt::format("{})/{}", groups, event.denominator);
}

template<TType T>
std::string key_text(const KeySignature<T>& event) {
    if (event.key < -7 || event.key > 7 || event.tonality < 0 || event.tonality > 1) {
        ExportIssues::invalid("key", "expected −7 to 7 fifths and major or minor tonality");
    }
    return std::string(detail::key_name(event.key, event.tonality != 0));
}

template<TType T>
template<class Event, class Format>
std::string ScoreWriter<T>::fields(
    char key, const pyvec<Event>& events, std::string fallback, Format format
) {
    struct Change {
        unit        source_time;
        std::string text;
    };
    std::map<i32, Change> ordered;
    const auto            context = fmt::format("{}: field", key);
    for (const auto& event : events) {
        const auto time     = scale(event.time, context);
        auto       text     = format(event);
        auto [it, inserted] = ordered.try_emplace(time, event.time, std::move(text));
        if (inserted) continue;
        auto& last = it->second;
        if (last.text != text) {
            issues.warn(
                context, "duplicate-time", "multiple changes at one tick; kept the last value"
            );
        }
        // Events need not be sorted. Keep the latest original time after rounding merges ticks.
        if (event.time >= last.source_time) last = {event.time, std::move(text)};
    }
    if (auto at_zero = ordered.extract(0); !at_zero.empty())
        fallback = std::move(at_zero.mapped().text);
    if (key != 'Q' && !ordered.empty())
        issues.warn(
            context,
            "midi-field-timing",
            "abc2midi may move the change at tick {} to earlier MIDI events in different voices; "
            "use MIDI export for exact timing",
            ordered.begin()->first
        );
    for (const auto& [time, value] : ordered)
        global[time].push_back(fmt::format("[{}:{}]", key, value.text));
    return fallback;
}

template<TType T>
void ScoreWriter<T>::header() {
    const auto m     = fields('M', *score.time_signatures, "none", meter_text<T>);
    const auto k     = fields('K', *score.key_signatures, "C", key_text<T>);
    const auto q     = fields('Q', *score.tempos, "1/4=120", [](const auto& event) {
        return tempo_text(event.mspq);
    });
    const auto title = tracks.empty() ? "Untitled" : tracks.front().track->name;
    // ABC 2.1 sections 2.1 and 3.1.17 identify the format and its creator.
    // Section 6.1.1 lets each voice wrap its source independently of the printed staff.
    output = fmt::format(
        "%abc-2.1\nX:1\nT:{}\nI:abc-creator symusic-" SYMUSIC_VERSION
        "\nI:linebreak <none>\nM:{}\nL:1/4\nQ:{}\n",
        encode_text(title, TextContext::Title),
        m,
        q
    );
    if (!score.note_tunings->empty()) {
        first_voice_fields = global;
        add_note_tunings(*first_voice_fields);
    }
    for (const auto& notice : score.copyright_notices) {
        planning_budget.spend(PlaybackWork::text(notice), "score copyright");
        if (!utf8::is_valid(notice.begin(), notice.end()))
            ExportIssues::invalid("score copyright", "copyright notice must be valid UTF-8");
        if (notice.empty()) {
            issues.warn("score copyright", "omitted-empty-copyright", "omitted an empty notice");
            continue;
        }
        auto text = encode_text(notice, TextContext::Title);
        // The parser trims a field before decoding the text.
        if (text.back() == ' ') text.replace(text.size() - 1, 1, "\\u0020");
        if (text.front() == ' ') text.replace(0, 1, "\\u0020");
        output += fmt::format("Z:abc-copyright {}\n", text);
    }
    output += fmt::format("K:{}\n", k);
}

template<TType T>
void ScoreWriter<T>::limitations(const Track<T>& track, std::string_view where) {
    if (track.is_drum && track.program != 0)
        issues.warn(where, "drum-program", "other ABC readers may ignore the drum program");
}

template<TType T>
Lyrics ScoreWriter<T>::lyrics(const Track<T>& track, std::string_view where) {
    Lyrics result;
    planning_budget.spend(track.lyrics->size(), where);
    for (const auto& lyric : *track.lyrics) {
        const auto start = details::sub_time(scale(lyric.time, where), start_offset);
        if (!utf8::is_valid(lyric.text.begin(), lyric.text.end()))
            ExportIssues::invalid(where, "lyric must be valid UTF-8");
        if (lyric.text.empty()) {
            issues.warn(
                where, "omitted-empty-lyric", "ABC blank syllables do not produce lyric events"
            );
            continue;
        }
        result.emplace(start, &lyric.text);
    }
    return result;
}

void warn_unplaced_lyrics(const Lyrics& lyrics, std::string_view where, ExportIssues& issues) {
    if (!lyrics.empty())
        issues.warn(
            where,
            "omitted-lyrics",
            "omitted {} lyric event(s) without a free ABC syllable slot (first at tick {})",
            lyrics.size(),
            details::add_time(lyrics.begin()->first, PlaybackGrid::attack_offset)
        );
}

template<TType T>
void ScoreWriter<T>::place_lyrics(
    const Track<T>& track, vec<VoiceLane>& lanes, std::string_view where
) {
    auto unplaced = lyrics(track, where);
    if (unplaced.empty()) return;
    planning_budget.spend_repeated(unplaced.size(), lanes.size(), where);

    // Give all lanes a chance to use an attack before creating any new tie segments.
    for (auto& lane : lanes) {
        for (auto word = unplaced.begin(); word != unplaced.end();) {
            const auto attack = lane.timeline.find(word->first);
            if (attack != lane.timeline.end() && !attack->second.attacks.empty()
                && !attack->second.lyric) {
                attack->second.lyric = word->second;
                word                 = unplaced.erase(word);
            } else {
                ++word;
            }
        }
        if (unplaced.empty()) return;
    }
    for (auto& lane : lanes) {
        auto   event    = lane.timeline.begin();
        size_t sounding = 0;
        for (auto word = unplaced.begin(); word != unplaced.end();) {
            const auto time = word->first;
            while (event != lane.timeline.end() && event->first <= time) {
                sounding += event->second.attacks.size();
                // Each release belongs to an earlier attack in this lane.
                sounding -= event->second.releases.size();
                ++event;
            }
            if (sounding && !lane.timeline[time].lyric) {
                lane.timeline[time].lyric = word->second;
                word                      = unplaced.erase(word);
            } else {
                ++word;
            }
        }
        if (unplaced.empty()) return;
    }
    warn_unplaced_lyrics(unplaced, where, issues);
}

vec<std::pair<i32, i32>> pedal_intervals(vec<ControlChange<Tick>> controls) {
    std::ranges::stable_sort(controls, {}, &ControlChange<Tick>::time);
    vec<std::pair<i32, i32>> intervals;
    std::optional<i32>       start;
    for (const auto& event : controls) {
        if (event.value >= 64) {
            if (!start) start = event.time;
        } else if (start) {
            intervals.emplace_back(*start, event.time);
            start.reset();
        }
    }
    return intervals;
}

template<TType T>
vec<std::pair<i32, i32>> ScoreWriter<T>::pedals(const Track<T>& track, std::string_view where) {
    vec<std::pair<i32, i32>> intervals;
    for (const auto& pedal : *track.pedals) {
        check_duration(pedal.duration, where, "pedal");
        const auto start = scale(pedal.time, where);
        const auto end   = scale(static_cast<long double>(pedal.time) + pedal.duration, where);
        if (start != end)
            intervals.emplace_back(start, end);
        else if (pedal.duration > 0)
            issues.warn(where, "short-pedal", "omitted a pedal interval shorter than the ABC grid");
    }
    return intervals;
}

std::string bend_field(int value, std::string_view where) {
    if (value < -detail::midi_bend_center || value >= detail::midi_bend_center)
        ExportIssues::invalid(where, "pitch bend must be in MIDI range −8192–8191");
    // The range check above guarantees a nonnegative 14-bit wire value.
    const auto wire = static_cast<u32>(value + detail::midi_bend_center);
    return fmt::format("[I:MIDI pitchbend {} {}]", wire & static_cast<u32>(midi_max), wire >> 7);
}

template<TType T>
void ScoreWriter<T>::add_control(
    Fields& fields, i32 at, int number, int value, std::string_view where
) {
    fields[at].push_back(fmt::format("[I:MIDI control {} {}]", number, value));
    issues.warn(
        where,
        "midi-control-timing",
        "abc2midi may move CC {} at tick {} or write it after a same-tick attack; use MIDI export "
        "for exact timing and order",
        number,
        at
    );
}

template<TType T>
Fields ScoreWriter<T>::channel_events(
    const Track<T>& track, std::string_view where, size_t copies
) {
    Fields                   fields;
    vec<ControlChange<Tick>> pedal_controls;
    for (const auto& event : *track.controls) {
        if (event.number > midi_max || event.value > midi_max)
            ExportIssues::invalid(where, "control number and value must be in MIDI range 0–127");
        const auto at = scale(event.time, where);
        if (event.number == 64) pedal_controls.emplace_back(at, event.number, event.value);
        add_control(fields, at, event.number, event.value, where);
    }
    add_bends(fields, track, where);
    reconcile_pedals(fields, pedals(track, where), std::move(pedal_controls), where, copies);
    return fields;
}

template<TType T>
void ScoreWriter<T>::add_bends(Fields& fields, const Track<T>& track, std::string_view where) {
    for (const auto& bend : *track.pitch_bends) {
        const auto at = scale(bend.time, where);
        fields[at].push_back(bend_field(bend.value, where));
        if (at > 0)
            issues.warn(
                where,
                "midi-bend-timing",
                "abc2midi may emit timed pitch bends early and shift later notes; "
                "use MIDI export for interoperable bend timing"
            );
    }
}

template<TType T>
void ScoreWriter<T>::reconcile_pedals(
    Fields&                  fields,
    vec<std::pair<i32, i32>> intervals,
    vec<ControlChange<Tick>> controls,
    std::string_view         where,
    size_t                   copies
) {
    // Prefer explicit CC64: it can carry half-pedal values. Compare only
    // what reaches the playback grid; float storage error is not a conflict.
    if (controls.empty()) {
        add_pedals(fields, std::move(intervals), where, copies);
        return;
    }
    if (intervals.empty()) return;
    std::ranges::sort(intervals);
    if (intervals != pedal_intervals(std::move(controls)))
        issues.warn(
            where,
            "pedal-conflict",
            "pedal intervals disagree with CC64; kept the explicit controls"
        );
}

template<TType T>
void ScoreWriter<T>::add_pedals(
    Fields& fields, vec<std::pair<i32, i32>> pedals, std::string_view where, size_t copies
) {
    if (pedals.empty()) return;
    std::ranges::sort(pedals);
    auto [start, end] = pedals.front();
    const auto append = [&] {
        planning_budget.spend_repeated(2 * PlaybackWork::token, copies, where);
        add_control(fields, start, 64, 127, where);
        add_control(fields, end, 64, 0, where);
    };
    for (const auto& [next_start, next_end] : std::span(pedals).subspan(1)) {
        if (next_start < end) {
            issues.warn(where, "overlapping-pedals", "merged overlapping sustain intervals");
            end = std::max(end, next_end);
        } else {
            append();
            start = next_start;
            end   = next_end;
        }
    }
    append();
}

template<TType T>
void ScoreWriter<T>::plan_track(const Track<T>& track, size_t index) {
    if (!utf8::is_valid(track.name.begin(), track.name.end()))
        ExportIssues::invalid(fmt::format("track {}", index + 1), "track name must be valid UTF-8");
    auto where
        = fmt::format("track {} ('{}')", index + 1, encode_text(track.name, TextContext::Title));
    if (track.program > midi_max)
        ExportIssues::invalid(where, "program must be in MIDI range 0–127");
    limitations(track, where);
    auto lanes
        = partition(notes(track, where), voice_limit(track.is_drum), placement_budget, where);
    if (lanes.size() > 1)
        issues.warn(where, "split-voice", "used multiple ABC voices to preserve note events");
    // These events emit one token per lane even when they all share a tick.
    // Charge them before formatting or retaining any of their strings.
    planning_budget.spend_repeated(track.controls->size(), lanes.size(), where);
    planning_budget.spend_repeated(track.pitch_bends->size(), lanes.size(), where);
    auto channel = channel_events(track, where, lanes.size());
    place_lyrics(track, lanes, where);
    if (lanes.front().timeline.empty() && channel.empty()) return;
    for (const auto& lane : lanes)
        planning_budget.spend(lane.timeline.size() * PlaybackWork::token, where);
    reserve_voices(lanes.size(), track.is_drum, where);
    tracks.push_back({std::move(lanes), std::move(channel), std::move(where), &track});
}

template<TType T>
VoiceLimit ScoreWriter<T>::voice_limit(bool drums) const {
    const auto remaining = kMaxExportVoices - total_voices;
    const auto melodic   = kMelodicChannels - melodic_voices;
    if (drums || remaining <= melodic) return {remaining, "30 ABC voices"};
    return {melodic, "15 melodic ABC voices"};
}

template<TType T>
void ScoreWriter<T>::reserve_voices(size_t lanes, bool drums, std::string_view where) {
    const auto limit = voice_limit(drums);
    if (lanes > limit.available) limit.fail(where);
    if (drums && total_voices - melodic_voices + lanes > 1)
        issues.warn(
            "score",
            "shared-drum-channel",
            "abc2midi uses channel 10 for all percussion voices; their notes and settings can "
            "interact"
        );
    if (!drums) melodic_voices += lanes;
    total_voices += lanes;
}

template<TType T>
void ScoreWriter<T>::add_note_tunings(Fields& fields) {
    bool timed   = false;
    bool rounded = false;
    planning_budget.spend(score.note_tunings->size(), "note tuning");
    for (const auto& tuning : *score.note_tunings) {
        if (!details::valid_note_tuning(tuning.key, tuning.pitch))
            ExportIssues::invalid("note tuning", "expected a MIDI key and encodable pitch");
        const auto time  = scale(tuning.time, "note tuning");
        const auto pitch = details::decode_tuning_pitch(details::encode_tuning_pitch(tuning.pitch));
        if (time == 0) {
            // Run opening changes before the first voice's Program Change.
            const auto line = fmt::format("I:MIDI snt {} {}\n", tuning.key, pitch);
            budget.spend(PlaybackWork::token + PlaybackWork::text(line), "note tuning");
            output += line;
        } else {
            // The message targets a device and program, independent of the voice.
            fields[time].push_back(fmt::format("[I:MIDI snt {} {}]", tuning.key, pitch));
            timed = true;
        }
        rounded |= pitch != tuning.pitch;
    }
    if (rounded)
        issues.warn("note tuning", "rounded-tuning", "rounded pitches to the MIDI tuning grid");
    if (timed)
        issues.warn(
            "note tuning",
            "midi-note-tuning-timing",
            "ABC places tuning at a preceding MIDI event's tick. Use MIDI for exact timing"
        );
}

template<TType T>
void ScoreWriter<T>::render_voices() {
    // The first voice carries score-wide tuning changes and trailing silence.
    const auto& opening = first_voice_fields ? *first_voice_fields : global;
    const auto  extent  = score.explicit_end ? scale(*score.explicit_end, "explicit end")
                                                   + PlaybackGrid::attack_offset - start_offset :
                                               0;
    if (tracks.empty()) {
        VoiceWriter(output, budget, issues, "score", false)
            .write(1, "", 0, {}, {}, opening, extent);
        return;
    }
    int id = 0;
    for (const auto& plan : tracks) {
        const auto& track = *plan.track;
        for (const auto& lane : plan.lanes) {
            const auto end = std::max(
                lane.timeline.empty() ? 0 : lane.timeline.rbegin()->first,
                plan.channel.empty() ? 0 : plan.channel.rbegin()->first
            );
            // ABC 2.1 section 7.3 repeats fields in each affected voice. The first
            // voice also carries changes after the music so they survive export.
            const FieldRange fields    = id == 0 ?
                                             FieldRange(opening) :
                                             FieldRange(global.cbegin(), global.lower_bound(end));
            const auto       voice_end = id == 0 ? extent : 0;
            VoiceWriter(output, budget, issues, plan.where, track.is_drum)
                .write(
                    ++id, track.name, track.program, lane.timeline, plan.channel, fields, voice_end
                );
        }
    }
}

template<TType T>
int ScoreWriter<T>::onset_offset() {
    for (const auto& track : *score.tracks) {
        for (const auto& note : *track->notes) {
            if (is_audible(note) && clock.rounds_to_origin(note.time)) {
                issues.warn(
                    "score",
                    "note-start-offset",
                    "shifted all note onsets one tick because a note starts at tick zero"
                );
                return 0;
            }
        }
    }
    return PlaybackGrid::attack_offset;
}

template<TType T>
Result<std::string> ScoreWriter<T>::write() {
    if (!score.markers->empty())
        issues.warn(
            "score", "omitted-markers", "ABC export does not represent arbitrary timed markers"
        );
    start_offset = onset_offset();
    for (size_t i = 0; i < score.tracks->size(); ++i) plan_track(*(*score.tracks)[i], i);
    header();
    render_voices();
    return {std::move(output), issues.take()};
}

}   // namespace

Result<std::string> write(const Score<Tick>& score) { return ScoreWriter(score).write(); }
Result<std::string> write(const Score<Quarter>& score) { return ScoreWriter(score).write(); }
Result<std::string> write(const Score<Second>& score) { return ScoreWriter(score).write(); }

}   // namespace symusic::abc

#include "symusic/io/abc.h"

#include <algorithm>
#include <array>
#include <bit>
#include <limits>
#include <map>
#include <numeric>
#include <ranges>
#include <set>
#include <span>

#include <utf8.h>

#include "settings.h"

namespace symusic::abc {
namespace {

using detail::midi_max;
using detail::tpq;
using detail::whole;

constexpr i32 midi_tempo_max = (1 << 24) - 1;
constexpr i32 midi_delta_max = (1 << 28) - 1;
// abc2midi reserves one channel for percussion and queues up to 50 releases.
// A lane can have 25 active notes, leaving room for the next chord's attacks.
constexpr int max_export_voices = 15;
constexpr int max_active_notes  = 25;

class ExportIssues {
public:
    [[noreturn]] static void invalid(std::string_view where, std::string_view message) {
        throw std::invalid_argument(fmt::format("ABC export {}: {}", where, message));
    }
    void warn(std::string_view where, std::string_view code, std::string_view message) {
        warnings.insert(fmt::format("ABC export {} [{}]: {}", where, code, message));
    }
    vec<std::string> take() { return {warnings.begin(), warnings.end()}; }

private:
    std::set<std::string> warnings;
};

std::string escape_text(std::string_view text) {
    std::string result;
    for (char c : text) {
        if (c == '\r' || c == '\n' || c == '\0') c = ' ';
        if (c == '"' || c == '\\' || c == '%') result += '\\';
        result += c;
    }
    return result;
}

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
    constexpr std::string_view chromatic[]
        = {"=C", "^C", "=D", "^D", "=E", "=F", "^F", "=G", "^G", "=A", "^A", "=B"};
    std::string result(chromatic[note % 12]);
    int         octave = note / 12 - 5;
    if (octave > 0) {
        result.back() = static_cast<char>(result.back() - 'A' + 'a');
        --octave;
    }
    result.append(static_cast<size_t>(std::abs(octave)), octave < 0 ? ',' : '\'');
    return result;
}

std::string tempo_text(i32 mspq, ExportIssues& issues) {
    if (mspq < 1 || mspq > midi_tempo_max)
        ExportIssues::invalid("tempo", "microseconds per quarter must fit the 24-bit MIDI range");
    // The denominator and integer BPM are independent. Stay below the
    // reference converter's signed BPM * 4 intermediate, and choose the
    // nearest representable tempo; prefer the shortest beat on a tie.
    constexpr auto max_bpm      = std::numeric_limits<int>::max() / 4;
    int64_t        rate         = 60'000'000;
    int64_t        best_error   = std::numeric_limits<int64_t>::max();
    int            best_divisor = 4;
    int            best_bpm     = 120;
    for (int denominator = 4; denominator <= (1 << 20); denominator *= 2, rate *= 2) {
        const auto floor_bpm = rate / mspq;
        for (const auto estimate : {floor_bpm, floor_bpm + 1}) {
            const auto bpm   = std::clamp<int64_t>(estimate, 1, max_bpm);
            const auto error = std::abs(rate / bpm - mspq);
            if (error == 0) return fmt::format("1/{}={}", denominator, bpm);
            if (error >= best_error || rate / bpm > midi_tempo_max) continue;
            best_error   = error;
            best_divisor = denominator;
            best_bpm     = static_cast<int>(bpm);
        }
    }
    issues.warn("tempo", "rounded-tempo", "rounded a tempo to the nearest supported ABC value");
    return fmt::format("1/{}={}", best_divisor, best_bpm);
}

struct WrittenNote {
    i32 start;
    i32 end;
    i8  pitch;
    i8  velocity;
};

struct Moment {
    vec<WrittenNote> attacks;
    vec<int>         releases;
    vec<std::string> fields;
};

using Timeline = std::map<i32, Moment>;

struct VoiceLane {
    Timeline             timeline;
    std::array<i32, 128> ends{};
    i32                  recent_start    = -1;
    int                  recent_velocity = 0;

    bool accepts(WrittenNote note) const {
        return ends[note.pitch] <= note.start
               && (recent_start != note.start || recent_velocity == note.velocity)
               && std::ranges::count_if(ends, [&](i32 end) { return end > note.start; })
                      < max_active_notes;
    }
    void add(WrittenNote note) {
        ends[note.pitch] = note.end;
        recent_start     = note.start;
        recent_velocity  = note.velocity;
        timeline[note.start].attacks.push_back(note);
        timeline[note.end].releases.push_back(note.pitch);
    }
};

// Keeps most polyphony in one ABC voice. Only overlapping notes of the same
// pitch and simultaneous attacks with different velocities require extra lanes.
vec<VoiceLane> partition(vec<WrittenNote> notes, ExportIssues& issues, std::string_view where) {
    std::ranges::stable_sort(notes, {}, &WrittenNote::start);
    vec<VoiceLane> lanes(1);
    for (const auto note : notes) {
        auto lane = std::ranges::find_if(lanes, [&](const VoiceLane& lane) {
            return lane.accepts(note);
        });
        if (lane == lanes.end()) {
            if (lanes.size() == max_export_voices)
                ExportIssues::invalid(where, "too many independent ABC voice lanes");
            lanes.emplace_back();
            lane = lanes.end() - 1;
        }
        lane->add(note);
    }
    if (lanes.size() > 1)
        issues.warn(
            where,
            "split-voice",
            "used multiple ABC voices to preserve polyphony, velocities, or note releases"
        );
    return lanes;
}

// Charge an upper bound on the work our output will require on import. Each
// emitted atom costs one step plus up to three units per member (pitch, tie and
// attack). There are no repeat, grace or accompaniment expansions in this ABC.
class ExportBudget {
public:
    void spend(size_t work, std::string_view where) {
        if (work > remaining) {
            ExportIssues::invalid(
                where, "[expansion-limit] output exceeds the ABC playback work limit"
            );
        }
        remaining -= work;
    }

private:
    size_t remaining = detail::playback_work_limit;
};

class VoiceWriter {
public:
    VoiceWriter(
        std::string& output, Timeline timeline, ExportBudget& budget, std::string_view where
    ) : output(output), timeline(std::move(timeline)), budget(budget), where(where) {}
    void write(int id, std::string_view name, int program, bool drums);

private:
    std::string&               output;
    Timeline                   timeline;
    ExportBudget&              budget;
    std::string_view           where;
    std::map<int, WrittenNote> active;
    int                        velocity = 0;
    size_t                     columns  = 0;

    void token(std::string_view text, size_t steps = 1);
    void interval(i32 start, i32 end);
    void atom(i32 duration, i32 end);
    void moment(const Moment& change);
};

void VoiceWriter::token(std::string_view text, size_t steps) {
    budget.spend(steps + detail::text_work(text), where);
    if (columns > 76) {
        output += '\n';
        columns = 0;
    }
    output += text;
    output += ' ';
    columns += text.size() + 1;
}

void VoiceWriter::interval(i32 start, i32 end) {
    // The converter multiplies the written numerator when applying tuplets
    // and converting to MIDI ticks. Bound each atom, including long silence.
    constexpr i32 max_atom_ticks = tpq * 1024;
    while (end - start > max_atom_ticks) {
        start += max_atom_ticks;
        atom(max_atom_ticks, start);
    }
    atom(end - start, end);
}

void VoiceWriter::atom(i32 duration, i32 end) {
    const auto [prefix, suffix] = length(duration);
    std::string text            = prefix;
    const auto  steps           = prefix.empty() ? 1 : 2;
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
    token(text, steps + 3 * active.size());
}

void VoiceWriter::moment(const Moment& change) {
    for (const auto& field : change.fields) token(field);
    for (const auto pitch : change.releases) active.erase(pitch);
    if (!change.attacks.empty() && velocity != change.attacks.front().velocity) {
        velocity = change.attacks.front().velocity;
        token(fmt::format("[I:MIDI beat {0} {0} {0} 1]", velocity));
    }
    for (const auto note : change.attacks) active.emplace(note.pitch, note);
}

void VoiceWriter::write(int id, std::string_view name, int program, bool drums) {
    auto declaration = fmt::format("V:{}", id);
    if (!name.empty()) declaration += fmt::format(" name=\"{}\"", escape_text(name));
    budget.spend(1 + detail::text_work(declaration), where);
    output += declaration;
    output += '\n';
    token("[I:MIDI gchordoff]");
    if (drums) token("[I:MIDI channel 10]");
    token(fmt::format("[I:MIDI program {}]", program));
    token("[I:MIDI chordattack 0]");
    token("[I:MIDI expand 1/480]");
    i32 previous = 0;
    for (const auto& [time, change] : timeline) {
        if (time > previous) interval(previous, time);
        moment(change);
        previous = time;
    }
    // A one-tick tail allows the final expanded note to release before EOF.
    token("(15:2:1z/64", 2);
    token("|]");
    output += '\n';
}

class ScoreWriter {
public:
    explicit ScoreWriter(const Score<Tick>& score) : score(score) {}
    Result<std::string> write();

private:
    const Score<Tick>& score;
    ExportIssues       issues;
    ExportBudget       budget;
    std::string        output;
    Timeline           global;
    int                voices       = 0;
    int                start_offset = 0;

    i32                      scale(int64_t time, std::string_view where);
    vec<WrittenNote>         notes(const Track<Tick>& track, std::string_view where);
    Timeline                 controls(const Track<Tick>& track, std::string_view where);
    vec<std::pair<i32, i32>> pedals(const Track<Tick>& track, std::string_view where);
    void add_pedals(Timeline& timeline, vec<std::pair<i32, i32>> pedals, std::string_view where);
    void limitations(const Track<Tick>& track, std::string_view where);
    void header();
    void track(const Track<Tick>& track, size_t index);
    int  onset_offset();
    int  next_voice(std::string_view where);

    template<class Event, class Format>
    std::string fields(char key, const pyvec<Event>& events, std::string fallback, Format format);
};

i32 ScoreWriter::scale(int64_t time, std::string_view where) {
    if (time < 0 || time > std::numeric_limits<i32>::max())
        ExportIssues::invalid(where, "event time is outside the score's tick range");
    const auto numerator = time * tpq;
    const auto tick      = (numerator + score.ticks_per_quarter / 2) / score.ticks_per_quarter;
    if (tick > std::numeric_limits<i32>::max() - 2)
        ExportIssues::invalid(where, "rescaled event leaves no room for the ABC note release");
    if (tick > midi_delta_max) {
        issues.warn(
            where, "midi-delta-range", "very late events may exceed the MIDI delta-time range"
        );
    }
    if (numerator % score.ticks_per_quarter != 0)
        issues.warn(where, "resampled-time", "rounded times to the ABC 480-tick grid");
    return static_cast<i32>(tick);
}

bool is_audible(const Note<Tick>& note) { return note.duration > 0 && note.velocity > 0; }

bool legacy_sustain_fits(i32 duration) {
    // abc2midi joins tied atoms before conversion. Its MIDI release queue
    // multiplies the reduced quarter-note numerator by its tick division.
    const auto numerator = duration / std::gcd(duration, tpq);
    return numerator <= std::numeric_limits<i32>::max() / tpq;
}

vec<WrittenNote> ScoreWriter::notes(const Track<Tick>& track, std::string_view where) {
    vec<WrittenNote> result;
    result.reserve(track.notes->size());
    for (const auto& note : *track.notes) {
        if (note.pitch < 0 || note.velocity < 0)
            ExportIssues::invalid(where, "note pitch and velocity must be in MIDI range 0–127");
        if (note.duration < 0) ExportIssues::invalid(where, "note duration cannot be negative");
        const auto start = scale(note.time, where);
        auto       end   = scale(static_cast<int64_t>(note.time) + note.duration, where);
        if (!is_audible(note)) {
            issues.warn(where, "silent-note", "omitted a zero-velocity or zero-duration note");
            continue;
        }
        if (end == start) {
            ++end;
            issues.warn(where, "short-note", "stretched a resampled note to one ABC tick");
        }
        if (!legacy_sustain_fits(end - start))
            issues.warn(
                where, "midi-sustain-range", "abc2midi may shorten an unusually long held note"
            );
        result.push_back({start - start_offset, end - start_offset, note.pitch, note.velocity});
    }
    return result;
}

std::string meter_text(const TimeSignature<Tick>& event) {
    if (!event.numerator || !event.denominator || !std::has_single_bit(event.denominator)) {
        ExportIssues::invalid(
            "meter", "expected a positive numerator and a power-of-two denominator"
        );
    }
    return fmt::format("{}/{}", event.numerator, event.denominator);
}

std::string key_text(const KeySignature<Tick>& event) {
    if (event.key < -7 || event.key > 7 || event.tonality < 0 || event.tonality > 1) {
        ExportIssues::invalid("key", "expected −7 to 7 fifths and major or minor tonality");
    }
    return std::string(detail::key_name(event.key, event.tonality != 0));
}

template<class Event, class Format>
std::string ScoreWriter::fields(
    char key, const pyvec<Event>& events, std::string fallback, Format format
) {
    std::map<i32, std::string> ordered;
    const auto                 context = fmt::format("{}: field", key);
    for (const auto& event : events) {
        const auto time = scale(event.time, context);
        const auto text = format(event);
        if (const auto found = ordered.find(time);
            found != ordered.end() && found->second != text) {
            issues.warn(
                context, "duplicate-time", "multiple changes at one tick; kept the last value"
            );
        }
        ordered.insert_or_assign(time, text);
    }
    if (auto at_zero = ordered.extract(0); !at_zero.empty()) fallback = std::move(at_zero.mapped());
    for (const auto& [time, value] : ordered) {
        const auto playback_time = key == 'Q' ? time - 1 : time;
        global[playback_time].fields.push_back(fmt::format("[{}:{}]", key, value));
    }
    return fallback;
}

void ScoreWriter::header() {
    const auto m     = fields('M', *score.time_signatures, "4/4", meter_text);
    const auto k     = fields('K', *score.key_signatures, "C", key_text);
    const auto q     = fields('Q', *score.tempos, "1/4=120", [&](const auto& event) {
        return tempo_text(event.mspq, issues);
    });
    const auto title = score.tracks->empty() ? "Untitled" : score.tracks->front()->name;
    output = fmt::format("X:1\nT:{}\nM:{}\nL:1/4\nQ:{}\nK:{}\n", escape_text(title), m, q, k);
}

void ScoreWriter::limitations(const Track<Tick>& track, std::string_view where) {
    if (!track.lyrics->empty())
        issues.warn(
            where, "omitted-lyrics", "ABC export does not represent arbitrary timed lyrics"
        );
    if (!track.pitch_bends->empty())
        issues.warn(where, "omitted-pitch-bends", "ABC export does not represent pitch bends");
    if (track.is_drum && track.program != 0)
        issues.warn(where, "drum-program", "other ABC readers may ignore the drum program");
}

vec<std::pair<i32, i32>> pedal_intervals(const pyvec<ControlChange<Tick>>& input) {
    vec<ControlChange<Tick>> controls;
    for (const auto& event : input) {
        if (event.number == 64) controls.push_back(event);
    }
    std::ranges::stable_sort(controls, {}, &ControlChange<Tick>::time);
    vec<std::pair<i32, i32>> intervals;
    std::optional<i32>       start;
    for (const auto& event : controls) {
        if (event.value >= 64) {
            if (!start) start = event.time;
        } else if (start) {
            intervals.emplace_back(*start, event.time - *start);
            start.reset();
        }
    }
    return intervals;
}

bool conflicting_pedals(const Track<Tick>& track) {
    if (track.pedals->empty()) return false;
    vec<std::pair<i32, i32>> intervals;
    for (const auto& pedal : *track.pedals) intervals.emplace_back(pedal.time, pedal.duration);
    std::ranges::sort(intervals);
    return intervals != pedal_intervals(*track.controls);
}

vec<std::pair<i32, i32>> ScoreWriter::pedals(const Track<Tick>& track, std::string_view where) {
    vec<std::pair<i32, i32>> intervals;
    for (const auto& pedal : *track.pedals) {
        if (pedal.duration < 0) ExportIssues::invalid(where, "pedal duration cannot be negative");
        const auto start = scale(pedal.time, where);
        const auto end   = scale(static_cast<int64_t>(pedal.time) + pedal.duration, where);
        if (start != end) intervals.emplace_back(start, end);
    }
    return intervals;
}

Timeline ScoreWriter::controls(const Track<Tick>& track, std::string_view where) {
    Timeline timeline;
    auto     intervals         = pedals(track, where);
    bool     has_pedal_control = false;
    for (const auto& event : *track.controls) {
        if (event.number > midi_max || event.value > midi_max)
            ExportIssues::invalid(where, "control number and value must be in MIDI range 0–127");
        has_pedal_control = has_pedal_control || event.number == 64;
        timeline[scale(event.time, where)].fields.push_back(
            fmt::format("[I:MIDI control {} {}]", event.number, event.value)
        );
    }
    // Prefer explicit CC64: it can carry half-pedal values.
    if (!has_pedal_control)
        add_pedals(timeline, std::move(intervals), where);
    else if (conflicting_pedals(track))
        issues.warn(
            where,
            "pedal-conflict",
            "pedal intervals disagree with CC64; kept the explicit controls"
        );
    return timeline;
}

void ScoreWriter::add_pedals(
    Timeline& timeline, vec<std::pair<i32, i32>> pedals, std::string_view where
) {
    if (pedals.empty()) return;
    std::ranges::sort(pedals);
    auto [start, end] = pedals.front();
    const auto append = [&] {
        timeline[start].fields.push_back("[I:MIDI control 64 127]");
        timeline[end].fields.push_back("[I:MIDI control 64 0]");
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

int ScoreWriter::next_voice(std::string_view where) {
    if (++voices > max_export_voices)
        ExportIssues::invalid(
            where, "export needs more than 15 ABC voices, the abc2midi channel limit"
        );
    return voices;
}

void ScoreWriter::track(const Track<Tick>& track, size_t index) {
    if (!utf8::is_valid(track.name.begin(), track.name.end()))
        ExportIssues::invalid(fmt::format("track {}", index + 1), "track name must be valid UTF-8");
    const auto where = fmt::format("track {} ('{}')", index + 1, escape_text(track.name));
    if (track.program > midi_max)
        ExportIssues::invalid(where, "program must be in MIDI range 0–127");
    limitations(track, where);
    auto       lanes          = partition(notes(track, where), issues, where);
    const auto control_fields = controls(track, where);
    for (auto& lane : lanes) {
        for (const auto& [tick, change] : control_fields)
            lane.timeline[tick].fields = change.fields;
        if (lane.timeline.empty()) continue;
        if (voices == 0) {
            for (const auto& [tick, change] : global) {
                auto& fields = lane.timeline[tick].fields;
                fields.insert(fields.begin(), change.fields.begin(), change.fields.end());
            }
        }
        VoiceWriter(output, std::move(lane.timeline), budget, where)
            .write(next_voice(where), track.name, track.program, track.is_drum);
    }
}

int ScoreWriter::onset_offset() {
    for (const auto& track : *score.tracks) {
        for (const auto& note : *track->notes) {
            if (is_audible(note) && scale(note.time, "note") == 0) {
                issues.warn(
                    "score",
                    "note-start-offset",
                    "shifted all note onsets one tick because a note starts at tick zero"
                );
                return 0;
            }
        }
    }
    return 1;
}

Result<std::string> ScoreWriter::write() {
    if (score.ticks_per_quarter <= 0)
        ExportIssues::invalid("score", "ticks per quarter must be positive");
    if (!score.markers->empty())
        issues.warn(
            "score", "omitted-markers", "ABC export does not represent arbitrary timed markers"
        );
    header();
    start_offset = onset_offset();
    for (size_t i = 0; i < score.tracks->size(); ++i) track(*(*score.tracks)[i], i);
    if (voices == 0) VoiceWriter(output, std::move(global), budget, "score").write(1, "", 0, false);
    return {std::move(output), issues.take()};
}

}   // namespace

Result<std::string> write(const Score<Tick>& score) { return ScoreWriter(score).write(); }

}   // namespace symusic::abc

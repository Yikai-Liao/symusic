#include "symusic/io/abc.h"

#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <limits>
#include <map>
#include <numeric>
#include <ranges>
#include <set>
#include <span>

#include <utf8.h>

#include "settings.h"
#include "symusic/detail/time_conversion.h"

namespace symusic::abc {
namespace {

using detail::midi_max;
using detail::PlaybackGrid;
using detail::PlaybackWork;
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

void check_tempo(i32 mspq) {
    if (mspq < 1 || mspq > midi_tempo_max)
        ExportIssues::invalid("tempo", "microseconds per quarter must fit the 24-bit MIDI range");
}

std::string tempo_text(i32 mspq, ExportIssues& issues) {
    check_tempo(mspq);
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
    size_t remaining = PlaybackWork::limit;
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

    void token(std::string_view text, size_t steps = PlaybackWork::token);
    void interval(i32 start, i32 end);
    void atom(i32 duration, i32 end);
    void moment(const Moment& change);
};

void VoiceWriter::token(std::string_view text, size_t steps) {
    budget.spend(steps + PlaybackWork::text(text), where);
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
    budget.spend(PlaybackWork::token + PlaybackWork::text(declaration), where);
    output += declaration;
    output += '\n';
    token("[I:MIDI gchordoff]");
    if (drums) token(fmt::format("[I:MIDI channel {}]", detail::midi_drum_channel));
    token(fmt::format("[I:MIDI program {}]", program));
    token("[I:MIDI chordattack 0]");
    token(fmt::format("[I:MIDI expand {}/{}]", PlaybackGrid::export_release_extension, tpq));
    i32 previous = 0;
    for (const auto& [time, change] : timeline) {
        if (time > previous) interval(previous, time);
        moment(change);
        previous = time;
    }
    // A one-tick tail allows the final expanded note to release before EOF.
    atom(PlaybackGrid::export_tail, previous + PlaybackGrid::export_tail);
    token("|]");
    output += '\n';
}

// Convert original endpoints once, on the ABC grid. In particular, a short
// positive interval must not be confused with a zero-duration source event.
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

    i32 scale(long double time, std::string_view where, ExportIssues& issues) const {
        if (!std::isfinite(time) || time < 0)
            ExportIssues::invalid(where, "event time must be finite and nonnegative");
        const auto exact   = on_grid(time, where);
        const auto rounded = std::round(exact);
        if (rounded > std::numeric_limits<i32>::max() - 2)
            ExportIssues::invalid(where, "rescaled event leaves no room for the ABC note release");
        if (rounded > midi_delta_max)
            issues.warn(
                where, "midi-delta-range", "very late events may exceed the MIDI delta-time range"
            );
        if (std::abs(rounded - exact) > grid_noise(exact))
            issues.warn(where, "resampled-time", "rounded times to the ABC 480-tick grid");
        return static_cast<i32>(rounded);
    }

private:
    i32 source_tpq;
    std::conditional_t<
        std::is_same_v<T, Second>,
        std::optional<details::SecondToQuarter<f64>>,
        std::monostate>
        seconds;

    long double on_grid(long double time, std::string_view where) const {
        if constexpr (std::is_same_v<T, Tick>) {
            if (time > std::numeric_limits<i32>::max())
                ExportIssues::invalid(where, "event time is outside the score's tick range");
            return time * tpq / source_tpq;
        } else if constexpr (std::is_same_v<T, Quarter>) {
            return time * tpq;
        } else {
            return static_cast<long double>(seconds->time_value(static_cast<f64>(time))) * tpq;
        }
    }

    static long double grid_noise(long double ticks) {
        if constexpr (std::is_same_v<T, Tick>) return 0;
        // Ignore floating storage noise near ordinary grid positions, without
        // hiding larger rounding errors in long floating-point scores.
        return std::min(0.001L, std::numeric_limits<typename T::unit>::epsilon() * ticks);
    }
};

// Plan voices before placing global events: partitioning can change the
// tempo clock even if the original Score had only one track.
template<TType T>
struct PlannedVoice {
    Timeline        timeline;
    std::string     where;
    const Track<T>* track;
};

template<TType T>
class ScoreWriter {
public:
    explicit ScoreWriter(const Score<T>& score) : score(score), clock(score) {}
    Result<std::string> write();

private:
    const Score<T>&      score;
    ExportClock<T>       clock;
    ExportIssues         issues;
    ExportBudget         budget;
    ExportBudget         planned_intervals;
    std::string          output;
    Timeline             global;
    vec<PlannedVoice<T>> voices;
    int                  start_offset = 0;

    i32 scale(long double time, std::string_view where) { return clock.scale(time, where, issues); }
    vec<WrittenNote>         notes(const Track<T>& track, std::string_view where);
    Timeline                 channel_events(const Track<T>& track, std::string_view where);
    vec<std::pair<i32, i32>> pedals(const Track<T>& track, std::string_view where);
    void add_pedals(Timeline& timeline, vec<std::pair<i32, i32>> pedals, std::string_view where);
    void limitations(const Track<T>& track, std::string_view where);
    void header();
    void render_voices();
    void plan_track(const Track<T>& track, size_t index);
    int  onset_offset();

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
        if (!legacy_sustain_fits(end - start))
            issues.warn(
                where, "midi-sustain-range", "abc2midi may shorten an unusually long held note"
            );
        result.push_back({start - start_offset, end - start_offset, note.pitch, note.velocity});
    }
    return result;
}

template<TType T>
std::string meter_text(const TimeSignature<T>& event) {
    if (!event.numerator || !event.denominator || !std::has_single_bit(event.denominator)) {
        ExportIssues::invalid(
            "meter", "expected a positive numerator and a power-of-two denominator"
        );
    }
    return fmt::format("{}/{}", event.numerator, event.denominator);
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
        const auto playback_time
            = key == 'Q' ? time - PlaybackGrid::tempo_offset(voices.size() > 1) : time;
        global[playback_time].fields.push_back(fmt::format("[{}:{}]", key, value));
    }
    return fallback;
}

template<TType T>
void ScoreWriter<T>::header() {
    const auto m     = fields('M', *score.time_signatures, "4/4", meter_text<T>);
    const auto k     = fields('K', *score.key_signatures, "C", key_text<T>);
    const auto q     = fields('Q', *score.tempos, "1/4=120", [&](const auto& event) {
        return tempo_text(event.mspq, issues);
    });
    const auto title = score.tracks->empty() ? "Untitled" : score.tracks->front()->name;
    output = fmt::format("X:1\nT:{}\nM:{}\nL:1/4\nQ:{}\nK:{}\n", escape_text(title), m, q, k);
}

template<TType T>
void ScoreWriter<T>::limitations(const Track<T>& track, std::string_view where) {
    if (!track.lyrics->empty())
        issues.warn(
            where, "omitted-lyrics", "ABC export does not represent arbitrary timed lyrics"
        );
    if (track.is_drum && track.program != 0)
        issues.warn(where, "drum-program", "other ABC readers may ignore the drum program");
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
    const auto wire = value + detail::midi_bend_center;
    return fmt::format("[I:MIDI pitchbend {} {}]", wire & midi_max, wire >> 7);
}

template<TType T>
Timeline ScoreWriter<T>::channel_events(const Track<T>& track, std::string_view where) {
    Timeline                 timeline;
    auto                     intervals = pedals(track, where);
    vec<ControlChange<Tick>> pedal_controls;
    for (const auto& event : *track.controls) {
        if (event.number > midi_max || event.value > midi_max)
            ExportIssues::invalid(where, "control number and value must be in MIDI range 0–127");
        const auto at = scale(event.time, where);
        if (event.number == 64) pedal_controls.emplace_back(at, event.number, event.value);
        timeline[at].fields.push_back(
            fmt::format("[I:MIDI control {} {}]", event.number, event.value)
        );
    }
    for (const auto& bend : *track.pitch_bends)
        timeline[scale(bend.time, where)].fields.push_back(bend_field(bend.value, where));
    // Prefer explicit CC64: it can carry half-pedal values. Compare only
    // what reaches the playback grid; float storage error is not a conflict.
    if (pedal_controls.empty())
        add_pedals(timeline, std::move(intervals), where);
    else if (!intervals.empty()) {
        std::ranges::sort(intervals);
        if (intervals != pedal_intervals(std::move(pedal_controls)))
            issues.warn(
                where,
                "pedal-conflict",
                "pedal intervals disagree with CC64; kept the explicit controls"
            );
    }
    return timeline;
}

template<TType T>
void ScoreWriter<T>::add_pedals(
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

template<TType T>
void ScoreWriter<T>::plan_track(const Track<T>& track, size_t index) {
    if (!utf8::is_valid(track.name.begin(), track.name.end()))
        ExportIssues::invalid(fmt::format("track {}", index + 1), "track name must be valid UTF-8");
    const auto where = fmt::format("track {} ('{}')", index + 1, escape_text(track.name));
    if (track.program > midi_max)
        ExportIssues::invalid(where, "program must be in MIDI range 0–127");
    limitations(track, where);
    auto       lanes          = partition(notes(track, where), issues, where);
    const auto control_fields = channel_events(track, where);
    for (auto& lane : lanes) {
        for (const auto& [tick, change] : control_fields)
            lane.timeline[tick].fields = change.fields;
        if (lane.timeline.empty()) continue;
        if (voices.size() >= max_export_voices)
            ExportIssues::invalid(
                where, "export needs more than 15 ABC voices, the abc2midi channel limit"
            );
        // Every scheduled tick needs at least one token (including the tail).
        // Bound the timelines retained before rendering spends the full budget.
        planned_intervals.spend(lane.timeline.size() * PlaybackWork::token, where);
        voices.push_back({std::move(lane.timeline), where, &track});
    }
}

template<TType T>
void ScoreWriter<T>::render_voices() {
    if (voices.empty()) {
        VoiceWriter(output, std::move(global), budget, "score").write(1, "", 0, false);
        return;
    }
    auto& first = voices.front().timeline;
    for (const auto& [tick, change] : global) {
        auto& fields = first[tick].fields;
        fields.insert(fields.begin(), change.fields.begin(), change.fields.end());
    }
    int id = 0;
    for (auto& voice : voices) {
        const auto& track = *voice.track;
        VoiceWriter(output, std::move(voice.timeline), budget, voice.where)
            .write(++id, track.name, track.program, track.is_drum);
    }
}

template<TType T>
int ScoreWriter<T>::onset_offset() {
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

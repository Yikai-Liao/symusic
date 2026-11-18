#include "accompaniment.h"

#include <cmath>
#include <limits>
#include <span>

namespace symusic::abc::detail {
namespace {

constexpr long double epsilon = 0.0000001L;

struct ChordType {
    std::string_view           name;
    std::initializer_list<int> intervals;
};

const ChordType chord_types[] = {
    {"", {0, 4, 7}},
    {"maj", {0, 4, 7}},
    {"m", {0, 3, 7}},
    {"min", {0, 3, 7}},
    {"7", {0, 4, 7, 10}},
    {"m7", {0, 3, 7, 10}},
    {"maj7", {0, 4, 7, 11}},
    {"M7", {0, 4, 7, 11}},
    {"6", {0, 4, 7, 9}},
    {"m6", {0, 3, 7, 9}},
    {"aug", {0, 4, 8}},
    {"+", {0, 4, 8}},
    {"aug7", {0, 4, 8, 10}},
    {"dim", {0, 3, 6}},
    {"dim7", {0, 3, 6, 9}},
    {"dim9", {0, 3, 6, 9, 13}},
    {"m7b5", {0, 3, 6, 10}},
    {"sus", {0, 5, 7}},
    {"sus4", {0, 5, 7}},
    {"sus2", {0, 2, 7}},
    {"sus9", {0, 2, 5, 9}},
    {"7sus2", {0, 2, 7, 10}},
    {"7sus4", {0, 5, 7, 10}},
    {"7sus9", {0, 2, 7, 10}},
    {"9", {0, 4, 7, 10, 14}},
    {"m9", {0, 3, 7, 10, 14}},
    {"maj9", {0, 4, 7, 11, 14}},
    {"M9", {0, 4, 7, 11, 14}},
    {"add9", {0, 4, 7, 14}},
    {"11", {0, 4, 7, 10, 14, 17}},
    {"13", {0, 4, 7, 10, 14, 21}},
    {"5", {0, 7}},
    {"7b5", {0, 4, 6, 10}},
    {"7#5", {0, 4, 8, 10}},
};

std::optional<int> root_note(std::string_view& symbol) {
    if (symbol.empty() || !note_letter(symbol.front())) return {};
    auto note = natural_pitch[letter_index(upper(symbol.front()))];
    symbol.remove_prefix(1);
    if (symbol.starts_with('#') || symbol.starts_with('b')) {
        note += symbol.front() == '#' ? 1 : -1;
        symbol.remove_prefix(1);
    }
    return (12 + note) % 12;
}


void add_if_audible(TrackNative<Tick>& track, i32 time, i32 duration, int pitch, int velocity) {
    if (pitch < 0 || pitch > midi_max || velocity <= 0) return;
    track.notes.emplace_back(time, duration, static_cast<i8>(pitch), static_cast<i8>(velocity));
}

void close_backing(TrackNative<Tick>& track, i32 end) {
    for (auto& note : track.notes)
        note.duration = std::min(note.duration, std::max(0, end - note.time));
}

}   // namespace

Accompaniment::Accompaniment(Meter meter, Diagnostics& diagnostics) :
    diagnostics(diagnostics), current_meter(meter) {
    auto defaults = default_chords(meter);
    if (defaults.empty()) defaults = "x";
    chords.install(pattern(defaults, "fcbzx", {}), meter, 0);
}

std::string_view Accompaniment::default_chords(Meter meter) {
    struct DefaultPattern {
        int              numerator;
        int              denominator;
        std::string_view sequence;
    };
    static constexpr DefaultPattern defaults[]{
        {2, 2, "fzczfzcz"},
        {2, 4, "fzcz"},
        {3, 4, "fzczcz"},
        {3, 8, "fzczcz"},
        {4, 4, "fzczfzcz"},
        {5, 4, "fzbcz"},
        {5, 8, "fzbcz"},
        {6, 4, "fzczfzczfzcz"},
        {6, 8, "fzcfzc"},
        {7, 4, "fzbczcz"},
        {7, 8, "fzbczcz"},
        {9, 8, "fzcfzcfzc"},
        {12, 8, "fzcfzcfzcfzc"}
    };
    for (const auto& entry : defaults) {
        if (entry.numerator == meter.numerator && entry.denominator == meter.denominator)
            return entry.sequence;
    }
    return {};
}

void Accompaniment::Pattern::restart(long double now) {
    next  = now;
    index = 0;
}

void Accompaniment::Pattern::bar(long double now) {
    if (bars == 1 || index >= steps.size()) restart(now);
}

void Accompaniment::Pattern::set_span(int span, long double now) {
    bars = span;
    restart(now);
}

void Accompaniment::Pattern::install(vec<Step> replacement, Meter meter, long double now) {
    const bool first = steps.empty();
    steps            = std::move(replacement);
    units            = 0;
    for (const auto& step : steps) units += step.length;
    cycle = meter.measure() * bars;
    // If this is the first definition, locate its first slot on or after
    // the change. Earlier slots cannot sound retroactively.
    if (first) {
        while (ready(now)) consume();
    }
}

bool Accompaniment::Pattern::ready(long double end) const {
    return index < steps.size() && next < end - epsilon;
}

long double Accompaniment::Pattern::consume() {
    // MIDI's release queue truncates each rational slot independently.
    const auto length = std::floor(cycle * steps[index++].length / units + epsilon);
    next += length;
    return length;
}

template<class Play>
void Accompaniment::Pattern::advance(
    long double end, Location at, Diagnostics& diagnostics, Play play
) {
    while (ready(end)) {
        diagnostics.spend(1, at);
        const auto time   = next;
        const auto step   = steps[index];
        const auto length = consume();
        if (enabled) play(step, time, length);
    }
}

int step_length(std::string_view& rest, Diagnostics& diagnostics, Location at) {
    size_t digits = 0;
    while (digits < rest.size() && digit(rest[digits])) ++digits;
    if (!digits) return 1;
    const auto count = integer(rest.substr(0, digits));
    rest.remove_prefix(digits);
    if (!count || *count < 1 || *count > 1024)
        diagnostics.error(
            at, "backing-pattern-range", "backing step length must be between 1 and 1024"
        );
    return *count;
}

vec<Accompaniment::Step> Accompaniment::pattern(
    std::string_view text, std::string_view allowed, Location at
) {
    vec<Step> result;
    while (!text.empty()) {
        const char kind = text.front();
        text.remove_prefix(1);
        if (allowed.find(kind) == allowed.npos)
            diagnostics.error(
                at,
                "invalid-backing-pattern",
                fmt::format(
                    "unexpected byte {:#04x} in MIDI backing pattern",
                    static_cast<unsigned char>(kind)
                )
            );
        if (result.size() == 1024)
            diagnostics.error(
                at, "backing-pattern-range", "MIDI backing pattern exceeds 1024 steps"
            );
        result.push_back({kind, step_length(text, diagnostics, at)});
    }
    if (result.empty())
        diagnostics.error(at, "invalid-backing-pattern", "expected a MIDI backing pattern");
    return result;
}

void Accompaniment::chord_pattern(std::string_view args, long double time, Location at) {
    chords.install(pattern(word(args), "fcbghijkGHIJKz", at), current_meter, time);
}

void Accompaniment::drum_pattern(std::string_view args, long double time, Location at) {
    auto steps = pattern(word(args), "dz", at);
    // Omitted values default independently for each strike, not each slot.
    for (auto& step : steps) {
        if (step.kind != 'd') continue;
        step.pitch    = trim(args).empty() ? 35 : midi_integer(args, 0, midi_max, diagnostics, at);
        step.velocity = 80;
    }
    for (auto& step : steps) {
        if (trim(args).empty()) break;
        if (step.kind == 'd') step.velocity = midi_integer(args, 1, midi_max, diagnostics, at);
    }
    if (!trim(args).empty())
        diagnostics.warn(at, "extra-drum-values", "ignored values beyond the drum strikes");
    drums.install(std::move(steps), current_meter, time);
}

void Accompaniment::chord_type(std::string_view args, Location at) {
    const auto       name = word(args);
    vec<int>         intervals;
    constexpr size_t max_intervals = 10;
    while (!trim(args).empty() && intervals.size() < max_intervals)
        intervals.push_back(midi_integer(args, -48, 48, diagnostics, at));
    if (intervals.empty() || !trim(args).empty()) {
        diagnostics.warn(
            at, "invalid-chord-name", "ignored chordname without one to ten intervals"
        );
        return;
    }
    custom_chords.insert_or_assign(name, std::move(intervals));
}

std::optional<int> slash_bass(std::string_view bass, Location at, Diagnostics& diagnostics) {
    bass            = bass.substr(0, bass.find_first_of(" )("));
    const auto note = root_note(bass);
    if (note && bass.empty()) return note;
    diagnostics.warn(at, "invalid-chord-bass", "ignored an invalid slash-chord bass note");
    return {};
}

std::span<const int> Accompaniment::chord_intervals(std::string_view name, Location at) const {
    const auto custom = custom_chords.find(name);
    if (custom != custom_chords.end()) return custom->second;
    const auto standard = std::ranges::find(chord_types, name, &ChordType::name);
    if (standard != std::end(chord_types)) return standard->intervals;
    diagnostics.warn(
        at, "unknown-chord", fmt::format("ignored unrecognized chord quality '{}'", name)
    );
    return {};
}

// Inversions move the voices before the selected chord tone up an octave.
// Compare pitch classes: the selected tone may cross an octave boundary.
vec<int> voiced_chord(std::span<const int> intervals, int root, std::optional<int> bass_note) {
    size_t inversion = 0;
    if (bass_note) {
        for (size_t index = 0; index < intervals.size(); ++index) {
            if ((root + intervals[index] - *bass_note) % 12 == 0) inversion = index;
        }
    }
    vec<int> pitches;
    pitches.reserve(intervals.size());
    for (size_t index = 0; index < intervals.size(); ++index) {
        pitches.push_back(48 + root + intervals[index] + (index < inversion ? 12 : 0));
    }
    if (inversion) std::ranges::sort(pitches);
    return pitches;
}

void Accompaniment::harmony(std::string_view symbol, int transpose, Location at) {
    if (symbol.starts_with('(')) symbol.remove_prefix(1);
    const bool bass_only = !symbol.empty() && lower(symbol.front());
    const auto root      = root_note(symbol);
    if (!root) return;   // quoted annotations are not chord symbols
    const auto name      = symbol.substr(0, symbol.find_first_of(" /()"));
    auto       bass_note = symbol.substr(name.size());
    const auto inversion = bass_note.starts_with('/') ?
                               slash_bass(bass_note.substr(1), at, diagnostics) :
                               std::nullopt;
    Chord      next{36 + inversion.value_or(*root) + transpose, {}, at};
    if (!bass_only) {
        const auto intervals = chord_intervals(name, at);
        if (intervals.empty()) return;
        next.pitches = voiced_chord(intervals, *root, inversion);
        for (auto& pitch : next.pitches) pitch += transpose;
    }
    current_chord = std::move(next);
}

void Accompaniment::meter(Meter change, long double time) {
    current_meter = change;
    if (auto defaults = default_chords(change); !defaults.empty())
        chords.install(pattern(defaults, "fcbz", {}), change, time);
}

void Accompaniment::bar(long double time) {
    chords.bar(time);
    drums.bar(time);
}

void Accompaniment::Instrument::play(int pitch, i32 time, i32 duration) {
    auto& track = tracks.try_emplace(program, "", static_cast<u8>(program), false).first->second;
    add_if_audible(track, time, duration, pitch + 12 * octave, velocity);
}

void Accompaniment::Instrument::finish(vec<TrackNative<Tick>>& output, i32 end) {
    for (auto& [program, track] : tracks) {
        if (track.empty()) continue;
        close_backing(track, end);
        output.push_back(std::move(track));
    }
}

std::optional<int> Accompaniment::Chord::arpeggio(char action) const {
    if (pitches.empty()) return {};
    const auto index = static_cast<size_t>(upper(action) - 'G');
    return pitches[std::min(index, pitches.size() - 1)] - (lower(action) ? 0 : 12);
}

void Accompaniment::chord_step(Step step, long double time, long double length, Location at) {
    if (!current_chord || step.kind == 'z') return;
    if (step.kind == 'x') {
        diagnostics.warn(
            current_chord->at, "no-default-backing", "no default guitar pattern for this meter"
        );
        return;
    }
    const auto& [root, pitches, chord_at] = *current_chord;
    diagnostics.spend(pitches.size(), at);
    const auto [start, duration] = backing_times(time, length, at);
    const auto action            = step.kind == 'c' && pitches.empty() ? 'f' : step.kind;
    switch (action) {
    case 'f': bass.play(root, start, duration); return;
    case 'b': bass.play(root, start, duration); [[fallthrough]];
    case 'c':
        for (const auto pitch : pitches) chord.play(pitch, start, duration);
        return;
    default: break;
    }
    if (const auto pitch = current_chord->arpeggio(action)) chord.play(*pitch, start, duration);
}

std::pair<i32, i32> Accompaniment::backing_times(
    long double time, long double length, Location at
) {
    const auto onset    = tick(time + PlaybackGrid::attack_offset, diagnostics, at);
    const auto duration = std::max(0, tick(length, diagnostics, at) - PlaybackGrid::release_trim);
    (void)tick(static_cast<long double>(onset) + duration, diagnostics, at);
    return {onset, duration};
}

void Accompaniment::drum_step(Step step, long double time, long double length, Location at) {
    if (step.kind != 'd') return;
    const auto [onset, duration] = backing_times(time, length, at);
    add_if_audible(drum_track, onset, duration, step.pitch, step.velocity);
}

void Accompaniment::advance(long double end, Location at) {
    chords.advance(
        end, at, diagnostics, [this, at](Step step, long double time, long double length) {
            chord_step(step, time, length, at);
        }
    );
    drums.advance(
        end, at, diagnostics, [this, at](Step step, long double time, long double length) {
            drum_step(step, time, length, at);
        }
    );
}

void Accompaniment::close_drone(long double time, Location at) {
    if (!drone.start || time <= *drone.start) return;
    auto& track = drone.tracks.try_emplace(drone.program, "", static_cast<u8>(drone.program), false)
                      .first->second;
    const auto start    = tick(*drone.start + 1, diagnostics, at);
    const auto duration = std::max(0, tick(time, diagnostics, at) - start);
    for (size_t n = 0; n < drone.pitch.size(); ++n)
        add_if_audible(track, start, duration, drone.pitch[n], drone.velocity[n]);
    drone.start.reset();
}

void Accompaniment::drone_settings(std::string_view args, long double time, Location at) {
    const bool playing = drone.start.has_value();
    close_drone(time, at);
    for (auto* value :
         {&drone.program,
          &drone.pitch[0],
          &drone.pitch[1],
          &drone.velocity[0],
          &drone.velocity[1]}) {
        if (trim(args).empty()) break;
        const auto next = midi_integer(args, -127, midi_max, diagnostics, at);
        if (next > 0) *value = next;
    }
    if (playing) drone.start = time;
}

bool Accompaniment::pattern_directive(
    std::string_view name, std::string_view args, long double time, Location at
) {
    auto& pattern = name.starts_with("drum") ? drums : chords;
    if (name == "gchord")
        chord_pattern(args, time, at);
    else if (name == "drum")
        drum_pattern(args, time, at);
    else if (name == "gchordbars" || name == "drumbars")
        pattern.set_span(midi_integer(args, 1, 32, diagnostics, at), time);
    else if (name == "gchordon" || name == "gchordoff" || name == "drumon" || name == "drumoff")
        pattern.set_enabled(name.ends_with("on"));
    else
        return false;
    return true;
}

bool Accompaniment::instrument_directive(
    std::string_view name, std::string_view args, Location at
) {
    auto& instrument = name.starts_with("bass") ? bass : chord;
    if (name == "chordname")
        chord_type(args, at);
    else if (name == "bassprog" || name == "chordprog") {
        instrument.program = midi_integer(args, 0, midi_max, diagnostics, at);
        if (auto options = word(args); options.starts_with("octave=")) {
            auto octave       = options.substr(7);
            instrument.octave = midi_integer(octave, -2, 2, diagnostics, at);
        }
    } else if (name == "bassvol" || name == "chordvol")
        instrument.velocity = midi_integer(args, 0, midi_max, diagnostics, at);
    else
        return false;
    return true;
}

bool Accompaniment::drone_directive(
    std::string_view name, std::string_view args, long double time, Location at
) {
    if (name == "drone")
        drone_settings(args, time, at);
    else if (name == "droneon") {
        if (!drone.start) drone.start = time;
    } else if (name == "droneoff") {
        close_drone(time, at);
        drone.start.reset();
    } else
        return false;
    return true;
}

bool Accompaniment::directive(
    std::string_view name, std::string_view args, long double time, Location at
) {
    return pattern_directive(name, args, time, at) || instrument_directive(name, args, at)
           || drone_directive(name, args, time, at);
}

void Accompaniment::finish(vec<TrackNative<Tick>>& tracks, long double end, Location at) {
    close_drone(end, at);
    // An unfinished pattern releases at the end of the melody, not at
    // the next scheduled backing step.
    const auto stop = tick(
        std::min<long double>(end + PlaybackGrid::attack_offset, std::numeric_limits<i32>::max()),
        diagnostics,
        at
    );
    bass.finish(tracks, stop);
    chord.finish(tracks, stop);
    if (!drum_track.empty()) {
        close_backing(drum_track, stop);
        tracks.push_back(std::move(drum_track));
    }
    for (auto& [program, track] : drone.tracks) {
        if (!track.empty()) tracks.push_back(std::move(track));
    }
}

}   // namespace symusic::abc::detail

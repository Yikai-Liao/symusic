#include "accompaniment.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <ranges>
#include <span>

namespace symusic::abc::detail {
namespace {

constexpr long double epsilon = 0.0000001L;

struct ChordType {
    std::string_view           name;
    std::initializer_list<int> intervals;
};

// The exact spellings registered by abc2midi's setup_chordnames().
const ChordType chord_types[] = {
    {"", {0, 4, 7}},
    {"m", {0, 3, 7}},
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
    {"11", {0, 4, 7, 10, 14, 17}},
    {"5", {0, 7}},
};

// Additional qualities recognized by the native reader.
const ChordType extra_chord_types[] = {
    {"maj", {0, 4, 7}},
    {"add9", {0, 4, 7, 14}},
    {"13", {0, 4, 7, 10, 14, 21}},
    {"7b5", {0, 4, 6, 10}},
    {"7#5", {0, 4, 8, 10}},
};

std::optional<int> root_note(std::string_view& symbol) {
    if (symbol.empty() || !note_letter(symbol.front())) return {};
    auto note = natural_pitch[letter_index(upper(symbol.front()))];
    symbol.remove_prefix(1);
    // ABC 2.1 sections 4.18 and 8.2: roots and slash basses accept musical glyphs.
    // abc2midi also accepts doubled ASCII accidentals. Match them before single signs.
    constexpr std::pair<std::string_view, int> kAccidentals[] = {
        {"##", 2}, {"bb", -2}, {"#", 1}, {"b", -1}, {"\u266f", 1}, {"\u266d", -1}, {"\u266e", 0}
    };
    for (const auto& [mark, shift] : kAccidentals) {
        if (!symbol.starts_with(mark)) continue;
        symbol.remove_prefix(mark.size());
        note += shift;
        break;
    }
    // Retain the written register: C-flat is below C, while B-sharp is above B.
    return note;
}

struct ChordSpelling {
    int              root;
    bool             bass_only;
    std::string_view quality;
    std::string_view suffix;
};

// If decoding was needed, the returned views borrow the caller's buffer.
std::optional<ChordSpelling> chord_spelling(std::string_view symbol, std::string& decoded) {
    if (!symbol.empty() && std::string_view("^_<>@").find(symbol.front()) != symbol.npos) return {};
    if (symbol.find_first_of("\\&") != symbol.npos) {
        decoded = decode_text(symbol, TextContext::ChordSymbol);
        symbol  = decoded;
    }
    if (symbol.starts_with('(')) symbol.remove_prefix(1);
    const bool bass_only = !symbol.empty() && lower(symbol.front());
    const auto root      = root_note(symbol);
    if (!root) return {};
    const auto quality = symbol.substr(0, symbol.find_first_of(" /()"));
    return ChordSpelling{*root, bass_only, quality, symbol.substr(quality.size())};
}

std::span<const int> chord_type(std::span<const ChordType> types, std::string_view quality) {
    const auto chord = std::ranges::find(types, quality, &ChordType::name);
    return chord == types.end() ? std::span<const int>{} : chord->intervals;
}

std::span<const int> standard_chord(std::string_view quality) {
    std::string canonical;
    if (quality.starts_with("min")) {
        canonical = fmt::format("m{}", quality.substr(3));
        quality   = canonical;
    } else if (quality.starts_with('+')) {
        canonical = fmt::format("aug{}", quality.substr(1));
        quality   = canonical;
    } else if (quality == "maj6") {
        quality = "6";
    }
    if (const auto intervals = chord_type(chord_types, quality); !intervals.empty())
        return intervals;
    return chord_type(extra_chord_types, quality);
}
void add_if_audible(TrackNative<Tick>& track, i32 time, i32 duration, i64 pitch, int velocity) {
    if (pitch < 0 || pitch > midi_max || velocity <= 0) return;
    track.notes.emplace_back(time, duration, static_cast<i8>(pitch), static_cast<i8>(velocity));
}

void close_backing(TrackNative<Tick>& track, i32 end) {
    for (auto& note : track.notes)
        note.duration = std::min(note.duration, std::max(0, end - note.time));
}

std::optional<bool> drum_gate_command(std::string_view command) {
    if (command == "drumon") return true;
    if (command == "drumoff") return false;
    return {};
}

int backing_span(std::string_view args) {
    const int bars = integer(word(args)).value_or(0);
    // abc2midi replaces an out-of-range span with the one-bar default.
    return bars >= 1 && bars <= 10 ? bars : 1;
}

}   // namespace

Accompaniment::Accompaniment(const ProgramNumbers& programs, Diagnostics& diagnostics) :
    diagnostics(diagnostics), programs(programs) {}

void Accompaniment::open_lane(
    int lane, Meter meter, long double bar_origin, const BackingOpening& opening
) {
    // OverlayLanes creates lane IDs consecutively and can select a lane again.
    if (static_cast<size_t>(lane) < harmonic_parts.size()) return;
    auto defaults = default_chords(meter);
    if (defaults.empty()) defaults = "x";
    auto& part               = harmonic_parts.emplace_back();
    part.transposition       = opening.chord_shift;
    part.drone.transposition = opening.drone_shift;
    part.pattern.set_gap(opening.chord_gap);
    part.pattern.install(pattern(defaults, "fcbzx", {}), meter);
    part.pattern.bar(bar_origin);
}

void Accompaniment::transpose(int lane, MidiTransposition change) {
    auto& part               = harmonic_parts[lane];
    part.transposition       = change.apply(part.transposition);
    part.drone.transposition = change.apply(part.drone.transposition);
}

void discover_backing(const Token& token, BackingTracks& tracks) {
    auto command = token.information().value_or("");
    if (word(command) == "MIDI") {
        const auto name = word(command);
        if (drum_gate_command(name).value_or(false)) tracks.drums = true;
        if (name == "droneon") tracks.drone = true;
    }
    if (const auto* harmony = std::get_if<Harmony>(&token.value))
        tracks.chords = tracks.chords || (harmony->voicing && harmony->voicing->declares_backing);
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

void Accompaniment::BackingQueue::Release::stop(i32 tick) const {
    auto& note    = track->notes[index];
    note.duration = std::max(0, tick - note.time);
}

void Accompaniment::BackingQueue::enqueue(Pending item) {
    // The MIDI converter bounds its queue at 50 pending slots and releases.
    if (pending.size() >= 50) return;
    const auto after = std::ranges::upper_bound(pending, item.time, {}, &Pending::time);
    if (after != pending.end()) {
        const auto pull = std::min<long double>(after->time - item.time, gap);
        for (auto& entry : std::ranges::subrange(after, pending.end())) entry.time -= pull;
    }
    pending.insert(after, item);
}

void Accompaniment::BackingQueue::slot(long double time, size_t index, bool unique) {
    if (unique) {
        const auto same_time = std::ranges::equal_range(pending, time, {}, &Pending::time);
        if (std::ranges::any_of(same_time, [&](const Pending& entry) {
                const auto* slot = std::get_if<Slot>(&entry.action);
                return slot && slot->index == index;
            }))
            return;
    }
    enqueue({time, Slot{index}});
}

void Accompaniment::BackingQueue::release(
    long double time, TrackNative<Tick>& track, size_t index
) {
    enqueue({time, Release{&track, index}});
}

template<class Play>
void Accompaniment::BackingQueue::advance(
    long double end, Location at, Diagnostics& diagnostics, Play play
) {
    while (!pending.empty() && pending.front().time < end - epsilon) {
        const auto item = pending.front();
        pending.erase(pending.begin());
        diagnostics.spend(1, at);
        if (const auto* release = std::get_if<Release>(&item.action))
            release->stop(tick(item.time + PlaybackGrid::attack_offset, diagnostics, at));
        else
            play(std::get<Slot>(item.action).index, item.time);
    }
}

void Accompaniment::BackingQueue::finish(i32 stop) {
    for (const auto& item : pending)
        if (const auto* release = std::get_if<Release>(&item.action)) release->stop(stop);
    pending.clear();
}

void Accompaniment::ChordPattern::bar(long double now) {
    if (bars == 1) cursor = 0;
    queue.slot(now, cursor);
}

void Accompaniment::ChordPattern::set_span(int span, long double now) {
    bars   = span;
    cursor = 0;
    queue.slot(now, cursor);
}

void Accompaniment::ChordPattern::install(vec<Step> replacement, Meter meter) {
    steps     = std::move(replacement);
    int units = 0;
    for (const auto& step : steps) units += step.length;
    slot_length = meter.measure() * bars / units;
}

template<class Play>
void Accompaniment::ChordPattern::advance(
    long double end, Location at, Diagnostics& diagnostics, Play play
) {
    queue.advance(end, at, diagnostics, [&](size_t index, long double time) {
        if (cursor >= steps.size()) cursor = 0;
        if (index != cursor || steps.empty()) return;
        const auto step   = steps[cursor];
        const auto length = std::floor(slot_length * step.length + epsilon);
        if (enabled) play(step, time, length);
        queue.slot(time + length, ++cursor);
        if (cursor == steps.size()) cursor = 0;
    });
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

Accompaniment::DrumDefinition Accompaniment::drum_pattern(
    std::string_view args, Meter meter, Location at
) {
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
    return {std::move(steps), meter};
}

bool ChordNames::consume(const Token& token, Diagnostics& diagnostics) {
    auto args = token.information().value_or("");
    if (word(args) != "MIDI" || word(args) != "chordname") return false;
    const auto at   = token.at;
    const auto name = word(args);
    if (name.empty() || name.size() > 7) {
        diagnostics.warn(at, "invalid-chord-name", "expected a chord name of one to seven bytes");
        return true;
    }
    vec<int>         intervals;
    constexpr size_t kMaxIntervals = 10;
    while (!trim(args).empty() && intervals.size() < kMaxIntervals) {
        constexpr auto range = std::numeric_limits<int>{};
        intervals.push_back(midi_integer(args, range.min(), range.max(), diagnostics, at));
    }
    if (intervals.empty() || !trim(args).empty()) {
        diagnostics.warn(
            at, "invalid-chord-name", "ignored chordname without one to ten intervals"
        );
        return true;
    }
    const bool builtin = !chord_type(chord_types, name).empty();
    const auto current = custom.find(name);
    // abc2midi's registration search excludes the last slot. On a duplicate
    // of that slot, lookup keeps returning the first copy.
    const bool duplicate = name == newest;
    if (duplicate || (!builtin && current == custom.end())) {
        constexpr size_t kRegistrySlots = 79;
        if (std::size(chord_types) + extra_slots == kRegistrySlots) {
            diagnostics.warn(at, "invalid-chord-name", "the chord name registry is full");
            return true;
        }
        ++extra_slots;
        newest = duplicate ? std::string_view{} : name;
        if (duplicate) return true;
    }
    if (current != custom.end())
        current->second.intervals = std::move(intervals);
    else
        custom.emplace(
            name, Definition{builtin ? std::nullopt : std::optional{at}, std::move(intervals)}
        );
    return true;
}

std::optional<int> slash_bass(std::string_view bass, Location at, Diagnostics& diagnostics) {
    bass            = bass.substr(0, bass.find_first_of(" )("));
    const auto note = root_note(bass);
    if (note && bass.empty()) return note;
    diagnostics.warn(at, "invalid-chord-bass", "ignored an invalid slash-chord bass note");
    return {};
}

vec<i64> voiced_chord(std::span<const int> intervals, int root, std::optional<int> bass_note) {
    std::optional<i64> inversion;
    if (bass_note) {
        for (const i64 interval : intervals) {
            if ((root + interval - *bass_note) % 12 == 0 && (!inversion || interval < *inversion))
                inversion = interval;
        }
    }
    vec<i64> pitches;
    pitches.reserve(intervals.size());
    for (i64 interval : intervals) {
        // Raise each lower tone by the minimum number of octaves.
        if (inversion && interval < *inversion)
            interval += 12 * ((*inversion - interval + 11) / 12);
        pitches.push_back(48 + root + interval);
    }
    if (inversion) std::ranges::sort(pitches);
    return pitches;
}

void ChordNames::resolve(Harmony& harmony, Location at, Diagnostics& diagnostics) const {
    diagnostics.spend(PlaybackWork::text(harmony.symbol), at);
    std::string decoded;
    const auto  spelling = chord_spelling(harmony.symbol, decoded);
    if (!spelling) return;   // quoted annotations are not chord symbols
    const auto [root, bass_only, quality, suffix] = *spelling;
    auto intervals                                = standard_chord(quality);
    if (const auto entry = custom.find(quality);
        entry != custom.end() && (!entry->second.first || *entry->second.first <= at))
        intervals = entry->second.intervals;
    const bool recognized = !intervals.empty();
    if (!recognized) {
        diagnostics.warn(
            at, "unknown-chord", fmt::format("unrecognized chord quality '{}'", quality)
        );
        // abc2midi falls back to a major chord if the voice has accompaniment.
        intervals = chord_type(chord_types, "");
    }
    const auto inversion
        = suffix.starts_with('/') ? slash_bass(suffix.substr(1), at, diagnostics) : std::nullopt;
    Harmony::Voicing next{36 + inversion.value_or(root), {}, recognized};
    if (!bass_only) {
        next.pitches = voiced_chord(intervals, root, inversion);
        if (inversion) {
            const auto lowest = std::ranges::min(next.pitches);
            // A slash bass must sound below the chord even if its accidental crosses an octave.
            if (next.bass >= lowest) next.bass -= 12 * ((next.bass - lowest) / 12 + 1);
        }
    }
    harmony.voicing = std::move(next);
}

void Accompaniment::harmony(const Harmony::Voicing& chord, int lane, int transpose, Location at) {
    Chord next{chord.bass + transpose, chord.pitches, at};
    for (auto& pitch : next.pitches) pitch += transpose;
    harmonic_parts[lane].current_chord = std::move(next);
}

void Accompaniment::meter(int lane, Meter change) {
    if (auto defaults = default_chords(change); !defaults.empty())
        harmonic_parts[lane].pattern.install(pattern(defaults, "fcbz", {}), change);
}

void Accompaniment::bar(int lane, long double time, Location at) {
    advance(lane, time, at);
    harmonic_parts[lane].pattern.bar(time);
    if (const auto part = drum_parts.find(lane); part != drum_parts.end() && part->second.declared)
        part->second.events.push_back({time, at, DrumBar{}});
}

TrackNative<Tick>& Accompaniment::Instrument::track_for(int patch) {
    // ProgramNumbers validates the MIDI byte before selecting a patch.
    return tracks.try_emplace(patch, "", static_cast<u8>(patch), false).first->second;
}

void Accompaniment::Instrument::play(
    i64 pitch, i32 start, i32 duration, long double time, ChordPattern& pattern
) {
    pitch += 12 * octave;
    if (pitch < 0 || pitch > midi_max || velocity == 0) return;
    auto&      track = track_for(program);
    const auto index = track.notes.size();
    track.notes.emplace_back(start, duration, static_cast<i8>(pitch), static_cast<i8>(velocity));
    pattern.release(time + duration, track, index);
}

void Accompaniment::Instrument::write_effects() {
    if (!std::ranges::is_sorted(writes, {}, &TimedWrite::time))
        std::ranges::stable_sort(writes, {}, &TimedWrite::time);
    SustainPedal pedal;
    for (const auto& [time, patch, event] : writes) {
        auto& track = track_for(patch);
        if (event.kind == ChannelEffect::Kind::wheel) {
            track.pitch_bends.emplace_back(time, event.value);
        } else {
            // Controller values are validated MIDI data bytes.
            track.controls.emplace_back(time, event.controller, static_cast<u8>(event.value));
            if (event.controller == 64) pedal.control(track, time, event.value);
        }
    }
}

void Accompaniment::Instrument::finish(vec<TrackNative<Tick>>& output, i32 end) {
    write_effects();
    for (auto& [program, track] : tracks) {
        if (track.empty()) continue;
        close_backing(track, end);
        output.push_back(std::move(track));
    }
}

std::optional<i64> Accompaniment::Chord::arpeggio(char action) const {
    if (pitches.empty()) return {};
    const auto index = static_cast<size_t>(upper(action) - 'G');
    return pitches[std::min(index, pitches.size() - 1)] - (lower(action) ? 0 : 12);
}

void Accompaniment::chord_step(
    HarmonicPart& part, Step step, long double time, long double length, Location at
) {
    if (!part.current_chord || step.kind == 'z') return;
    if (step.kind == 'x') {
        diagnostics.warn(
            part.current_chord->at, "no-default-backing", "no default guitar pattern for this meter"
        );
        return;
    }
    const auto& [root, pitches, chord_at] = *part.current_chord;
    diagnostics.spend(pitches.size(), at);
    const auto [start, duration] = backing_times(time, length, at);
    const auto action            = step.kind == 'c' && pitches.empty() ? 'f' : step.kind;
    const auto play              = [&](Instrument& instrument, i64 pitch) {
        instrument.play(pitch + part.transposition, start, duration, time, part.pattern);
    };
    switch (action) {
    case 'f': play(part.bass, root); return;
    case 'b': play(part.bass, root); [[fallthrough]];
    case 'c':
        for (const auto pitch : pitches) play(part.chord, pitch);
        return;
    default: break;
    }
    if (const auto pitch = part.current_chord->arpeggio(action)) play(part.chord, *pitch);
}

std::pair<i32, i32> Accompaniment::backing_times(
    long double time, long double length, Location at
) {
    const auto onset    = tick(time + PlaybackGrid::attack_offset, diagnostics, at);
    const auto duration = std::max(0, tick(length, diagnostics, at) - PlaybackGrid::release_trim);
    (void)tick(static_cast<long double>(onset) + duration, diagnostics, at);
    return {onset, duration};
}

void Accompaniment::declare_drums(int lane, int opening_gap) {
    auto& part       = drum_parts[lane];
    part.declared    = true;
    part.opening_gap = opening_gap;
}

void Accompaniment::chord_gap(int lane, int gap, long double time, Location at) {
    harmonic_parts[lane].pattern.set_gap(gap);
    if (const auto part = drum_parts.find(lane); part != drum_parts.end() && part->second.declared)
        part->second.events.push_back({time, at, DrumGap{gap}});
}

void Accompaniment::map_drums(int lane, DrumRoute route, long double time, Location at) {
    if (const auto part = drum_parts.find(lane); part != drum_parts.end() && part->second.declared)
        part->second.events.push_back({time, at, route});
}

void Accompaniment::advance(int lane, long double end, Location at) {
    auto& part = harmonic_parts[lane];
    part.pattern.advance(
        end, at, diagnostics, [this, &part, at](Step step, long double time, long double length) {
            chord_step(part, step, time, length, at);
        }
    );
}

void Accompaniment::drum_gate(int lane, bool enabled, long double time, Location at) {
    if (const auto part = drum_parts.find(lane); part != drum_parts.end() && part->second.declared)
        part->second.events.push_back({time, at, DrumGate{enabled}});
}

void Accompaniment::start_drone(Drone& drone, long double time, Location at) {
    const auto written = tick(time + PlaybackGrid::attack_offset, diagnostics, at);
    const auto pending = written - drone.program_at;
    const auto delta   = static_cast<i64>(pending) - drone.previous_delta;
    // A Program Change consumes the pending delta. abc2midi nevertheless
    // writes the previously computed delta again for each of the two attacks.
    auto cursor = static_cast<i64>(drone.emitted);
    if (drone.previous_delta == 0) {
        cursor += pending;
        drone.program_at = written;
    }
    const std::array attacks{
        tick(cursor + delta, diagnostics, at), tick(cursor + 2 * delta, diagnostics, at)
    };
    drone.emitted        = attacks.back();
    drone.previous_delta = written - drone.program_at;
    if (!drone.attacks) {
        drone.attacks        = attacks;
        drone.setup          = drone_setup;
        drone.sounding_shift = drone.transposition;
    }
}

void Accompaniment::stop_drone(Drone& drone, long double time, Location at) {
    const auto written = tick(time + PlaybackGrid::attack_offset, diagnostics, at);
    const auto pending = written - drone.program_at;
    drone.emitted
        = tick(static_cast<i64>(drone.emitted) + pending - drone.previous_delta, diagnostics, at);
    drone.previous_delta = pending;
    close_drone(drone, drone.emitted);
}

void Accompaniment::close_drone(Drone& drone, i32 release) {
    if (!drone.attacks) return;
    const auto program = programs.midi(drone.setup.program);
    auto&      track
        = drone.tracks.try_emplace(program, "", static_cast<u8>(program), false).first->second;
    for (size_t n = 0; n < drone.setup.pitch.size(); ++n) {
        const auto start = (*drone.attacks)[n];
        add_if_audible(
            track,
            start,
            std::max(0, release - start),
            drone.setup.pitch[n] + drone.sounding_shift,
            drone.setup.velocity[n]
        );
    }
    drone.attacks.reset();
}

void Accompaniment::drone_settings(
    Drone& drone, std::string_view args, long double time, Location at
) {
    const bool playing = drone.attacks.has_value();
    if (playing) stop_drone(drone, time, at);
    for (auto* value :
         {&drone_setup.program,
          &drone_setup.pitch[0],
          &drone_setup.pitch[1],
          &drone_setup.velocity[0],
          &drone_setup.velocity[1]}) {
        if (trim(args).empty()) break;
        const auto next = midi_integer(args, -127, midi_max, diagnostics, at);
        if (next > 0) *value = next;
    }
    if (playing) start_drone(drone, time, at);
}

bool Accompaniment::instrument_directive(
    HarmonicPart& part, std::string_view name, std::string_view args, Location at
) {
    auto& instrument = name.starts_with("bass") ? part.bass : part.chord;
    if (name == "bassprog" || name == "chordprog") {
        instrument.program = name == "chordprog" ? programs.read_chord(args, diagnostics, at) :
                                                   programs.read(args, diagnostics, at);
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
    Drone& drone, std::string_view name, std::string_view args, long double time, Location at
) {
    if (name == "drone")
        drone_settings(drone, args, time, at);
    else if (name == "droneon")
        start_drone(drone, time, at);
    else if (name == "droneoff")
        stop_drone(drone, time, at);
    else
        return false;
    return true;
}

bool Accompaniment::directive(
    std::string_view name,
    std::string_view args,
    int              lane,
    Meter            meter,
    long double      time,
    Location         at
) {
    auto& part = harmonic_parts[lane];
    if (name == "gchord")
        part.pattern.install(pattern(word(args), "fcbghijkGHIJKz", at), meter);
    else if (name == "gchordbars")
        part.pattern.set_span(backing_span(args), time);
    else if (name == "gchordon" || name == "gchordoff")
        part.pattern.set_enabled(name == "gchordon");
    else if (name == "drum")
        drum_parts[lane].events.push_back({time, at, drum_pattern(args, meter, at)});
    else if (name == "drumbars")
        drum_parts[lane].events.push_back({time, at, DrumSpan{backing_span(args)}});
    else if (const auto gate = drum_gate_command(name))
        drum_gate(lane, *gate, time, at);
    else
        return instrument_directive(part, name, args, at)
               || drone_directive(part.drone, name, args, time, at);
    return true;
}

void Accompaniment::apply(const ChannelEffect& effect, int lane, i32 time) {
    auto& part = harmonic_parts[lane];
    auto& instrument
        = effect.destination == ChannelEffect::Destination::bass ? part.bass : part.chord;
    for (const auto& event : effect.writes())
        instrument.writes.push_back({time, instrument.program, event});
}

// A bar queues a new downbeat without canceling a pending slot or release.
// In a short bar, inserting earlier MIDI events can pull those entries forward.
struct Accompaniment::DrumSequence {
    struct Installed {
        std::span<const Step> steps;
        long double           slot = 0;
    };
    Installed&    installed;
    Diagnostics&  diagnostics;
    RenderedTrack output{TrackNative<Tick>("", 0, true), {}};
    DrumMap       routing;
    BackingQueue  queue;
    size_t        cursor  = 0;
    int           bars    = 1;
    bool          enabled = false;

    void slot(long double time, size_t index) {
        // A duplicate downbeat would strike twice in a one-slot pattern.
        queue.slot(time, index, installed.steps.size() <= 1);
    }

    void advance(long double end, Location at) {
        queue.advance(end, at, diagnostics, [&](size_t index, long double time) {
            if (index != cursor || installed.steps.empty()) return;
            const auto step   = installed.steps[cursor];
            const auto length = std::floor(installed.slot * step.length + epsilon);
            if (enabled && step.kind == 'd') {
                const auto start = tick(time + PlaybackGrid::attack_offset, diagnostics, at);
                const auto note  = output.track.notes.size();
                output.append_drum(
                    Note<Tick>{
                        start, 0, static_cast<i8>(step.pitch), static_cast<i8>(step.velocity)
                    },
                    routing
                );
                queue.release(
                    time + std::max(0.L, length - PlaybackGrid::release_trim), output.track, note
                );
            }
            slot(time + length, ++cursor);
            if (cursor == installed.steps.size()) cursor = 0;
        });
    }

    void apply(const DrumEvent& event, bool declared) {
        if (declared) advance(event.time, event.at);
        std::visit([&](const auto& change) { change_at(change, event.time); }, event.change);
    }

    void change_at(const DrumDefinition& definition, long double) {
        int units = 0;
        for (const auto& step : definition.steps) units += step.length;
        installed = {definition.steps, definition.meter.measure() * bars / units};
        if (cursor >= installed.steps.size()) cursor = 0;
    }
    void change_at(DrumBar, long double time) {
        if (bars == 1) {
            cursor = 0;
            slot(time, cursor);
        }
        slot(time, cursor);
    }
    void change_at(DrumSpan span, long double time) {
        bars   = span.bars;
        cursor = 0;
        slot(time, cursor);
    }
    void change_at(DrumGate gate, long double) { enabled = gate.enabled; }
    void change_at(DrumGap gap, long double) { queue.set_gap(gap.ticks); }
    void change_at(DrumRoute route, long double) { routing.apply(route); }

    RenderedTrack finish(long double end, Location at) {
        advance(end, at);
        const auto stop = tick(
            std::min<long double>(
                end + PlaybackGrid::attack_offset, std::numeric_limits<i32>::max()
            ),
            diagnostics,
            at
        );
        queue.finish(stop);
        return std::move(output);
    }
};

void Accompaniment::finish_drums(
    std::span<PerformedBacking> backing, std::span<const long double> lane_ends, Location at
) {
    DrumSequence::Installed pattern;
    // abc2midi carries the last pattern definition between consecutive MIDI
    // tracks, but begins each drum pass with a fresh gate, queue, and cursor.
    for (const auto& [lane, part] : drum_parts) {
        DrumSequence sequence{pattern, diagnostics};
        sequence.queue.set_gap(part.opening_gap);
        sequence.slot(0, 0);
        for (const auto& event : part.events) sequence.apply(event, part.declared);
        if (!part.declared || static_cast<size_t>(lane) >= lane_ends.size()) continue;
        auto track = sequence.finish(lane_ends[lane], at);
        if (!track.track.empty()) backing[lane].drums.push_back(std::move(track));
    }
}

vec<PerformedBacking> Accompaniment::finish(std::span<const long double> lane_ends, Location at) {
    vec<PerformedBacking> backing(lane_ends.size());
    for (size_t lane = 0; lane < lane_ends.size(); ++lane) {
        auto&      part = harmonic_parts[lane];
        const auto end  = lane_ends[lane];
        const auto stop = tick(
            std::min<long double>(
                end + PlaybackGrid::attack_offset, std::numeric_limits<i32>::max()
            ),
            diagnostics,
            at
        );
        advance(static_cast<int>(lane), end, at);
        part.pattern.finish(stop);
        vec<TrackNative<Tick>> harmony;
        part.bass.finish(harmony, stop);
        part.chord.finish(harmony, stop);
        for (auto& track : harmony) backing[lane].harmony.push_back({std::move(track), {}});
    }
    finish_drums(backing, lane_ends, at);
    for (size_t lane = 0; lane < lane_ends.size(); ++lane) {
        auto& drone = harmonic_parts[lane].drone;
        if (drone.attacks) stop_drone(drone, lane_ends[lane], at);
        for (auto& [program, track] : drone.tracks) {
            if (!track.empty()) backing[lane].drone.push_back({std::move(track), {}});
        }
    }
    return backing;
}

}   // namespace symusic::abc::detail

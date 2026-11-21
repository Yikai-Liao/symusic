#include "accompaniment.h"

#include <algorithm>
#include <limits>
#include <ranges>
#include <span>

namespace symusic::abc::detail {
namespace {

struct ChordType {
    std::string_view           name;
    std::initializer_list<i32> intervals;
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

std::optional<i32> root_note(std::string_view& symbol) {
    if (symbol.empty() || !note_letter(symbol.front())) return {};
    auto note = kNaturalPitch[letter_index(upper(symbol.front()))];
    symbol.remove_prefix(1);
    // ABC 2.1 sections 4.18 and 8.2: roots and slash basses accept musical glyphs.
    // abc2midi also accepts doubled ASCII accidentals. Match them before single signs.
    constexpr std::pair<std::string_view, i32> kAccidentals[] = {
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
    i32              root;
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

std::span<const i32> chord_type(std::span<const ChordType> types, std::string_view quality) {
    const auto chord = std::ranges::find(types, quality, &ChordType::name);
    return chord == types.end() ? std::span<const i32>{} : chord->intervals;
}

std::span<const i32> standard_chord(std::string_view quality) {
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

TrackNative<Tick>& program_track(std::map<i32, TrackNative<Tick>>& tracks, i32 program) {
    // ProgramNumbers validates the MIDI byte before selecting a patch.
    return tracks.try_emplace(program, "", static_cast<u8>(program), false).first->second;
}

void close_backing(TrackNative<Tick>& track, i32 end) {
    for (auto& note : track.notes)
        note.duration = std::min(note.duration, std::max(0, end - note.time));
}

i32 backing_end(f64 time, Diagnostics& diagnostics, Location at) {
    const auto end
        = std::min<f64>(time + PlaybackGrid::kAttackOffset, std::numeric_limits<i32>::max());
    return tick(end, diagnostics, at);
}

i32 backing_span(std::string_view args) {
    const i32 bars = integer(word(args)).value_or(0);
    // abc2midi replaces an out-of-range span with the one-bar default.
    return bars >= 1 && bars <= 10 ? bars : 1;
}

}   // namespace

Accompaniment::Accompaniment(const ProgramNumbers& programs, Diagnostics& diagnostics) :
    diagnostics(diagnostics), programs(programs) {}

void Accompaniment::open_lane(
    size_t lane, Meter meter, f64 bar_origin, const BackingOpening& opening
) {
    // OverlayLanes creates lane IDs consecutively and can select a lane again.
    if (lane < harmonic_parts.size()) return;
    auto defaults = default_chords(meter);
    if (defaults.empty()) defaults = "x";
    auto& part               = harmonic_parts.emplace_back();
    part.transposition       = opening.chord_shift;
    part.drone.transposition = opening.drone_shift;
    part.pattern.install(pattern(defaults, "fcbzx", {}), meter);
    part.pattern.start_track(bar_origin, opening.chord_gap);
}

void Accompaniment::transpose(size_t lane, MidiTransposition change) {
    auto& part               = harmonic_parts[lane];
    part.transposition       = change.apply(part.transposition);
    part.drone.transposition = change.apply(part.drone.transposition);
}

void discover_backing(const Token& token, BackingTracks& tracks) {
    const Instruction command(token);
    if (command.midi == "drumon") tracks.drums = true;
    if (command.midi == "droneon") tracks.drone = true;
    if (const auto* harmony = std::get_if<Harmony>(&token.value))
        tracks.chords = tracks.chords || (harmony->voicing && harmony->voicing->declares_backing);
}

std::string_view Accompaniment::default_chords(Meter meter) {
    struct DefaultPattern {
        i32              numerator;
        i32              denominator;
        std::string_view sequence;
    };
    static constexpr DefaultPattern kDefaults[]{
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
    for (const auto& entry : kDefaults) {
        if (entry.numerator == meter.numerator && entry.denominator == meter.denominator)
            return entry.sequence;
    }
    return {};
}

i32 step_length(std::string_view& rest, Diagnostics& diagnostics, Location at) {
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

vec<BackingStep> Accompaniment::pattern(
    std::string_view text, std::string_view allowed, Location at
) {
    vec<BackingStep> result;
    while (!text.empty()) {
        const char kind = text.front();
        text.remove_prefix(1);
        if (allowed.find(kind) == allowed.npos)
            diagnostics.error(
                at,
                "invalid-backing-pattern",
                fmt::format(
                    "unexpected byte {:#04x} in MIDI backing pattern", static_cast<u8>(kind)
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
        step.pitch    = trim(args).empty() ? 35 : midi_integer(args, 0, kMidiMax, diagnostics, at);
        step.velocity = 80;
    }
    for (auto& step : steps) {
        if (trim(args).empty()) break;
        if (step.kind == 'd') step.velocity = midi_integer(args, 1, kMidiMax, diagnostics, at);
    }
    if (!trim(args).empty())
        diagnostics.warn(at, "extra-drum-values", "ignored values beyond the drum strikes");
    return {std::move(steps), meter};
}

bool ChordNames::consume(const Token& token, Diagnostics& diagnostics) {
    Instruction command(token);
    if (command.midi != "chordname") return false;
    auto       args = command.args;
    const auto at   = token.at;
    const auto name = word(args);
    if (name.empty() || name.size() > 7) {
        diagnostics.warn(at, "invalid-chord-name", "expected a chord name of one to seven bytes");
        return true;
    }
    vec<i32>         intervals;
    constexpr size_t kMaxIntervals = 10;
    while (!trim(args).empty() && intervals.size() < kMaxIntervals) {
        constexpr auto kRange = std::numeric_limits<i32>{};
        intervals.push_back(midi_integer(args, kRange.min(), kRange.max(), diagnostics, at));
    }
    if (intervals.empty() || !trim(args).empty()) {
        diagnostics.warn(
            at, "invalid-chord-name", "ignored chordname without one to ten intervals"
        );
        return true;
    }
    constexpr size_t kRegistrySlots = 79;
    if (slots.empty()) {
        for (const auto& [quality, voicing] : chord_types)
            slots.push_back({std::string(quality), {}, voicing});
    }
    // Registration searches all but the last slot. Playback searches every
    // slot from the start, so an appended duplicate keeps the first voicing.
    const auto searchable = std::span(slots).first(slots.size() - 1);
    const auto found      = std::ranges::find(searchable, name, &Definition::name);
    if (found != searchable.end())
        found->intervals = std::move(intervals);
    else if (slots.size() == kRegistrySlots)
        diagnostics.warn(at, "invalid-chord-name", "the chord name registry is full");
    else
        slots.push_back({std::string(name), at.written, std::move(intervals)});
    return true;
}

std::optional<i32> slash_bass(std::string_view bass, Location at, Diagnostics& diagnostics) {
    bass            = bass.substr(0, bass.find_first_of(" )("));
    const auto note = root_note(bass);
    if (note && bass.empty()) return note;
    diagnostics.warn(at, "invalid-chord-bass", "ignored an invalid slash-chord bass note");
    return {};
}

vec<i64> voiced_chord(std::span<const i32> intervals, i32 root, std::optional<i32> bass_note) {
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
    const auto           entry = std::ranges::find(slots, quality, &Definition::name);
    std::span<const i32> intervals
        = entry != slots.end() && (!entry->first || *entry->first <= at.written) ?
              std::span<const i32>(entry->intervals) :
              standard_chord(quality);
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

void Accompaniment::harmony(
    const Harmony::Voicing& chord, size_t lane, i32 transpose, Location at
) {
    Chord next{chord.bass + transpose, chord.pitches, at};
    for (auto& pitch : next.pitches) pitch += transpose;
    harmonic_parts[lane].current_chord = std::move(next);
}

void Accompaniment::meter(size_t lane, Meter change) {
    if (auto defaults = default_chords(change); !defaults.empty())
        harmonic_parts[lane].pattern.install(pattern(defaults, "fcbz", {}), change);
}

void Accompaniment::bar(size_t lane, f64 time, Location at) {
    advance(lane, time, at);
    harmonic_parts[lane].pattern.bar(time);
    record_if_declared(lane, {time, at, DrumBar{}});
}

void Accompaniment::Instrument::play(
    i64 pitch, i32 start, i32 duration, f64 time, BackingPattern& pattern
) {
    pitch += i64{12} * octave;
    if (pitch < 0 || pitch > kMidiMax || velocity == 0) return;
    auto& track = program_track(tracks, program);
    track.notes.emplace_back(start, duration, static_cast<i8>(pitch), static_cast<i8>(velocity));
    pattern.release_last(time + duration, track);
}

void Accompaniment::Instrument::apply(const ChannelEffect& effect, i32 time) {
    auto& track = program_track(tracks, program);
    for (const auto& event : effect.writes()) event.emit(track, time, pedal);
}

void Accompaniment::Instrument::finish(vec<TrackNative<Tick>>& output, i32 end) {
    for (auto& [program, track] : tracks) {
        if (track.empty()) continue;
        close_backing(track, end);
        output.push_back(std::move(track));
    }
}

void Accompaniment::chord_step(HarmonicPart& part, char action, f64 time, f64 length, Location at) {
    if (!part.current_chord) return;
    if (action == 'x') {
        diagnostics.warn(
            part.current_chord->at, "no-default-backing", "no default guitar pattern for this meter"
        );
        return;
    }
    const auto& [root, pitches, chord_at] = *part.current_chord;
    diagnostics.spend(pitches.size(), at);
    const auto [start, duration] = backing_times(time, length, at);
    const auto play              = [&](Instrument& instrument, i64 pitch) {
        instrument.play(pitch + part.transposition, start, duration, time, part.pattern);
    };
    if (action == 'c' && pitches.empty()) action = 'f';
    switch (action) {
    case 'f': play(part.bass, root); return;
    case 'b': play(part.bass, root); [[fallthrough]];
    case 'c':
        for (const auto pitch : pitches) play(part.chord, pitch);
        return;
    default:
        if (pitches.empty()) return;
        const auto member = std::min(static_cast<size_t>(upper(action) - 'G'), pitches.size() - 1);
        play(part.chord, pitches[member] - (lower(action) ? 0 : 12));
    }
}

std::pair<i32, i32> Accompaniment::backing_times(f64 time, f64 length, Location at) {
    const auto onset    = tick(time + PlaybackGrid::kAttackOffset, diagnostics, at);
    const auto duration = std::max(0, tick(length, diagnostics, at) - PlaybackGrid::kReleaseTrim);
    (void)tick(static_cast<f64>(onset) + duration, diagnostics, at);
    return {onset, duration};
}

void Accompaniment::declare_drums(size_t lane, const BackingOpening& opening) {
    auto& part       = drum_parts[lane];
    part.opening_gap = opening.drum_gap;
    part.routing     = opening.drums;
}

void Accompaniment::record_if_declared(size_t lane, DrumEvent event) {
    if (const auto part = drum_parts.find(lane);
        part != drum_parts.end() && part->second.opening_gap)
        part->second.events.push_back(std::move(event));
}

void Accompaniment::chord_gap(size_t lane, i32 gap, f64 time, Location at) {
    harmonic_parts[lane].pattern.gap = gap;
    record_if_declared(lane, {time, at, DrumGap{gap}});
}

void Accompaniment::map_drums(size_t lane, DrumRoute route, f64 time, Location at) {
    record_if_declared(lane, {time, at, route});
}

void Accompaniment::advance(size_t lane, f64 end, Location at) {
    auto& part = harmonic_parts[lane];
    part.pattern.advance(
        end, at, diagnostics, [&](const BackingStep& step, f64 time, f64 length, Location where) {
            chord_step(part, step.kind, time, length, where);
        }
    );
}

void Accompaniment::drum_gate(size_t lane, bool enabled, f64 time, Location at) {
    record_if_declared(lane, {time, at, enabled});
}

void Accompaniment::start_drone(Drone& drone, f64 time, Location at) {
    const auto written = tick(time + PlaybackGrid::kAttackOffset, diagnostics, at);
    const auto pending = written - drone.program_at;
    const auto delta   = static_cast<i64>(pending) - drone.previous_delta;
    // A Program Change consumes the pending delta. abc2midi nevertheless
    // writes the previously computed delta again for each of the two attacks.
    auto cursor = static_cast<i64>(drone.emitted);
    if (drone.previous_delta == 0) {
        cursor += pending;
        drone.program_at = written;
    }
    // These expressions combine i32 tick values and fit exactly in f64.
    const std::array attacks{
        tick(static_cast<f64>(cursor + delta), diagnostics, at),
        tick(static_cast<f64>(cursor + 2 * delta), diagnostics, at)
    };
    drone.emitted        = attacks.back();
    drone.previous_delta = written - drone.program_at;
    if (drone.sounding_program) return;
    drone.sounding_program = programs.midi(drone_setup.program);
    auto& track            = program_track(drone.tracks, *drone.sounding_program);
    drone.first_attack     = track.notes.size();
    for (size_t i = 0; i < attacks.size(); ++i) {
        const auto pitch = static_cast<i64>(drone_setup.pitch[i]) + drone.transposition;
        if (pitch < 0 || pitch > kMidiMax) continue;
        track.notes.emplace_back(
            attacks[i], 0, static_cast<i8>(pitch), static_cast<i8>(drone_setup.velocity[i])
        );
    }
}

void Accompaniment::stop_drone(Drone& drone, f64 time, Location at) {
    const auto written = tick(time + PlaybackGrid::kAttackOffset, diagnostics, at);
    const auto pending = written - drone.program_at;
    // The sum of three i32 tick values is exact in f64.
    drone.emitted = tick(
        static_cast<f64>(static_cast<i64>(drone.emitted) + pending - drone.previous_delta),
        diagnostics,
        at
    );
    drone.previous_delta = pending;
    if (!drone.sounding_program) return;
    auto& notes = drone.tracks.at(*drone.sounding_program).notes;
    for (auto& note : std::span(notes).subspan(drone.first_attack))
        note.duration = std::max(0, drone.emitted - note.time);
    drone.sounding_program.reset();
}

void Accompaniment::drone_settings(Drone& drone, std::string_view args, f64 time, Location at) {
    const bool playing = drone.sounding_program.has_value();
    if (playing) stop_drone(drone, time, at);
    for (auto* value :
         {&drone_setup.program,
          &drone_setup.pitch[0],
          &drone_setup.pitch[1],
          &drone_setup.velocity[0],
          &drone_setup.velocity[1]}) {
        if (trim(args).empty()) break;
        const auto next = midi_integer(args, -127, kMidiMax, diagnostics, at);
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
        instrument.velocity = midi_integer(args, 0, kMidiMax, diagnostics, at);
    else
        return false;
    return true;
}

bool Accompaniment::directive(
    std::string_view name, std::string_view args, size_t lane, Meter meter, f64 time, Location at
) {
    auto& part = harmonic_parts[lane];
    if (name == "gchord")
        part.pattern.install(pattern(word(args), "fcbghijkGHIJKz", at), meter);
    else if (name == "gchordbars")
        part.pattern.set_span(backing_span(args), time);
    else if (name == "gchordon" || name == "gchordoff")
        part.pattern.enabled = name == "gchordon";
    else if (name == "drum")
        drum_parts[lane].events.push_back({time, at, drum_pattern(args, meter, at)});
    else if (name == "drumbars")
        drum_parts[lane].events.push_back({time, at, DrumSpan{backing_span(args)}});
    else if (name == "drumon" || name == "drumoff")
        drum_gate(lane, name == "drumon", time, at);
    else if (name == "drone")
        drone_settings(part.drone, args, time, at);
    else if (name == "droneon")
        start_drone(part.drone, time, at);
    else if (name == "droneoff")
        stop_drone(part.drone, time, at);
    else
        return instrument_directive(part, name, args, at);
    return true;
}

void Accompaniment::apply(const ChannelEffect& effect, size_t lane, i32 time) {
    auto& part = harmonic_parts[lane];
    auto& instrument
        = effect.destination == ChannelEffect::Destination::Bass ? part.bass : part.chord;
    // Each lane's playback clock advances in performed order, including repeats and overlays.
    instrument.apply(effect, time);
}

void Accompaniment::DrumEvent::apply(BackingPattern& pattern, DrumMap& routing) {
    if (auto* definition = std::get_if<DrumDefinition>(&change))
        pattern.install(std::move(definition->steps), definition->meter);
    else if (std::holds_alternative<DrumBar>(change))
        pattern.bar(time);
    else if (const auto* span = std::get_if<DrumSpan>(&change))
        pattern.set_span(span->bars, time);
    else if (const auto* gate = std::get_if<bool>(&change))
        pattern.enabled = *gate;
    else if (const auto* gap = std::get_if<DrumGap>(&change))
        pattern.gap = gap->ticks;
    else
        routing.apply(std::get<DrumRoute>(change));
}

void Accompaniment::finish_drums(
    size_t lane, BackingPattern& pattern, vec<TrackNative<Tick>>& tracks, f64 end, Location at
) {
    const auto found = drum_parts.find(lane);
    if (found == drum_parts.end()) return;
    auto& part = found->second;
    pattern.start_track(0, part.opening_gap.value_or(ChordAttackSettings::kDefaultGap));
    TrackNative<Tick> track("", 0, true);
    const auto        strike = [&](const BackingStep& step, f64 time, f64 length, Location where) {
        const auto start = tick(time + PlaybackGrid::kAttackOffset, diagnostics, where);
        track.notes.emplace_back(
            start,
            0,
            static_cast<i8>(part.routing.pitch(step.pitch)),
            static_cast<i8>(step.velocity)
        );
        pattern.release_last(time + std::max(0.0, length - PlaybackGrid::kReleaseTrim), track);
    };
    // Even lanes without a drum track can define the following track's pattern.
    for (auto& event : part.events) {
        if (part.opening_gap) pattern.advance(event.time, event.at, diagnostics, strike);
        event.apply(pattern, part.routing);
    }
    if (!part.opening_gap) return;
    pattern.advance(end, at, diagnostics, strike);
    pattern.finish(backing_end(end, diagnostics, at));
    if (!track.empty()) tracks.push_back(std::move(track));
}

void Accompaniment::finish(
    std::span<PlaybackPass> passes, std::span<const f64> lane_ends, Location at
) {
    BackingPattern drums{BackingPattern::Part::Drums};
    for (size_t lane = 0; lane < lane_ends.size(); ++lane) {
        auto&      part   = harmonic_parts[lane];
        auto&      tracks = passes[lane].tracks;
        const auto end    = lane_ends[lane];
        const auto stop   = backing_end(end, diagnostics, at);
        advance(lane, end, at);
        part.pattern.finish(stop);
        part.bass.finish(tracks, stop);
        part.chord.finish(tracks, stop);
        finish_drums(lane, drums, tracks, end, at);
        if (part.drone.sounding_program) stop_drone(part.drone, end, at);
        for (auto& [program, track] : part.drone.tracks)
            if (!track.empty()) tracks.push_back(std::move(track));
    }
}

}   // namespace symusic::abc::detail

#include "attacks.h"

#include <fmt/format.h>

namespace symusic::abc::detail {
namespace {

using namespace std::literals;

struct Sound {
    MidiPitch pitch;
    i32       letter;
    i32       octave;
    bool      explicit_accidental;
    bool      tie;
};

// Written notes update measure accidentals. An inferred neighbor takes its
// accidental from the key, including across an octave boundary.
struct AttackPitches {
    const TuneSettings& settings;
    const Key&          key;
    Accidentals&        accidentals;
    Location            at;

    [[nodiscard]] Sound read(Pitch pitch, bool tie) const {
        const auto alteration = accidentals.resolve(pitch, key, settings.accidentals.at(at));
        const auto octave     = key.effective_octave(pitch.octave);
        return {
            settings.tuning.pitch(pitch.letter, octave, alteration, at),
            static_cast<i32>(letter_index(pitch.letter)),
            octave,
            pitch.accidental.has_value(),
            tie || pitch.tie
        };
    }

    [[nodiscard]] MidiPitch neighbor(Pitch pitch, i32 direction) const {
        constexpr std::string_view kScale   = "CDEFGAB";
        constexpr i32              kDegrees = static_cast<i32>(kScale.size());
        const auto                 offset = static_cast<i32>(kScale.find(pitch.letter)) + direction;
        if (offset < 0) --pitch.octave;
        if (offset == kDegrees) ++pitch.octave;
        const auto letter = kScale[(offset + kDegrees) % kDegrees];
        return settings.tuning.pitch(
            letter, key.effective_octave(pitch.octave), key.alterations[letter_index(letter)], at
        );
    }
};

GateArticulation route_gate(VoiceOutput::Selection route, GateArticulation articulation) {
    // abc2midi queues percussion releases at their written length on channel 10.
    return route.is_drum() ? GateArticulation::Unshaped : articulation;
}

// The head fixes these values even when later fields change the voice. Ties
// extend the host before we divide it into individual, stress-sampled pulses.
struct OrnamentPhrase {
    std::array<MidiPitch, 5> pitches;
    size_t                   pitch_count;
    StressNote               host;
    f64                      start;
    f64                      written_start;
    f64                      accent_origin;
    Meter                    meter;
    Location                 at;
    SlurPosition             slur;
    GateArticulation         articulation;
    bool                     accented;
    size_t                   lane;
    i32                      gap;

    template<class Emit>
    void play(
        const TuneSettings&      settings,
        Expression&              expression,
        ExpandedReleaseSchedule& releases,
        Diagnostics&             diagnostics,
        Emit                     emit
    ) const {
        const auto count = pitch_count == 2 ?
                               static_cast<size_t>(trill_attacks(
                                   host.duration, settings.unit, settings.tempo, at, diagnostics
                               )) :
                               pitch_count;
        if (count == 0) return;
        const auto step           = host.duration / static_cast<f64>(count);
        const bool sample_attacks = settings.stress.articulates(at, meter);
        for (size_t i = 0; i < count; ++i) {
            const auto offset = static_cast<f64>(i) * step;
            const auto sample = sample_attacks ?
                                    settings.stress.note(at, written_start + offset, step, meter) :
                                    StressNote{step, host.velocity, {}};
            // abc2midi releases the slur for the final two generated attacks.
            const bool slurred
                = slur == SlurPosition::Inside || (slur == SlurPosition::Closing && count - i > 2);
            const auto gate  = expression.gate(step, slurred, articulation, sample.gate_ratio);
            const auto onset = start + offset;
            const auto volume
                = expression
                      .attack_velocity(onset - accent_origin, meter, accented, sample.velocity)
                      .value;
            const auto hit = emit(i, pitches[i % pitch_count], onset, gate, volume);
            releases.pulse(lane, onset, step, at, hit, gap);
        }
    }
};

}   // namespace

// Lane ties borrow these entries. The voice keeps their addresses stable until
// every reserved head and its later pulses have received their final gates.
struct TiedOrnament {
    OrnamentPhrase phrase;
    Expression     expression;
    AttackGate     first;
};

namespace {

std::optional<TieNote> take_tie(vec<TieNote>& ties, const Sound& sound) {
    const auto match = std::ranges::find_if(ties, [&](const TieNote& pending) {
        // An omitted accidental can inherit the tie's pitch at the same written octave.
        return sound.pitch.note == pending.untransposed_pitch
               || (!sound.explicit_accidental && sound.letter == pending.letter
                   && sound.octave == pending.octave);
    });
    if (match == ties.end()) return {};
    auto tie = *match;
    ties.erase(match);
    return tie;
}

struct TieSegment {
    f64 end;
    f64 written_gate;
    f64 step;
    i32 release;
    i32 overlap_release;
};

void sustain(TieNote& tie, TieSegment segment, const TuneStress& stress) {
    if (auto* expanded = std::get_if<TiedOrnament*>(&tie.note)) {
        auto& phrase         = (*expanded)->phrase;
        phrase.host.duration = std::max(phrase.host.duration, segment.end - phrase.start);
        return;
    }
    const auto& note = std::get<AttackGate>(tie.note);
    // A longer chord member can still be sounding when its continuation begins.
    tie.overlap_release = std::max(tie.overlap_release, segment.overlap_release);
    note.release_at(std::max(tie.overlap_release, segment.release));
    if (auto& span = tie.stress) {
        span->release_offset
            = std::max(span->release_offset, span->next_offset + segment.written_gate);
        // The opening profile decides the gain of the continuous attack.
        if (stress.note(span->at, span->start, span->release_offset, span->meter).velocity == 0) {
            note.use_unstressed_velocity();
            span.reset();
        } else {
            span->next_offset += segment.step;
        }
    }
}

struct ChordMember {
    Sound                  sound;
    MidiPitch              sounding;
    VoiceOutput::Selection route;
    std::optional<TieNote> continuation;
    f64                    audible;
    f64                    written_gate;
    i32                    overlap_release;
    i32                    stress_velocity;
    i32                    delay;
    i32                    gap;
};

GateArticulation member_articulation(const Prepared& host, std::span<const ChordDecoration> marks) {
    if (host.staccato()) return GateArticulation::Staccato;
    auto gate = host.ornament.tenuto ? GateArticulation::Unshaped : GateArticulation::Normal;
    for (const auto& mark : marks) {
        const auto flag = ornament_flag(mark.spelling);
        if (flag == &Ornament::staccato) return GateArticulation::Staccato;
        if (flag == &Ornament::tenuto && gate == GateArticulation::Normal)
            gate = GateArticulation::Unshaped;
    }
    return gate;
}

}   // namespace

auto NoteEmitter::pitches(Location at) const {
    return AttackPitches{settings, lanes.notation().fields.tone, lanes.notation().accidentals, at};
}

AttackVelocity NoteEmitter::velocity(f64 start, bool accented, i32 stressed) const {
    return lane().expression.attack_velocity(
        start - lane().accent_origin, lanes.notation().fields.meter, accented, stressed
    );
}

void NoteEmitter::lead(
    MidiPitch pitch, f64 start, StressNote timing, f64 gate, const Prepared& atom
) {
    const auto id     = lanes.id();
    const auto route  = output.chord_route(id, 0);
    const auto volume = velocity(start, atom.ornament.accent, timing.velocity).value;
    const auto hit    = output.attack(
        id,
        route,
        output.sounding_pitch(id, route, pitch, shift()),
        start,
        gate,
        0,
        {volume},
        atom.at
    );
    releases.pulse(id, start, timing.duration, atom.at, hit.gate, lane().midi.attack.gap);
}

// One chord resolves all continuations before any of its attacks can select a
// different channel or change the shared wheel. Grace chords use the same path.
struct NoteEmitter::Chord {
    NoteEmitter&    voice;
    const Prepared& host;
    AttackTiming    timing;
    f64             borrowed;
    // One resolved member per written note, in the same order.
    vec<ChordMember> members{};

    [[nodiscard]] f64 written_gate(
        std::optional<Length> override_length, GateArticulation gate
    ) const {
        const bool staccato = gate == GateArticulation::Staccato;
        if (!override_length) return staccato ? timing.written_staccato : timing.written_length;
        auto length = override_length->fraction() * (host.tuplet_unit * host.rhythm_scale);
        if (host.ornament.fermata) length = fermata_length(length, host.unit, host.lengths.fermata);
        return std::max(0.0, (staccato ? staccato_length(length) : length) - borrowed);
    }
    void                  resolve();
    bool                  play_ornament();
    std::optional<Attack> emit(const ChordMember& member, std::span<const ChordDecoration> marks);
    std::optional<LyricAnchor> play();
};

void NoteEmitter::Chord::resolve() {
    auto& lane        = voice.lane();
    auto& diagnostics = voice.diagnostics;
    diagnostics.spend(PlaybackWork::chord(host.atom->notes.size(), lane.ties.size()), host.at);
    members.reserve(host.atom->notes.size());
    auto       route   = voice.output.current_route(voice.lanes.id());
    const auto pitches = voice.pitches(host.at);
    const auto shift   = voice.shift();
    const auto spread
        = host.atom->chord ?
              Expression::take_chord_spread(lane.midi.attack, host.ornament.arpeggio) :
              ChordSpread{};
    i64        stagger  = 0;
    i32        gap      = host.atom->chord && !spread.random ? spread.ticks : lane.midi.attack.gap;
    size_t     ordinary = 0;
    const bool slurred  = host.slur == SlurPosition::Inside;
    for (const auto& [pitch, own_length, marks] : host.atom->notes) {
        const auto gate         = member_articulation(host, marks);
        const auto sound        = pitches.read(pitch, host.tied);
        auto       continuation = take_tie(lane.ties, sound);
        // Only ordinary attacks advance the chord channel and stagger. A tie
        // head uses the last selected route and a continuation emits no attack.
        if (!continuation && !sound.tie) {
            if (ordinary != 0) {
                gap = voice.chord_attacks.next_gap(spread);
                stagger += gap;
            }
            route = voice.output.chord_route(voice.lanes.id(), ordinary++);
        }
        const auto length = written_gate(own_length, gate);
        const auto step   = timing.written_length;
        const auto mapped
            = length == step ?
                  timing.full :
                  voice.settings.stress.note(
                      host.at, timing.written_start, length, voice.lanes.notation().fields.meter
                  );
        const bool overlaps = sound.tie && length > step + kEpsilon;
        const auto end      = lane.time + mapped.duration;
        // abc2midi schedules a tied note's combined duration without trim or expand.
        const auto release = continuation || overlaps ? tick(end, diagnostics, host.at) : 0;
        const auto overlap = overlaps ? release : 0;
        if (continuation)
            sustain(*continuation, {end, length, step, release, overlap}, voice.settings.stress);
        const auto articulation = sound.tie ? GateArticulation::Unshaped : route_gate(route, gate);
        const auto audible
            = lane.expression.gate(mapped.duration, slurred, articulation, mapped.gate_ratio);
        i32 delay = 0;
        if (stagger && !continuation) {
            const auto limit
                = std::max(0, tick(audible, diagnostics, host.at) - PlaybackGrid::kReleaseTrim - 1);
            delay = static_cast<i32>(std::min<i64>(limit, stagger));
        }
        members.push_back(
            {sound,
             voice.output.sounding_pitch(voice.lanes.id(), route, sound.pitch, shift),
             route,
             continuation,
             audible,
             length,
             overlap,
             mapped.velocity,
             delay,
             gap}
        );
    }
    // A short gate can clip a later attack. Clip the preceding attacks too so
    // their order and individual releases both survive the spread.
    if (stagger) {
        auto latest = std::numeric_limits<i32>::max();
        for (auto& member : members | std::views::reverse)
            if (!member.continuation) member.delay = latest = std::min(latest, member.delay);
    }
}

bool NoteEmitter::Chord::play_ornament() {
    if (host.harp_cut() || members.size() != 1 || members.front().continuation) return false;
    if (!host.ornament.roll && !host.ornament.trill) return false;
    auto&       lane    = voice.lane();
    const auto& head    = members.front().sound;
    const auto  pitch   = host.atom->notes.front().pitch;
    const auto  pitches = voice.pitches(host.at);
    const auto  id      = voice.lanes.id();
    const auto  route   = voice.output.chord_route(id, 0);
    const auto  shift   = voice.shift();
    const auto  main    = voice.output.sounding_pitch(id, route, head.pitch, shift);
    const auto  upper   = voice.output.sounding_pitch(id, route, pitches.neighbor(pitch, 1), shift);
    OrnamentPhrase phrase{
        {upper, main},
        2,
        timing.full,
        lane.time,
        timing.written_start,
        lane.accent_origin,
        voice.lanes.notation().fields.meter,
        host.at,
        host.slur,
        route_gate(
            route, host.ornament.tenuto ? GateArticulation::Unshaped : GateArticulation::Normal
        ),
        host.ornament.accent,
        id,
        lane.midi.attack.gap
    };
    if (host.ornament.roll) {
        const auto lower
            = voice.output.sounding_pitch(id, route, pitches.neighbor(pitch, -1), shift);
        phrase.pitches     = {main, upper, main, lower, main};
        phrase.pitch_count = 5;
    }
    if (!head.tie || timing.full.duration == 0) {
        phrase.play(
            voice.settings,
            lane.expression,
            voice.releases,
            voice.diagnostics,
            [&](size_t, MidiPitch note, f64 start, f64 gate, i8 volume) {
                return voice.output.attack(id, route, note, start, gate, 0, {volume}, host.at).gate;
            }
        );
        return true;
    }
    // Reserve the attack and consume its one-shot dynamics on the opening lane.
    // The final tie duration determines the gain and gate of the first pulse.
    const auto opening = lane.expression;
    (void)voice.velocity(lane.time, phrase.accented, timing.full.velocity);
    const auto first
        = voice.output.attack(id, route, phrase.pitches.front(), lane.time, 0, 0, {1}, host.at);
    auto& tied = voice.ornaments.emplace_back(TiedOrnament{phrase, opening, first.gate});
    lane.ties.push_back({head.pitch.note, head.letter, head.octave, &tied, host.at, {}});
    return true;
}

std::optional<Attack> NoteEmitter::Chord::emit(
    const ChordMember& member, std::span<const ChordDecoration> marks
) {
    const auto& sound    = member.sound;
    bool        accented = host.ornament.accent;
    for (const auto& mark : marks) {
        accented = accented || ornament_flag(mark.spelling) == &Ornament::accent;
        voice.apply_decoration(mark.spelling, mark.at, member.continuation);
    }
    if (const auto& continuation = member.continuation) {
        if (sound.tie) {
            auto tie   = *continuation;
            tie.letter = sound.letter;
            tie.octave = sound.octave;
            tie.at     = host.at;
            voice.lane().ties.push_back(tie);
        }
        return {};
    }
    auto volume = voice.velocity(voice.lane().time, accented, member.stress_velocity);
    if (!sound.tie
        || !voice.settings.stress.articulates(host.at, voice.lanes.notation().fields.meter))
        volume.without_stress.reset();
    const auto hit = voice.output.attack(
        voice.lanes.id(),
        member.route,
        member.sounding,
        voice.lane().time,
        member.audible,
        member.delay,
        volume,
        host.at
    );
    if (sound.tie) {
        std::optional<TieStress> span;
        if (volume.without_stress)
            span = TieStress{
                host.at,
                voice.lanes.notation().fields.meter,
                timing.written_start,
                timing.written_length,
                member.written_gate
            };
        voice.lane().ties.push_back(
            {sound.pitch.note,
             sound.letter,
             sound.octave,
             hit.gate,
             host.at,
             span,
             member.overlap_release}
        );
    }
    return hit;
}

std::optional<LyricAnchor> NoteEmitter::Chord::play() {
    resolve();
    // Each member now owns its matched continuation. New ties start in written pitch order.
    voice.lane().ties.clear();
    if (play_ornament()) return {};
    std::optional<LyricAnchor> first;
    voice.releases.chord(
        voice.lanes.id(), voice.lane().time, timing.full.duration, host.at, [&](auto record) {
            for (size_t i = 0; i < members.size(); ++i) {
                const auto& member = members[i];
                if (const auto hit = emit(member, host.atom->notes[i].decorations)) {
                    record(hit->gate, member.delay, member.gap);
                    if (!first) first = hit->lyric;
                }
            }
        }
    );
    return first;
}

NoteEmitter::NoteEmitter(
    const TuneSettings&  settings,
    OverlayLanes&        lanes,
    VoiceOutput&         output,
    ChordAttackSequence& chord_attacks,
    Diagnostics&         diagnostics,
    const VoicePlan&     plan
) :
    settings(settings), lanes(lanes), output(output), chord_attacks(chord_attacks),
    diagnostics(diagnostics), releases(plan, diagnostics) {}

NoteEmitter::~NoteEmitter() = default;

void NoteEmitter::play_grace_attack(const Atom& atom, Location at, AttackTiming timing, f64 unit) {
    const Prepared grace{
        .atom        = &atom,
        .at          = at,
        .unit        = unit,
        .tuplet_unit = unit,
        .tied        = atom.tie,
        .slur        = SlurPosition::Inside
    };
    Chord{*this, grace, timing, 0}.play();
}

void NoteEmitter::play_notes(
    const Prepared& atom, AttackTiming timing, f64 borrowed, const std::string* syllable
) {
    if (atom.atom->notes.empty()) {
        report_ties(lane().ties, diagnostics);
        return;
    }
    auto anchor = Chord{*this, atom, timing, borrowed}.play();
    if (syllable) {
        diagnostics.spend(PlaybackWork::text(*syllable), atom.at);
        if (!anchor) anchor = output.lyric_anchor(lanes.id(), lane().time, atom.at);
        anchor->append(*syllable);
    }
}

void NoteEmitter::finish() {
    for (auto& tied : ornaments) {
        const auto at = tied.phrase.at;
        tied.phrase.play(
            settings,
            tied.expression,
            releases,
            diagnostics,
            [&](size_t pulse, MidiPitch note, f64 start, f64 gate, i8 volume) {
                if (pulse == 0) {
                    output.revise_attack(tied.first, gate, volume, at);
                    return tied.first;
                }
                return output.ornament_pulse(tied.first, note, start, gate, volume, at);
            }
        );
    }
    releases.apply();
}

void NoteEmitter::play_harp_lead(const Prepared& atom, StressNote first, StressNote second) {
    auto&      active = lane();
    const auto pitch  = pitches(atom.at).read(atom.atom->notes.front().pitch, false).pitch;
    lead(pitch, active.time, first, first.duration, atom);
    lead(pitch, active.time + first.duration, second, second.duration, atom);
    const auto length = first.duration + second.duration;
    active.advance(length);
    if (length > 0) active.ties.clear();
}

void NoteEmitter::play_cut(const Prepared& atom, StressNote timing, i32 neighbor) {
    auto&      active = lane();
    const auto pitch  = pitches(atom.at).neighbor(atom.atom->notes.front().pitch, neighbor);
    const auto route  = output.chord_route(lanes.id(), 0);
    // The lead occupies a slurred grace slot and can overlap the host.
    const auto gate = active.expression.gate(
        timing.duration, true, route_gate(route, GateArticulation::Normal), {}
    );
    lead(pitch, active.time, timing, gate, atom);
    active.advance(timing.duration);
    active.ties.clear();
}

namespace {

std::optional<i32> pedal_mark(std::string_view mark) {
    if (mark == "ped" || mark == "ped(") return kMidiMax;
    if (mark == "ped-up" || mark == "ped)" || mark == "ped-end") return 0;
    return {};
}

std::optional<VoiceOutput::EnvelopeMark> envelope_mark(std::string_view mark) {
    using Mark = VoiceOutput::EnvelopeMark;
    if (mark == "bend") return Mark::Bend;
    if (mark == "shape") return Mark::Shape;
    return {};
}

}   // namespace

void NoteEmitter::apply_decoration(
    std::string_view spelling, Location at, const std::optional<TieNote>& continuation
) {
    const auto mark = read_mark(spelling);
    diagnostics.spend(PlaybackWork::text(mark), at);
    if (mark.starts_with('"') || mark == "nil" || mark == "none") return;
    if (ornament_flag(spelling)) return;
    if (const auto change = settings.dynamics.mark(mark, at)) {
        lane().expression.apply(*change);
        return;
    }
    if (const auto pedal = pedal_mark(mark)) {
        const auto onset = tick(lane().time, diagnostics, at);
        output.control(lanes.id(), onset, 64, *pedal);
        return;
    }
    if (const auto envelope = envelope_mark(mark)) {
        if (!continuation) output.envelope_next(lanes.id(), *envelope, at);
        return;
    }
    if (mark.size() == 1 && mark.front() >= '0' && mark.front() <= '5') return;
    constexpr std::array kAcceptedMarks{
        "coda"sv, "segno"sv, "tenuto"sv, "wedge"sv, "downbow"sv, "upbow"sv
    };
    if (std::ranges::find(kAcceptedMarks, mark) != kAcceptedMarks.end()) return;
    // ABC 2.1 section 4.14 defines these shorthands. abc2midi leaves them unperformed.
    if (spelling.size() == 1 && "MOPSuv"sv.find(mark) != std::string_view::npos) return;
    // Section 4.14 defines these spellings. abc2midi gives them no
    // MIDI effect, including when navigation marks appear together.
    constexpr std::array kUnperformedMarks{
        "trill("sv,
        "trill)"sv,
        "lowermordent"sv,
        "uppermordent"sv,
        "mordent"sv,
        "pralltriller"sv,
        "turn"sv,
        "turnx"sv,
        "invertedturn"sv,
        "invertedturnx"sv,
        "+"sv,
        "plus"sv,
        "snap"sv,
        "slide"sv,
        "open"sv,
        "thumb"sv,
        "D.S."sv,
        "D.C."sv,
        "dacoda"sv,
        "dacapo"sv,
        "fine"sv,
        "shortphrase"sv,
        "mediumphrase"sv,
        "longphrase"sv
    };
    if (std::ranges::find(kUnperformedMarks, mark) != kUnperformedMarks.end()) return;
    diagnostics.warn(
        at, "ignored-decoration", fmt::format("ignored unsupported decoration '{}'", mark)
    );
}

}   // namespace symusic::abc::detail

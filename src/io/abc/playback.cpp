#include "playback.h"

#include <algorithm>
#include <cmath>
#include <locale>
#include <optional>
#include <sstream>
#include <utility>
#include <variant>

#include <fmt/format.h>

#include "attacks.h"
#include "channel_effect.h"
#include "lanes.h"

namespace symusic::abc::detail {
namespace {

struct GraceGroup {
    const Grace*      notes = nullptr;
    Location          at{};
    f64               fixed_unit = 0;
    GraceSettings     lengths;
    vec<const Token*> after;
};

// Verses address individual written notes, including tied continuations.
class LyricLine {
public:
    explicit LyricLine(const WrittenLyrics& lyrics) : written(lyrics), slot_visits(lyrics.size()) {}
    void               enter_part(size_t occurrence);
    void               jump(bool continues_lyrics);
    const std::string* take_syllable(size_t source_line, const Atom& atom);
    void               bar(size_t line) { start(line); }

private:
    void start(size_t next);
    void finish();

    const WrittenLyrics& written;
    size_t               line           = 0;
    size_t               occurrence     = 0;
    bool                 carry_verse    = false;
    bool                 replaying_line = false;
    size_t               part_visit     = 1;
    // Most recent part visit that played each written lyric slot. Zero means unread.
    vec<size_t> slot_visits;
};

void LyricLine::start(size_t next) {
    if (!next) return;
    // abc2midi resumes the current lyric line when the jump bypasses its
    // opening. On this replay, keep only syllables on newly visited notes.
    if (std::exchange(carry_verse, false))
        replaying_line = true;
    else if (line != next)
        replaying_line = false;
    line = next;
}

const std::string* LyricLine::take_syllable(size_t source_line, const Atom& atom) {
    start(source_line);
    if (!atom.lyric_slot) return nullptr;
    const auto slot     = atom.lyric_slot - 1;
    const bool repeated = std::exchange(slot_visits[slot], part_visit) == part_visit;
    if (replaying_line && repeated) return nullptr;
    const auto& lyrics   = written[slot];
    const auto  verse    = occurrence % lyrics.verse_count;
    const auto  syllable = std::ranges::lower_bound(
        lyrics.by_verse, verse, {}, &std::pair<size_t, std::string>::first
    );
    return syllable != lyrics.by_verse.end() && syllable->first == verse ? &syllable->second :
                                                                           nullptr;
}

void LyricLine::jump(bool continues_lyrics) {
    if (continues_lyrics)
        carry_verse = true;
    else
        finish();
}

void LyricLine::enter_part(size_t next) {
    finish();
    occurrence = next;
    ++part_visit;
}

void LyricLine::finish() {
    line           = 0;
    carry_verse    = false;
    replaying_line = false;
}

f64 held_length(const Prepared& atom) {
    const auto duration = atom.played_duration();
    return atom.ornament.fermata ? fermata_length(duration, atom.unit, atom.lengths.fermata) :
                                   duration;
}

f64 grace_room(const Prepared& atom, const GraceGroup& grace) {
    if (const auto attacks = harp_attacks(atom))
        return attacks->first * atom.unit * atom.rhythm_scale;
    // A directive after the grace can change the host's hold. A unit change
    // alone cannot: the host already has its new unit.
    const bool hold_known = std::ranges::all_of(grace.after, [](const auto* effect) {
        const auto* field = std::get_if<Field>(&effect->value);
        return field && field->kind == 'L';
    });
    const auto duration   = hold_known ? held_length(atom) : atom.played_duration();
    return atom.staccato() ? staccato_length(duration) : duration;
}

bool two_neighbor_roll(const Prepared& atom) {
    // abc2midi selects the two-neighbor roll when the host is three units
    // after tuplets and fermata, before broken rhythm. Three equal slices
    // preserve that length even when the input ratio was unreduced.
    if (!atom.cut || atom.harp_cut() || !atom.neighbor_cut()) return false;
    const auto length = atom.ornament.fermata ?
                            fermata_length(atom.cut->length, atom.lengths.fermata) :
                            atom.cut->length;
    return length.numerator == 3 * length.denominator;
}

// Render one written slot on the current lane. Each slice advances the stress
// clock by its written length and the playback clock by its warped length.
struct PerformedRhythm {
    PlaybackLane&     lane;
    const Meter&      meter;
    const TuneStress& stress;
    NoteEmitter&      notes;
    Accompaniment&    accompaniment;
    size_t            lane_id;
    Diagnostics&      diagnostics;

    void play(Prepared atom, const std::string* syllable, const auto& lead_in) {
        if (!two_neighbor_roll(atom)) {
            play_segment(atom, syllable, lead_in(atom));
            return;
        }
        // The first slice receives the grace and lyric. Only the last starts a tie.
        const bool tied       = std::exchange(atom.tied, false);
        atom.duration         = atom.unit;
        atom.ornament.fermata = atom.ornament.cut = false;
        play_segment(atom, syllable, lead_in(atom));
        atom.ornament.cut = true;
        play_segment(atom, nullptr, 0);
        atom.tied = tied;
        play_segment(atom, nullptr, 0, -1);
    }
    f64 play_grace(const Prepared& host, const GraceGroup& grace);

private:
    AttackTiming take(Location at, f64 length, f64 staccato) {
        const auto phase = lane.stress_phase;
        lane.stress_phase += length;
        return {stress.note(at, phase, length, meter), phase, length, staccato};
    }
    void play_segment(
        const Prepared& atom, const std::string* syllable, f64 borrowed, i32 neighbor = 1
    );
};

f64 PerformedRhythm::play_grace(const Prepared& atom, const GraceGroup& grace) {
    const auto& attacks = grace.notes->attacks;
    f64         weights = 0;
    for (const auto& attack : attacks) weights += attack.weight();
    if (weights == 0) return 0;
    const auto room   = grace_room(atom, grace);
    const auto length = grace.lengths.step(room, grace.fixed_unit, weights);
    const auto borrow = length * weights;
    if (borrow + kEpsilon >= room) {
        diagnostics.warn(
            atom.at,
            "grace-too-long",
            atom.harp_cut() ?
                "grace sequence would consume the first harp attack; ignored the grace notes" :
                "grace sequence would consume its host note; ignored the grace notes"
        );
        return 0;
    }
    if (length == 0) return 0;
    for (const auto& attack : attacks) {
        const auto duration = attack.weight() * length;
        const auto timing   = take(grace.at, duration, staccato_length(duration));
        notes.play_grace_attack(attack.atom, grace.at, timing, length * attack.rhythm_scale);
        lane.advance(timing.full.duration);
    }
    return borrow;
}

void PerformedRhythm::play_segment(
    const Prepared& atom, const std::string* syllable, f64 borrowed, i32 neighbor
) {
    const auto unwarped = held_length(atom);
    auto       tail     = unwarped - borrowed;
    auto       cut      = 0.0;
    if (const auto attacks = harp_attacks(atom)) {
        const auto scale = atom.unit * atom.rhythm_scale;
        const auto step  = attacks->first * scale;
        // The explicit grace takes time from the first harp attack.
        const auto first  = take(atom.at, std::max(0.0, step - borrowed), 0).full;
        const auto second = take(atom.at, step, 0).full;
        notes.play_harp_lead(atom, first, second);
        tail = attacks->last * scale * atom.last_scale;
    } else if (atom.neighbor_cut()) {
        const auto length = atom.lengths.grace.step(tail, atom.unit, 1);
        if (length + kEpsilon < tail) {
            cut = length;
            notes.play_cut(atom, take(atom.at, cut, 0).full, neighbor);
            tail -= cut;
        }
    }
    const auto lead   = borrowed + cut;
    const auto timing = take(atom.at, tail, std::max(0.0, staccato_length(unwarped) - lead));
    const auto end    = lane.time + timing.full.duration;
    tick(end, diagnostics, atom.at);
    notes.play_notes(atom, timing, lead, syllable);
    lane.advance(timing.full.duration);
    accompaniment.advance(lane_id, lane.time, atom.at);
}

// Interpret one voice's navigation schedule. Pitch policy and symbol slots
// use source positions, while timed commands run on the active lane's clock.
// The output and accompaniment adapters own event storage, not the clock.
class VoicePlayer {
public:
    VoicePlayer(
        const TuneSettings&  settings,
        VoicePlan&           plan,
        std::string          name,
        const WrittenLyrics& lyrics,
        ChordAttackSequence& chord_attacks,
        Diagnostics&         diagnostics
    ) :
        diagnostics(diagnostics), output(std::move(name), settings.programs, diagnostics),
        settings(settings),
        notation(written_navigation(
            plan, {settings.meter, settings.key, settings.unit}, settings, diagnostics
        )),
        accompaniment(settings.programs, diagnostics),
        lanes(
            {settings.meter, settings.key, settings.unit},
            diagnostics,
            std::move(plan.lanes),
            accompaniment
        ),
        notes(settings, lanes, output, chord_attacks, diagnostics, plan), lyric_line(lyrics) {
        lane().played_meter = settings.meter;
        accompaniment.open_lane(0, settings.meter, 0, lanes.plan(0).backing_opening);
        for (size_t i = 0; i < lanes.plan_count(); ++i) {
            auto& lane = lanes.plan(i);
            if (lane.backing.drums) accompaniment.declare_drums(i, lane.backing_opening);
            output.add_lane(std::move(lane.channels), lane.opening_drums);
        }
    }

    Performance render(const VoicePlan& plan);

private:
    Diagnostics&            diagnostics;
    VoiceOutput             output;
    const TuneSettings&     settings;
    const WrittenNavigation notation;
    Accompaniment           accompaniment;
    OverlayLanes            lanes;
    WrittenFields&          fields() { return lanes.notation().fields; }
    PlaybackLane&           lane() { return lanes.active(); }
    NoteEmitter             notes;
    GraceGroup              grace;
    LyricLine               lyric_line;
    Performance             result;
    struct RepeatEntry {
        WrittenState           written;
        VoiceOutput::Selection route;
        bool                   before_first_bar;
    };
    std::optional<RepeatEntry> repeat_entry;

    [[nodiscard]] bool can_defer(const Token& token) const;
    void               visit_token(const Token& token);
    void visit_step(const PlayRange& range, const Token& token, const WrittenState* entry);
    void restore_written_fields(const WrittenFields& entry);
    void schedule(const Token& token);
    void enter_repeat();
    void enter_implicit_repeat(Location at);
    void jump_repeat(Location at);

    i32  at_tick(Location at) { return tick(lane().time, diagnostics, at); }
    void orphan_grace();
    void grace_effects();
    void midi(std::string_view command, Location at);
    void information(std::string_view value, Location at);
    void change_key(std::string_view value, Location at);
    void set_notated_meter(Meter next);
    void publish_meter(Meter next, Location at);
    void apply_written_meter(Meter next, Location at);
    void visit(const DrumMapChange& change, Location at) {
        diagnostics.spend(change.text_work, at);
        if (!change.route) return;
        output.map_drums(lanes.id(), *change.route);
        accompaniment.map_drums(lanes.id(), *change.route, lane().time, at);
    }
    void visit(const ChordChannelRequest& request, Location at) {
        diagnostics.spend(request.text_work, at);
        output.allocate_channels(lanes.id(), request.count, at);
    }
    void visit(const StressCommand& command, Location at) {
        diagnostics.spend(command.text_work, at);
        if (command.profile_loaded) lane().expression.enable_stress();
    }
    void visit(const Directive& command, Location at);
    void visit(const TempoNotation& speed, Location at);
    void visit(const PitchChange& change, Location);
    void visit(const MidiTrackCommand& command, Location at);
    void visit(const VoiceInstrument& change, Location at);
    void visit(const Field& field, Location at);
    void visit(const Atom& atom, const Token& token);
    void visit(const Bar& bar, const Token& token) { play_bar(bar, token.at, token.music_line); }
    void visit(const auto& value, const Token& token) { visit(value, token.at); }
    // Written-only constructs have already shaped their notes.
    void visit(const auto&, Location) {}
    void visit(const Grace& grace, const Token& token);
    void visit(const Harmony& harmony, const Token& token);
    void visit(const Decoration& decoration, Location at) {
        notes.apply_decoration(decoration.spelling, at);
    }
    void visit(Overlay change, Location at);
    void close_played_measure(Location at);
    void play_bar(const Bar& bar, Location at, size_t music_line, Flow flow = Flow::Token);
};

void VoicePlayer::orphan_grace() {
    if (grace.notes)
        diagnostics.warn(grace.at, "orphan-grace", "ignored grace notes without a following note");
    grace_effects();
    grace = {};
}

void VoicePlayer::grace_effects() {
    for (const auto* token : std::exchange(grace.after, {})) visit_token(*token);
}

void VoicePlayer::visit(const Atom& atom, const Token& token) {
    const auto*     syllable = lyric_line.take_syllable(token.music_line, atom);
    PerformedRhythm rhythm{
        lane(), fields().meter, settings.stress, notes, accompaniment, lanes.id(), diagnostics
    };
    rhythm.play(notation.notes.at(&token), syllable, [&](const Prepared& host) {
        if (!grace.notes) return 0.0;
        const auto borrowed = rhythm.play_grace(host, grace);
        if (!grace.after.empty()) {
            accompaniment.advance(lanes.id(), lane().time, token.at);
            grace_effects();
        }
        grace = {};
        return borrowed;
    });
}

void VoicePlayer::visit(const Grace& next, const Token& token) {
    if (grace.notes) orphan_grace();
    // ABC 2.1 section 4.12 leaves the grace unit to the player. abc2midi
    // scales fixed grace notes by the tuplet active at the opening brace.
    grace
        = {&next,
           token.at,
           notation.grace_units.at(&token),
           settings.note_lengths.at(token.at).grace,
           {}};
}

void VoicePlayer::visit(const Harmony& chord, const Token& token) {
    if (chord.voicing && (chord.voicing->declares_backing || lanes.plan(lanes.id()).backing.chords))
        accompaniment.harmony(
            *chord.voicing, lanes.id(), fields().tone.transpose.value_or(0), token.at
        );
}

void VoicePlayer::visit(Overlay change, Location at) {
    orphan_grace();
    if (change == Overlay::Split) accompaniment.bar(lanes.id(), lane().time, at);
    lanes.control(change, at);
    if (change == Overlay::Split) {
        accompaniment.open_lane(
            lanes.id(), lane().played_meter, lane().bar, lanes.plan(lanes.id()).backing_opening
        );
        accompaniment.advance(lanes.id(), lane().time, at);
    }
}

void VoicePlayer::close_played_measure(Location at) {
    if (lane().time - lane().bar > kEpsilon) {
        lane().before_first_bar = false;
        lane().part_pickup      = false;
    }
    lane().reset_played_bar(lane().time);
    accompaniment.bar(lanes.id(), lane().time, at);
    if (lanes.id() == 0) lanes.primary_boundary(at);
}

void VoicePlayer::play_bar(const Bar& bar, Location at, size_t music_line, Flow flow) {
    orphan_grace();
    if (!bar.resets_measure) return;
    lyric_line.bar(music_line);
    lanes.close_implicit();
    lanes.notation().accidentals.reset();
    lane().reset_written_bar();
    // An inline ending inherits the accidental reset without a new downbeat.
    const bool soft = bar.repeat_start || bar.repeat_end || bar.section;
    if (flow != Flow::InlineEnding && (!soft || lane().closes_soft_bar())) close_played_measure(at);
    // A written double bar sets the overlay origin even if it breaks a measured bar early.
    else if (lanes.id() == 0 && bar.section)
        lanes.primary_boundary();
}

// The MIDI message that "snt" selects has no counterpart in the score, so the
// arguments are validated and then discarded.
bool valid_note_tuning(std::string_view args) {
    const auto key  = integer(word(args));
    const auto text = word(args);
    if (!key || text.empty() || !trim(args).empty()) return false;
    constexpr f32      kLimit = 128.0f - 1.0f / 16384;
    f32                pitch  = 0;
    std::istringstream number{std::string(text)};
    number.imbue(std::locale::classic());
    return number >> pitch && number.eof() && *key >= 0 && *key <= 127 && pitch >= 0
           && pitch < kLimit;
}

void VoicePlayer::midi(std::string_view command, Location at) {
    const auto name = word(command);
    if (PitchTuning::handles_midi(name) || WrittenNoteLengths::handles(name)) return;
    if (name == "snt") {
        if (!valid_note_tuning(command))
            diagnostics.warn(at, "invalid-midi", "expected 'snt <MIDI key> <absolute pitch>'");
        return;
    }
    if (const auto effect = channel_effect(name, command, at, diagnostics)) {
        const auto time = at_tick(at);
        if (effect->destination == ChannelEffect::Destination::Voice)
            output.apply(*effect, lanes.id(), time);
        else
            accompaniment.apply(*effect, lanes.id(), time);
        return;
    }
    if (!lane().expression.directive(name, command, fields().unit, fields().meter, at, diagnostics)
        && !output.directive(name, command, lanes.id(), at)
        && !accompaniment.directive(
            name, command, lanes.id(), lane().played_meter, lane().time, at
        )) {
        diagnostics.warn(at, "unsupported-midi", fmt::format("ignored MIDI directive '{}'", name));
    }
}

void VoicePlayer::information(std::string_view value, Location at) {
    const auto command = word(value);
    if (command == "MIDI") midi(value, at);
}

void VoicePlayer::visit(const Directive& command, Location at) {
    diagnostics.spend(PlaybackWork::text(command.value), at);
    information(command.value, at);
}

void VoicePlayer::visit(const VoiceInstrument& change, Location) { output.set_instrument(change); }

void VoicePlayer::visit(const MidiTrackCommand& command, Location at) {
    const auto id = lanes.id();
    lane().midi.apply(command);
    if (const auto* change = std::get_if<MidiTransposition>(&command)) {
        accompaniment.transpose(id, *change);
    } else if (const auto& spacing = std::get<ChordAttackChange>(command); !spacing.random) {
        accompaniment.chord_gap(id, spacing.ticks, lane().time, at);
    }
}

void VoicePlayer::change_key(std::string_view value, Location at) {
    if (!lanes.notation().change_key(value, diagnostics, at)) return;
    result.keys.emplace_back(
        KeySignature<Tick>(
            at_tick(at), static_cast<i8>(fields().tone.fifths), static_cast<i8>(fields().tone.minor)
        ),
        at
    );
}

void VoicePlayer::set_notated_meter(Meter next) {
    lane().rebase_accents(fields().meter, next);
    fields().meter = next;
}

void VoicePlayer::publish_meter(Meter next, Location at) {
    set_notated_meter(next);
    result.meters.emplace_back(score_meter(next, at_tick(at)), at);
}

void VoicePlayer::apply_written_meter(Meter next, Location at) {
    // Only a performed M: resets the beat spacing and guitar pattern in abc2midi.
    lane().expression.reset_beat_spacing();
    lane().set_played_meter(next);
    accompaniment.meter(lanes.id(), next);
    publish_meter(next, at);
}

void VoicePlayer::visit(const TempoNotation& speed, Location at) {
    if (const auto usec = tempo(speed, fields().unit, diagnostics, at)) {
        result.tempos.emplace_back(Tempo<Tick>(at_tick(at), usec), at);
    }
}

void VoicePlayer::visit(const PitchChange& change, Location) { fields().tone.apply(change); }

void VoicePlayer::visit(const Field& field, Location at) {
    const auto value = field.value;
    diagnostics.spend(PlaybackWork::text(value), at);
    switch (field.kind) {
    case 'K': change_key(value, at); break;
    case 'M': apply_written_meter(std::get<Meter>(notation.metric_fields.at(&field)), at); break;
    case 'L': fields().unit = std::get<f64>(notation.metric_fields.at(&field)); break;
    case 'P':
        // A part can start with a pickup, but its label is not a barline.
        lane().part_pickup = std::fabs(lane().time - lane().bar) < kEpsilon;
        break;
    case 'I': information(value, at); break;
    default: break;
    }
}

void VoicePlayer::enter_repeat() {
    repeat_entry = {lanes.notation(), output.current_route(lanes.id()), lane().before_first_bar};
}

void VoicePlayer::enter_implicit_repeat(Location at) {
    // An implied opening can start a downbeat inside a written measure.
    if (lane().closes_soft_bar()) close_played_measure(at);
    enter_repeat();
}

void VoicePlayer::jump_repeat(Location at) {
    orphan_grace();
    lanes.close_implicit();
    lanes.break_ties();
    const auto& opening          = repeat_entry.value();
    lanes.notation().accidentals = opening.written.accidentals;
    // abc2midi restores the written bar count on a jump. If the repeat starts
    // before the first completed bar, its short closing bar can end a pickup.
    lane().before_first_bar = opening.before_first_bar;
    const auto& entry       = opening.written.fields;
    fields().tone           = entry.tone;
    fields().unit           = entry.unit;
    // Restore written grouping as well as the meter ratio, without rewinding the clock.
    if (fields().meter != entry.meter) publish_meter(entry.meter, at);
    output.restore_route(lanes.id(), opening.route);
}

bool VoicePlayer::can_defer(const Token& token) const {
    if (const auto* decoration = std::get_if<Decoration>(&token.value))
        return !ornament_flag(decoration->spelling);
    if (std::holds_alternative<Harmony>(token.value) || token.is_voice_setting()) return true;
    const auto* field = std::get_if<Field>(&token.value);
    return field && std::string_view("KMLI").find(field->kind) != std::string_view::npos;
}

void VoicePlayer::visit_token(const Token& token) {
    std::visit([&](const auto& value) { visit(value, token); }, token.value);
}

void VoicePlayer::schedule(const Token& token) {
    if (grace.notes && can_defer(token))
        grace.after.push_back(&token);
    else
        visit_token(token);
}

void VoicePlayer::restore_written_fields(const WrittenFields& entry) {
    // Pitch spelling and lengths follow the written voice. MIDI metadata and
    // sounding transposition still follow the performed K: commands.
    const auto transpose    = fields().tone.transpose;
    fields().tone           = entry.tone;
    fields().tone.transpose = transpose;
    fields().unit           = entry.unit;
    if (fields().meter != entry.meter) set_notated_meter(entry.meter);
}

void VoicePlayer::visit_step(
    const PlayRange& range, const Token& token, const WrittenState* entry
) {
    diagnostics.spend(PlaybackWork::kToken, token.at);
    const bool joins_overlay = range.flow == Flow::EnterPart && lanes.in_implicit_overlay();
    if (entry || joins_overlay) { orphan_grace(); }
    if (joins_overlay) lanes.enter_part();
    if (entry) {
        restore_written_fields(entry->fields);
        lanes.notation().accidentals = entry->accidentals;
    }
    switch (range.flow) {
    case Flow::EnterPart: lyric_line.enter_part(range.part_occurrence); break;
    case Flow::EnterRepeat: enter_repeat(); break;
    case Flow::EnterImplicitRepeat: enter_implicit_repeat(token.at); break;
    case Flow::JumpRepeat:
        jump_repeat(token.at);
        lyric_line.jump(range.continues_lyrics);
        break;
    case Flow::SoftBar:
    case Flow::InlineEnding:
    case Flow::HardBar:
        play_bar(
            Bar{.repeat_end = range.flow == Flow::SoftBar ? 2u : 0u},
            token.at,
            token.music_line,
            range.flow
        );
        break;
    case Flow::Token: schedule(token); break;
    }
}

Performance VoicePlayer::render(const VoicePlan& plan) {
    Location end{};
    for (const auto& range : plan.order) {
        const auto  tokens = range.tokens(std::span(plan.source));
        const auto* entry = range.restore_written ? &notation.entries.at(&tokens.front()) : nullptr;
        for (const auto& token : tokens) visit_step(range, token, std::exchange(entry, nullptr));
        end = tokens.back().at;
    }
    orphan_grace();
    lanes.finish_voice(end);
    lanes.break_ties();
    notes.finish();
    auto melody = output.finish();
    for (size_t i = 0; i < melody.size(); ++i)
        result.passes.push_back({lanes.plan(i).declaration, std::move(melody[i])});
    vec<f64> lane_ends;
    for (const auto& clock : lanes.clocks()) lane_ends.push_back(clock.time);
    accompaniment.finish(result.passes, lane_ends, end);
    return std::move(result);
}

}   // namespace

Performance perform(
    const TuneSettings&  settings,
    VoicePlan&&          plan,
    const WrittenLyrics& lyrics,
    std::string          name,
    ChordAttackSequence& chord_attacks,
    Diagnostics&         diagnostics
) {
    auto owned = std::move(plan);
    return VoicePlayer(settings, owned, std::move(name), lyrics, chord_attacks, diagnostics)
        .render(owned);
}

}   // namespace symusic::abc::detail

#include "playback.h"

#include <algorithm>
#include <array>
#include <bitset>
#include <cmath>
#include <deque>
#include <limits>
#include <locale>
#include <map>
#include <memory>
#include <optional>
#include <ranges>
#include <set>
#include <span>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>
#include <variant>

#include <fmt/format.h>

#include "channel_effect.h"

namespace symusic::abc::detail {
namespace {

using namespace std::literals;
constexpr long double epsilon = 0.0000001L;

struct DecorationMark {
    std::string_view name;
    bool             shorthand;
};

DecorationMark read_mark(std::string_view spelling) {
    if (spelling.size() >= 2 && (spelling.front() == '!' || spelling.front() == '+')
        && spelling.back() == spelling.front())
        return {spelling.substr(1, spelling.size() - 2), false};
    return {spelling, spelling.size() == 1};
}

bool Ornament::* ornament_flag(DecorationMark mark);

long double first_broken_factor(Broken mark, const AttackRhythm& rhythm) {
    return mark.first_factor(2.L * rhythm.shorter / (rhythm.longer + rhythm.shorter));
}

bool accepts_broken_pair(long double left_attack, long double right_length, bool right_harp) {
    // A right-hand harp cut includes the preceding attack in abc2midi's range.
    return right_harp || std::fabs(left_attack - right_length) <= epsilon;
}

void warn_unequal_pair(Diagnostics& diagnostics, Location at) {
    diagnostics.warn(
        at, "unequal-broken-rhythm", "ignored broken rhythm between notes of unequal length"
    );
}

struct ChordDecorationTarget {
    long double onset;
    bool        attacks;
};

struct Sound {
    MidiPitch pitch;
    int       letter;
    int       octave;
    bool      explicit_accidental;
    bool      tie;
};

// The measure remembers values, not the policy that wrote them. A pitch-wide
// write replaces older octave overrides for that letter; a local write does not.
class Accidentals {
public:
    Alteration resolve(Pitch pitch, const Key& key, AccidentalScope scope);
    void       reset() { letters = {}; }

private:
    struct Slots {
        std::optional<Alteration> all_octaves;
        std::map<int, Alteration> by_octave;
    };
    std::array<Slots, 7> letters;
};

Alteration Accidentals::resolve(Pitch pitch, const Key& key, AccidentalScope scope) {
    const auto index = letter_index(pitch.letter);
    // ABC 2.1 sections 4.6 and 11.3: changing octave is equivalent to writing
    // an octave mark on the note. Local propagation uses the resulting octave.
    const auto octave = key.effective_octave(pitch.octave);
    auto&      slots  = letters[index];
    if (pitch.accidental) {
        if (scope == AccidentalScope::pitch)
            slots = {pitch.accidental, {}};
        else if (scope == AccidentalScope::octave)
            slots.by_octave.insert_or_assign(octave, *pitch.accidental);
        return *pitch.accidental;
    }
    if (const auto slot = slots.by_octave.find(octave); slot != slots.by_octave.end())
        return slot->second;
    return slots.all_octaves.value_or(key.alterations[index]);
}

struct TieStress {
    Location    at;
    Meter       meter;
    long double start;
    long double next_offset;
    long double release_offset;
};

struct TiedOrnament;

struct TieNote {
    int                                     untransposed_pitch;
    int                                     letter;
    int                                     octave;
    std::variant<AttackGate, TiedOrnament*> note;
    Location                                at;
    std::optional<TieStress>                stress;
    i32                                     overlap_release = 0;
};

struct TieSegment {
    long double ornament_end;
    long double written_gate;
    i32         release;
    i32         overlap_release;
};

struct ChordMember {
    Sound                            sound;
    MidiPitch                        sounding;
    VoiceOutput::Selection           route;
    std::optional<TieNote>           continuation;
    long double                      audible;
    long double                      written_gate;
    i32                              overlap_release;
    int                              stress_velocity;
    std::span<const ChordDecoration> decorations;
    bool                             introduces_gap;
    int                              delay = 0;
    int                              gap   = 0;
};

// A spread chord inserts its releases before some already queued releases.
// abc2midi deducts the current gap from the interval to the next release,
// then adds the full spread to releases sent while stepping past that chord.
class ExpandedReleaseSchedule {
public:
    explicit ExpandedReleaseSchedule(Diagnostics& diagnostics) : diagnostics(diagnostics) {}

    void begin(int lane, long double start, long double duration, Location at) {
        slots.push_back(
            {lane, start, tick(start + duration, diagnostics, at), attacks.size(), 0, at}
        );
    }

    void add(AttackGate gate, int delay, int gap) {
        attacks.push_back({gate, delay, gap});
        ++slots.back().count;
        staggered |= delay != 0;
    }

    void apply() {
        if (!staggered) return;
        // A tied ornament's slots are recorded when its final duration is known.
        if (!std::ranges::is_sorted(slots, {}, &Slot::start))
            std::ranges::stable_sort(slots, {}, &Slot::start);
        for (const auto& slot : slots) {
            if (slot.count == 0) continue;
            auto& pending = lanes[slot.lane];
            release_before(pending, tick(slot.start, diagnostics, slot.at), 0);
            const auto notes = std::span(attacks).subspan(slot.first, slot.count);
            for (const auto& note : notes) insert(pending, note, slot.at);
            release_before(pending, slot.end, notes.back().delay);
        }
        for (auto& [lane, pending] : lanes)
            release_before(pending, std::numeric_limits<i64>::max(), 0);
    }

private:
    struct Slot {
        int         lane;
        long double start;
        i32         end;
        size_t      first;
        size_t      count;
        Location    at;
    };
    struct ScheduledAttack {
        AttackGate gate;
        int        delay;
        int        gap;
    };
    struct Release {
        i64        logical;
        AttackGate gate;
    };
    using Queue = std::deque<Release>;

    Diagnostics&         diagnostics;
    vec<Slot>            slots;
    vec<ScheduledAttack> attacks;
    std::map<int, Queue> lanes;
    bool                 staggered = false;

    void insert(Queue& pending, const ScheduledAttack& note, Location at) {
        const i64  at_tick = static_cast<i64>(note.gate.end()) - note.delay;
        const auto next    = std::ranges::upper_bound(pending, at_tick, {}, &Release::logical);
        if (next != pending.end()) {
            const auto shift = std::min<i64>(note.gap, next->logical - at_tick);
            diagnostics.spend(static_cast<size_t>(pending.end() - next), at);
            for (auto later = next; later != pending.end(); ++later) later->logical -= shift;
        }
        pending.insert(next, {at_tick, note.gate});
    }

    static void release_before(Queue& pending, i64 end, int delay) {
        // Logical release ticks here include the one-tick MIDI onset offset.
        while (!pending.empty() && pending.front().logical <= end) {
            const auto release = pending.front();
            release.gate.release_at(
                static_cast<i32>(
                    std::clamp<i64>(release.logical + delay, 0, std::numeric_limits<i32>::max())
                )
            );
            pending.pop_front();
        }
    }
};

struct EnvelopeCue {
    VoiceOutput::EnvelopeMark mark;
    Location                  at;
};

struct MeterChange {
    Meter    meter;
    Location at;
};

struct UnitChange {
    long double unit;
};

struct PartVerse {
    u32 occurrence;
};

struct ImplicitRepeatStart {
    Location at;
};

// The next note needs the written meter and unit before it is known when
// to apply these changes on the performed clock.
struct DeferredEffects {
    vec<std::variant<const Token*, MeterChange, UnitChange, PartVerse, ImplicitRepeatStart>>
                               ordered;
    std::optional<Meter>       meter;
    std::optional<long double> unit;
};

struct GraceGroup {
    const Grace*    notes = nullptr;
    Location        at{};
    long double     fixed_unit = 0;
    GraceSettings   lengths;
    DeferredEffects after;
};

struct GraceBorrow {
    long double ticks = 0;

    [[nodiscard]] long double shorten(long double gate) const { return gate - ticks; }
};

struct GraceTiming {
    long double written   = 0;
    long double performed = 0;
    GraceBorrow host;
};

// The written interval is the input to the stress map; full and staccato are
// performed durations. Shortening a gate never advances the written cursor.
struct AttackTiming {
    StressNote                full{};
    std::optional<StressNote> staccato;
    long double               written_start    = 0;
    long double               written_length   = 0;
    long double               written_staccato = 0;
};

long double staccato_length(long double duration) { return duration / 2; }

// The chord slot supplies the rhythm and the time borrowed by a preceding grace.
struct FermataHold {
    long double unit;
    FermataMode mode;
};

struct MemberLengths {
    long double                unit;
    GraceBorrow                grace;
    std::optional<FermataHold> hold;
    long double                cut = 0;
};

struct AttackSet {
    const Atom&   atom;
    Location      at;
    bool          tied;
    bool          accented;
    AttackTiming  timing;
    MemberLengths lengths;
};

enum class SlurPosition { outside, inside, closing };

struct OrnamentTiming {
    AttackTiming     timing;
    Meter            meter;
    Location         at;
    SlurPosition     slur;
    GateArticulation gate;
};

// Retain the head's expression and MIDI destination while the written clock
// continues through the tie. An ordinary bar or field can occur inside it.
struct TiedOrnament {
    std::array<MidiPitch, 5>   pattern{};
    size_t                     pattern_size;
    long double                start;
    long double                accent_origin;
    bool                       accented;
    OrnamentTiming             duration;
    Expression                 expression;
    VoiceOutput::RetainedRoute route;
    AttackGate                 first;
    int                        lane;
    int                        gap;
};

// Hold one rhythmic atom until its following note or a closing boundary.
// This is the lookahead needed to size both halves of broken rhythm before
// emitting either half. duration and unit are fractional, unwarped ticks.
struct Prepared {
    const Atom*                atom;
    Location                   at;
    size_t                     music_line;
    long double                duration;
    long double                unit;
    long double                tuplet_unit;
    GraceGroup                 grace;
    Ornament                   ornament;
    NoteLengthSettings         lengths;
    std::optional<CutNotation> cut;
    bool                       tied;
    SlurPosition               slur;
    bool                       unit_numerator;
    long double                rhythm_scale = 1;
    long double                last_scale   = 1;
    [[nodiscard]] long double  played_duration() const { return duration * rhythm_scale; }
    [[nodiscard]] bool         harp_cut() const { return cut && cut->harp; }
    [[nodiscard]] bool         neighbor_cut() const {
        return ornament.cut && !ornament.roll && !ornament.trill && atom->pitches.size() == 1;
    }
    [[nodiscard]] bool staccato() const { return ornament.staccato && !harp_cut(); }
};

struct TupletState {
    int      left = 0;
    Length   ratio;
    Location at{};

    [[nodiscard]] Length scale() const { return left ? ratio : Length{}; }
};

// Written notation can change while reading an overlay, but the primary
// lane resumes with the fields it had at the first split.
struct WrittenState {
    Meter       meter;
    Key         tone;
    long double unit;
};

int default_tuplet(int count, Meter meter) {
    if (count == 2 || count == 4 || count == 8) return 3;
    if (count == 3 || count == 6) return 2;
    // Three quarter-note beats are simple triple time, not compound time.
    const bool compound = !meter.free && meter.numerator > 3 && meter.numerator % 3 == 0;
    return compound ? 3 : 2;
}

TupletState written_tuplet(Tuplet group, Meter meter, Location at) {
    const auto in_time_of
        = group.in_time_of ? group.in_time_of : default_tuplet(group.count, meter);
    return {group.notes ? group.notes : group.count, {in_time_of, group.count}, at};
}

// The performed clock supplies event times and does not rewind on a repeat.
// Written phase chooses hornpipe pairs; stress phase is the position on the
// unwarped stress grid. The three positions are all in fractional ticks.
struct PlaybackLane {
    long double time              = 0;
    long double bar               = 0;
    bool        before_first_bar  = true;
    bool        part_pickup       = false;
    // Soft barlines use the last performed M: and the accumulated beat count.
    Meter       played_meter{.free = true};
    long double bar_check_origin = 0;
    // Hornpipe pairs follow written lengths, not the performed clock.
    long double  written_phase = 0;
    long double  stress_phase  = 0;
    long double  accent_origin = 0;
    Accidentals  accidentals;
    vec<TieNote> ties;
    Expression   expression{};
    int          slur = 0;
    // Re-entry inherits the primary measure phase at the last synchronization.
    long double                reentry_bar = 0;
    std::optional<long double> primary_until;

    void reset_played_bar(long double origin) { bar = accent_origin = bar_check_origin = origin; }
    void reset_written_bar() { written_phase = stress_phase = 0; }

    void reenter() {
        part_pickup = true;
        accidentals = {};
        reset_played_bar(reentry_bar);
        reset_written_bar();
    }

    void rebase_accents(Meter previous, Meter next) {
        accent_origin = rebase(accent_origin, previous, next);
    }

    void set_played_meter(Meter next) {
        bar_check_origin = rebase(bar_check_origin, played_meter, next);
        played_meter     = next;
    }

    [[nodiscard]] bool closes_soft_bar() const {
        return !played_meter.free
               && (before_first_bar || part_pickup
                   || time - bar_check_origin + epsilon >= played_meter.measure());
    }

private:
    [[nodiscard]] long double rebase(long double origin, Meter previous, Meter next) const {
        // Preserve the number of beats counted before a denominator change.
        return time - (time - origin) / previous.beat() * next.beat();
    }
};

void report_ties(vec<TieNote>& ties, Diagnostics& diagnostics) {
    for (const auto& tie : ties)
        diagnostics.warn(tie.at, "unmatched-tie", "tie has no matching next note");
    ties.clear();
}

// Owns every lane for the lifetime of a written voice. A split selects another
// clock; it never copies playback state between active and suspended lanes.
// Only the written fields are restored on joining the primary lane. During
// rendering, synchronized idle lanes deliver their bar boundaries to backing.
class OverlayLanes {
public:
    OverlayLanes(
        Expression expression, Diagnostics& diagnostics, Accompaniment* backing = nullptr
    ) :
        diagnostics(diagnostics), initial_expression(expression), backing(backing),
        lanes{new_lane(0, 0)} {}
    PlaybackLane&                               active() { return lanes[selected]; }
    [[nodiscard]] int                           id() const { return static_cast<int>(selected); }
    [[nodiscard]] std::span<const PlaybackLane> clocks() const { return lanes; }
    [[nodiscard]] bool in_implicit_overlay() const { return group && !group->explicit_start_bar; }
    void               control(Overlay command, WrittenState& written, Location at);
    bool               close_implicit(WrittenState& written);
    void               enter_part(WrittenState& written);
    void               finish_voice(WrittenState& written, Location at);
    void               primary_boundary(std::optional<Location> closing_bar = {});
    void               break_ties();

private:
    struct PrimaryBoundary {
        long double             time;
        long double             phase;
        std::optional<Location> closing_bar;
    };

    struct Group {
        std::optional<long double>  explicit_start_bar;
        std::optional<WrittenState> primary_fields;
        vec<PrimaryBoundary>        deferred_boundaries;
    };

    Diagnostics&         diagnostics;
    const Expression     initial_expression;
    Accompaniment*       backing;   // Absent when only discovering drum-map lane IDs.
    vec<PlaybackLane>    lanes;
    size_t               selected              = 0;
    long double          last_primary_boundary = 0;
    std::optional<Group> group;

    PlaybackLane new_lane(long double time, long double bar) const;
    void         split(WrittenState& written, Location at);
    void         join(WrittenState& written);
    void         advance_idle_lanes(PrimaryBoundary boundary, size_t first_lane);
};

void OverlayLanes::control(Overlay command, WrittenState& written, Location at) {
    switch (command) {
    case Overlay::Split: split(written, at); break;
    case Overlay::Start:
    case Overlay::StartAtCursor:
        if (group)
            diagnostics.error(at, "nested-overlay", "nested overlays have no unambiguous playback");
        // The written start of a two-bar overlay can follow a repeat that did
        // not close the performed measure. Its lanes still begin together.
        if (command == Overlay::StartAtCursor) primary_boundary();
        group = Group{.explicit_start_bar = active().bar};
        break;
    case Overlay::End:
        if (!group) {
            diagnostics.warn(at, "unexpected-overlay-end", "ignored '&)' without an overlay");
            return;
        }
        // Only a split introduces the virtual bar that ends the primary branch.
        if (group->primary_fields) lanes.front().accidentals.reset();
        join(written);
        active().reset_played_bar(active().time);
        active().reset_written_bar();
        break;
    case Overlay::PreviousTwo:
        diagnostics.error(
            at, "unresolved-overlay", "two-bar overlay was not resolved before playback"
        );
    }
}

PlaybackLane OverlayLanes::new_lane(long double time, long double bar) const {
    return {.time = time, .expression = initial_expression, .reentry_bar = bar};
}

void OverlayLanes::split(WrittenState& written, Location at) {
    if (!group) group = Group{};
    if (!group->primary_fields) group->primary_fields = written;
    const auto parent_meter = active().played_meter;
    if (++selected >= max_voice_lanes)
        diagnostics.error(at, "overlay-limit", "too many overlay lanes");
    if (selected == lanes.size()) {
        const auto bar = group->explicit_start_bar.value_or(lanes.front().bar);
        lanes.push_back(new_lane(last_primary_boundary, bar));
        active().played_meter = parent_meter;
    }
    active().reenter();
}

void OverlayLanes::join(WrittenState& written) {
    if (group->primary_fields) {
        written = *group->primary_fields;
        if (group->explicit_start_bar) {
            // abc2midi terminates the primary branch with a virtual bar at its split.
            // The next group can start there without an intervening written bar.
            for (auto& idle : std::span(lanes).subspan(selected + 1))
                if (lanes.front().time > idle.primary_until.value_or(last_primary_boundary))
                    report_ties(idle.ties, diagnostics);
            // The written branches delivered their own bars. Only unplayed
            // lanes need the intervening boundaries to restart their backing.
            for (const auto& boundary : group->deferred_boundaries)
                advance_idle_lanes(boundary, selected + 1);
            advance_idle_lanes({lanes.front().time, 0, {}}, selected + 1);
        }
        for (auto& lane : std::span(lanes).subspan(1, selected)) {
            lane.primary_until = lanes.front().time;
            if (group->explicit_start_bar) lane.reentry_bar = lane.bar;
        }
    } else
        for (const auto& boundary : group->deferred_boundaries) advance_idle_lanes(boundary, 1);
    selected = 0;
    group.reset();
}

bool OverlayLanes::close_implicit(WrittenState& written) {
    if (!in_implicit_overlay()) return false;
    join(written);
    return true;
}

void OverlayLanes::enter_part(WrittenState& written) {
    if (!close_implicit(written)) return;
    // The melody resumes on a downbeat. Secondary lanes continue their
    // existing measure rather than receiving the melody's reset.
    primary_boundary();
    active().reset_played_bar(active().time);
    active().reset_written_bar();
}

void OverlayLanes::finish_voice(WrittenState& written, Location at) {
    if (!group) return;
    if (group->explicit_start_bar)
        diagnostics.warn(at, "unterminated-overlay", "closed overlay at the end of the voice");
    join(written);
}

void OverlayLanes::primary_boundary(std::optional<Location> closing_bar) {
    const auto            now = lanes.front().time;
    const PrimaryBoundary boundary{now, now - lanes.front().bar, closing_bar};
    // Bars in the first branch must not advance the clocks of branches we have yet to read.
    if (group && group->explicit_start_bar)
        group->deferred_boundaries.push_back(boundary);
    else
        advance_idle_lanes(boundary, 1);
}

void OverlayLanes::advance_idle_lanes(PrimaryBoundary boundary, size_t first_lane) {
    for (size_t i = first_lane; i < lanes.size(); ++i) {
        auto& lane = lanes[i];
        if (!lane.primary_until && boundary.time > last_primary_boundary)
            report_ties(lane.ties, diagnostics);
        lane.time += boundary.time - lane.primary_until.value_or(last_primary_boundary);
        lane.reentry_bar = lane.time - boundary.phase;
        lane.primary_until.reset();
        if (backing && boundary.closing_bar)
            backing->bar(static_cast<int>(i), lane.time, *boundary.closing_bar);
    }
    last_primary_boundary = boundary.time;
}

void OverlayLanes::break_ties() {
    // A suspended primary lane is outside a secondary lane's repeat jump.
    for (size_t index = selected == 0 ? 0 : 1; index < lanes.size(); ++index)
        report_ties(lanes[index].ties, diagnostics);
}

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

bool declares_legacy_drum(const Token& token) {
    const auto legacy_drum
        = [](std::string_view spelling) { return read_mark(spelling).name == "drum"; };
    const auto decorated_chord = [&](const Atom& atom) {
        return std::ranges::any_of(atom.decorations, legacy_drum, &ChordDecoration::spelling);
    };
    if (const auto* decoration = std::get_if<Decoration>(&token.value))
        return legacy_drum(decoration->spelling);
    if (const auto* atom = std::get_if<Atom>(&token.value)) return decorated_chord(*atom);
    if (const auto* grace = std::get_if<Grace>(&token.value))
        return std::ranges::any_of(grace->attacks, decorated_chord, &GraceAttack::atom);
    return false;
}

// abc2midi initializes each track from the first channel declared for its
// voice. Every written split sets the child's initial channel from its parent.
bool discover_tracks(VoicePlan& plan, const vec<Token>& music, Location declaration) {
    auto& result = plan.lanes;
    result.push_back({.declaration = declaration});
    size_t active           = 0;
    bool   explicit_overlay = false;
    bool   has_requests     = false;
    for (const auto& token : music) {
        if (const auto* overlay = std::get_if<Overlay>(&token.value)) {
            switch (*overlay) {
            case Overlay::Start:
            case Overlay::StartAtCursor: explicit_overlay = true; break;
            case Overlay::End:
                active           = 0;
                explicit_overlay = false;
                break;
            case Overlay::Split:
                if (active == max_voice_lanes - 1) break;   // Diagnosed during playback.
                plan.uses_conductor = true;
                if (++active == result.size()) result.push_back({.declaration = token.at});
                result[active].declared_channel = result[active - 1].declared_channel;
                break;
            case Overlay::PreviousTwo: break;
            }
            continue;
        }
        if (!explicit_overlay) {
            if (const auto* bar = std::get_if<Bar>(&token.value); bar && bar->resets_measure)
                active = 0;
            if (const auto* field = std::get_if<Field>(&token.value); field && field->kind == 'P')
                active = 0;
        }
        auto& lane = result[active];
        if (std::holds_alternative<MidiTrackCommand>(token.value)) has_requests = true;
        discover_backing(token, lane.backing);
        if (!plan.uses_conductor)
            plan.uses_conductor = lane.backing.any() || declares_legacy_drum(token);
        auto command = token.information().value_or("");
        if (word(command) != "MIDI") continue;
        const auto name = word(command);
        if (name == "channel" && !lane.declared_channel) {
            const auto channel = integer(word(command));
            if (channel && *channel >= 1 && *channel <= midi_channels)
                lane.declared_channel = channel;
        }
        has_requests = has_requests || name == "expand"
                       || chord_channel_request(name, command).value_or(0) != 0;
    }
    return has_requests;
}

// Project the same explicit and implicit overlay boundaries as playback,
// without advancing musical clocks or emitting events.
class PerformedLanes {
public:
    explicit PerformedLanes(Diagnostics& diagnostics) : lanes(Expression{}, diagnostics) {}
    int  id() const { return lanes.id(); }
    bool boundary(PlayStep step);

private:
    OverlayLanes lanes;
    WrittenState state{};
};

bool PerformedLanes::boundary(PlayStep step) {
    switch (step.flow) {
    case Flow::enter_part:
    case Flow::jump_repeat:
    case Flow::soft_bar:
    case Flow::inline_ending:
    case Flow::hard_bar: lanes.close_implicit(state); return true;
    case Flow::enter_repeat:
    case Flow::enter_implicit_repeat: return true;
    case Flow::token: break;
    }
    const auto& token = *step.token;
    if (const auto* overlay = std::get_if<Overlay>(&token.value)) {
        lanes.control(*overlay, state, token.at);
        return true;
    }
    if (const auto* bar = std::get_if<Bar>(&token.value); bar && bar->resets_measure) {
        lanes.close_implicit(state);
        return true;
    }
    return false;
}

bool enters_written_passage(PlayStep step, const Token*& next_written) {
    const bool entry
        = step.token > next_written || (step.flow == Flow::enter_part && step.token < next_written);
    // Entry and jump controls leave the target token for a later step to consume.
    const bool control = step.flow == Flow::enter_part || step.flow == Flow::enter_repeat
                         || step.flow == Flow::enter_implicit_repeat
                         || step.flow == Flow::jump_repeat;
    next_written       = step.token + (control ? 0 : 1);
    return entry;
}

struct WrittenEntry {
    WrittenState fields;
    Accidentals  accidentals;
};

struct RhythmScale {
    long double whole = 1;
    long double tail  = 1;
};

struct WrittenNavigation {
    std::map<const Token*, WrittenEntry> entries;
    std::map<const Atom*, RhythmScale>   rhythm_scales;
    std::set<const Token*>               resolved_rhythms;
    std::map<const Token*, Length>       tuplets;

    [[nodiscard]] Length tuplet_scale(const Token& token) const {
        const auto found = tuplets.find(&token);
        return found == tuplets.end() ? Length{} : found->second;
    }
};

// Resolve both written lengths when part navigation can play either half alone.
class PartRhythmPairs {
public:
    PartRhythmPairs(
        const TuneSettings& settings, WrittenNavigation& result, Diagnostics& diagnostics
    ) : settings(settings), result(result), diagnostics(diagnostics) {}

    void consume(const Token& token, const WrittenState& written, Length scale);
    void boundary();

private:
    struct Operand {
        const Atom* atom;
        long double last_attack;
        bool        harp;
    };

    const TuneSettings&    settings;
    WrittenNavigation&     result;
    Diagnostics&           diagnostics;
    Ornament               ornament;
    std::optional<Operand> left;
    const Token*           mark         = nullptr;
    bool                   crosses_part = false;

    long double resolve_pair(const Atom& atom, long double duration, bool harp);
};

void PartRhythmPairs::boundary() {
    left.reset();
    mark         = nullptr;
    crosses_part = false;
    ornament     = {};
}

long double PartRhythmPairs::resolve_pair(const Atom& atom, long double duration, bool harp) {
    if (!left || !mark) return 1;
    if (crosses_part) result.resolved_rhythms.insert(mark);
    if (!accepts_broken_pair(left->last_attack, duration, harp)) {
        if (crosses_part) warn_unequal_pair(diagnostics, mark->at);
        return 1;
    }
    const auto first  = first_broken_factor(std::get<Broken>(mark->value), atom.rhythm);
    const auto second = 2 - first;
    if (crosses_part) {
        auto& lhs = result.rhythm_scales[left->atom];
        // The part marker separates abcMIDI's two event ranges. The opening
        // attack of a right-hand harp cut belongs only to that cut.
        (left->harp ? lhs.tail : lhs.whole) *= first;
        result.rhythm_scales[&atom].whole *= second;
    }
    // Even a pair handled by the renderer can change the right operand's
    // length before it becomes the left operand of the next written pair.
    return second;
}

void PartRhythmPairs::consume(const Token& token, const WrittenState& written, Length scale) {
    if (const auto* atom = std::get_if<Atom>(&token.value)) {
        const auto unit     = atom->measure_rest ? written.meter.measure() : written.unit;
        const auto duration = atom->length.fraction() * scale.fraction() * unit;
        const auto cut      = settings.harp_cuts.for_atom(*atom, ornament, token.at, scale);
        const bool harp     = cut && cut->harp;
        const auto second   = resolve_pair(*atom, duration, harp);
        auto       last     = duration;
        if (harp) {
            const auto mode    = settings.note_lengths.at(token.at).fermata;
            const auto held    = ornament.fermata ? fermata_length(cut->length, mode) : cut->length;
            const auto attacks = detail::harp_attacks(held);
            last               = unit * attacks.last;
        }
        left         = Operand{atom, last * second, harp};
        mark         = nullptr;
        crosses_part = false;
        ornament     = {};
    } else if (std::holds_alternative<Broken>(token.value)) {
        if (left && !mark) mark = &token;
    } else if (const auto* field = std::get_if<Field>(&token.value); field && field->kind == 'P') {
        crosses_part = true;
    } else if (const auto* decoration = std::get_if<Decoration>(&token.value)) {
        if (!decoration->last_on_line)
            if (const auto flag
                = ornament_flag(read_mark(decoration->spelling)))
                ornament.*flag = true;
    } else if (
        std::holds_alternative<Bar>(token.value) || std::holds_alternative<Overlay>(token.value)
    ) {
        boundary();
    } else if (std::holds_alternative<Spacer>(token.value)) {
        ornament = {};
    }
}

void compile_tuplet(
    const Token&       token,
    const Meter&       meter,
    TupletState&       tuplet,
    WrittenNavigation& navigation,
    Diagnostics&       diagnostics
) {
    if (const auto* group = std::get_if<Tuplet>(&token.value)) {
        if (group->count < 2 || group->count > 1024 || group->in_time_of > 1024
            || group->notes > 1024) {
            diagnostics.error(
                token.at,
                "invalid-tuplet",
                "tuplet count must be between 2 and 1024, with at most 1024 notes"
            );
        }
        if (tuplet.left)
            diagnostics.warn(
                token.at, "nested-tuplet", "new tuplet interrupted the previous tuplet"
            );
        tuplet = written_tuplet(*group, meter, token.at);
    } else if (tuplet.left) {
        if (std::holds_alternative<Atom>(token.value)) {
            navigation.tuplets.emplace(&token, tuplet.ratio);
            --tuplet.left;
        } else if (std::holds_alternative<Grace>(token.value)) {
            // The grace group uses the current unit without consuming a tuplet slot.
            navigation.tuplets.emplace(&token, tuplet.ratio);
        }
    }
}

WrittenNavigation written_navigation(
    const VoicePlan&    plan,
    WrittenState        written,
    const TuneSettings& settings,
    Diagnostics&        diagnostics
) {
    WrittenNavigation navigation;
    auto&             entries       = navigation.entries;
    auto              next_written  = plan.source.data();
    bool              ordered_parts = false;
    for (const auto step : plan.order) {
        if (enters_written_passage(step, next_written)) entries.try_emplace(step.token);
        ordered_parts |= step.flow == Flow::enter_part;
    }
    std::optional<PartRhythmPairs> part_rhythms;
    if (ordered_parts && std::ranges::any_of(plan.source, [](const Token& token) {
            return std::holds_alternative<Broken>(token.value);
        }))
        part_rhythms.emplace(settings, navigation, diagnostics);
    const bool has_tuplets     = std::ranges::any_of(plan.source, [](const Token& token) {
        return std::holds_alternative<Tuplet>(token.value);
    });
    const bool needs_full_scan = part_rhythms.has_value() || has_tuplets;
    if (entries.empty() && !needs_full_scan) return navigation;

    // Pitch spelling, measure rests, tuplets and note lengths use the written
    // fields. Part labels can occur inside a bar, so keep the lane's active
    // accidentals even if the notes that established them will be skipped.
    OverlayLanes lanes(Expression{}, diagnostics);
    const auto   remember = [&](const Atom& atom, AccidentalScope scope) {
        for (const auto& pitch : atom.pitches)
            if (pitch.accidental)
                (void)lanes.active().accidentals.resolve(pitch, written.tone, scope);
    };
    auto        entry = entries.begin();
    TupletState tuplet;
    for (const auto& token : plan.source) {
        const auto* field = std::get_if<Field>(&token.value);
        if (field && field->kind == 'P' && lanes.close_implicit(written) && part_rhythms)
            part_rhythms->boundary();
        if (part_rhythms) part_rhythms->consume(token, written, tuplet.scale());
        if (has_tuplets) compile_tuplet(token, written.meter, tuplet, navigation, diagnostics);
        if (entry != entries.end() && entry->first == &token) {
            entry->second = {written, lanes.active().accidentals};
            ++entry;
        }
        if (entry == entries.end() && !needs_full_scan) break;
        if (const auto* overlay = std::get_if<Overlay>(&token.value)) {
            lanes.control(*overlay, written, token.at);
        } else if (const auto* bar = std::get_if<Bar>(&token.value); bar && bar->resets_measure) {
            lanes.close_implicit(written);
            lanes.active().accidentals.reset();
        } else if (const auto* atom = std::get_if<Atom>(&token.value)) {
            if (entry != entries.end()) remember(*atom, settings.accidentals.at(token.at));
        } else if (const auto* grace = std::get_if<Grace>(&token.value)) {
            if (entry != entries.end()) {
                const auto scope = settings.accidentals.at(token.at);
                for (const auto& attack : grace->attacks) remember(attack.atom, scope);
            }
        } else if (const auto* pitch = std::get_if<PitchChange>(&token.value)) {
            written.tone.apply(*pitch);
        } else if (field) {
            switch (field->kind) {
            case 'K':
                if (const auto next
                    = key(field->value, written.tone, diagnostics, token.at, false)) {
                    written.tone = next->state;
                    written.tone.apply(next->pitch);
                    if (next->sets_signature) lanes.active().accidentals.reset();
                }
                break;
            case 'L': written.unit = unit_length(field->value, diagnostics, token.at); break;
            case 'M': written.meter = meter(field->value, diagnostics, token.at); break;
            default: break;
            }
        }
    }
    if (tuplet.left)
        diagnostics.warn(tuplet.at, "unfinished-tuplet", "tuplet has fewer notes than declared");
    return navigation;
}

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

// Interpret one voice's navigation schedule. Pitch policy and symbol slots
// use source positions, while timed commands run on the active lane's clock.
// The output and accompaniment adapters own event storage, not the clock.
class VoicePlayer {
public:
    VoicePlayer(
        const TuneSettings&  settings,
        const VoicePlan&     plan,
        std::string          name,
        const WrittenLyrics& lyrics,
        ChordAttackSequence& chord_attacks,
        Diagnostics&         diagnostics
    ) :
        diagnostics(diagnostics), output(std::move(name), settings.programs, diagnostics),
        tuning(settings.tuning), accidental_policy(settings.accidentals),
        harp_cuts(settings.harp_cuts), note_lengths(settings.note_lengths),
        dynamics(settings.dynamics), stress(settings.stress),
        written{settings.meter, settings.key, settings.unit},
        notation(written_navigation(plan, written, settings, diagnostics)),
        accompaniment(settings.programs, diagnostics),
        lanes(Expression{}, diagnostics, &accompaniment),
        planned_lanes(plan.lanes),
        opening_meter(settings.meter), opening_unit(settings.unit), initial_tempo(settings.tempo),
        separate_conductor(settings.separate_conductor),
        chord_attacks(chord_attacks), lyric_line(lyrics) {
        lane().played_meter = settings.meter;
        accompaniment.open_lane(0, settings.meter, 0, plan.lanes.front().backing_opening);
        if (plan.expands_releases) expanded_releases.emplace(diagnostics);
        for (size_t i = 0; i < plan.lanes.size(); ++i) {
            const auto& lane = plan.lanes[i];
            result.passes.push_back({.declaration = lane.declaration});
            if (lane.backing.drums)
                accompaniment.declare_drums(static_cast<int>(i), lane.backing_opening.drum_gap);
            output.set_channels(static_cast<int>(i), lane.channels);
            midi_track_settings.push_back(lane.note_settings);
        }
    }

    Performance render(const VoicePlan& plan);

private:
    Diagnostics&                               diagnostics;
    VoiceOutput                                output;
    const PitchTuning&                         tuning;
    const AccidentalPolicy&                    accidental_policy;
    const HarpCuts&                            harp_cuts;
    const WrittenNoteLengths&                  note_lengths;
    const WrittenDynamics&                     dynamics;
    const TuneStress&                          stress;
    WrittenState                               written;
    const WrittenNavigation                    notation;
    Accompaniment                              accompaniment;
    OverlayLanes                               lanes;
    const vec<LanePlan>&                       planned_lanes;
    PlaybackLane&                              lane() { return lanes.active(); }
    const Meter&                               opening_meter;
    const long double                          opening_unit;
    const i32                                  initial_tempo;
    vec<MidiTrackSettings>                     midi_track_settings;
    [[nodiscard]] int                          sounding_shift() const;
    std::optional<Prepared>                    pending;
    std::optional<std::pair<Broken, Location>> broken;
    GraceGroup                                 grace;
    Ornament                                   ornament;
    std::deque<TiedOrnament>                   tied_ornaments;
    std::optional<ExpandedReleaseSchedule>     expanded_releases;
    const bool                                 separate_conductor;
    ChordAttackSequence&                       chord_attacks;
    LyricLine                                  lyric_line;
    Performance                                result;
    struct RepeatEntry {
        WrittenEntry           written;
        VoiceOutput::Selection route;
        bool                   before_first_bar;
    };
    std::optional<RepeatEntry> repeat_entry;

    DeferredEffects timed_effects;
    Meter           host_meter() const;
    long double     host_unit() const;
    void            defer(DeferredEffects& effects, const Token& token);
    void            apply_effects(const DeferredEffects& effects);
    void            apply_decoration(
        DecorationMark                       mark,
        bool                                 last_on_line,
        Location                             at,
        std::optional<ChordDecorationTarget> target = {}
    );
    bool                       can_defer(const Token& token) const;
    std::optional<EnvelopeCue> envelope_cue(const Token& token) const;
    void                       visit_token(const Token& token);
    void                       visit_step(PlayStep step, const WrittenEntry* entry);
    void                       restore_written_fields(const WrittenState& entry);
    void                       schedule(const Token& token);
    void                       schedule_envelope(EnvelopeCue cue, const Token& token);
    void                       enter_repeat();
    void                       enter_implicit_repeat(Location at);
    void                       jump_repeat(Location at);
    void                       finish_rhythm();

    i32            at_tick(Location at) { return tick(lane().time, diagnostics, at); }
    AttackVelocity velocity(long double at, bool accented, int stressed) {
        return lane().expression.attack_velocity(
            at - lane().accent_origin, written.meter, accented, stressed
        );
    }
    void                       push(const Atom& atom, const Token& token);
    void                       flush();
    void                       orphan_grace();
    void                       arm_envelope(std::optional<EnvelopeCue> cue);
    bool                       swing_candidate();
    void                       broken_pair(Prepared& next);
    std::optional<HarpAttacks> harp_attacks(const Prepared& atom);
    long double                last_attack_length(const Prepared& atom);
    void        apply_broken_pair(Prepared& left, Prepared& right, Broken mark, Location at);
    void        play(Prepared& atom);
    void        play_segment(const Prepared& atom, const std::string* syllable, int neighbor = 1);
    GraceTiming lead_in(const Prepared& atom);
    long double unwarped_duration(const Prepared& atom);
    long double grace_room(const Prepared& atom);
    GraceTiming play_grace(const Prepared& atom);
    void        play_grace_attack(const AttackSet& notes, long double start);
    void        play_notes(
        const Prepared&    atom,
        AttackTiming       timing,
        MemberLengths      lengths,
        const std::string* syllable
    );
    bool play_expanded_ornament(
        const Prepared&         atom,
        const vec<ChordMember>& members,
        long double             start,
        const AttackTiming&     timing
    );
    std::optional<LyricAnchor> emit_chord(
        const AttackSet&       notes,
        std::span<ChordMember> members,
        long double            start,
        ChordSpread            spread
    );
    void plan_chord_attacks(std::span<ChordMember> members, ChordSpread spread, Location at);
    template<class Emit>
    void ornament_pulses(
        size_t attacks, const OrnamentTiming& duration, const Expression& expression, Emit emit
    );
    void play_ornament_pattern(
        std::span<const Sound> pattern,
        const Prepared&        atom,
        long double            start,
        const AttackTiming&    timing,
        const Sound&           head
    );
    size_t           ornament_count(size_t pattern_size, long double duration, Location at);
    void             finish_ornaments();
    void             play_harp_lead(const Prepared& atom, StressNote first, StressNote second);
    void             play_cut(const Prepared& atom, StressNote timing, int neighbor);
    Sound            ornament_neighbor(Pitch pitch, Location at, int direction = 1);
    Sound            resolve_sound(Pitch pitch, Alteration alteration, Location at, bool tie);
    Sound            sound(Pitch pitch, Location at, bool tie);
    void ornament_attack(
        Sound           sound,
        long double     start,
        long double     step,
        long double     gate,
        const Prepared& atom,
        int             stress_velocity
    );
    std::optional<TieNote> take_tie(const Sound& sound);
    void                   sustain(TieNote& tie, TieSegment segment, long double step);
    vec<ChordMember>       resolve_chord(
        const AttackSet& notes, long double start, bool slur, GateArticulation gate
    );
    void midi(std::string_view command, Location at);
    void information(std::string_view value, Location at);
    void change_key(std::string_view value, Location at);
    void set_notated_meter(Meter next);
    void publish_meter(Meter next, Location at);
    void apply_written_meter(MeterChange change);
    void visit(const Directive& command, Location at);
    void visit(const TempoNotation& speed, Location at);
    void visit(const PitchChange& change, Location);
    void visit(const MidiTrackCommand& command, Location at);
    void visit(const VoiceInstrument& change, Location at);
    void visit(const Field& field, Location at);
    void visit(const Atom& atom, const Token& token) { push(atom, token); }
    void visit(const Bar& bar, const Token& token) { play_bar(bar, token.at, token.music_line); }
    // A spacer consumes note-local marks without changing the rhythmic cursor.
    void visit(Spacer, const Token&) { ornament = {}; }
    void visit(EmptyMusicLine, const Token&) {}
    void visit(const auto& value, const Token& token) { visit(value, token.at); }
    void visit(const Grace& grace, const Token& token);
    void visit(const Harmony& harmony, const Token& token);
    void visit(const Decoration& decoration, Location at);
    void visit(const SymbolDefinition&, Location) {}   // Expanded during tokenization.
    void visit(Slur change, Location at);
    void visit(Tie, Location at);
    void visit(Tuplet, Location) {}   // Ratios were compiled in written order.
    void visit(Broken next, const Token& token);
    void visit(Overlay change, Location at);
    void close_played_measure(Location at);
    void play_bar(const Bar& bar, Location at, size_t music_line, Flow flow = Flow::token);
};

void VoicePlayer::orphan_grace() {
    if (grace.notes)
        diagnostics.warn(grace.at, "orphan-grace", "ignored grace notes without a following note");
    const auto discarded = std::exchange(grace, {});
    apply_effects(discarded.after);
}

void VoicePlayer::arm_envelope(std::optional<EnvelopeCue> cue) {
    if (cue) output.envelope_next(lanes.id(), cue->mark, cue->at);
}

bool swing_note(const Prepared& note) {
    return note.unit_numerator && !note.atom->chord && !note.atom->pitches.empty();
}

bool VoicePlayer::swing_candidate() {
    return pending && swing_note(*pending)
           && starts_hornpipe_pair(opening_meter, lane().written_phase, pending->duration);
}

void VoicePlayer::broken_pair(Prepared& next) {
    if (!broken && next.atom->rhythm.swing && swing_candidate() && swing_note(next)
        && !next.grace.notes && std::fabs(pending->duration - next.duration) < epsilon)
        broken = std::pair{Broken{1, 1}, next.at};
    if (!broken) return;
    const auto [mark, marked_at] = std::exchange(broken, {}).value();
    if (!pending) {
        diagnostics.warn(
            marked_at,
            "interrupted-broken-rhythm",
            "ignored broken rhythm interrupted by a field or bar"
        );
        return;
    }
    apply_broken_pair(*pending, next, mark, marked_at);
}

void VoicePlayer::apply_broken_pair(Prepared& left, Prepared& right, Broken mark, Location at) {
    // The right attack completes the written pair. Its ratio also gives
    // directives between the notes precedence over the left note's ratio.
    const auto first  = first_broken_factor(mark, right.atom->rhythm);
    const auto second = 2 - first;
    if (!accepts_broken_pair(last_attack_length(left), right.duration, right.harp_cut())) {
        warn_unequal_pair(diagnostics, at);
        return;
    }
    // A harp cut in this part scales the shared attack with both factors.
    const auto left_scale = first * (right.harp_cut() ? second : 1);
    (left.harp_cut() ? left.last_scale : left.rhythm_scale) *= left_scale;
    right.rhythm_scale = second;
}

std::optional<HarpAttacks> VoicePlayer::harp_attacks(const Prepared& atom) {
    if (!atom.harp_cut()) return {};
    const auto length = atom.cut->length;
    return detail::harp_attacks(
        atom.ornament.fermata ? fermata_length(length, atom.lengths.fermata) : length
    );
}

long double VoicePlayer::last_attack_length(const Prepared& atom) {
    if (const auto attacks = harp_attacks(atom))
        return attacks->last * atom.unit * atom.rhythm_scale * atom.last_scale;
    return atom.played_duration();
}

void VoicePlayer::push(const Atom& atom, const Token& token) {
    const auto at = token.at;
    // abc2midi tests the written numerator before reducing the duration.
    const auto scale          = notation.tuplet_scale(token);
    const auto part_scale     = notation.rhythm_scales.find(&atom);
    const bool shaped         = part_scale != notation.rhythm_scales.end();
    const bool unit_numerator = !shaped && atom.length.numerator == 1 && scale.numerator == 1;
    const auto cut            = harp_cuts.for_atom(atom, ornament, at, scale);
    const auto unit           = host_unit();
    const auto length_unit    = atom.measure_rest ? host_meter().measure() : unit;
    // ABC 2.1 section 4.13 scales each written length, including unequal notes in one tuplet.
    Prepared next{
        &atom,
        at,
        token.music_line,
        length_unit * atom.length.fraction() * scale.fraction(),
        unit,
        length_unit * scale.fraction(),
        std::exchange(grace, {}),
        std::exchange(ornament, {}),
        note_lengths.at(at),
        cut,
        atom.tie,
        !(cut && cut->harp) && lane().slur > 0 ? SlurPosition::inside : SlurPosition::outside,
        unit_numerator
    };
    broken_pair(next);
    if (shaped) {
        next.rhythm_scale *= part_scale->second.whole;
        next.last_scale *= part_scale->second.tail;
    }
    flush();
    pending = std::move(next);
}

void VoicePlayer::flush() {
    if (!pending) return;
    auto       prepared     = std::exchange(pending, {});
    const auto next_effects = std::exchange(timed_effects, {});
    play(*prepared);
    apply_effects(next_effects);
}

int VoicePlayer::sounding_shift() const {
    return midi_track_settings[lanes.id()].shift + written.tone.transpose.value_or(0);
}

Sound VoicePlayer::sound(Pitch pitch, Location at, bool tie) {
    const auto alteration
        = lane().accidentals.resolve(pitch, written.tone, accidental_policy.at(at));
    return resolve_sound(pitch, alteration, at, tie);
}

Sound VoicePlayer::resolve_sound(Pitch pitch, Alteration alteration, Location at, bool tie) {
    const auto octave = written.tone.effective_octave(pitch.octave);
    return {
        tuning.pitch(pitch.letter, octave, alteration, at),
        static_cast<int>(letter_index(pitch.letter)),
        octave,
        pitch.accidental.has_value(),
        tie || pitch.tie
    };
}

Sound VoicePlayer::ornament_neighbor(Pitch pitch, Location at, int direction) {
    constexpr std::string_view scale   = "CDEFGAB";
    const auto                 offset  = static_cast<int>(scale.find(pitch.letter)) + direction;
    constexpr int              degrees = static_cast<int>(scale.size());
    if (offset < 0) --pitch.octave;
    if (offset == degrees) ++pitch.octave;
    pitch.letter = scale[(offset + degrees) % degrees];
    pitch.accidental.reset();
    pitch.tie = false;
    // An inferred ornament uses the key, not the measure's accidental for its neighbor.
    return resolve_sound(pitch, written.tone.alterations[letter_index(pitch.letter)], at, false);
}

void VoicePlayer::ornament_attack(
    Sound           sound,
    long double     start,
    long double     step,
    long double     gate,
    const Prepared& atom,
    int             stress_velocity
) {
    const auto lane  = lanes.id();
    const auto route = output.chord_route(lane, 0);
    const auto hit   = output.attack(
        lane,
        route,
        route.transpose(sound.pitch, sounding_shift()),
        start,
        gate,
        0,
        {velocity(start, atom.ornament.accent, stress_velocity).value},
        atom.at
    );
    if (expanded_releases) {
        expanded_releases->begin(lane, start, step, atom.at);
        expanded_releases->add(hit.gate, 0, midi_track_settings[lane].attack.gap);
    }
}

std::optional<TieNote> VoicePlayer::take_tie(const Sound& sound) {
    auto match = std::ranges::find_if(lane().ties, [&](const TieNote& pending) {
        // ABC 2.1 sections 4.6 and 4.11: an omitted accidental can inherit
        // the tie's pitch only at the same octave, including clef and octave shifts.
        return sound.pitch.note == pending.untransposed_pitch
               || (!sound.explicit_accidental && sound.letter == pending.letter
                   && sound.octave == pending.octave);
    });
    if (match == lane().ties.end()) return {};
    auto tie = *match;
    lane().ties.erase(match);
    return tie;
}

void VoicePlayer::sustain(TieNote& tie, TieSegment segment, long double step) {
    if (auto* expanded = std::get_if<TiedOrnament*>(&tie.note)) {
        auto& duration = (*expanded)->duration.timing.full.duration;
        duration       = std::max(duration, segment.ornament_end - (*expanded)->start);
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
        // Once the interval no longer fits, more continuations cannot restore that gain.
        if (stress.note(span->at, span->start, span->release_offset, span->meter).velocity == 0) {
            note.use_unstressed_velocity();
            span.reset();
        } else {
            span->next_offset += step;
        }
    }
}

vec<ChordMember> VoicePlayer::resolve_chord(
    const AttackSet& notes, long double start, bool slur, GateArticulation gate
) {
    diagnostics.spend(PlaybackWork::chord(notes.atom.pitches.size(), lane().ties.size()), notes.at);
    const auto&                      expression  = lane().expression;
    std::span<const ChordDecoration> remaining   = notes.atom.decorations;
    auto                             next_length = notes.atom.member_lengths.begin();
    vec<ChordMember>                 members;
    members.reserve(notes.atom.pitches.size());
    size_t     ordinary_attacks = 0;
    auto       active_route     = output.current_route(lanes.id());
    const auto transpose        = sounding_shift();
    // Extend every continuation before an attack can change the shared wheel.
    for (const auto& pitch : notes.atom.pitches) {
        const auto after = std::ranges::find_if(
            remaining, [index = members.size()](const auto& mark) { return mark.member != index; }
        );
        const std::span decorations(remaining.begin(), after);
        remaining        = std::span(after, remaining.end());
        auto member_gate = gate;
        for (const auto& mark : decorations) {
            const auto flag = ornament_flag(read_mark(mark.spelling));
            if (flag == &Ornament::staccato) member_gate = GateArticulation::staccato;
            if (flag == &Ornament::tenuto && member_gate == GateArticulation::normal)
                member_gate = GateArticulation::unshaped;
        }
        auto played       = sound(pitch, notes.at, notes.tied);
        auto continuation = take_tie(played);
        // As in abc2midi, only ordinary attacks select a chord channel. A new
        // tied attack uses the last selection; a continuation emits no attack.
        const bool ordinary       = !continuation && !played.tie;
        const bool introduces_gap = ordinary && ordinary_attacks != 0;
        if (ordinary) active_route = output.chord_route(lanes.id(), ordinary_attacks++);

        const bool member_staccato = member_gate == GateArticulation::staccato;
        const bool short_gate   = member_staccato;
        auto       timing       = short_gate ? notes.timing.staccato.value() : notes.timing.full;
        const auto step         = notes.timing.written_length;
        auto       written_gate = short_gate ? notes.timing.written_staccato : step;
        if (next_length != notes.atom.member_lengths.end()
            && next_length->member == members.size()) {
            const auto& member_length = next_length->length;
            {
                auto length = member_length.fraction() * notes.lengths.unit;
                if (const auto& hold = notes.lengths.hold)
                    length = fermata_length(length, hold->unit, hold->mode);
                if (short_gate) length = staccato_length(length);
                written_gate
                    = std::max(0.L, notes.lengths.grace.shorten(length) - notes.lengths.cut);
                timing = stress.note(
                    notes.at, notes.timing.written_start, written_gate, written.meter
                );
            }
            ++next_length;
        }

        const bool overlaps = played.tie && written_gate > step + epsilon;
        const auto release
            = continuation || overlaps ?
                  tick(start + expression.tied_gate(timing.duration, member_gate), diagnostics, notes.at) :
                  0;
        const auto overlap_release = overlaps ? release : 0;
        if (continuation) {
            sustain(
                *continuation,
                {start + timing.duration, written_gate, release, overlap_release},
                step
            );
        }
        members.push_back(
            {played,
             active_route.transpose(played.pitch, transpose),
             active_route,
             continuation,
             expression.gate(timing.duration, slur, member_gate, timing.gate_ratio),
             written_gate,
             overlap_release,
             timing.velocity,
             decorations,
             introduces_gap}
        );
    }
    return members;
}

void VoicePlayer::play_notes(
    const Prepared& atom, AttackTiming timing, MemberLengths lengths, const std::string* syllable
) {
    const auto spread = atom.atom->chord ?
                            lane().expression.take_chord_spread(
                                midi_track_settings[lanes.id()].attack, atom.ornament.arpeggio
                            ) :
                            ChordSpread{};
    if (atom.atom->pitches.empty()) {
        report_ties(lane().ties, diagnostics);
        return;
    }
    const auto start = lane().time;
    auto       gate  = GateArticulation::normal;
    if (atom.staccato())
        gate = GateArticulation::staccato;
    else if (atom.ornament.tenuto)
        gate = GateArticulation::unshaped;
    const AttackSet notes{*atom.atom, atom.at, atom.tied, atom.ornament.accent, timing, lengths};
    auto            members = resolve_chord(notes, start, atom.slur == SlurPosition::inside, gate);
    std::optional<LyricAnchor> anchor;
    if (!play_expanded_ornament(atom, members, start, timing))
        anchor = emit_chord(notes, members, start, spread);
    if (syllable) {
        diagnostics.spend(PlaybackWork::text(*syllable), atom.at);
        if (!anchor) anchor = output.lyric_anchor(lanes.id(), start, atom.at);
        anchor->append(*syllable);
    }
}

bool VoicePlayer::play_expanded_ornament(
    const Prepared&         atom,
    const vec<ChordMember>& members,
    long double             start,
    const AttackTiming&     timing
) {
    if (atom.harp_cut() || members.size() != 1 || members.front().continuation) return false;
    if (!atom.ornament.roll && !atom.ornament.trill) return false;
    lane().ties.clear();
    const auto pitch = atom.atom->pitches.front();
    const auto main  = members.front().sound;
    if (atom.ornament.roll) {
        const std::array pattern{
            main,
            ornament_neighbor(pitch, atom.at),
            main,
            ornament_neighbor(pitch, atom.at, -1),
            main
        };
        play_ornament_pattern(pattern, atom, start, timing, main);
    } else {
        const std::array pattern{ornament_neighbor(pitch, atom.at), main};
        play_ornament_pattern(pattern, atom, start, timing, main);
    }
    return true;
}

void VoicePlayer::plan_chord_attacks(
    std::span<ChordMember> members, ChordSpread spread, Location at
) {
    i64 stagger = 0;
    int gap     = spread.random ? midi_track_settings[lanes.id()].attack.gap : spread.ticks;
    for (auto& member : members) {
        if (member.continuation) continue;
        if (member.introduces_gap) {
            gap = chord_attacks.next_gap(spread);
            stagger += gap;
        }
        member.gap = gap;
        if (stagger == 0) continue;
        const auto max_delay
            = std::max(0, tick(member.audible, diagnostics, at) - PlaybackGrid::release_trim - 1);
        member.delay = static_cast<int>(std::min<i64>(max_delay, stagger));
    }

    // A short gate may clip its start. Clip preceding attacks with it so the
    // chord keeps both its written order and its individual releases.
    auto latest = std::numeric_limits<int>::max();
    for (auto& member : members | std::views::reverse) {
        if (!member.continuation) member.delay = latest = std::min(latest, member.delay);
    }
}

std::optional<LyricAnchor> VoicePlayer::emit_chord(
    const AttackSet& notes, std::span<ChordMember> members, long double start, ChordSpread spread
) {
    const auto at = notes.at;
    plan_chord_attacks(members, spread, at);
    if (expanded_releases)
        expanded_releases->begin(lanes.id(), start, notes.timing.full.duration, at);
    std::optional<LyricAnchor> first_attack;
    vec<TieNote>               next_ties;
    for (const auto& member : members) {
        const auto& played   = member.sound;
        bool        accented = notes.accented;
        for (const auto& mark : member.decorations) {
            const auto resolved = read_mark(mark.spelling);
            accented = accented || ornament_flag(resolved) == &Ornament::accent;
            apply_decoration(
                resolved, false, mark.at, ChordDecorationTarget{start, !member.continuation}
            );
        }
        if (const auto& continuation = member.continuation) {
            if (played.tie) {
                auto tie   = *continuation;
                tie.letter = played.letter;
                tie.octave = played.octave;
                tie.at     = at;
                next_ties.push_back(tie);
            }
            continue;
        }
        auto volume = velocity(start, accented, member.stress_velocity);
        if (!played.tie || !stress.articulates(at, written.meter)) volume.without_stress.reset();
        const auto hit = output.attack(
            lanes.id(),
            member.route,
            member.sounding,
            start,
            member.audible,
            member.delay,
            volume,
            at
        );
        if (expanded_releases) {
            const auto gap
                = notes.atom.chord ? member.gap : midi_track_settings[lanes.id()].attack.gap;
            expanded_releases->add(hit.gate, member.delay, gap);
        }
        if (!first_attack) first_attack = hit.lyric;
        if (played.tie) {
            std::optional<TieStress> span;
            if (volume.without_stress)
                span = TieStress{
                    at,
                    written.meter,
                    notes.timing.written_start,
                    notes.timing.written_length,
                    member.written_gate
                };
            next_ties.push_back(
                {played.pitch.note,
                 played.letter,
                 played.octave,
                 hit.gate,
                 at,
                 span,
                 member.overlap_release}
            );
        }
    }
    // abc2midi treats a dash between unlike pitches as an ordinary attack.
    lane().ties = std::move(next_ties);
    return first_attack;
}

long double VoicePlayer::unwarped_duration(const Prepared& atom) {
    const auto duration = atom.played_duration();
    return atom.ornament.fermata ? fermata_length(duration, atom.unit, atom.lengths.fermata) :
                                   duration;
}

long double VoicePlayer::grace_room(const Prepared& atom) {
    if (const auto attacks = harp_attacks(atom))
        return attacks->first * atom.unit * atom.rhythm_scale;
    // A directive after the grace can change the host's hold. A unit change
    // alone cannot: the host already has its new unit.
    const bool hold_known = std::ranges::all_of(atom.grace.after.ordered, [](const auto& effect) {
        return std::holds_alternative<UnitChange>(effect);
    });
    const auto duration   = hold_known ? unwarped_duration(atom) : atom.played_duration();
    return atom.staccato() ? staccato_length(duration) : duration;
}

void VoicePlayer::play_grace_attack(const AttackSet& notes, long double start) {
    auto       members = resolve_chord(notes, start, true, GateArticulation::normal);
    const auto spread
        = notes.atom.chord ?
              lane().expression.take_chord_spread(midi_track_settings[lanes.id()].attack) :
              ChordSpread{};
    emit_chord(notes, members, start, spread);
}

GraceTiming VoicePlayer::play_grace(const Prepared& atom) {
    if (!atom.grace.notes) return {};
    const auto& attacks      = atom.grace.notes->attacks;
    const auto& settings     = atom.grace.lengths;
    const bool  proportional = settings.proportional();
    long double weights      = 0;
    for (const auto& attack : attacks) weights += attack.weight();
    if (weights == 0) return {};
    const auto room   = grace_room(atom);
    const auto length = settings.step(room, atom.grace.fixed_unit, weights);
    const auto borrow = length * weights;
    if (borrow + epsilon >= room) {
        diagnostics.warn(
            atom.at,
            "grace-too-long",
            atom.harp_cut() ?
                "grace sequence would consume the first harp attack; ignored the grace notes" :
                "grace sequence would consume its host note; ignored the grace notes"
        );
        return {};
    }
    if (length == 0) return {};
    GraceTiming time;
    time.host.ticks = borrow;
    for (const auto& attack : attacks) {
        const auto&  note     = attack.atom;
        const auto   duration = attack.weight() * length;
        const auto   unit     = length * attack.rhythm_scale;
        const auto   phase    = lane().stress_phase + time.written;
        AttackTiming timing{
            stress.note(atom.grace.at, phase, duration, written.meter),
            {},
            phase,
            duration,
            staccato_length(duration)
        };
        if (!note.decorations.empty())
            timing.staccato
                = stress.note(atom.grace.at, phase, timing.written_staccato, written.meter);
        play_grace_attack(
            {note, atom.grace.at, note.tie, false, timing, {unit, {}, {}}},
            lane().time + time.performed
        );
        time.written += duration;
        time.performed += timing.full.duration;
    }
    return time;
}

template<class Emit>
void VoicePlayer::ornament_pulses(
    size_t attacks, const OrnamentTiming& duration, const Expression& expression, Emit emit
) {
    if (attacks == 0) return;
    const auto& timing         = duration.timing;
    const auto  step           = timing.full.duration / attacks;
    const bool  sample_attacks = stress.articulates(duration.at, duration.meter);
    for (size_t i = 0; i < attacks; ++i) {
        const auto offset = i * step;
        // After a timing warp, new attacks keep their host's velocity.
        const auto sample
            = sample_attacks ?
                  stress.note(duration.at, timing.written_start + offset, step, duration.meter) :
                  StressNote{step, timing.full.velocity, {}};
        // abc2midi releases the slur for the final two generated attacks.
        const auto slur     = duration.slur == SlurPosition::inside
                              || (duration.slur == SlurPosition::closing && attacks - i > 2);
        const auto sounding = expression.gate(step, slur, duration.gate, sample.gate_ratio);
        emit(i, offset, step, sounding, sample.velocity);
    }
}

size_t VoicePlayer::ornament_count(size_t pattern_size, long double duration, Location at) {
    if (pattern_size != 2) return pattern_size;
    return static_cast<size_t>(
        trill_attacks(duration, opening_unit, initial_tempo, at, diagnostics)
    );
}

void VoicePlayer::play_ornament_pattern(
    std::span<const Sound> pattern,
    const Prepared&        atom,
    long double            start,
    const AttackTiming&    timing,
    const Sound&           head
) {
    const int            lane_id = lanes.id();
    const auto           route   = output.chord_route(lane_id, 0);
    const OrnamentTiming duration{
        timing,
        written.meter,
        atom.at,
        atom.slur,
        atom.ornament.tenuto ? GateArticulation::unshaped : GateArticulation::normal
    };
    auto& expression = lane().expression;
    if (!head.tie || timing.full.duration == 0) {
        ornament_pulses(
            ornament_count(pattern.size(), timing.full.duration, atom.at),
            duration,
            expression,
            [&](
                size_t pulse, long double offset, long double step, long double gate, int stressed
            ) {
                ornament_attack(
                    pattern[pulse % pattern.size()], start + offset, step, gate, atom, stressed
                );
            }
        );
        return;
    }

    const auto               shift = sounding_shift();
    std::array<MidiPitch, 5> pitches{};
    std::ranges::transform(pattern, pitches.begin(), [&](const auto& sound) {
        return route.transpose(sound.pitch, shift);
    });
    // Reserve the first attack and its envelope on the opening channel.
    // Consume the head's one-shot dynamics now, then resolve the actual gain
    // when the first pulse's duration is known.
    const auto opening = expression;
    (void)velocity(start, atom.ornament.accent, timing.full.velocity);
    const auto first = output.attack(lane_id, route, pitches.front(), start, 0, 0, {1}, atom.at);
    auto&      tied  = tied_ornaments.emplace_back(
        TiedOrnament{
            pitches,
            pattern.size(),
            start,
            lane().accent_origin,
            atom.ornament.accent,
            duration,
            opening,
            output.retain_route(lane_id),
            first.gate,
            lane_id,
            midi_track_settings[lane_id].attack.gap
        }
    );
    lane().ties.push_back({head.pitch.note, head.letter, head.octave, &tied, atom.at, {}});
}

void VoicePlayer::finish_ornaments() {
    for (auto& tied : tied_ornaments) {
        const auto at = tied.duration.at;
        ornament_pulses(
            ornament_count(tied.pattern_size, tied.duration.timing.full.duration, at),
            tied.duration,
            tied.expression,
            [&](
                size_t pulse, long double offset, long double step, long double gate, int stressed
            ) {
                const auto start   = tied.start + offset;
                const auto volume  = tied.expression
                                         .attack_velocity(
                                             start - tied.accent_origin,
                                             tied.duration.meter,
                                             tied.accented,
                                             stressed
                                         )
                                         .value;
                auto       release = tied.first;
                if (pulse == 0) {
                    output.revise_attack(tied.first, gate, volume, at);
                } else {
                    const auto hit = output.attack(
                        tied.route, tied.pattern[pulse % tied.pattern_size], start, gate, volume, at
                    );
                    release = hit.gate;
                }
                if (expanded_releases) {
                    expanded_releases->begin(tied.lane, start, step, at);
                    expanded_releases->add(release, 0, tied.gap);
                }
            }
        );
    }
}

void VoicePlayer::play_harp_lead(const Prepared& atom, StressNote first, StressNote second) {
    const auto pitch = sound(atom.atom->pitches.front(), atom.at, false);
    ornament_attack(pitch, lane().time, first.duration, first.duration, atom, first.velocity);
    ornament_attack(
        pitch, lane().time + first.duration, second.duration, second.duration, atom, second.velocity
    );
    const auto length = first.duration + second.duration;
    lane().time += length;
    if (length > 0) lane().ties.clear();
}

void VoicePlayer::play_cut(const Prepared& atom, StressNote timing, int neighbor) {
    // The lead occupies a grace slot. It bypasses trim but may overlap the host.
    const auto pitch = ornament_neighbor(atom.atom->pitches.front(), atom.at, neighbor);
    const auto gate
        = lane().expression.gate(timing.duration, true, GateArticulation::normal, std::nullopt);
    ornament_attack(pitch, lane().time, timing.duration, gate, atom, timing.velocity);
    lane().time += timing.duration;
    lane().ties.clear();
}

GraceTiming VoicePlayer::lead_in(const Prepared& atom) {
    const auto grace = play_grace(atom);
    lane().time += grace.performed;
    if (!atom.grace.after.ordered.empty()) {
        accompaniment.advance(lanes.id(), lane().time, atom.at);
        apply_effects(atom.grace.after);
    }
    return grace;
}

void VoicePlayer::play(Prepared& atom) {
    lane().written_phase += atom.duration;
    const auto* syllable = lyric_line.take_syllable(atom.music_line, *atom.atom);

    // abc2midi selects the two-neighbor roll when the host is three units
    // after tuplets and fermata, before broken rhythm. Three equal slices
    // preserve that length even when the input ratio was unreduced.
    if (!atom.cut || atom.harp_cut() || !atom.neighbor_cut()) {
        play_segment(atom, syllable);
        return;
    }
    const auto length = atom.ornament.fermata ?
                            fermata_length(atom.cut->length, atom.lengths.fermata) :
                            atom.cut->length;
    if (length.numerator != 3 * length.denominator) {
        play_segment(atom, syllable);
        return;
    }

    // Explicit grace and the written syllable belong to the first slice.
    // The final slice alone can start a tie to the following written note.
    const bool tied       = std::exchange(atom.tied, false);
    atom.duration         = atom.unit;
    atom.ornament.fermata = false;
    atom.ornament.cut     = false;
    play_segment(atom, syllable);
    atom.grace        = {};
    atom.ornament.cut = true;
    play_segment(atom, nullptr);
    atom.tied = tied;
    play_segment(atom, nullptr, -1);
}

void VoicePlayer::play_segment(const Prepared& atom, const std::string* syllable, int neighbor) {
    // Allocate grace time on the written clock. Warping the slices separately
    // keeps them within the warped atom even when they straddle a pulse.
    const auto grace  = lead_in(atom);
    auto       offset = grace.written;
    const auto take   = [&](long double length) {
        const auto slice
            = stress.note(atom.at, lane().stress_phase + offset, length, written.meter);
        offset += length;
        return slice;
    };
    const auto unwarped = unwarped_duration(atom);
    auto       tail     = unwarped - grace.host.ticks;
    auto       cut      = 0.L;
    if (const auto attacks = harp_attacks(atom)) {
        const auto scale = atom.unit * atom.rhythm_scale;
        const auto step  = attacks->first * scale;
        // The explicit grace takes time from the first harp attack.
        const auto first  = take(std::max(0.L, step - grace.host.ticks));
        const auto second = take(step);
        play_harp_lead(atom, first, second);
        tail = attacks->last * scale * atom.last_scale;
    } else if (atom.neighbor_cut()) {
        const auto length = atom.lengths.grace.step(tail, atom.unit, 1);
        if (length + epsilon < tail) {
            cut = length;
            play_cut(atom, take(cut), neighbor);
            tail -= cut;
        }
    }
    AttackTiming timing;
    timing.written_start  = lane().stress_phase + offset;
    timing.written_length = tail;
    if (atom.staccato() || !atom.atom->decorations.empty()) {
        // A dot moves the release, not the next attack. Map the shortened
        // written gate without advancing the clock past it separately.
        timing.written_staccato
            = std::max(0.L, grace.host.shorten(staccato_length(unwarped)) - cut);
        timing.staccato
            = stress.note(atom.at, timing.written_start, timing.written_staccato, written.meter);
    }
    timing.full = take(tail);
    lane().stress_phase += offset;
    const auto end      = lane().time + timing.full.duration;
    const auto end_tick = tick(end, diagnostics, atom.at);
    // A chord member can switch the drum part at this chord's onset.
    const MemberLengths lengths{
        atom.tuplet_unit * atom.rhythm_scale,
        grace.host,
        atom.ornament.fermata ? std::optional(FermataHold{atom.unit, atom.lengths.fermata}) :
                                std::nullopt,
        cut
    };
    play_notes(atom, timing, lengths, syllable);
    accompaniment.advance(lanes.id(), end, atom.at);
    lane().time              = end;
}

void VoicePlayer::visit(const Grace& next, const Token& token) {
    if (grace.notes) orphan_grace();
    // ABC 2.1 section 4.12 leaves the grace unit to the player. abc2midi
    // scales fixed grace notes by the tuplet active at the opening brace.
    grace
        = {&next,
           token.at,
           host_unit() * notation.tuplet_scale(token).fraction(),
           note_lengths.at(token.at).grace,
           {}};
}

void VoicePlayer::visit(const Harmony& chord, const Token& token) {
    // A print annotation has no voicing and must leave the pending attack open.
    if (!chord.voicing) return;
    if (!chord.voicing->declares_backing && !planned_lanes[lanes.id()].backing.chords) return;
    // The chord belongs to the next note. A rhythm mark written after the chord
    // can still change the preceding note and the tick at which the harmony starts.
    if (pending)
        defer(timed_effects, token);
    else
        accompaniment.harmony(
            *chord.voicing, lanes.id(), written.tone.transpose.value_or(0), token.at
        );
}

void VoicePlayer::visit(Slur change, Location at) {
    if (change.start)
        ++lane().slur;
    else if (lane().slur == 0)
        diagnostics.warn(at, "unmatched-slur", "ignored ')' without a matching slur");
    else {
        --lane().slur;
        if (lane().slur == 0 && pending && pending->slur == SlurPosition::inside
            && !pending->atom->chord)
            pending->slur = SlurPosition::closing;
    }
}

void VoicePlayer::visit(Tie, Location at) {
    if (pending)
        pending->tied = true;
    else
        diagnostics.warn(at, "unexpected-tie", "ignored tie without a preceding note");
}

bool Ornament::* ornament_flag(DecorationMark mark) {
    if (mark.name == "tenuto") return &Ornament::tenuto;
    if (mark.name == "invertedfermata") return &Ornament::fermata;
    if (mark.name == "arpeggio") return &Ornament::arpeggio;
    if (!mark.shorthand && mark.name == ">") return &Ornament::accent;
    if (!mark.shorthand && mark.name.size() == 1) return nullptr;
    struct Feature {
        std::string_view name;
        bool Ornament::* flag;
    };
    constexpr std::array features{
        Feature{".", &Ornament::staccato},
        Feature{"'", &Ornament::staccato},
        Feature{"breath", &Ornament::staccato},
        Feature{"accent", &Ornament::accent},
        Feature{"emphasis", &Ornament::accent},
        Feature{"sfz", &Ornament::accent},
        Feature{"L", &Ornament::accent},
        Feature{"fermata", &Ornament::fermata},
        Feature{"H", &Ornament::fermata},
        Feature{"trill", &Ornament::trill},
        Feature{"T", &Ornament::trill},
        // ABC 2.1 names '~' as !roll!; abc2midi's 'R' is a separate expansion.
        Feature{"roll", &Ornament::cut},
        Feature{"~", &Ornament::cut},
        Feature{"R", &Ornament::roll}
    };
    const auto feature = std::ranges::find(features, mark.name, &Feature::name);
    return feature == features.end() ? nullptr : feature->flag;
}

std::optional<int> pedal_mark(std::string_view mark) {
    if (mark == "ped" || mark == "ped(") return midi_max;
    if (mark == "ped-up" || mark == "ped)" || mark == "ped-end") return 0;
    return {};
}

std::optional<VoiceOutput::EnvelopeMark> envelope_mark(std::string_view mark) {
    using Mark = VoiceOutput::EnvelopeMark;
    if (mark == "bend") return Mark::bend;
    if (mark == "shape") return Mark::shape;
    return {};
}

void VoicePlayer::visit(const Decoration& decoration, Location at) {
    apply_decoration(read_mark(decoration.spelling), decoration.last_on_line, at);
}

void VoicePlayer::apply_decoration(
    DecorationMark                       resolved,
    bool                                 last_on_line,
    Location                             at,
    std::optional<ChordDecorationTarget> target
) {
    const auto mark = resolved.name;
    diagnostics.spend(PlaybackWork::text(mark), at);
    if (mark.starts_with('"') || mark == "nil" || mark == "none") return;
    if (mark == "drum" || mark == "nodrum") {
        if (!target) flush();
        accompaniment.drum_gate(
            lanes.id(), mark == "drum", target ? target->onset : lane().time, at
        );
        return;
    }
    if (const auto flag = ornament_flag(resolved)) {
        // Member gates were set before extending ties. Chord members do not expand ornaments.
        if (target) return;
        if (last_on_line) {
            diagnostics.warn(
                at, "orphan-decoration", "ornament at end of line has no note or rest"
            );
            ornament = {};
        } else
            ornament.*flag = true;
        return;
    }
    if (const auto change = dynamics.mark(mark, at)) {
        if (!target) flush();
        lane().expression.apply(*change);
        return;
    }
    if (const auto pedal = pedal_mark(mark)) {
        if (!target) flush();
        const auto onset = target ? tick(target->onset, diagnostics, at) : at_tick(at);
        output.control(lanes.id(), onset, 64, *pedal);
        return;
    }
    if (const auto envelope = envelope_mark(mark)) {
        if (!target || target->attacks) arm_envelope(EnvelopeCue{*envelope, at});
        return;
    }
    if (mark.size() == 1 && mark.front() >= '0' && mark.front() <= '5') return;
    constexpr std::array accepted_marks{
        "coda"sv, "segno"sv, "tenuto"sv, "wedge"sv, "downbow"sv, "upbow"sv
    };
    if (std::ranges::find(accepted_marks, mark) != accepted_marks.end()) return;
    // ABC 2.1 section 4.14 defines these shorthands. abc2midi leaves them unperformed.
    if (resolved.shorthand && "MOPSuv"sv.find(mark) != std::string_view::npos) return;
    // Section 4.14 defines these spellings. abc2midi gives them no
    // MIDI effect, including when navigation marks appear together.
    constexpr std::array unperformed_marks{
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
    if (std::ranges::find(unperformed_marks, mark) != unperformed_marks.end()) return;
    diagnostics.warn(
        at, "ignored-decoration", fmt::format("ignored unsupported decoration '{}'", mark)
    );
}

void VoicePlayer::visit(Broken next, const Token& token) {
    if (notation.resolved_rhythms.contains(&token)) return;
    if (pending && !broken)
        broken = std::pair{next, token.at};
    else
        diagnostics.warn(
            token.at,
            "unexpected-broken-rhythm",
            "ignored a broken rhythm mark without a preceding note"
        );
}

void VoicePlayer::visit(Overlay change, Location at) {
    flush();
    finish_rhythm();
    orphan_grace();
    ornament = {};
    if (change == Overlay::Split) accompaniment.bar(lanes.id(), lane().time, at);
    lanes.control(change, written, at);
    if (change == Overlay::Split) {
        accompaniment.open_lane(
            lanes.id(), lane().played_meter, lane().bar, planned_lanes[lanes.id()].backing_opening
        );
        accompaniment.advance(lanes.id(), lane().time, at);
    }
    if (static_cast<size_t>(lanes.id()) >= result.passes.size())
        result.passes.push_back({.declaration = at});
}

void VoicePlayer::finish_rhythm() {
    if (broken)
        diagnostics.warn(
            broken->second,
            "unfinished-broken-rhythm",
            "broken rhythm did not reach a following note"
        );
    broken.reset();
}

void VoicePlayer::close_played_measure(Location at) {
    if (lane().time - lane().bar > epsilon) {
        lane().before_first_bar = false;
        lane().part_pickup      = false;
    }
    lane().reset_played_bar(lane().time);
    accompaniment.bar(lanes.id(), lane().time, at);
    if (lanes.id() == 0) lanes.primary_boundary(at);
}

void VoicePlayer::play_bar(const Bar& bar, Location at, size_t music_line, Flow flow) {
    flush();
    finish_rhythm();
    orphan_grace();
    ornament = {};
    if (!bar.resets_measure) return;
    lyric_line.bar(music_line);
    lanes.close_implicit(written);
    lane().accidentals.reset();
    lane().reset_written_bar();
    // An inline ending inherits the accidental reset without a new downbeat.
    const bool soft = bar.repeat_start || bar.repeat_end || bar.section;
    if (flow != Flow::inline_ending && (!soft || lane().closes_soft_bar()))
        close_played_measure(at);
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
    constexpr float limit = 128.0f - 1.0f / (1 << 14);
    float           pitch = 0;
    std::istringstream number{std::string(text)};
    number.imbue(std::locale::classic());
    return number >> pitch && number.eof() && *key >= 0 && *key <= 127 && pitch >= 0
           && pitch < limit;
}

void VoicePlayer::midi(std::string_view command, Location at) {
    const auto spelling = command;
    const auto name     = word(command);
    if (PitchTuning::handles_midi(name) || WrittenNoteLengths::handles(name)) return;
    if (name == "snt") {
        if (!valid_note_tuning(command))
            diagnostics.warn(at, "invalid-midi", "expected 'snt <MIDI key> <absolute pitch>'");
        return;
    }
    if (TuneStress::handles(name)) {
        if (stress.loads(at, spelling)) lane().expression.enable_stress();
        return;
    }
    if (name == "drummap") {
        if (const auto route = DrumMap::read(command, at, diagnostics)) {
            output.map_drums(lanes.id(), *route);
            accompaniment.map_drums(lanes.id(), *route, lane().time, at);
        }
        return;
    }
    if (const auto effect = channel_effect(name, command, at, diagnostics)) {
        const auto time = at_tick(at);
        if (effect->destination == ChannelEffect::Destination::voice)
            output.apply(*effect, lanes.id(), time);
        else
            accompaniment.apply(*effect, lanes.id(), time);
        return;
    }
    if (!lane().expression.directive(name, command, written.unit, written.meter, at, diagnostics)
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
    flush();
    diagnostics.spend(PlaybackWork::text(command.value), at);
    information(command.value, at);
}

void VoicePlayer::visit(const VoiceInstrument& change, Location) {
    flush();
    output.set_instrument(change);
}

void VoicePlayer::visit(const MidiTrackCommand& command, Location at) {
    flush();
    const int id = lanes.id();
    midi_track_settings[id].apply(command);
    if (const auto* change = std::get_if<MidiTransposition>(&command)) {
        accompaniment.transpose(id, *change);
    } else if (const auto& spacing = std::get<ChordAttackChange>(command); !spacing.random) {
        accompaniment.chord_gap(id, spacing.ticks, lane().time, at);
    }
}

void VoicePlayer::change_key(std::string_view value, Location at) {
    const auto change = key(value, written.tone, diagnostics, at, false);
    if (!change) return;
    written.tone = change->state;
    written.tone.apply(change->pitch);
    if (!change->sets_signature) return;
    lane().accidentals.reset();
    result.keys.emplace_back(
        KeySignature<Tick>(
            at_tick(at), static_cast<i8>(written.tone.fifths), static_cast<i8>(written.tone.minor)
        ),
        at
    );
}

void VoicePlayer::set_notated_meter(Meter next) {
    lane().rebase_accents(written.meter, next);
    written.meter = next;
}

void VoicePlayer::publish_meter(Meter next, Location at) {
    set_notated_meter(next);
    result.meters.emplace_back(score_meter(next, at_tick(at)), at);
}

void VoicePlayer::apply_written_meter(MeterChange change) {
    // Only a performed M: resets the beat spacing and guitar pattern in abc2midi.
    lane().expression.reset_beat_spacing();
    lane().set_played_meter(change.meter);
    accompaniment.meter(lanes.id(), change.meter);
    publish_meter(change.meter, change.at);
}

void VoicePlayer::visit(const TempoNotation& speed, Location at) {
    flush();
    if (const auto usec = tempo(speed, written.unit, diagnostics, at)) {
        result.tempos.emplace_back(Tempo<Tick>(at_tick(at), usec), at);
    }
}

void VoicePlayer::visit(const PitchChange& change, Location) {
    flush();
    written.tone.apply(change);
}

void VoicePlayer::visit(const Field& field, Location at) {
    flush();
    const auto value = field.value;
    diagnostics.spend(PlaybackWork::text(value), at);
    switch (field.kind) {
    case 'K': change_key(value, at); break;
    case 'M': apply_written_meter({detail::meter(value, diagnostics, at), at}); break;
    case 'L': written.unit = unit_length(value, diagnostics, at); break;
    case 'P':
        // A part can start with a pickup, but its label is not a barline.
        lane().part_pickup = std::fabs(lane().time - lane().bar) < epsilon;
        break;
    case 'I': information(value, at); break;
    default: break;
    }
}

void VoicePlayer::enter_repeat() {
    repeat_entry = {
        {written, lane().accidentals}, output.current_route(lanes.id()), lane().before_first_bar
    };
}

void VoicePlayer::enter_implicit_repeat(Location at) {
    // An implied opening can start a downbeat inside a written measure.
    if (lane().closes_soft_bar()) close_played_measure(at);
    enter_repeat();
}

void VoicePlayer::jump_repeat(Location at) {
    flush();
    orphan_grace();
    lanes.close_implicit(written);
    lanes.break_ties();
    finish_rhythm();
    lane().accidentals = repeat_entry->written.accidentals;
    lane().slur        = 0;
    // abc2midi restores the written bar count on a jump. If the repeat starts
    // before the first completed bar, its short closing bar can end a pickup.
    lane().before_first_bar = repeat_entry->before_first_bar;
    const auto& entry       = repeat_entry->written.fields;
    written.tone            = entry.tone;
    written.unit            = entry.unit;
    // Restore written grouping as well as the meter ratio, without rewinding the clock.
    if (written.meter != entry.meter) publish_meter(entry.meter, at);
    output.restore_route(lanes.id(), repeat_entry->route);
}

bool VoicePlayer::can_defer(const Token& token) const {
    if (const auto* decoration = std::get_if<Decoration>(&token.value))
        return !ornament_flag(read_mark(decoration->spelling));
    if (std::holds_alternative<Harmony>(token.value)
        || std::holds_alternative<Directive>(token.value)
        || std::holds_alternative<TempoNotation>(token.value)
        || std::holds_alternative<PitchChange>(token.value)
        || std::holds_alternative<MidiTrackCommand>(token.value)
        || std::holds_alternative<VoiceInstrument>(token.value))
        return true;
    const auto* field = std::get_if<Field>(&token.value);
    return field && std::string_view("KMLI").find(field->kind) != std::string_view::npos;
}

Meter VoicePlayer::host_meter() const {
    // Preparing a note may precede playing the pending host and applying
    // the fields written after that host's grace group.
    const auto previous
        = pending ? pending->grace.after.meter.value_or(written.meter) : written.meter;
    return grace.after.meter.value_or(timed_effects.meter.value_or(previous));
}

long double VoicePlayer::host_unit() const {
    const auto previous = pending ? pending->grace.after.unit.value_or(written.unit) : written.unit;
    return grace.after.unit.value_or(timed_effects.unit.value_or(previous));
}

void VoicePlayer::defer(DeferredEffects& effects, const Token& token) {
    const auto* field = std::get_if<Field>(&token.value);
    if (field && field->kind == 'M') {
        diagnostics.spend(PlaybackWork::text(field->value), token.at);
        const auto next = detail::meter(field->value, diagnostics, token.at);
        effects.ordered.emplace_back(MeterChange{next, token.at});
        effects.meter = next;
    } else if (field && field->kind == 'L') {
        diagnostics.spend(PlaybackWork::text(field->value), token.at);
        const auto unit = unit_length(field->value, diagnostics, token.at);
        effects.ordered.emplace_back(UnitChange{unit});
        effects.unit = unit;
    } else {
        effects.ordered.emplace_back(&token);
    }
}

void VoicePlayer::apply_effects(const DeferredEffects& effects) {
    for (const auto& entry : effects.ordered) {
        if (const auto* change = std::get_if<MeterChange>(&entry))
            apply_written_meter(*change);
        else if (const auto* unit = std::get_if<UnitChange>(&entry))
            written.unit = unit->unit;
        else if (const auto* part = std::get_if<PartVerse>(&entry))
            lyric_line.enter_part(part->occurrence);
        else if (const auto* repeat = std::get_if<ImplicitRepeatStart>(&entry))
            enter_implicit_repeat(repeat->at);
        else
            visit_token(*std::get<const Token*>(entry));
    }
}

void VoicePlayer::visit_token(const Token& token) {
    std::visit([&](const auto& value) { visit(value, token); }, token.value);
}

std::optional<EnvelopeCue> VoicePlayer::envelope_cue(const Token& token) const {
    const auto* decoration = std::get_if<Decoration>(&token.value);
    if (!decoration) return {};
    if (const auto kind = envelope_mark(read_mark(decoration->spelling).name))
        return EnvelopeCue{*kind, token.at};
    return {};
}

void VoicePlayer::schedule_envelope(EnvelopeCue cue, const Token& token) {
    if (pending)
        // The cue belongs to the next attack without interrupting a tie or
        // a broken-rhythm pair that still needs to modify the previous atom.
        defer(timed_effects, token);
    else
        arm_envelope(cue);
}

void VoicePlayer::schedule(const Token& token) {
    if (const auto* field = std::get_if<Field>(&token.value);
        pending && field && field->kind == 'P') {
        // The label can precede the rhythm mark that completes the pending note.
        defer(timed_effects, token);
        return;
    }
    // L: sets the host's written length without changing the grace group's unit.
    if (grace.notes && can_defer(token)) {
        defer(grace.after, token);
        return;
    }
    if (const auto cue = envelope_cue(token)) {
        schedule_envelope(*cue, token);
        return;
    }
    // Timed effects belong at the note boundary. The next attack or rhythm
    // mark can still change where that boundary falls.
    if (pending && can_defer(token))
        defer(timed_effects, token);
    else
        visit_token(token);
}

void VoicePlayer::restore_written_fields(const WrittenState& entry) {
    // Pitch spelling and lengths follow the written voice. MIDI metadata and
    // sounding transposition still follow the performed K: commands.
    const auto transpose   = written.tone.transpose;
    written.tone           = entry.tone;
    written.tone.transpose = transpose;
    written.unit           = entry.unit;
    if (written.meter != entry.meter) set_notated_meter(entry.meter);
}

void VoicePlayer::visit_step(PlayStep step, const WrittenEntry* entry) {
    const auto& token = *step.token;
    diagnostics.spend(PlaybackWork::token, token.at);
    const bool joins_overlay = step.flow == Flow::enter_part && lanes.in_implicit_overlay();
    if (entry || joins_overlay) {
        flush();
        finish_rhythm();
        orphan_grace();
    }
    if (joins_overlay) lanes.enter_part(written);
    if (entry) {
        restore_written_fields(entry->fields);
        lane().accidentals = entry->accidentals;
    }
    switch (step.flow) {
    case Flow::enter_part:
        // An adjacent part can complete a pending swing or broken-rhythm pair.
        // The first note still has to use the verse from the preceding part.
        if (pending)
            timed_effects.ordered.emplace_back(PartVerse{step.part_occurrence});
        else
            lyric_line.enter_part(step.part_occurrence);
        break;
    case Flow::enter_repeat:
        flush();
        enter_repeat();
        break;
    case Flow::enter_implicit_repeat:
        // A synthetic opening shares the part label's boundary with the preceding note.
        if (pending)
            timed_effects.ordered.emplace_back(ImplicitRepeatStart{token.at});
        else
            enter_implicit_repeat(token.at);
        break;
    case Flow::jump_repeat:
        jump_repeat(token.at);
        lyric_line.jump(step.continues_lyrics);
        break;
    case Flow::soft_bar:
    case Flow::inline_ending:
    case Flow::hard_bar:
        play_bar(
            Bar{.repeat_end = step.flow == Flow::soft_bar ? 2u : 0u},
            token.at,
            token.music_line,
            step.flow
        );
        break;
    case Flow::token: schedule(token); break;
    }
}

Performance VoicePlayer::render(const VoicePlan& plan) {
    Location end{};
    auto     next_written = plan.source.data();
    for (const auto step : plan.order) {
        end               = step.token->at;
        const auto* entry = enters_written_passage(step, next_written) ?
                                &notation.entries.at(step.token) :
                                nullptr;
        visit_step(step, entry);
    }
    flush();
    finish_rhythm();
    orphan_grace();
    lanes.finish_voice(written, end);
    lanes.break_ties();
    finish_ornaments();
    if (expanded_releases) expanded_releases->apply();
    output.finish(result.passes);
    vec<long double> lane_ends;
    for (const auto& clock : lanes.clocks()) {
        lane_ends.push_back(clock.time);
    }
    auto backing = accompaniment.finish(lane_ends, end);
    for (size_t i = 0; i < backing.size(); ++i) {
        auto&      parts    = backing[i];
        PlaybackPass pass{.declaration = result.passes[i].declaration};
        std::ranges::move(parts.harmony, std::back_inserter(pass.tracks));
        std::ranges::move(parts.drums, std::back_inserter(pass.tracks));
        std::ranges::move(parts.drone, std::back_inserter(pass.tracks));
        if (!pass.tracks.empty()) result.passes.push_back(std::move(pass));
    }
    return std::move(result);
}

}   // namespace

VoicePlan plan_voice(
    const vec<Token>& written_voice, PlayOrder order, Location declaration, Diagnostics& diagnostics
) {
    VoicePlan plan{.source = written_voice, .order = std::move(order)};
    if (!discover_tracks(plan, written_voice, declaration)) return plan;

    PerformedLanes lanes(diagnostics);
    for (const auto step : plan.order) {
        if (lanes.boundary(step)) {
            if (static_cast<size_t>(lanes.id()) == plan.lanes.size())
                plan.lanes.push_back({.declaration = step.token->at});
            continue;
        }
        if (const auto* command = std::get_if<MidiTrackCommand>(&step.token->value)) {
            plan.lanes[lanes.id()].midi_commands.push_back(*command);
            continue;
        }
        auto command = step.token->information().value_or("");
        if (word(command) != "MIDI") continue;
        const auto name = word(command);
        plan.expands_releases |= name == "expand";
        if (const int count = chord_channel_request(name, command).value_or(0)) {
            auto& request = plan.lanes[lanes.id()].chord_channels;
            request       = std::min(midi_channels, request + count);
        }
    }
    return plan;
}

void assign_midi_tracks(std::span<VoicePlan> voices, bool separate_tracks) {
    vec<LanePlan*> tracks;
    for (auto& voice : voices)
        for (auto& lane : voice.lanes) tracks.push_back(&lane);
    std::ranges::stable_sort(tracks, {}, [](const LanePlan* lane) { return lane->declaration; });

    MidiTrackSettings settings;
    const auto        replay_settings = [&settings](const LanePlan& lane) {
        const auto opening = settings;
        for (const auto& command : lane.midi_commands) settings.apply(command);
        return opening;
    };
    // The conductor replays the primary lane. Each later MIDI track starts
    // with the settings left by the preceding track.
    if (separate_tracks && !tracks.empty()) replay_settings(*tracks.front());

    std::bitset<midi_channels + 1> used;
    used.set(midi_drum_channel);
    const auto take_melodic_channel = [&] {
        for (int channel = 1; channel <= midi_channels; ++channel) {
            if (used[channel]) continue;
            used.set(channel);
            return channel;
        }
        return 0;
    };
    for (auto* lane : tracks) {
        lane->note_settings = replay_settings(*lane);
        if (lane->backing.chords) {
            const auto opening                = replay_settings(*lane);
            lane->backing_opening.chord_shift = opening.shift;
            lane->backing_opening.chord_gap   = opening.attack.gap;
        }
        if (lane->backing.drums) lane->backing_opening.drum_gap = replay_settings(*lane).attack.gap;
        if (lane->backing.drone) lane->backing_opening.drone_shift = replay_settings(*lane).shift;
        lane->midi_commands.clear();
        if (separate_tracks && lane->declared_channel) {
            lane->channels.initial = *lane->declared_channel;
            used.set(lane->channels.initial);
        } else if (const int channel = take_melodic_channel()) {
            // Score can still store independent melody tracks when MIDI numbers run out.
            lane->channels.initial = channel;
        }
        for (int i = 0; i < lane->chord_channels; ++i) {
            const int channel = take_melodic_channel();
            if (channel == 0) break;
            lane->channels.extras.push_back(channel);
        }

        // abc2midi writes each lane's backing next. Bass and chords use two
        // channels, the drone one, and drums use reserved channel 10. Each
        // backing track also executes that lane's chord-channel directives.
        const auto backing = lane->backing;
        const int  backing_channels
            = 2 * backing.chords + backing.drone
              + (backing.chords + backing.drone + backing.drums) * lane->chord_channels;
        for (int i = 0; i < backing_channels && take_melodic_channel() != 0; ++i) {}
    }
}

void prime_drum_map(const PlayOrder& first_voice, DrumMap& drum_map, Diagnostics& diagnostics) {
    PerformedLanes lanes(diagnostics);
    for (const auto step : first_voice)
        if (!lanes.boundary(step) && lanes.id() == 0) drum_map.consume(*step.token, diagnostics);
}

Performance perform(
    const TuneSettings&  settings,
    const VoicePlan&     plan,
    const WrittenLyrics& lyrics,
    std::string          name,
    ChordAttackSequence& chord_attacks,
    Diagnostics&         diagnostics
) {
    return VoicePlayer(settings, plan, std::move(name), lyrics, chord_attacks, diagnostics)
        .render(plan);
}

}   // namespace symusic::abc::detail

#include "playback.h"

#include <cmath>
#include <limits>
#include <map>

#include "accompaniment.h"
#include "expression.h"
#include "midi_output.h"


namespace symusic::abc::detail {
namespace {

using namespace std::literals;
constexpr long double epsilon = 0.0000001L;

struct DecorationMark {
    std::string_view name;
    bool             shorthand;
};

struct Sound {
    MidiPitch pitch;
    int       letter;
    int       written_octave;
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
    auto&      slots = letters[index];
    if (pitch.accidental) {
        if (scope == AccidentalScope::pitch)
            slots = {*pitch.accidental, {}};
        else if (scope == AccidentalScope::octave)
            slots.by_octave.insert_or_assign(pitch.octave, *pitch.accidental);
        return *pitch.accidental;
    }
    if (const auto slot = slots.by_octave.find(pitch.octave); slot != slots.by_octave.end())
        return slot->second;
    return slots.all_octaves.value_or(key.alterations[index]);
}

struct TieNote {
    Sound       sound;
    NoteAddress note;
    Location    at;
};

struct ChordMember {
    Sound                      sound;
    std::optional<NoteAddress> continuation;
};

struct GraceGroup {
    const Grace* notes = nullptr;
    Location     at{};
};

struct Prepared {
    const Atom* atom;
    Location    at;
    size_t      music_line;
    long double duration;
    GraceGroup  grace;
    Ornament    ornament;
    bool        tied;
    bool        slur;
    bool        unit_numerator;
};

struct TupletState {
    int                        left      = 0;
    long double                factor    = 1;
    int                        numerator = 1;
    std::optional<long double> first;
    Location                   at{};
};

// Written notation can change while reading an overlay, but the primary
// lane resumes with the fields it had at the first split.
struct WrittenState {
    Meter       meter;
    Key         tone;
    long double unit;
};

struct PlaybackLane {
    long double  time   = 0;
    long double  bar    = 0;
    bool         pickup = true;
    Accidentals  accidentals;
    vec<TieNote> ties;
    Expression   expression{};
    int          slur = 0;
    // Re-entry inherits the primary measure phase at the last synchronization.
    long double                reentry_bar = 0;
    std::optional<long double> primary_until;

    void reenter() {
        pickup      = true;
        accidentals = {};
        bar         = reentry_bar;
    }
};

void report_ties(vec<TieNote>& ties, Diagnostics& diagnostics) {
    for (const auto& tie : ties)
        diagnostics.warn(tie.at, "unmatched-tie", "tie has no matching next note");
    ties.clear();
}

// Owns every lane for the lifetime of a written voice. A split selects another
// clock; it never copies playback state between active and suspended lanes.
// Only the written fields are restored on joining the primary lane.
class OverlayLanes {
public:
    OverlayLanes(RhythmStyle rhythm, Diagnostics& diagnostics) :
        diagnostics(diagnostics), rhythm(rhythm), lanes{new_lane(0, 0)} {}
    PlaybackLane&      active() { return lanes[selected]; }
    [[nodiscard]] int  id() const { return static_cast<int>(selected); }
    void               control(Overlay command, WrittenState& written, Location at);
    bool               close_implicit(WrittenState& written);
    void               enter_part(WrittenState& written);
    void               finish_voice(WrittenState& written, Location at);
    void               primary_boundary();
    void               break_ties();
    [[nodiscard]] bool has_secondary() const { return lanes.size() > 1; }

private:
    struct Group {
        bool                        explicit_group = false;
        std::optional<WrittenState> primary_fields;
    };

    Diagnostics&         diagnostics;
    const RhythmStyle    rhythm;
    vec<PlaybackLane>    lanes;
    size_t               selected              = 0;
    long double          last_primary_boundary = 0;
    std::optional<Group> group;

    PlaybackLane new_lane(long double time, long double bar) const;
    void         split(WrittenState& written, Location at);
    void         join(WrittenState& written);
};

void OverlayLanes::control(Overlay command, WrittenState& written, Location at) {
    switch (command) {
    case Overlay::split: split(written, at); break;
    case Overlay::start:
        if (group)
            diagnostics.error(at, "nested-overlay", "nested overlays have no unambiguous playback");
        group = Group{.explicit_group = true};
        break;
    case Overlay::end:
        if (!group) {
            diagnostics.warn(at, "unexpected-overlay-end", "ignored '&)' without an overlay");
            return;
        }
        join(written);
        active().bar = active().time;
        break;
    }
}

PlaybackLane OverlayLanes::new_lane(long double time, long double bar) const {
    return {.time = time, .expression = Expression(rhythm), .reentry_bar = bar};
}

void OverlayLanes::split(WrittenState& written, Location at) {
    if (!group) group.emplace();
    if (!group->primary_fields) group->primary_fields = written;
    if (++selected >= max_voice_lanes)
        diagnostics.error(at, "overlay-limit", "too many overlay lanes");
    if (selected == lanes.size()) {
        lanes.push_back(new_lane(last_primary_boundary, lanes.front().bar));
    }
    active().reenter();
}

void OverlayLanes::join(WrittenState& written) {
    if (group->primary_fields) {
        written = *group->primary_fields;
        for (size_t lane = 1; lane <= selected; ++lane)
            lanes[lane].primary_until = lanes.front().time;
    }
    selected = 0;
    group.reset();
}

bool OverlayLanes::close_implicit(WrittenState& written) {
    if (!group || group->explicit_group) return false;
    join(written);
    return true;
}

void OverlayLanes::enter_part(WrittenState& written) {
    if (!close_implicit(written)) return;
    // The melody resumes on a downbeat. Secondary lanes continue their
    // existing measure rather than receiving the melody's reset.
    primary_boundary();
    active().bar = active().time;
}

void OverlayLanes::finish_voice(WrittenState& written, Location at) {
    if (!group) return;
    if (group->explicit_group)
        diagnostics.warn(at, "unterminated-overlay", "closed overlay at the end of the voice");
    join(written);
}

void OverlayLanes::primary_boundary() {
    const auto now   = lanes.front().time;
    const auto phase = now - lanes.front().bar;
    for (size_t index = 1; index < lanes.size(); ++index) {
        auto& lane = lanes[index];
        if (!lane.primary_until && now > last_primary_boundary) report_ties(lane.ties, diagnostics);
        lane.time += now - lane.primary_until.value_or(last_primary_boundary);
        lane.reentry_bar = lane.time - phase;
        lane.primary_until.reset();
    }
    last_primary_boundary = now;
}

void OverlayLanes::break_ties() {
    // A suspended primary lane is outside a secondary lane's repeat jump.
    for (size_t index = selected == 0 ? 0 : 1; index < lanes.size(); ++index)
        report_ties(lanes[index].ties, diagnostics);
}

enum class LyricUnit { separator, bar, skip, syllable };

LyricUnit lyric_unit(char c) {
    switch (c) {
    case ' ':
    case '\t':
    case '-': return LyricUnit::separator;
    case '|': return LyricUnit::bar;
    case '_':
    case '*': return LyricUnit::skip;
    default: return LyricUnit::syllable;
    }
}

std::string lyric_syllable(std::string_view& text) {
    std::string result;
    while (!text.empty() && lyric_unit(text.front()) == LyricUnit::syllable) {
        if (text.starts_with("\\-")) text.remove_prefix(1);
        result.push_back(text.front() == '~' ? ' ' : text.front());
        text.remove_prefix(1);
    }
    return result;
}

// The lyric cursor follows performed attacks of one logical music line. Bar
// punctuation advances to the next notated boundary, not the next attack.
class LyricLine {
public:
    LyricLine(const VoiceLyrics& written, Diagnostics& diagnostics) :
        written(written), diagnostics(diagnostics) {}
    void start(size_t line);
    void enter_part(size_t occurrence);
    void jump(bool continues_lyrics);
    void attack(LyricAnchor anchor) {
        if (verse) attacks.push_back(anchor);
    }
    void               bar(size_t line);
    void               finish();
    [[nodiscard]] bool present() const { return !written.empty(); }

private:
    const VoiceLyrics& written;
    Diagnostics&       diagnostics;
    size_t             line        = 0;
    size_t             occurrence  = 0;
    bool               carry_verse = false;
    const LyricVerse*  verse       = nullptr;
    vec<LyricAnchor>   attacks;
    vec<size_t>        boundaries;

    bool align(std::string_view text) const;
};

void LyricLine::start(size_t next) {
    if (!next) return;
    if (std::exchange(carry_verse, false)) line = next;
    if (line == next) return;
    finish();
    line = next;
    if (const auto found = written.find(line); found != written.end()) {
        const auto& verses = found->second;
        verse              = &verses[occurrence % verses.size()];
    }
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
}

void LyricLine::finish() {
    if (verse) {
        diagnostics.spend(PlaybackWork::text(verse->text), verse->at);
        if (!align(verse->text))
            diagnostics.warn(
                verse->at, "extra-lyrics", "more lyric syllables than notes on the music line"
            );
    }
    line        = 0;
    carry_verse = false;
    verse       = nullptr;
    attacks.clear();
    boundaries.clear();
}

void LyricLine::bar(size_t source_line) {
    start(source_line);
    if (attacks.empty()) return;
    if (boundaries.empty() || attacks.size() != boundaries.back())
        boundaries.push_back(attacks.size());
}

bool LyricLine::align(std::string_view text) const {
    size_t next = 0;
    auto   bar  = boundaries.begin();
    while (!text.empty()) {
        switch (lyric_unit(text.front())) {
        case LyricUnit::separator: break;
        case LyricUnit::bar:
            bar  = std::lower_bound(bar, boundaries.end(), next);
            next = bar == boundaries.end() ? attacks.size() : *bar++;
            break;
        case LyricUnit::skip:
            if (next == attacks.size()) return false;
            ++next;
            break;
        case LyricUnit::syllable:
            if (next == attacks.size()) return false;
            attacks[next++].append(lyric_syllable(text));
            continue;
        }
        text.remove_prefix(1);
    }
    return true;
}

class VoicePlayer {
public:
    VoicePlayer(
        const TuneSettings& settings,
        std::string         name,
        const VoiceLyrics&  lyrics,
        DrumMap&            drum_map,
        Diagnostics&        diagnostics
    ) :
        diagnostics(diagnostics), drum_map(drum_map),
        output(std::move(name), drum_map, settings.programs, diagnostics), tuning(settings.tuning),
        accidental_policy(settings.accidentals),
        written{settings.meter, settings.key, settings.unit},
        accompaniment(settings.meter, drum_map, settings.programs, diagnostics),
        symbols(settings.symbols), lanes(settings.rhythm, diagnostics), tempo_unit(settings.unit),
        initial_tempo(settings.tempo), midi_shift(settings.midi_shift),
        lyric_line(lyrics, diagnostics) {}

    Performance render(const vec<Token>& preamble, const PlayOrder& order);

private:
    Diagnostics&                               diagnostics;
    DrumMap&                                   drum_map;
    VoiceOutput                                output;
    const PitchTuning&                         tuning;
    const AccidentalPolicy&                    accidental_policy;
    WrittenState                               written;
    Accompaniment                              accompaniment;
    std::array<std::string_view, 128>          symbols;
    OverlayLanes                               lanes;
    PlaybackLane&                              lane() { return lanes.active(); }
    const long double                          tempo_unit;
    const i32                                  initial_tempo;
    int                                        midi_shift;
    [[nodiscard]] int                          sounding_shift();
    TupletState                                tuplet;
    std::optional<Prepared>                    pending;
    std::optional<std::pair<Broken, Location>> broken;
    GraceGroup                                 grace;
    Ornament                                   ornament;
    LyricLine                                  lyric_line;
    Performance                                result;
    struct RepeatEntry {
        Key         tone;
        long double unit;
        int         channel;
    };
    std::optional<RepeatEntry> repeat_entry;

    vec<const Token*> timed_effects;
    DecorationMark    decoration_mark(const Decoration& decoration) const;
    bool              can_defer(const Token& token) const;
    bool              apply_notated_ratio(const Token& token);
    void              visit_token(const Token& token);
    void              visit_step(PlayStep step);
    void              schedule(const Token& token);
    void              jump_repeat();
    void              finish_rhythm();

    i32 at_tick(Location at) { return tick(lane().time, diagnostics, at); }
    int velocity(long double at) {
        return lane().expression.attack_velocity(at - lane().bar, written.meter);
    }
    void        push(const Atom& atom, Location at, size_t music_line);
    void        flush();
    void        orphan_grace(Location at);
    bool        swing_candidate();
    void        broken_pair(Prepared& next);
    long double atom_duration(const Atom& atom, Location at);
    void        play(const Prepared& atom);
    long double play_grace(const Prepared& atom);
    void play_notes(const Prepared& atom, long double start, long double duration, int spread);
    std::optional<LyricAnchor> emit_chord(
        const Prepared&         atom,
        const vec<ChordMember>& members,
        long double             start,
        long double             audible,
        int                     spread
    );
    void play_trill(
        Pitch pitch, Sound main, const Prepared& atom, long double start, long double duration
    );
    void play_roll(
        Pitch pitch, Sound main, const Prepared& atom, long double start, long double duration
    );
    long double play_cut(
        Pitch pitch, const Prepared& atom, long double start, long double duration
    );
    Sound       ornament_neighbor(Pitch pitch, Location at, int direction = 1);
    Sound       resolve_sound(Pitch pitch, Alteration alteration, Location at, bool tie);
    Sound       sound(Pitch pitch, Location at, bool tie);
    LyricAnchor note(
        Sound sound, long double start, long double duration, int stagger, int loudness, Location at
    );
    std::optional<NoteAddress> sustain(Sound& sound, i32 end);
    vec<ChordMember>           resolve_chord(const Prepared& atom, long double end);
    void                       set_symbols(std::string_view value, Location at);
    void                       midi(std::string_view command, Location at);
    void                       information(std::string_view value, Location at);
    void                       change_key(std::string_view value, Location at);
    void                       change_meter(std::string_view value, Location at);
    void                       change_tempo(std::string_view value, Location at);
    void                       change_voice(std::string_view value, Location at);
    void                       articulation(const Prepared& atom, long double duration);
    void                       visit(const Directive& command, Location at);
    void                       visit(const Field& field, Location at);
    void visit(const Atom& atom, const Token& token) { push(atom, token.at, token.music_line); }
    void visit(const Bar& bar, const Token& token) { play_bar(bar, token.at, token.music_line); }
    void visit(EmptyMusicLine, const Token&) {}
    void visit(const auto& value, const Token& token) { visit(value, token.at); }
    void visit(const Grace& grace, Location at);
    void visit(const Harmony& harmony, Location at);
    void visit(const Decoration& decoration, Location at);
    void visit(Slur change, Location at);
    void visit(Tie, Location at);
    void visit(Tuplet group, Location at);
    void visit(Broken next, Location at);
    void visit(Overlay change, Location at);
    void play_bar(const Bar& bar, Location at, size_t music_line);
};

void VoicePlayer::orphan_grace(Location) {
    if (grace.notes)
        diagnostics.warn(grace.at, "orphan-grace", "ignored grace notes without a following note");
    grace = {};
}

bool swing_note(const Prepared& note) {
    return note.unit_numerator && !note.atom->chord && !note.atom->pitches.empty();
}

bool VoicePlayer::swing_candidate() {
    return pending && swing_note(*pending)
           && lane().expression.starts_swing_pair(
               written.meter, lane().time - lane().bar, pending->duration
           );
}

void VoicePlayer::broken_pair(Prepared& next) {
    if (swing_candidate() && swing_note(next) && !next.grace.notes
        && std::fabs(pending->duration - next.duration) < epsilon)
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
    if (std::fabs(pending->duration - next.duration) > epsilon) {
        diagnostics.warn(
            marked_at,
            "unequal-broken-rhythm",
            "ignored broken rhythm between notes of unequal length"
        );
        return;
    }
    const auto first = lane().expression.broken_factor(mark);
    pending->duration *= first;
    next.duration *= 2 - first;
}

long double VoicePlayer::atom_duration(const Atom& atom, Location at) {
    const auto raw
        = (atom.measure_rest ? written.meter.measure() : written.unit) * atom.length.fraction();
    if (!tuplet.left) return raw;
    --tuplet.left;
    if (!tuplet.first)
        tuplet.first = raw;
    else if (std::fabs(*tuplet.first - raw) > epsilon) {
        diagnostics.warn(at, "unequal-tuplet", "tuplet contains unequal note lengths");
    }
    return raw * tuplet.factor;
}

void VoicePlayer::push(const Atom& atom, Location at, size_t music_line) {
    // abc2midi tests the written numerator before reducing the duration.
    const bool unit_numerator
        = atom.length.numerator == 1 && (!tuplet.left || tuplet.numerator == 1);
    Prepared next{
        &atom,
        at,
        music_line,
        atom_duration(atom, at),
        std::exchange(grace, {}),
        std::exchange(ornament, {}),
        atom.tie,
        lane().slur > 0,
        unit_numerator
    };
    broken_pair(next);
    flush();
    pending = next;
}

void VoicePlayer::flush() {
    if (!pending) return;
    play(*pending);
    pending.reset();
    const auto next_effects = std::exchange(timed_effects, {});
    for (const auto* token : next_effects) visit_token(*token);
}

int VoicePlayer::sounding_shift() {
    if (output.selected_channel(lanes.id()) == midi_drum_channel) return 0;
    return midi_shift + written.tone.transpose.value_or(0);
}

Sound VoicePlayer::sound(Pitch pitch, Location at, bool tie) {
    const auto alteration
        = lane().accidentals.resolve(pitch, written.tone, accidental_policy.at(at));
    return resolve_sound(pitch, alteration, at, tie);
}

Sound VoicePlayer::resolve_sound(Pitch pitch, Alteration alteration, Location at, bool tie) {
    auto midi = tuning.pitch(pitch.letter, pitch.octave + written.tone.octave, alteration, at);
    midi.note += sounding_shift();
    return {
        midi,
        static_cast<int>(letter_index(pitch.letter)),
        pitch.octave,
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

LyricAnchor VoicePlayer::note(
    Sound sound, long double start, long double duration, int stagger, int loudness, Location at
) {
    return output.attack(lanes.id(), sound.pitch, start, duration, stagger, loudness, at).lyric;
}

std::optional<NoteAddress> VoicePlayer::sustain(Sound& sound, i32 end) {
    auto match = std::ranges::find_if(lane().ties, [&](const TieNote& pending) {
        // The wheel bends the channel, not the note number matched by a tie.
        return sound.pitch.note == pending.sound.pitch.note
               || (!sound.explicit_accidental && sound.letter == pending.sound.letter
                   && sound.written_octave == pending.sound.written_octave);
    });
    if (match == lane().ties.end()) return {};
    auto address = match->note;
    sound.pitch  = match->sound.pitch;
    lane().ties.erase(match);
    address.release_at(end);
    return address;
}

vec<ChordMember> VoicePlayer::resolve_chord(const Prepared& atom, long double end) {
    diagnostics.spend(PlaybackWork::chord(atom.atom->pitches.size(), lane().ties.size()), atom.at);
    const auto       release = lane().ties.empty() ? 0 : tick(end, diagnostics, atom.at);
    vec<ChordMember> members;
    members.reserve(atom.atom->pitches.size());
    // Extend every continuation before an attack can change the shared wheel.
    for (const auto& pitch : atom.atom->pitches) {
        auto played       = sound(pitch, atom.at, atom.tied);
        auto continuation = sustain(played, release);
        members.push_back({played, continuation});
    }
    return members;
}

void VoicePlayer::play_notes(
    const Prepared& atom, long double start, long double duration, int spread
) {
    const auto& expression = lane().expression;
    const auto  audible    = expression.gate(duration, atom.slur, atom.ornament.staccato);
    const auto  members
        = resolve_chord(atom, start + expression.tied_gate(duration, atom.ornament.staccato));
    if (members.size() == 1 && !members.front().continuation) {
        const auto pitch = atom.atom->pitches.front();
        const auto main  = members.front().sound;
        if (atom.ornament.roll || atom.ornament.trill) {
            lane().ties.clear();
            if (atom.ornament.roll)
                play_roll(pitch, main, atom, start, duration);
            else
                play_trill(pitch, main, atom, start, duration);
            return;
        }
    }
    if (const auto anchor = emit_chord(atom, members, start, audible, spread))
        lyric_line.attack(*anchor);
}

std::optional<LyricAnchor> VoicePlayer::emit_chord(
    const Prepared&         atom,
    const vec<ChordMember>& members,
    long double             start,
    long double             audible,
    int                     spread
) {
    // Outgoing ties disable chord spread; incoming sustains do not count as attacks.
    const auto has_ties = atom.tied || std::ranges::any_of(atom.atom->pitches, &Pitch::tie);
    const auto attack   = has_ties ? 0 : spread;
    const auto max_stagger
        = std::max(0, tick(audible, diagnostics, atom.at) - PlaybackGrid::release_trim);
    int                        stagger      = 0;
    size_t                     chord_member = 0;
    std::optional<LyricAnchor> first_attack;
    vec<TieNote>               next_ties;
    for (const auto& [played, continuation] : members) {
        auto address = continuation;
        if (!address) {
            // Converted tie attacks keep the selected channel in abc2midi.
            const auto member = played.tie ? std::nullopt : std::optional{chord_member++};
            const auto hit    = output.attack(
                lanes.id(), played.pitch, start, audible, stagger, velocity(start), atom.at, member
            );
            address = hit.note;
            if (!first_attack) first_attack = hit.lyric;
            stagger = static_cast<int>(
                std::min<int64_t>(max_stagger, stagger + static_cast<int64_t>(attack))
            );
        }
        if (address && played.tie) next_ties.push_back({played, *address, atom.at});
    }
    // abc2midi treats a dash between unlike pitches as an ordinary attack.
    lane().ties = std::move(next_ties);
    return first_attack;
}

long double VoicePlayer::play_grace(const Prepared& atom) {
    if (!atom.grace.notes || atom.grace.notes->pitches.empty()) return 0;
    const auto& pitches = atom.grace.notes->pitches;
    long double weights = 0;
    for (const auto& pitch : pitches) weights += pitch.length.fraction();
    if (weights == 0) return 0;
    const auto length = lane().expression.grace_ratio > 0 ?
                            atom.duration * lane().expression.grace_ratio / weights :
                            written.unit / lane().expression.grace_divider;
    const auto total  = length * weights;
    if (total + epsilon >= atom.duration) {
        diagnostics.warn(
            atom.at,
            "grace-too-long",
            "grace sequence would consume its host note; ignored the grace notes"
        );
        return 0;
    }
    auto start = lane().time;
    for (const auto& pitch : pitches) {
        const auto duration = pitch.length.fraction() * length;
        note(sound(pitch, atom.grace.at, false), start, duration, 0, velocity(start), atom.at);
        start += duration;
    }
    return total;
}

void VoicePlayer::play_roll(
    Pitch pitch, Sound main, const Prepared& atom, long double start, long double duration
) {
    const std::array notes{
        main, ornament_neighbor(pitch, atom.at), main, ornament_neighbor(pitch, atom.at, -1), main
    };
    const auto step = duration / notes.size();
    for (size_t index = 0; index < notes.size(); ++index) {
        const auto onset  = start + index * step;
        const auto anchor = note(notes[index], onset, step, 0, velocity(onset), atom.at);
        if (index == 0) lyric_line.attack(anchor);
    }
}

long double VoicePlayer::play_cut(
    Pitch pitch, const Prepared& atom, long double start, long double duration
) {
    const auto length = written.unit / lane().expression.grace_divider;
    if (length + epsilon >= duration) return 0;
    note(ornament_neighbor(pitch, atom.at), start, length, 0, velocity(start), atom.at);
    return length;
}

void VoicePlayer::play_trill(
    Pitch pitch, Sound main, const Prepared& atom, long double start, long double duration
) {
    const auto count = trill_attacks(duration, tempo_unit, initial_tempo, atom.at, diagnostics);
    if (count == 0) return;
    const auto step   = duration / count;
    const auto higher = ornament_neighbor(pitch, atom.at);
    for (int i = 0; i < count; ++i) {
        const auto onset  = start + i * step;
        const auto anchor = note(i % 2 ? main : higher, onset, step, 0, velocity(onset), atom.at);
        if (i == 0) lyric_line.attack(anchor);
    }
}

void VoicePlayer::articulation(const Prepared& atom, long double duration) {
    const auto& pitches = atom.atom->pitches;
    const auto  spread  = atom.atom->chord ? lane().expression.take_chord_spread() : 0;
    if (pitches.empty()) {
        report_ties(lane().ties, diagnostics);
        return;
    }
    auto lead_in = play_grace(atom);
    if (pitches.size() == 1 && atom.ornament.cut && !atom.ornament.roll) {
        lead_in += play_cut(pitches.front(), atom, lane().time + lead_in, duration - lead_in);
    }
    // Grace attacks, including an Irish cut, break the incoming tie. A
    // trill or roll is instead an attribute of an attack that may not occur.
    if (lead_in > 0) lane().ties.clear();
    play_notes(atom, lane().time + lead_in, duration - lead_in, spread);
}

void VoicePlayer::play(const Prepared& atom) {
    const auto duration = atom.ornament.fermata ?
                              lane().expression.fermata(atom.duration, written.unit) :
                              atom.duration;
    const auto end      = lane().time + duration;
    (void)tick(end, diagnostics, atom.at);
    if (lanes.id() == 0) accompaniment.advance(end, atom.at);
    lyric_line.start(atom.music_line);
    articulation(atom, duration);
    lane().time = end;
}

void VoicePlayer::visit(const Grace& next, Location at) {
    if (grace.notes) orphan_grace(at);
    grace = {&next, at};
}

void VoicePlayer::visit(const Harmony& chord, Location at) {
    flush();
    diagnostics.spend(PlaybackWork::text(chord.symbol), at);
    if (lanes.id() == 0)
        accompaniment.harmony(chord.symbol, midi_shift + written.tone.transpose.value_or(0), at);
}

void VoicePlayer::visit(Slur change, Location at) {
    if (change.start)
        ++lane().slur;
    else if (lane().slur == 0)
        diagnostics.warn(at, "unmatched-slur", "ignored ')' without a matching slur");
    else {
        --lane().slur;
        // The closing note of a melodic slur still receives normal articulation.
        if (lane().slur == 0 && pending && !pending->atom->chord) pending->slur = false;
    }
}

void VoicePlayer::visit(Tie, Location at) {
    if (pending)
        pending->tied = true;
    else
        diagnostics.warn(at, "unexpected-tie", "ignored tie without a preceding note");
}

DecorationMark VoicePlayer::decoration_mark(const Decoration& decoration) const {
    auto spelling = decoration.spelling;
    if (spelling.size() == 1 && static_cast<unsigned char>(spelling.front()) < symbols.size()) {
        if (const auto alias = symbols[static_cast<unsigned char>(spelling.front())];
            !alias.empty())
            spelling = alias;
    }
    if (spelling.size() >= 2 && (spelling.front() == '!' || spelling.front() == '+')
        && spelling.back() == spelling.front())
        return {spelling.substr(1, spelling.size() - 2), false};
    return {spelling, spelling.size() == 1};
}

bool Ornament::* ornament_flag(DecorationMark mark) {
    if (!mark.shorthand && mark.name.size() == 1) return nullptr;
    struct Feature {
        std::string_view name;
        bool Ornament::* flag;
    };
    constexpr std::array features{
        Feature{".", &Ornament::staccato},
        Feature{"'", &Ornament::staccato},
        Feature{"breath", &Ornament::staccato},
        Feature{"fermata", &Ornament::fermata},
        Feature{"H", &Ornament::fermata},
        Feature{"trill", &Ornament::trill},
        Feature{"T", &Ornament::trill},
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

void VoicePlayer::visit(const Decoration& decoration, Location at) {
    const auto resolved = decoration_mark(decoration);
    const auto mark     = resolved.name;
    diagnostics.spend(PlaybackWork::text(mark), at);
    if (const auto flag = ornament_flag(resolved)) {
        if (decoration.last_on_line) {
            diagnostics.warn(
                at, "orphan-decoration", "ornament at end of line has no note or rest"
            );
            ornament = {};
        } else
            ornament.*flag = true;
        return;
    }
    if (const auto change = dynamic_mark(mark)) {
        flush();
        lane().expression.apply(*change);
        return;
    }
    if (const auto pedal = pedal_mark(mark)) {
        flush();
        output.control(lanes.id(), at_tick(at), 64, *pedal, at);
        return;
    }
    if (mark == "arpeggio") {
        flush();
        lane().expression.arpeggiate_next_chord();
        return;
    }
    constexpr std::array marks{
        "coda"sv,
        "segno"sv,
        "accent"sv,
        ">"sv,
        "tenuto"sv,
        "L"sv,
        "wedge"sv,
        "downbow"sv,
        "upbow"sv,
        "P"sv,
        "S"sv,
        "u"sv,
        "v"sv
    };
    if (std::ranges::find(marks, mark) == marks.end()) {
        diagnostics.warn(
            at, "ignored-decoration", fmt::format("ignored unsupported decoration '{}'", mark)
        );
    }
}

int default_tuplet(int count, bool compound) {
    if (count == 2 || count == 4 || count == 8) return 3;
    if (count == 3 || count == 6) return 2;
    return compound ? 3 : 2;
}

void VoicePlayer::visit(Tuplet group, Location at) {
    if (group.count < 2 || group.count > 1024 || group.in_time_of > 1024 || group.notes > 1024) {
        diagnostics.error(
            at, "invalid-tuplet", "tuplet count must be between 2 and 1024, with at most 1024 notes"
        );
    }
    if (tuplet.left)
        diagnostics.warn(at, "nested-tuplet", "new tuplet interrupted the previous tuplet");
    const auto in_time_of = group.in_time_of ?
                                group.in_time_of :
                                default_tuplet(group.count, written.meter.compound());
    tuplet
        = {group.notes ? group.notes : group.count,
           static_cast<long double>(in_time_of) / group.count,
           in_time_of,
           {},
           at};
}

void VoicePlayer::visit(Broken next, Location at) {
    if (pending && !broken)
        broken = std::pair{next, at};
    else
        diagnostics.warn(
            at, "unexpected-broken-rhythm", "ignored a broken rhythm mark without a preceding note"
        );
}

void VoicePlayer::visit(Overlay change, Location at) {
    flush();
    finish_rhythm();
    orphan_grace(at);
    ornament = {};
    lanes.control(change, written, at);
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

bool closes_measure(const Bar& bar, const PlaybackLane& lane, Meter meter) {
    const bool soft = bar.repeat_start || bar.repeat_end || bar.section;
    return !soft
           || (!meter.free && (lane.pickup || lane.time - lane.bar + epsilon >= meter.measure()));
}

void VoicePlayer::play_bar(const Bar& bar, Location at, size_t music_line) {
    flush();
    finish_rhythm();
    orphan_grace(at);
    ornament = {};
    if (!bar.resets_measure) return;
    lyric_line.bar(music_line);
    lanes.close_implicit(written);
    lane().accidentals.reset();
    if (!closes_measure(bar, lane(), written.meter)) return;
    if (lane().time - lane().bar > epsilon) lane().pickup = false;
    lane().bar = lane().time;
    if (lanes.id() == 0) {
        lanes.primary_boundary();
        accompaniment.bar(lane().time);
    }
}

void VoicePlayer::set_symbols(std::string_view value, Location at) {
    const auto eq     = value.find('=');
    const auto symbol = trim(value.substr(0, eq));
    if (eq == value.npos || symbol.size() != 1
        || static_cast<unsigned char>(symbol.front()) >= symbols.size()) {
        diagnostics.warn(
            at, "invalid-symbol", "U: needs a single character followed by a decoration"
        );
        return;
    }
    symbols[static_cast<unsigned char>(symbol.front())] = trim(value.substr(eq + 1));
}

void VoicePlayer::midi(std::string_view command, Location at) {
    const auto name = word(command);
    if (PitchTuning::handles_midi(name)) return;   // Already compiled in source order.
    if (name == "drummap") {
        drum_map.assign(command, at, diagnostics);
        return;
    }
    if (midi_transposition(name, command, midi_shift, diagnostics, at)) return;
    if (!lane().expression.directive(name, command, written.unit, at, diagnostics)
        && !output.effect(name, command, lanes.id(), at_tick(at), at)
        && !accompaniment.directive(name, command, lane().time, at)) {
        diagnostics.warn(at, "unsupported-midi", fmt::format("ignored MIDI directive '{}'", name));
    }
}

void VoicePlayer::information(std::string_view value, Location at) {
    const auto command = word(value);
    if (command == "MIDI")
        midi(value, at);
    else if (command == "abc-include" || command == "include") {
        diagnostics.warn(
            at,
            "external-include",
            "ignored external include; ABC conversion never opens referenced files"
        );
    }
}

void VoicePlayer::visit(const Directive& command, Location at) {
    flush();
    diagnostics.spend(PlaybackWork::text(command.value), at);
    information(command.value, at);
}

void VoicePlayer::change_key(std::string_view value, Location at) {
    const auto change = key(value, written.tone, diagnostics, at, false);
    if (!change) return;
    written.tone = *change;
    lane().accidentals.reset();
    result.keys.emplace_back(
        KeySignature<Tick>(
            at_tick(at), static_cast<i8>(written.tone.fifths), static_cast<i8>(written.tone.minor)
        ),
        at
    );
}

void VoicePlayer::change_meter(std::string_view value, Location at) {
    written.meter = detail::meter(value, diagnostics, at);
    if (lanes.id() == 0) accompaniment.meter(written.meter, lane().time);
    result.meters.emplace_back(
        TimeSignature<Tick>(
            at_tick(at),
            static_cast<u8>(written.meter.numerator),
            static_cast<u8>(written.meter.denominator)
        ),
        at
    );
}

void VoicePlayer::change_tempo(std::string_view value, Location at) {
    if (const auto usec = tempo(value, tempo_unit, diagnostics, at)) {
        result.tempos.emplace_back(Tempo<Tick>(at_tick(at), usec), at);
    }
}

void VoicePlayer::change_voice(std::string_view value, Location at) {
    const auto voice = voice_spec(value, diagnostics, at);
    if (voice.transpose) written.tone.transpose = *voice.transpose;
    if (voice.octave) written.tone.octave = *voice.octave;
}

void VoicePlayer::visit(const Field& field, Location at) {
    flush();
    const auto value = field.value;
    diagnostics.spend(PlaybackWork::text(value), at);
    switch (field.kind) {
    case 'K': change_key(value, at); break;
    case 'Q': change_tempo(value, at); break;
    case 'M': change_meter(value, at); break;
    case 'L': written.unit = unit_length(value, diagnostics, at); break;
    case 'P':
        // A part can start with a pickup, but its label is not a barline.
        lane().pickup = std::fabs(lane().time - lane().bar) < epsilon;
        break;
    case 'V': change_voice(value, at); break;
    case 'I': information(value, at); break;
    case 'U': set_symbols(value, at); break;
    default: break;
    }
}

void VoicePlayer::jump_repeat() {
    flush();
    lanes.close_implicit(written);
    lanes.break_ties();
    finish_rhythm();
    lane().accidentals.reset();
    lane().slur  = 0;
    written.tone = repeat_entry->tone;
    written.unit = repeat_entry->unit;
    output.select_channel(lanes.id(), repeat_entry->channel);
}

bool VoicePlayer::can_defer(const Token& token) const {
    if (const auto* decoration = std::get_if<Decoration>(&token.value))
        return !ornament_flag(decoration_mark(*decoration));
    if (std::holds_alternative<Harmony>(token.value)
        || std::holds_alternative<Directive>(token.value))
        return true;
    const auto* field = std::get_if<Field>(&token.value);
    return field && std::string_view("KQMIP").find(field->kind) != std::string_view::npos;
}

bool VoicePlayer::apply_notated_ratio(const Token& token) {
    const auto information = token.information();
    if (!information) return false;
    auto args = *information;
    if (word(args) != "MIDI") return false;
    const auto name = word(args);
    // A ratio changes the notated pair, not the attack at this position.
    return lane().expression.set_broken_ratio(name, args, token.at, diagnostics);
}

void VoicePlayer::visit_token(const Token& token) {
    std::visit([&](const auto& value) { visit(value, token); }, token.value);
}

void VoicePlayer::schedule(const Token& token) {
    if (apply_notated_ratio(token)) return;
    if ((broken || swing_candidate()) && can_defer(token))
        timed_effects.push_back(&token);
    else
        visit_token(token);
}

void VoicePlayer::visit_step(PlayStep step) {
    const auto& token = *step.token;
    diagnostics.spend(PlaybackWork::token, token.at);
    switch (step.flow) {
    case Flow::enter_part:
        flush();
        finish_rhythm();
        lanes.enter_part(written);
        lyric_line.enter_part(step.part_occurrence);
        break;
    case Flow::enter_repeat:
        flush();
        repeat_entry = {written.tone, written.unit, output.selected_channel(lanes.id())};
        break;
    case Flow::jump_repeat:
        jump_repeat();
        lyric_line.jump(step.continues_lyrics);
        break;
    case Flow::soft_bar:
    case Flow::hard_bar:
        play_bar(Bar{.repeat_end = step.flow == Flow::soft_bar}, token.at, token.music_line);
        break;
    case Flow::token: schedule(token); break;
    }
}

Performance VoicePlayer::render(const vec<Token>& preamble, const PlayOrder& order) {
    Location end{};
    for (const auto& token : preamble) {
        end = token.at;
        visit_step({&token});
    }
    for (const auto step : order) {
        end = step.token->at;
        visit_step(step);
    }
    flush();
    finish_rhythm();
    lanes.finish_voice(written, end);
    lanes.break_ties();
    orphan_grace(end);
    if (tuplet.left)
        diagnostics.warn(tuplet.at, "unfinished-tuplet", "tuplet has fewer notes than declared");
    lyric_line.finish();
    output.finish(result.tracks);
    const auto melody_tracks = result.tracks.size();
    accompaniment.finish(result.tracks, lane().time, end);
    result.needs_conductor
        = lyric_line.present() || lanes.has_secondary() || result.tracks.size() != melody_tracks;
    return std::move(result);
}

}   // namespace

Performance perform(
    const TuneSettings& settings,
    const vec<Token>&   preamble,
    const PlayOrder&    order,
    const VoiceLyrics&  lyrics,
    std::string         name,
    DrumMap&            drum_map,
    Diagnostics&        diagnostics
) {
    return VoicePlayer(settings, std::move(name), lyrics, drum_map, diagnostics)
        .render(preamble, order);
}

}   // namespace symusic::abc::detail

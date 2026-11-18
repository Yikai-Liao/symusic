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

struct Sound {
    int  note;
    int  letter;
    int  written_octave;
    bool explicit_accidental;
    bool tie;
};

enum class AccidentalScope { pitch, octave, none };

class Accidentals {
public:
    int  resolve(Pitch pitch, const Key& key);
    void reset() { measure.clear(); }
    void propagation(std::string_view value, Location at, Diagnostics& diagnostics);

private:
    AccidentalScope                     scope = AccidentalScope::pitch;
    std::map<std::pair<char, int>, int> measure;
};

int Accidentals::resolve(Pitch pitch, const Key& key) {
    if (scope == AccidentalScope::none)
        return pitch.accidental.value_or(key.alterations[letter_index(pitch.letter)]);
    const auto slot = std::pair{pitch.letter, scope == AccidentalScope::pitch ? 0 : pitch.octave};
    if (pitch.accidental) measure.insert_or_assign(slot, *pitch.accidental);
    const auto found = measure.find(slot);
    return found == measure.end() ? key.alterations[letter_index(pitch.letter)] : found->second;
}

void Accidentals::propagation(std::string_view value, Location at, Diagnostics& diagnostics) {
    if (value == "octave")
        scope = AccidentalScope::octave;
    else if (value == "pitch")
        scope = AccidentalScope::pitch;
    else if (value == "not")
        scope = AccidentalScope::none;
    else
        diagnostics.warn(
            at,
            "invalid-accidental-scope",
            "expected 'pitch', 'octave' or 'not' for propagate-accidentals"
        );
}

struct TieNote {
    Sound       sound;
    NoteAddress note;
    Location    at;
};

struct Prepared {
    const Atom*  atom;
    Location     at;
    long double  duration;
    const Grace* grace;
    Ornament     ornament;
    bool         tied;
    bool         slur;
};

struct TupletState {
    int                        left   = 0;
    long double                factor = 1;
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
    long double         time      = 0;
    long double         bar       = 0;
    bool                first_bar = true;
    Accidentals         accidentals;
    vec<TieNote>        ties;
    Expression          expression;
    int                 slur       = 0;
    int                 channel    = 1;
    int                 base_pitch = 60;
    std::array<int, 16> programs{};
    // Re-entry uses the previous primary bar boundary, not an inner barline.
    long double                reentry_bar = 0;
    std::optional<long double> primary_until;

    void reenter() {
        first_bar   = true;
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
    explicit OverlayLanes(Diagnostics& diagnostics) : diagnostics(diagnostics) {}
    PlaybackLane&     active() { return lanes[selected]; }
    [[nodiscard]] int id() const { return static_cast<int>(selected); }
    void              control(Overlay command, WrittenState& written, Location at);
    void              close_implicit(WrittenState& written);
    void              finish_voice(WrittenState& written, Location at);
    void              primary_bar();
    void              break_ties();

private:
    struct Group {
        bool                        explicit_group = false;
        std::optional<WrittenState> primary_fields;
    };

    Diagnostics&         diagnostics;
    vec<PlaybackLane>    lanes{PlaybackLane{}};
    size_t               selected         = 0;
    long double          last_primary_bar = 0;
    std::optional<Group> group;

    void split(WrittenState& written, Location at);
    void join(WrittenState& written);
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

void OverlayLanes::split(WrittenState& written, Location at) {
    if (!group) group.emplace();
    if (!group->primary_fields) group->primary_fields = written;
    if (++selected >= max_voice_lanes)
        diagnostics.error(at, "overlay-limit", "too many overlay lanes");
    if (selected == lanes.size()) {
        lanes.push_back(PlaybackLane{.time = last_primary_bar, .reentry_bar = last_primary_bar});
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

void OverlayLanes::close_implicit(WrittenState& written) {
    if (group && !group->explicit_group) join(written);
}

void OverlayLanes::finish_voice(WrittenState& written, Location at) {
    if (!group) return;
    if (group->explicit_group)
        diagnostics.warn(at, "unterminated-overlay", "closed overlay at the end of the voice");
    join(written);
}

void OverlayLanes::primary_bar() {
    const auto now = lanes.front().time;
    for (size_t index = 1; index < lanes.size(); ++index) {
        auto& lane = lanes[index];
        if (!lane.primary_until && now > last_primary_bar) report_ties(lane.ties, diagnostics);
        lane.time += now - lane.primary_until.value_or(last_primary_bar);
        lane.reentry_bar = lane.time;
        lane.primary_until.reset();
    }
    last_primary_bar = now;
}

void OverlayLanes::break_ties() {
    // A suspended primary lane is outside a secondary lane's repeat jump.
    for (size_t index = selected == 0 ? 0 : 1; index < lanes.size(); ++index)
        report_ties(lanes[index].ties, diagnostics);
}

class VoicePlayer {
public:
    VoicePlayer(const TuneSettings& settings, std::string name, Diagnostics& diagnostics) :
        diagnostics(diagnostics), output(std::move(name), diagnostics),
        written{settings.meter, settings.key, settings.unit},
        accompaniment(settings.meter, diagnostics), symbols(settings.symbols), lanes(diagnostics) {}

    Performance render(const vec<Token>& preamble, const PlayOrder& order);

private:
    Diagnostics&                               diagnostics;
    VoiceOutput                                output;
    WrittenState                               written;
    Accompaniment                              accompaniment;
    std::array<std::string_view, 128>          symbols;
    OverlayLanes                               lanes;
    PlaybackLane&                              lane() { return lanes.active(); }
    TupletState                                tuplet;
    std::optional<Prepared>                    pending;
    std::optional<std::pair<Broken, Location>> broken;
    const Grace*                               grace = nullptr;
    Location                                   grace_at{};
    Ornament                                   ornament;
    vec<NoteAddress>                           line_notes;
    size_t                                     music_line = 0;
    Performance                                result;
    struct RepeatEntry {
        Key         tone;
        long double unit;
        int         channel;
    };
    std::optional<RepeatEntry> repeat_entry;

    vec<const Token*> timed_effects;
    std::string_view  decoration_name(const Decoration& decoration) const;
    bool              can_defer(const Token& token) const;
    void              visit_token(const Token& token);
    void              visit_step(PlayStep step);
    void              jump_repeat();
    void              finish_rhythm();

    MidiTarget target() {
        return {lanes.id(), lane().channel, lane().programs[lane().channel - 1]};
    }
    i32 at_tick(Location at) { return tick(lane().time, diagnostics, at); }
    int velocity(long double at) {
        return lane().expression.velocity(at - lane().bar, written.meter);
    }
    void        push(const Atom& atom, Location at);
    void        flush();
    void        orphan_grace(Location at);
    void        broken_pair(Prepared& next);
    long double atom_duration(const Atom& atom, Location at);
    void        play(const Prepared& atom);
    long double play_grace(const Prepared& atom);
    void        play_chord(const Prepared& atom, long double start, long double duration);
    void  play_trill(Pitch pitch, const Prepared& atom, long double start, long double duration);
    void  play_roll(Pitch pitch, const Prepared& atom, long double start, long double duration);
    Sound ornament_neighbor(Pitch pitch);
    Sound resolve_sound(Pitch pitch, int semitone, bool tie);
    Sound sound(Pitch pitch, bool tie);
    std::optional<NoteAddress> note(
        Sound sound, long double start, long double duration, int stagger, int loudness, Location at
    );
    std::optional<NoteAddress> sustain(Sound sound, long double end, Location at);
    void                       set_symbols(std::string_view value, Location at);
    void                       control(std::string_view args, Location at, bool in_field);
    void                       program(std::string_view args, Location at);
    void                       midi(std::string_view command, Location at, bool in_field);
    void                       information(std::string_view value, Location at);
    void                       lyrics(std::string_view text, Location at);
    void                       change_key(std::string_view value, Location at);
    void                       change_meter(std::string_view value, Location at);
    void                       change_tempo(std::string_view value, Location at);
    void                       change_voice(std::string_view value, Location at);
    void                       articulation(const Prepared& atom, long double duration);
    void                       visit(const Directive& command, Location at);
    void                       visit(const Field& field, Location at);
    void                       visit(const Atom& atom, Location at) { push(atom, at); }
    void                       visit(const Grace& grace, Location at);
    void                       visit(const Harmony& harmony, Location at);
    void                       visit(const Decoration& decoration, Location at);
    void                       visit(Slur change, Location at);
    void                       visit(Tie, Location at);
    void                       visit(Tuplet group, Location at);
    void                       visit(Broken next, Location at);
    void                       visit(Overlay change, Location at);
    void                       visit(const Bar& bar, Location at);
};

void VoicePlayer::orphan_grace(Location) {
    if (grace)
        diagnostics.warn(grace_at, "orphan-grace", "ignored grace notes without a following note");
    grace = nullptr;
}

void VoicePlayer::broken_pair(Prepared& next) {
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

void VoicePlayer::push(const Atom& atom, Location at) {
    Prepared next{
        &atom,
        at,
        atom_duration(atom, at),
        std::exchange(grace, nullptr),
        std::exchange(ornament, {}),
        atom.tie,
        lane().slur > 0
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

Sound VoicePlayer::sound(Pitch pitch, bool tie) {
    return resolve_sound(pitch, lane().accidentals.resolve(pitch, written.tone), tie);
}

Sound VoicePlayer::resolve_sound(Pitch pitch, int semitone, bool tie) {
    const auto index = letter_index(pitch.letter);
    return {
        lane().base_pitch + natural_pitch[index] + 12 * (pitch.octave + written.tone.octave)
            + semitone + written.tone.transpose,
        static_cast<int>(index),
        pitch.octave,
        pitch.accidental.has_value(),
        tie || pitch.tie
    };
}

Sound VoicePlayer::ornament_neighbor(Pitch pitch) {
    if (pitch.letter == 'B') ++pitch.octave;
    pitch.letter = pitch.letter == 'G' ? 'A' : static_cast<char>(pitch.letter + 1);
    pitch.accidental.reset();
    pitch.tie = false;
    // An inferred ornament uses the key, not the measure's accidental for its neighbor.
    return resolve_sound(pitch, written.tone.alterations[letter_index(pitch.letter)], false);
}

std::optional<NoteAddress> VoicePlayer::note(
    Sound sound, long double start, long double duration, int stagger, int loudness, Location at
) {
    return output.attack(target(), sound.note, start, duration, stagger, loudness, at);
}

std::optional<NoteAddress> VoicePlayer::sustain(Sound sound, long double end, Location at) {
    auto match = std::ranges::find_if(lane().ties, [&](const TieNote& pending) {
        return sound.note == pending.sound.note
               || (!sound.explicit_accidental && sound.letter == pending.sound.letter
                   && sound.written_octave == pending.sound.written_octave);
    });
    if (match == lane().ties.end()) return {};
    auto address = match->note;
    lane().ties.erase(match);
    address.release_at(tick(end, diagnostics, at));
    return address;
}

void VoicePlayer::play_chord(const Prepared& atom, long double start, long double duration) {
    diagnostics.spend(PlaybackWork::chord(atom.atom->pitches.size(), lane().ties.size()), atom.at);
    vec<TieNote> next_ties;
    const auto   audible
        = lane().expression.gate(duration, written.unit, atom.slur, atom.ornament.staccato);
    const auto loudness = velocity(start);
    // Outgoing ties disable chord spread; incoming sustains do not count as attacks.
    const auto has_ties = atom.tied || std::ranges::any_of(atom.atom->pitches, &Pitch::tie);
    const auto attack   = has_ties ? 0 : lane().expression.chord_attack;
    const auto max_stagger
        = std::max(0, tick(audible, diagnostics, atom.at) - PlaybackGrid::release_trim);
    int  stagger       = 0;
    bool aligned_lyric = false;
    for (const auto& pitch : atom.atom->pitches) {
        auto played  = sound(pitch, atom.tied);
        auto address = sustain(played, start + audible, atom.at);
        if (!address) {
            address = note(played, start, audible, stagger, loudness, atom.at);
            stagger = static_cast<int>(
                std::min<int64_t>(max_stagger, stagger + static_cast<int64_t>(attack))
            );
            if (address && !aligned_lyric) {
                line_notes.push_back(*address);
                aligned_lyric = true;
            }
        }
        if (address && played.tie) {
            played.note = address->pitch();
            next_ties.push_back({played, *address, atom.at});
        }
    }
    // abc2midi treats a dash between unlike pitches as an ordinary attack.
    lane().ties = std::move(next_ties);
}

long double VoicePlayer::play_grace(const Prepared& atom) {
    if (!atom.grace || atom.grace->pitches.empty()) return 0;
    const auto& pitches = atom.grace->pitches;
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
        note(sound(pitch, false), start, duration, 0, velocity(start), atom.at);
        start += duration;
    }
    return total;
}

void VoicePlayer::play_roll(
    Pitch pitch, const Prepared& atom, long double start, long double duration
) {
    const auto ornament_length = written.unit / lane().expression.grace_divider;
    if (ornament_length + epsilon >= duration) {
        play_chord(atom, start, duration);
        return;
    }
    note(ornament_neighbor(pitch), start, ornament_length, 0, velocity(start), atom.at);
    play_chord(atom, start + ornament_length, duration - ornament_length);
}

void VoicePlayer::play_trill(
    Pitch pitch, const Prepared& atom, long double start, long double duration
) {
    const auto count
        = std::clamp(static_cast<int>(std::min(256.L, duration / (written.unit / 8))), 2, 256);
    const auto step   = duration / count;
    const auto higher = ornament_neighbor(pitch);
    const auto lower  = sound(pitch, false);
    for (int i = 0; i < count; ++i) {
        const auto onset = start + i * step;
        note(i % 2 ? lower : higher, onset, step, 0, velocity(onset), atom.at);
    }
}

void VoicePlayer::articulation(const Prepared& atom, long double duration) {
    const auto& pitches = atom.atom->pitches;
    if (pitches.empty()) {
        report_ties(lane().ties, diagnostics);
        return;
    }
    const auto grace_time = play_grace(atom);
    const auto onset      = lane().time + grace_time;
    const auto remaining  = duration - grace_time;
    if (atom.ornament.roll && pitches.size() == 1)
        play_roll(pitches.front(), atom, onset, remaining);
    else if (atom.ornament.trill && pitches.size() == 1)
        play_trill(pitches.front(), atom, onset, remaining);
    else
        play_chord(atom, onset, remaining);
}

void VoicePlayer::play(const Prepared& atom) {
    const auto duration = atom.ornament.fermata ?
                              lane().expression.fermata(atom.duration, written.unit) :
                              atom.duration;
    const auto end      = lane().time + duration;
    (void)tick(end, diagnostics, atom.at);
    if (lanes.id() == 0) accompaniment.advance(end, atom.at);
    if (music_line != atom.atom->music_line) {
        line_notes.clear();
        music_line = atom.atom->music_line;
    }
    articulation(atom, duration);
    lane().time = end;
}

void VoicePlayer::visit(const Grace& next, Location at) {
    if (grace) orphan_grace(at);
    grace    = &next;
    grace_at = at;
}

void VoicePlayer::visit(const Harmony& chord, Location at) {
    flush();
    diagnostics.spend(PlaybackWork::text(chord.symbol), at);
    if (lanes.id() == 0) accompaniment.harmony(chord.symbol, written.tone.transpose, at);
}

void VoicePlayer::visit(Slur change, Location at) {
    if (change.start)
        ++lane().slur;
    else if (lane().slur == 0)
        diagnostics.warn(at, "unmatched-slur", "ignored ')' without a matching slur");
    else
        --lane().slur;
}

void VoicePlayer::visit(Tie, Location at) {
    if (pending)
        pending->tied = true;
    else
        diagnostics.warn(at, "unexpected-tie", "ignored tie without a preceding note");
}

std::string_view VoicePlayer::decoration_name(const Decoration& decoration) const {
    const auto mark = decoration.name;
    if (mark.size() == 1 && static_cast<unsigned char>(mark.front()) < symbols.size()) {
        if (const auto alias = symbols[static_cast<unsigned char>(mark.front())]; !alias.empty())
            return alias;
    }
    return mark;
}

bool Ornament::* ornament_flag(std::string_view mark) {
    if (mark == "." || mark == "breath") return &Ornament::staccato;
    if (mark == "fermata" || mark == "H") return &Ornament::fermata;
    if (mark == "trill" || mark == "T") return &Ornament::trill;
    if (mark == "~") return &Ornament::roll;
    return nullptr;
}

void VoicePlayer::visit(const Decoration& decoration, Location at) {
    const auto mark = decoration_name(decoration);
    diagnostics.spend(PlaybackWork::text(mark), at);
    if (const auto flag = ornament_flag(mark)) {
        ornament.*flag = true;
        return;
    }
    if (const auto change = dynamic_mark(mark)) {
        flush();
        lane().expression.apply(*change);
        return;
    }
    if (mark == "ped" || mark == "ped(") {
        flush();
        output.control(target(), at_tick(at), 64, 127, at);
        return;
    }
    if (mark == "ped-up" || mark == "ped)") {
        flush();
        output.control(target(), at_tick(at), 64, 0, at);
        return;
    }
    constexpr std::array marks{
        "coda"sv,
        "segno"sv,
        "accent"sv,
        ">"sv,
        "tenuto"sv,
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

void VoicePlayer::visit(const Bar& bar, Location at) {
    flush();
    finish_rhythm();
    orphan_grace(at);
    ornament = {};
    if (!bar.resets_measure) return;
    lanes.close_implicit(written);
    lane().accidentals.reset();
    const bool soft   = bar.repeat_start || bar.repeat_end || bar.section;
    const auto length = lane().time - lane().bar;
    if (soft && !lane().first_bar && length + epsilon < written.meter.measure()) return;
    if (length > epsilon) lane().first_bar = false;
    if (lanes.id() == 0) {
        lanes.primary_bar();
        accompaniment.bar(lane().time);
    }
    lane().bar = lane().time;
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
    auto alias = trim(value.substr(eq + 1));
    if (alias.size() >= 2 && (alias.front() == '!' || alias.front() == '+')
        && alias.back() == alias.front()) {
        alias = alias.substr(1, alias.size() - 2);
    }
    symbols[static_cast<unsigned char>(symbol.front())] = alias;
}

void VoicePlayer::program(std::string_view args, Location at) {
    auto target = midi_integer(args, 0, midi_max, diagnostics, at);
    if (trim(args).empty()) {
        lane().programs[lane().channel - 1] = target;
        return;
    }
    if (target < 1 || target > 16)
        diagnostics.error(at, "invalid-midi-channel", "program channel must be between 1 and 16");
    lane().programs[target - 1] = midi_integer(args, 0, midi_max, diagnostics, at);
}

void VoicePlayer::control(std::string_view args, Location at, bool in_field) {
    const auto number = midi_integer(args, 0, midi_max, diagnostics, at);
    const auto value  = midi_integer(args, 0, midi_max, diagnostics, at);
    const auto time   = tick(lane().time + (in_field ? 0 : 1), diagnostics, at);
    output.control(target(), time, number, value, at);
}

void VoicePlayer::midi(std::string_view command, Location at, bool in_field) {
    const auto name = word(command);
    const auto value
        = [&](int low, int high) { return midi_integer(command, low, high, diagnostics, at); };
    if (name == "program")
        program(command, at);
    else if (name == "channel")
        lane().channel = value(1, 16);
    else if (name == "control")
        control(command, at, in_field);
    else if (name == "transpose")
        written.tone.transpose = value(-127, 127);
    else if (name == "rtranspose")
        written.tone.transpose = std::clamp(written.tone.transpose + value(-127, 127), -127, 127);
    else if (name == "C" || name == "c")
        lane().base_pitch = value(0, midi_max);
    else if (
        !lane().expression.directive(name, command, at, diagnostics)
        && !accompaniment.directive(name, command, lane().time, at)
    ) {
        diagnostics.warn(at, "unsupported-midi", fmt::format("ignored MIDI directive '{}'", name));
    }
}

void VoicePlayer::information(std::string_view value, Location at) {
    const auto command = word(value);
    if (command == "MIDI")
        midi(value, at, true);
    else if (command == "propagate-accidentals")
        lane().accidentals.propagation(trim(value), at, diagnostics);
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
    auto value = command.value;
    diagnostics.spend(PlaybackWork::text(value), at);
    if (word(value) == "MIDI")
        midi(value, at, false);
    else
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
    if (const auto usec = tempo(value, written.unit, diagnostics, at)) {
        result.tempos.emplace_back(
            Tempo<Tick>(tick(lane().time + PlaybackGrid::tempo_offset, diagnostics, at), usec), at
        );
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
    case 'P': visit(Bar{}, at); break;
    case 'V': change_voice(value, at); break;
    case 'I': information(value, at); break;
    case 'U': set_symbols(value, at); break;
    case 'w': lyrics(value, at); break;
    default: break;
    }
}

void VoicePlayer::lyrics(std::string_view text, Location at) {
    size_t index = 0;
    while (!text.empty() && index < line_notes.size()) {
        const auto split = text.find_first_of(" \t-_|*");
        if (split == 0) {
            if (text.front() == '_' || text.front() == '*') ++index;
            text.remove_prefix(1);
            continue;
        }
        auto syllable = std::string(text.substr(0, split));
        std::ranges::replace(syllable, '~', ' ');
        line_notes[index++].lyric(std::move(syllable));
        if (split == text.npos) return;
        text.remove_prefix(split + (text[split] == '-'));
    }
    if (!trim(text).empty())
        diagnostics.warn(at, "extra-lyrics", "more lyric syllables than notes on the music line");
}

void VoicePlayer::jump_repeat() {
    flush();
    lanes.close_implicit(written);
    lanes.break_ties();
    finish_rhythm();
    lane().accidentals.reset();
    lane().slur    = 0;
    written.tone   = repeat_entry->tone;
    written.unit   = repeat_entry->unit;
    lane().channel = repeat_entry->channel;
}

bool VoicePlayer::can_defer(const Token& token) const {
    if (const auto* decoration = std::get_if<Decoration>(&token.value))
        return !ornament_flag(decoration_name(*decoration));
    if (std::holds_alternative<Harmony>(token.value)
        || std::holds_alternative<Directive>(token.value))
        return true;
    const auto* field = std::get_if<Field>(&token.value);
    return field && std::string_view("KQMI").find(field->kind) != std::string_view::npos;
}

void VoicePlayer::visit_token(const Token& token) {
    std::visit([&](const auto& value) { visit(value, token.at); }, token.value);
}

void VoicePlayer::visit_step(PlayStep step) {
    const auto& token = *step.token;
    diagnostics.spend(PlaybackWork::token, token.at);
    switch (step.flow) {
    case Flow::enter_repeat:
        flush();
        repeat_entry = {written.tone, written.unit, lane().channel};
        break;
    case Flow::jump_repeat: jump_repeat(); break;
    case Flow::soft_bar: visit(Bar{.repeat_end = true}, token.at); break;
    case Flow::hard_bar: visit(Bar{}, token.at); break;
    case Flow::token:
        if (broken && can_defer(token))
            timed_effects.push_back(&token);
        else
            visit_token(token);
        break;
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
    output.finish(result.tracks);
    accompaniment.finish(result.tracks, lane().time, end);
    return std::move(result);
}

}   // namespace

Performance perform(
    const TuneSettings& settings,
    const vec<Token>&   preamble,
    const PlayOrder&    order,
    std::string         name,
    Diagnostics&        diagnostics
) {
    return VoicePlayer(settings, std::move(name), diagnostics).render(preamble, order);
}

}   // namespace symusic::abc::detail

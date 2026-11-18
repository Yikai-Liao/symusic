#include "playback.h"

#include <cmath>
#include <limits>
#include <map>

#include "accompaniment.h"
#include "expression.h"


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

struct NoteAddress {
    TrackNative<Tick>* track;
    size_t             index;
    Note<Tick>&        note() const { return track->notes[index]; }
};

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

struct Cursor {
    long double  time      = 0;
    long double  bar       = 0;
    int          lane      = 0;
    bool         first_bar = true;
    Accidentals  accidentals;
    vec<TieNote> ties;
};

struct OverlayState {
    std::optional<Cursor> main;
    bool                  explicit_group;
    int                   lane = 0;
};

struct OverlayClock {
    long double                time;
    long double                bar;
    std::optional<long double> primary_until;
};

class VoicePlayer {
public:
    VoicePlayer(const TuneSettings& settings, std::string name, Diagnostics& diagnostics) :
        diagnostics(diagnostics), name(std::move(name)), meter(settings.meter), tone(settings.key),
        unit(settings.unit), accompaniment(meter, diagnostics), symbols(settings.symbols) {}

    Performance render(const PlayOrder& order);

private:
    using TrackKey = std::tuple<int, int, int>;   // overlay lane, MIDI channel, program
    Diagnostics&                                                      diagnostics;
    std::string                                                       name;
    Meter                                                             meter;
    Key                                                               tone;
    long double                                                       unit;
    Accompaniment                                                     accompaniment;
    std::array<std::string_view, 128>                                 symbols;
    Expression                                                        expression;
    Cursor                                                            cursor;
    long double                                                       written_bar = 0;
    std::map<int, OverlayClock>                                       overlay_clocks;
    void                                                              record_lane();
    void                                                              advance_lanes();
    void                                                              switch_lane(Location at);
    std::optional<OverlayState>                                       overlay;
    TupletState                                                       tuplet;
    std::optional<Prepared>                                           pending;
    std::optional<std::pair<Broken, Location>>                        broken;
    const Grace*                                                      grace = nullptr;
    Location                                                          grace_at{};
    Ornament                                                          ornament;
    int                                                               slur       = 0;
    int                                                               channel    = 1;
    int                                                               base_pitch = 60;
    std::array<int, 16>                                               programs{};
    std::map<TrackKey, TrackNative<Tick>>                             tracks;
    std::map<std::pair<int, int>, std::pair<TrackNative<Tick>*, i32>> pedal_starts;
    vec<NoteAddress>                                                  line_notes;
    size_t                                                            music_line = 0;
    Performance                                                       result;
    struct WrittenState {
        Key         tone;
        long double unit;
        int         channel;
    };
    std::optional<WrittenState> repeat_entry;

    vec<const Token*> timed_effects;
    void              visit_token(const Token& token);
    void              visit_step(PlayStep step);
    void              jump_repeat(Location at);
    void              finish_rhythm();

    TrackNative<Tick>& track(int for_channel);
    i32                at_tick(Location at) const { return tick(cursor.time, diagnostics, at); }
    int  velocity(long double at) const { return expression.velocity(at - cursor.bar, meter); }
    void push(const Atom& atom, Location at);
    void flush();
    void orphan_grace(Location at);
    void finish_overlay(Location at, bool unconditional);
    void broken_pair(Prepared& next);
    long double atom_duration(const Atom& atom, Location at);
    void        play(const Prepared& atom);
    long double play_grace(const Prepared& atom);
    void        play_chord(const Prepared& atom, long double start, long double duration);
    void  play_trill(Pitch pitch, const Prepared& atom, long double start, long double duration);
    void  play_roll(Pitch pitch, const Prepared& atom, long double start, long double duration);
    Sound ornament_neighbor(Pitch pitch) const;
    Sound resolve_sound(Pitch pitch, int semitone, bool tie) const;
    Sound sound(Pitch pitch, bool tie);
    std::optional<NoteAddress> note(
        Sound sound, long double start, long double duration, int stagger, int loudness, Location at
    );
    std::optional<NoteAddress> sustain(Sound sound, long double end, Location at);
    void                       report_ties();
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

TrackNative<Tick>& VoicePlayer::track(int for_channel) {
    const auto program = programs[for_channel - 1];
    return tracks
        .try_emplace(
            {cursor.lane, for_channel, program},
            cursor.lane == 0 ? name : "",
            static_cast<u8>(program),
            for_channel == 10
        )
        .first->second;
}

void VoicePlayer::report_ties() {
    for (const auto& tie : cursor.ties)
        diagnostics.warn(tie.at, "unmatched-tie", "tie has no matching next note");
    cursor.ties.clear();
}

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
    const auto short_factor = std::ldexp(1.L, -mark.count);
    const auto first        = mark.direction > 0 ? 2 - short_factor : short_factor;
    pending->duration *= first;
    next.duration *= 2 - first;
}

long double VoicePlayer::atom_duration(const Atom& atom, Location at) {
    const auto raw = (atom.measure_rest ? meter.measure() : unit) * atom.length.fraction();
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
        slur > 0
    };
    broken_pair(next);
    flush();
    pending = next;
}

void VoicePlayer::flush() {
    if (!pending) return;
    play(*pending);
    pending.reset();
    const auto effects = std::exchange(timed_effects, {});
    for (const auto* token : effects) visit_token(*token);
}

Sound VoicePlayer::sound(Pitch pitch, bool tie) {
    return resolve_sound(pitch, cursor.accidentals.resolve(pitch, tone), tie);
}

Sound VoicePlayer::resolve_sound(Pitch pitch, int semitone, bool tie) const {
    const auto index = letter_index(pitch.letter);
    return {
        base_pitch + natural_pitch[index] + 12 * (pitch.octave + tone.octave) + semitone
            + tone.transpose,
        static_cast<int>(index),
        pitch.octave,
        pitch.accidental.has_value(),
        tie || pitch.tie
    };
}

Sound VoicePlayer::ornament_neighbor(Pitch pitch) const {
    if (pitch.letter == 'B') ++pitch.octave;
    pitch.letter = pitch.letter == 'G' ? 'A' : static_cast<char>(pitch.letter + 1);
    pitch.accidental.reset();
    pitch.tie = false;
    // An inferred ornament uses the key, not the measure's accidental for its neighbor.
    return resolve_sound(pitch, tone.alterations[letter_index(pitch.letter)], false);
}

std::optional<NoteAddress> VoicePlayer::note(
    Sound sound, long double start, long double duration, int stagger, int loudness, Location at
) {
    if (sound.note < 0 || sound.note > midi_max) {
        diagnostics.error(
            at, "pitch-range", fmt::format("pitch {} is outside MIDI range 0–127", sound.note)
        );
    }
    if (loudness == 0) return {};
    const auto time = tick(start + 1 + stagger, diagnostics, at);
    const auto gate = std::max(0, tick(duration, diagnostics, at) - 1 - stagger);
    if (gate > std::numeric_limits<i32>::max() - time)
        diagnostics.error(at, "time-range", "note end exceeds the score's tick range");
    diagnostics.spend(1, at);
    auto& output = track(channel);
    output.notes.emplace_back(time, gate, static_cast<i8>(sound.note), static_cast<i8>(loudness));
    return NoteAddress{&output, output.notes.size() - 1};
}

std::optional<NoteAddress> VoicePlayer::sustain(Sound sound, long double end, Location at) {
    auto match = std::ranges::find_if(cursor.ties, [&](const TieNote& pending) {
        return sound.note == pending.sound.note
               || (!sound.explicit_accidental && sound.letter == pending.sound.letter
                   && sound.written_octave == pending.sound.written_octave);
    });
    if (match == cursor.ties.end()) return {};
    auto address = match->note;
    cursor.ties.erase(match);
    address.note().duration = std::max(0, tick(end, diagnostics, at) - address.note().time);
    return address;
}

void VoicePlayer::play_chord(const Prepared& atom, long double start, long double duration) {
    diagnostics.spend(atom.atom->pitches.size() + cursor.ties.size(), atom.at);
    vec<TieNote> next_ties;
    const auto   audible  = expression.gate(duration, unit, atom.slur, atom.ornament.staccato);
    const auto   loudness = velocity(start);
    const bool   fully_tied
        = atom.tied || (!cursor.ties.empty() && cursor.ties.size() == atom.atom->pitches.size());
    const auto attack        = fully_tied ? 0 : expression.chord_attack;
    const auto max_stagger   = std::max(0, tick(audible, diagnostics, atom.at) - 1);
    int        stagger       = 0;
    bool       aligned_lyric = false;
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
            played.note = address->note().pitch;
            next_ties.push_back({played, *address, atom.at});
        }
    }
    // abc2midi treats a dash between unlike pitches as an ordinary attack.
    cursor.ties = std::move(next_ties);
}

long double VoicePlayer::play_grace(const Prepared& atom) {
    if (!atom.grace || atom.grace->pitches.empty()) return 0;
    const auto& pitches = atom.grace->pitches;
    long double weights = 0;
    for (const auto& pitch : pitches) weights += pitch.length.fraction();
    if (weights == 0) return 0;
    const auto length = expression.grace_ratio > 0 ?
                            atom.duration * expression.grace_ratio / weights :
                            unit / expression.grace_divider;
    const auto total  = length * weights;
    if (total + epsilon >= atom.duration) {
        diagnostics.warn(
            atom.at,
            "grace-too-long",
            "grace sequence would consume its host note; ignored the grace notes"
        );
        return 0;
    }
    auto start = cursor.time;
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
    const auto ornament_length = unit / expression.grace_divider;
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
    const auto count = std::clamp(static_cast<int>(std::min(256.L, duration / (unit / 8))), 2, 256);
    const auto step  = duration / count;
    const auto higher = ornament_neighbor(pitch);
    const auto lower  = sound(pitch, false);
    for (int i = 0; i < count; ++i) {
        const auto onset = start + i * step;
        note(i % 2 ? lower : higher, onset, step, 0, velocity(start), atom.at);
    }
}

void VoicePlayer::articulation(const Prepared& atom, long double duration) {
    const auto& pitches = atom.atom->pitches;
    if (pitches.empty()) {
        report_ties();
        return;
    }
    const auto grace_time = play_grace(atom);
    const auto onset      = cursor.time + grace_time;
    const auto remaining  = duration - grace_time;
    if (atom.ornament.roll && pitches.size() == 1)
        play_roll(pitches.front(), atom, onset, remaining);
    else if (atom.ornament.trill && pitches.size() == 1)
        play_trill(pitches.front(), atom, onset, remaining);
    else
        play_chord(atom, onset, remaining);
}

void VoicePlayer::play(const Prepared& atom) {
    const auto duration = atom.ornament.fermata ? atom.duration * 2 : atom.duration;
    const auto end      = cursor.time + duration;
    (void)tick(end, diagnostics, atom.at);
    if (cursor.lane == 0) accompaniment.advance(end, atom.at);
    if (music_line != atom.at.line) {
        line_notes.clear();
        music_line = atom.at.line;
    }
    articulation(atom, duration);
    cursor.time = end;
}

void VoicePlayer::visit(const Grace& next, Location at) {
    if (grace) orphan_grace(at);
    grace    = &next;
    grace_at = at;
}

void VoicePlayer::visit(const Harmony& chord, Location at) {
    flush();
    if (cursor.lane == 0) accompaniment.harmony(chord.symbol, tone.transpose, at);
}

void VoicePlayer::visit(Slur change, Location at) {
    if (change.start)
        ++slur;
    else if (slur == 0)
        diagnostics.warn(at, "unmatched-slur", "ignored ')' without a matching slur");
    else
        --slur;
}

void VoicePlayer::visit(Tie, Location at) {
    if (pending)
        pending->tied = true;
    else
        diagnostics.warn(at, "unexpected-tie", "ignored tie without a preceding note");
}

void VoicePlayer::visit(const Decoration& decoration, Location at) {
    auto mark = decoration.name;
    if (mark.size() == 1 && static_cast<unsigned char>(mark.front()) < symbols.size()) {
        if (const auto alias = symbols[static_cast<unsigned char>(mark.front())]; !alias.empty())
            mark = alias;
    }
    if (mark == ".")
        ornament.staccato = true;
    else if (mark == "fermata" || mark == "H")
        ornament.fermata = true;
    else if (mark == "trill" || mark == "T")
        ornament.trill = true;
    else if (mark == "~")
        ornament.roll = true;
    else {
        const auto previous = expression;
        if (expression.dynamic(mark)) {
            const auto next = std::exchange(expression, previous);
            flush();
            expression = next;
            return;
        }
        constexpr std::array marks{
            "coda"sv,
            "segno"sv,
            "accent"sv,
            ">"sv,
            "tenuto"sv,
            "breath"sv,
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
    const auto in_time_of
        = group.in_time_of ? group.in_time_of : default_tuplet(group.count, meter.compound());
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

void VoicePlayer::record_lane() {
    auto& clock = overlay_clocks.at(cursor.lane);
    clock.time  = cursor.time;
    report_ties();
}

void VoicePlayer::advance_lanes() {
    for (auto& [lane, clock] : overlay_clocks) {
        clock.time += cursor.time - clock.primary_until.value_or(written_bar);
        clock.bar = clock.time;
        clock.primary_until.reset();
    }
    written_bar = cursor.time;
}

void VoicePlayer::finish_overlay(Location at, bool unconditional) {
    if (!overlay || (!unconditional && overlay->explicit_group)) return;
    if (overlay->main) {
        record_lane();
        cursor = std::move(*overlay->main);
        for (int lane = 1; lane <= overlay->lane; ++lane)
            overlay_clocks.at(lane).primary_until = cursor.time;
    }
    if (overlay->explicit_group)
        diagnostics.warn(at, "unterminated-overlay", "closed overlay at the end of the voice");
    overlay.reset();
}

void VoicePlayer::switch_lane(Location at) {
    if (!overlay) overlay = OverlayState{};
    if (!overlay->main)
        overlay->main = std::move(cursor);
    else
        record_lane();
    if (++overlay->lane >= max_voice_lanes)
        diagnostics.error(at, "overlay-limit", "too many overlay lanes");
    auto& clock = overlay_clocks.try_emplace(overlay->lane, written_bar, written_bar).first->second;
    cursor      = Cursor{.time = clock.time, .bar = clock.bar, .lane = overlay->lane};
}

void VoicePlayer::visit(Overlay change, Location at) {
    flush();
    if (change == Overlay::split) {
        switch_lane(at);
        return;
    }
    if (change == Overlay::start) {
        if (overlay)
            diagnostics.error(at, "nested-overlay", "nested overlays have no unambiguous playback");
        overlay = OverlayState{.explicit_group = true};
        return;
    }
    if (!overlay) {
        diagnostics.warn(at, "unexpected-overlay-end", "ignored '&)' without an overlay");
        return;
    }
    overlay->explicit_group = false;
    finish_overlay(at, true);
    cursor.bar = cursor.time;
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
    finish_overlay(at, false);
    cursor.accidentals.reset();
    const bool soft   = bar.repeat_start || bar.repeat_end || bar.section;
    const auto length = cursor.time - cursor.bar;
    if (soft && !cursor.first_bar && length + epsilon < meter.measure()) return;
    if (length > epsilon) cursor.first_bar = false;
    if (cursor.lane == 0) {
        advance_lanes();
        accompaniment.bar(cursor.time);
    }
    cursor.bar = cursor.time;
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
        programs[channel - 1] = target;
        return;
    }
    if (target < 1 || target > 16)
        diagnostics.error(at, "invalid-midi-channel", "program channel must be between 1 and 16");
    programs[target - 1] = midi_integer(args, 0, midi_max, diagnostics, at);
}

void VoicePlayer::control(std::string_view args, Location at, bool in_field) {
    const auto number = midi_integer(args, 0, midi_max, diagnostics, at);
    const auto value  = midi_integer(args, 0, midi_max, diagnostics, at);
    const auto time   = tick(cursor.time + (in_field ? 0 : 1), diagnostics, at);
    auto&      output = track(channel);
    output.controls.emplace_back(time, static_cast<u8>(number), static_cast<u8>(value));
    if (number != 64) return;
    const auto key = std::pair{cursor.lane, channel};
    if (value >= 64)
        pedal_starts.try_emplace(key, &output, time);
    else if (auto pedal = pedal_starts.extract(key); !pedal.empty()) {
        const auto [owner, start] = pedal.mapped();
        if (time >= start)
            owner->pedals.emplace_back(start, time - start);
        else
            diagnostics.warn(at, "backward-pedal", "ignored pedal release before its press");
    }
}

void VoicePlayer::midi(std::string_view command, Location at, bool in_field) {
    const auto name = word(command);
    const auto value
        = [&](int low, int high) { return midi_integer(command, low, high, diagnostics, at); };
    if (name == "program")
        program(command, at);
    else if (name == "channel")
        channel = value(1, 16);
    else if (name == "control")
        control(command, at, in_field);
    else if (name == "transpose")
        tone.transpose = value(-127, 127);
    else if (name == "rtranspose")
        tone.transpose = std::clamp(tone.transpose + value(-127, 127), -127, 127);
    else if (name == "c")
        base_pitch = value(0, midi_max);
    else if (
        !expression.directive(name, command, at, diagnostics)
        && !accompaniment.directive(name, command, cursor.time, at)
    ) {
        diagnostics.warn(at, "unsupported-midi", fmt::format("ignored MIDI directive '{}'", name));
    }
}

void VoicePlayer::information(std::string_view value, Location at) {
    const auto command = word(value);
    if (command == "MIDI")
        midi(value, at, true);
    else if (command == "propagate-accidentals")
        cursor.accidentals.propagation(trim(value), at, diagnostics);
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
    if (word(value) == "MIDI")
        midi(value, at, false);
    else
        information(command.value, at);
}

void VoicePlayer::change_key(std::string_view value, Location at) {
    const auto change = key(value, tone, diagnostics, at, false);
    if (!change) return;
    tone = *change;
    cursor.accidentals.reset();
    result.keys.emplace_back(
        KeySignature<Tick>(at_tick(at), static_cast<i8>(tone.fifths), static_cast<i8>(tone.minor)),
        at
    );
}

void VoicePlayer::change_meter(std::string_view value, Location at) {
    meter = detail::meter(value, diagnostics, at);
    accompaniment.meter(meter, cursor.time);
    result.meters.emplace_back(
        TimeSignature<Tick>(
            at_tick(at), static_cast<u8>(meter.numerator), static_cast<u8>(meter.denominator)
        ),
        at
    );
}

void VoicePlayer::change_tempo(std::string_view value, Location at) {
    if (const auto usec = tempo(value, unit, diagnostics, at)) {
        result.tempos.emplace_back(Tempo<Tick>(tick(cursor.time + 1, diagnostics, at), usec), at);
    }
}

void VoicePlayer::change_voice(std::string_view value, Location at) {
    const auto voice = voice_spec(value, diagnostics, at);
    if (voice.transpose) tone.transpose = *voice.transpose;
    if (voice.octave) tone.octave = *voice.octave;
}

void VoicePlayer::visit(const Field& field, Location at) {
    flush();
    const auto value = field.value;
    switch (field.kind) {
    case 'K': change_key(value, at); break;
    case 'Q': change_tempo(value, at); break;
    case 'M': change_meter(value, at); break;
    case 'L': unit = unit_length(value, diagnostics, at); break;
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
        auto address = line_notes[index++];
        address.track->lyrics.emplace_back(address.note().time, std::move(syllable));
        if (split == text.npos) return;
        text.remove_prefix(split + (text[split] == '-'));
    }
    if (!trim(text).empty())
        diagnostics.warn(at, "extra-lyrics", "more lyric syllables than notes on the music line");
}

void VoicePlayer::jump_repeat(Location at) {
    flush();
    finish_overlay(at, false);
    report_ties();
    finish_rhythm();
    cursor.accidentals.reset();
    slur    = 0;
    tone    = repeat_entry->tone;
    unit    = repeat_entry->unit;
    channel = repeat_entry->channel;
}

bool can_defer(const Token& token) {
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
    diagnostics.spend(1, token.at);
    switch (step.flow) {
    case Flow::enter_repeat:
        flush();
        repeat_entry = {tone, unit, channel};
        break;
    case Flow::jump_repeat: jump_repeat(token.at); break;
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

Performance VoicePlayer::render(const PlayOrder& order) {
    Location end{};
    for (const auto step : order) {
        end = step.token->at;
        visit_step(step);
    }
    flush();
    finish_rhythm();
    finish_overlay(end, true);
    report_ties();
    orphan_grace(end);
    if (tuplet.left)
        diagnostics.warn(tuplet.at, "unfinished-tuplet", "tuplet has fewer notes than declared");
    for (auto& [key, value] : tracks) {
        if (!value.empty()) result.tracks.push_back(std::move(value));
    }
    accompaniment.finish(result.tracks, cursor.time, end);
    return std::move(result);
}

}   // namespace

Performance perform(
    const TuneSettings& settings, const PlayOrder& order, std::string name, Diagnostics& diagnostics
) {
    return VoicePlayer(settings, std::move(name), diagnostics).render(order);
}

}   // namespace symusic::abc::detail

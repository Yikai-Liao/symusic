#include "symusic/io/abc.h"

#include <algorithm>
#include <array>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <map>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <variant>

#include <fmt/format.h>
#include <utf8.h>

#include "playback.h"
#include "symusic/conversion.h"
#include "symusic/io/common.h"
#include "symusic/utils.h"

namespace symusic::abc {
namespace detail {
namespace {

struct Header {
    TuneSettings     settings;
    PitchChange      opening_pitch;
    std::string_view order;
    Location         order_at;
};

void header_field(
    Header& header, const Field& field, const Token& token, Diagnostics& diagnostics
) {
    const auto value = field.value;
    switch (field.kind) {
    case 'M': header.settings.meter = meter(value, diagnostics, token.at); break;
    case 'K': {
        const auto closing   = key(value, header.settings.key, diagnostics, token.at, true).value();
        header.settings.key  = closing.state;
        header.opening_pitch = closing.pitch;
        break;
    }
    case 'P':
        header.order    = value;
        header.order_at = token.at;
        break;
    default: break;
    }
}

void finish_header(Header& header, const Token* unit, const Tune& tune, Diagnostics& diagnostics) {
    // The opening M: determines the default L:; later meter changes do not reset the unit.
    header.settings.unit
        = unit ? unit_length(std::get<Field>(unit->value).value, diagnostics, unit->at) :
                 (header.settings.meter.measure() < 3 * tpq ? whole / 16 : whole / 8);
    for (const auto& token : tune.header) {
        if (const auto* speed = std::get_if<TempoNotation>(&token.value)) {
            const auto usec = tempo(*speed, header.settings.unit, diagnostics, token.at);
            if (usec) header.settings.tempo = usec;
        }
    }
}

Header read_header(const Tune& tune, Diagnostics& diagnostics) {
    Header header{
        .settings
        = {.tuning = PitchTuning(tune, diagnostics), .programs = ProgramNumbers()}
    };
    const Token*     unit = nullptr;
    std::string_view title;
    for (const auto& token : tune.header) {
        if (const auto* field = std::get_if<Field>(&token.value)) {
            switch (field->kind) {
            case 'L': unit = &token; break;
            case 'T':
                if (title.empty()) title = field->value;
                break;
            default: header_field(header, *field, token, diagnostics); break;
            }
        }
    }
    // A tune-local I:abc-version can occur after its T: field.
    header.settings.title = decode_text(title, TextContext::Title);
    finish_header(header, unit, tune, diagnostics);
    return header;
}

struct UnderlayRun {
    size_t            music_end;
    vec<const Token*> lines;
};

// ABC 2.1 sections 4.15, 5.1 and 7.4: both kinds of underlay address
// written note groups, regardless of grace notes, rests or overlay clocks.
struct NoteSlots {
    vec<Token*> notes;
    vec<size_t> bars;

    void append(std::span<Token> passage) {
        for (auto& token : passage) {
            if (const auto* bar = std::get_if<Bar>(&token.value); bar && bar->resets_measure) {
                // An opening repeat without intervening music starts a measure rather than
                // ending one. A real rest-only measure still supplies a skip boundary.
                if (!bar->repeat_start || bar->repeat_end || has_music)
                    bars.push_back(notes.size());
                has_music = false;
            }
            if (const auto* atom = std::get_if<Atom>(&token.value)) {
                has_music = true;
                if (!atom->pitches.empty()) notes.push_back(&token);
            }
        }
    }

    void clear() {
        notes.clear();
        bars.clear();
        has_music = false;
    }

private:
    bool has_music = false;
};

// ABC 2.1 sections 5.1-5.2: adjacent fields are verses of the same written
// passage. Each run claims all eligible notes since the preceding run.
WrittenLyrics collect_written_lyrics(
    std::span<Token> music, const vec<UnderlayRun>& runs, Diagnostics& diagnostics
) {
    WrittenLyrics result;
    NoteSlots     slots;
    size_t        start = 0;
    for (const auto& [end, verses] : runs) {
        slots.append(music.subspan(start, end - start));
        for (size_t verse = 0; const auto* line : verses) {
            const auto text = std::get<Field>(line->value).value;
            for (auto& [slot, syllable] : align_lyrics(
                     text,
                     slots.notes.size(),
                     slots.bars,
                     line->at,
                     diagnostics
                 )) {
                auto& atom = std::get<Atom>(slots.notes[slot]->value);
                if (!atom.lyric_slot) {
                    result.emplace_back(WrittenSyllables{verses.size()});
                    atom.lyric_slot = result.size();
                }
                auto& entry = result[atom.lyric_slot - 1];
                entry.by_verse.emplace_back(verse, std::move(syllable));
            }
            ++verse;
        }
        slots.clear();
        start = end;
    }
    return result;
}

void attach_symbols(
    vec<Token>&             music,
    const vec<UnderlayRun>& runs,
    DecorationDelimiter     delimiter,
    Diagnostics&            diagnostics
) {
    std::multimap<const Token*, Token> prefixes;
    NoteSlots                          slots;
    size_t                             scanned = 0;
    for (const auto& [end, lines] : runs) {
        slots.append(std::span(music).subspan(scanned, end - scanned));
        scanned      = end;
        bool has_row = false;
        for (const auto* line : lines) {
            const auto& field = std::get<Field>(line->value);
            const auto  row   = align_symbols(
                field, slots.notes.size(), slots.bars, delimiter, line->at, diagnostics
            );
            if (!row) continue;
            has_row = true;
            for (const auto& [slot, at, symbol] : *row) {
                const auto* host = slots.notes[slot];
                std::visit(
                    [&](auto mark) { prefixes.emplace(host, Token{at, mark, host->music_line}); },
                    symbol
                );
            }
        }
        if (has_row) slots.clear();
    }
    if (prefixes.empty()) return;

    // Insert in stack order so playback sees the same tokens as inline notation.
    vec<Token> annotated;
    annotated.reserve(music.size() + prefixes.size());
    auto prefix = prefixes.begin();
    for (auto& token : music) {
        while (prefix != prefixes.end() && prefix->first == &token) {
            annotated.push_back(std::move(prefix->second));
            ++prefix;
        }
        annotated.push_back(std::move(token));
    }
    music = std::move(annotated);
}

bool part_marker(const Token& token) {
    const auto* field = std::get_if<Field>(&token.value);
    return field && field->kind == 'P';
}

bool delimits_overlay(const Bar& bar) {
    // A section bar can end a numbered ending even without repeat dots.
    return bar.section || bar.repeat_start || bar.repeat_end || !bar.endings.empty();
}

using MusicPosition = vec<Token>::iterator;

MusicPosition two_bar_end(MusicPosition first, MusicPosition end) {
    bool has_music = false;
    int  measures  = 0;
    for (auto token = first; token != end; ++token) {
        if (part_marker(*token) || std::holds_alternative<Overlay>(token->value)) break;
        if (const auto* bar = std::get_if<Bar>(&token->value)) {
            if (!bar->resets_measure || !has_music) break;
            if (++measures == 2) return token;
            if (delimits_overlay(*bar)) break;
            has_music = false;
        } else if (std::holds_alternative<Atom>(token->value)) {
            has_music = true;
        }
    }
    return end;
}

// Lower the §7.4 example to an explicit overlay before repeat routing. Underlay has
// already been placed on the written notes, so inserted bars cannot take lyric slots.
void resolve_two_bar_overlays(vec<Token>& music, Diagnostics& diagnostics) {
    const auto marker = std::ranges::find_if(music, [](const Token& token) {
        const auto* overlay = std::get_if<Overlay>(&token.value);
        return overlay && *overlay == Overlay::PreviousTwo;
    });
    const auto end    = music.end();
    if (marker == end) return;

    vec<std::pair<MusicPosition, Token>> insertions;
    std::array                           starts{end, end};
    auto                                 measure   = music.begin();
    bool                                 has_music = false;
    std::optional<Overlay>               open;
    const auto                           reset = [&](MusicPosition next) {
        starts    = {end, end};
        measure   = next;
        has_music = false;
    };

    for (auto token = music.begin(); token != end; ++token) {
        const auto next = std::next(token);
        if (const auto* overlay = std::get_if<Overlay>(&token->value)) {
            if (*overlay == Overlay::PreviousTwo) {
                const auto close
                    = !open && !has_music && starts.front() != end ? two_bar_end(next, end) : end;
                if (close == end) {
                    diagnostics.warn(
                        token->at,
                        "invalid-two-bar-overlay",
                        "'&&' needs two complete measures on each side without internal jumps"
                    );
                    token->value = EmptyMusicLine{};
                } else {
                    const auto first = starts.front();
                    insertions.emplace_back(
                        first, Token{first->at, Overlay::StartAtCursor, first->music_line}
                    );
                    token->value = Overlay::Split;
                    auto join    = std::next(close);
                    if (delimits_overlay(std::get<Bar>(close->value))) {
                        // Close the overlay before a repeat can jump. The written bar still
                        // controls the repeat, while the secondary lane sees a plain bar.
                        insertions.emplace_back(close, Token{close->at, Bar{}, close->music_line});
                        join = close;
                    }
                    insertions.emplace_back(
                        join, Token{close->at, Overlay::End, close->music_line}
                    );
                    token = close;
                }
                reset(std::next(token));
            } else {
                if (*overlay == Overlay::End)
                    open.reset();
                else if (!open || *overlay == Overlay::Start)
                    open = *overlay;
                reset(next);
            }
        } else if (const auto* bar = std::get_if<Bar>(&token->value)) {
            if (bar->resets_measure && open == Overlay::Split) open.reset();
            if (open || delimits_overlay(*bar) || !bar->resets_measure || !has_music) {
                reset(next);
            } else {
                starts    = {starts.back(), measure};
                measure   = next;
                has_music = false;
            }
        } else if (part_marker(*token)) {
            reset(next);
        } else if (std::holds_alternative<Atom>(token->value)) {
            has_music = true;
        }
    }

    if (insertions.empty()) return;
    vec<Token> lowered;
    lowered.reserve(music.size() + insertions.size());
    auto copied = music.begin();
    for (auto& [position, token] : insertions) {
        lowered.insert(
            lowered.end(), std::make_move_iterator(copied), std::make_move_iterator(position)
        );
        lowered.push_back(std::move(token));
        copied = position;
    }
    lowered.insert(lowered.end(), std::make_move_iterator(copied), std::make_move_iterator(end));
    music = std::move(lowered);
}

// ABC 2.1 section 7 allows numeric IDs and distinguishes 20 characters of string IDs.
struct VoiceId {
    std::string_view spelling;
    bool             numeric = false;

    auto               operator<=>(const VoiceId&) const = default;
    [[nodiscard]] bool empty() const { return spelling.empty(); }
};

VoiceId voice_id(std::string_view id) {
    if (!id.empty() && std::ranges::all_of(id, digit)) {
        const auto significant = id.find_first_not_of('0');
        return {id.substr(significant == id.npos ? id.size() - 1 : significant), true};
    }
    auto end = id.begin();
    for (int i = 0; i < 20 && end != id.end(); ++i) utf8::unchecked::next(end);
    return {id.substr(0, static_cast<size_t>(end - id.begin()))};
}

struct Voice {
    VoiceId          id;
    std::string_view name;
    vec<Token>       music;
    Location         declaration;
    WrittenLyrics    lyrics;
    Location         last_part{};
    vec<UnderlayRun> verse_runs;
    vec<UnderlayRun> symbol_runs;
};

struct InstrumentCommand {
    VoiceId         id;
    VoiceInstrument settings;
};

std::optional<InstrumentCommand> instrument_command(std::string_view args) {
    InstrumentCommand result{};
    auto              option = word(args);
    if (!option.empty() && option != "mute" && option.find('=') == option.npos) {
        result.id = voice_id(option);
        option    = word(args);
    }
    if (option.starts_with("instrument=")) {
        const auto instrument = integer(option.substr(11));
        if (!instrument || *instrument < 1 || *instrument > midi_max + 1) return {};
        result.settings.patch = VoiceInstrument::Patch{*instrument - 1, 0};
        option                = word(args);
        if (option.starts_with("bank=")) {
            const auto bank = integer(option.substr(5));
            if (!bank || *bank < 1 || *bank > midi_max_bank + 1) return {};
            result.settings.patch->bank = *bank - 1;
            option                      = word(args);
        }
    }
    if (option == "mute") {
        result.settings.mute = true;
        option               = word(args);
    }
    if (!option.empty()) return {};
    return result;
}

// Distribute the first tune's tokens into per-voice streams. Resolve
// written-order policy and underlay here, while one token
// still represents one written occurrence rather than a performed replay.
// The input Tune must outlive this object and the schedules built from it.
class Voices {
public:
    Voices(const Tune& tune, Header& header, Diagnostics& diagnostics);
    [[nodiscard]] const auto& all() const { return voices; }
    [[nodiscard]] bool        conductor_from_fields() const { return uses_conductor; }
    [[nodiscard]] DrumMap     initial_percussion_routes() const { return file_drum_map; }

private:
    Diagnostics&                  diagnostics;
    TuneSettings&                 settings;
    WrittenRhythm                 rhythm;
    ChordNames                    chord_names;
    const Location                reference;
    vec<Voice>                    voices;
    DrumMap                       file_drum_map;
    std::map<VoiceId, vec<Token>> pending_instruments;
    size_t                        selected           = 0;
    size_t                        opening_midi_voice = 0;
    bool                          uses_conductor     = false;
    std::optional<Token>          current_part;
    char                          previous_underlay = '\0';

    size_t find_voice(VoiceId id, Location at);
    bool   select(const Token& token);
    void   header(const Token& token);
    void   apply_opening_pitch(PitchChange pitch, Location at);
    void   body(const Token& token);
    bool   written_setting(const Token& token);
    bool   set_instrument(const Token& token);
    void   record_part(Voice& voice);
    void   append(const Token& token);
};

size_t Voices::find_voice(VoiceId id, Location at) {
    if (voices.front().id.empty()) {
        auto& first = voices.front();
        if (std::ranges::any_of(first.music, [](const Token& token) {
                return std::holds_alternative<Atom>(token.value);
            })) {
            diagnostics.warn(
                at, "late-first-voice", "continued the opening music in the first declared voice"
            );
        }
        first.id = id;
        return 0;
    }
    const auto match = std::ranges::find(voices, id, &Voice::id);
    if (match != voices.end()) return static_cast<size_t>(match - voices.begin());
    if (voices.size() >= max_voice_lanes) diagnostics.error(at, "voice-limit", "too many voices");
    voices.push_back({.id = id, .declaration = at});
    return voices.size() - 1;
}

bool Voices::select(const Token& token) {
    const auto* field = std::get_if<Field>(&token.value);
    if (!field || field->kind != 'V') return false;
    const auto spec = voice_spec(field->value, diagnostics, token.at);
    const auto id   = voice_id(spec.id);
    selected        = find_voice(id, token.at);
    if (auto pending = pending_instruments.extract(id); !pending.empty()) {
        auto& music = voices[selected].music;
        music.insert(music.end(), pending.mapped().begin(), pending.mapped().end());
    }
    auto& voice = voices[selected];
    if (!spec.name.empty()) voice.name = spec.name;
    // Selection locates the destination of later commands from other voices,
    // even when it emits no pitch change or other playback event.
    record_part(voice);
    // Selecting a voice without changing its pitch properties makes no playback event.
    if (!spec.pitch.empty()) append({token.at, spec.pitch, token.music_line});
    return true;
}

void Voices::record_part(Voice& voice) {
    if (current_part && voice.last_part != current_part->at) {
        voice.music.push_back(*current_part);
        voice.last_part = current_part->at;
    }
}

void Voices::append(const Token& token) {
    auto& voice = voices[selected];
    record_part(voice);
    voice.music.push_back(token);
    if (auto* atom = std::get_if<Atom>(&voice.music.back().value))
        atom->rhythm = rhythm.at_attack();
}

void Voices::header(const Token& token) {
    if (token.at < reference && file_drum_map.consume(token, diagnostics)) return;
    if (select(token)) return;
    if (const auto change = midi_track_command(token, diagnostics)) {
        append({token.at, *change, token.music_line});
    } else if (auto command = token.information().value_or(""); word(command) == "MIDI") {
        append(token);
    }
}

void Voices::body(const Token& token) {
    const auto* field = std::get_if<Field>(&token.value);
    if (field && field->kind == 'w') uses_conductor = true;
    if (field && (field->kind == 's' || field->kind == 'w')) {
        auto& voice = voices[selected];
        auto& runs  = field->kind == 's' ? voice.symbol_runs : voice.verse_runs;
        if (std::exchange(previous_underlay, field->kind) != field->kind)
            runs.push_back({voice.music.size(), {}});
        runs.back().lines.push_back(&token);
        return;
    }
    previous_underlay = '\0';
    if (select(token)) {
        opening_midi_voice = 0;
        uses_conductor     = true;
        return;
    }
    if (field && field->kind == 'P') {
        opening_midi_voice = 0;
        current_part       = token;
        return;
    }
    if (std::holds_alternative<Atom>(token.value) || std::holds_alternative<Grace>(token.value)
        || std::holds_alternative<Bar>(token.value) || std::holds_alternative<Overlay>(token.value))
        opening_midi_voice = 0;
    if (rhythm.consume(token, diagnostics) || written_setting(token)) return;
    if (const auto change = midi_track_command(token, diagnostics)) {
        if (opening_midi_voice)
            voices[opening_midi_voice].music.push_back({token.at, *change, token.music_line});
        else
            append({token.at, *change, token.music_line});
        return;
    }
    append(token);
}

bool Voices::written_setting(const Token& token) {
    // Keep the command token as a boundary between a grace and its host.
    settings.note_lengths.consume(token, diagnostics);
    const auto* field = std::get_if<Field>(&token.value);
    return (field && field->kind == 'm') || std::holds_alternative<SymbolDefinition>(token.value)
           || settings.programs.consume(token, reference, diagnostics)
           || chord_names.consume(token, diagnostics)
           || settings.accidentals.consume(token, diagnostics)
           || settings.harp_cuts.consume(token, diagnostics)
           || settings.dynamics.consume(token, diagnostics) || set_instrument(token);
}

bool Voices::set_instrument(const Token& token) {
    const auto information = token.information();
    if (!information) return false;
    auto args = *information;
    if (word(args) != "MIDI" || word(args) != "voice") return false;

    const auto command = instrument_command(args);
    if (!command) {
        diagnostics.warn(token.at, "invalid-midi-voice", "ignored malformed MIDI voice options");
    } else if (command->settings.patch || command->settings.mute) {
        const Token change{token.at, command->settings, token.music_line};
        if (command->id.empty() || command->id == voices[selected].id) {
            append(change);
        } else if (
            const auto target = std::ranges::find(voices, command->id, &Voice::id);
            target != voices.end()
        ) {
            // An instruction in another voice uses the target's own written cursor.
            target->music.push_back(change);
        } else {
            pending_instruments[command->id].push_back(change);
        }
    }
    return true;
}

void Voices::apply_opening_pitch(PitchChange pitch, Location at) {
    const bool named = !voices[selected].id.empty();
    // The opening signature is shared, but pitch options after V: change only
    // that voice and can override its earlier declarations, including with zero.
    if (!named)
        settings.key.apply(pitch);
    else if (!pitch.empty())
        append({at, pitch});
}

Voices::Voices(const Tune& tune, Header& metadata, Diagnostics& diagnostics) :
    diagnostics(diagnostics), settings(metadata.settings), reference(tune.reference) {
    voices.push_back({});
    for (const auto& token : tune.header)
        if (!rhythm.consume(token, diagnostics) && !written_setting(token)) header(token);
    rhythm.finish_header(settings.meter, diagnostics);
    apply_opening_pitch(metadata.opening_pitch, tune.key);
    // The converter still addresses the last header V: when a MIDI shift
    // appears in the opening body before a voice is selected or music starts.
    opening_midi_voice = selected;
    selected           = 0;
    for (const auto& token : tune.body) body(token);
    for (const auto& [id, commands] : pending_instruments)
        for (const auto& token : commands)
            diagnostics.warn(
                token.at, "unknown-midi-voice", fmt::format("no voice with ID '{}'", id.spelling)
            );
    for (auto& voice : voices) {
        voice.lyrics = collect_written_lyrics(voice.music, voice.verse_runs, diagnostics);
        attach_symbols(voice.music, voice.symbol_runs, tune.decoration_delimiter, diagnostics);
        for (auto& token : voice.music)
            if (auto* chord = std::get_if<Harmony>(&token.value))
                chord_names.resolve(*chord, token.at, diagnostics);
        resolve_two_bar_overlays(voice.music, diagnostics);
        voice.verse_runs.clear();
        voice.symbol_runs.clear();
    }
}

// Reconcile global fields after each voice has reduced its own changes to
// the last value per tick. The first voice to supply a value wins a conflict;
// an explicit field at tick zero may replace the opening default.
template<class Event>
vec<Event> global_events(
    Event initial, vec<Located<Event>>& input, Diagnostics& diagnostics, std::string_view label
) {
    std::ranges::stable_sort(input, {}, [](const auto& item) { return item.first.time; });
    vec<Event> result{initial};
    bool       have_change = false;
    for (auto& [event, at] : input) {
        if (result.back().time != event.time)
            result.push_back(std::move(event));
        else if (result.back() != event) {
            if (have_change)
                diagnostics.warn(
                    at,
                    "conflicting-metadata",
                    fmt::format(
                        "conflicting {} at tick {}; kept the first value", label, event.time
                    )
                );
            else
                result.back() = std::move(event);
        }
        have_change = true;
    }
    return result;
}

void merge(Performance& all, Performance voice) {
    const auto append = [](auto& into, auto& from) {
        into.insert(
            into.end(), std::make_move_iterator(from.begin()), std::make_move_iterator(from.end())
        );
    };
    const auto append_fields = [](auto& into, auto& from) {
        std::ranges::stable_sort(from, {}, [](const auto& item) { return item.first.time; });
        // A voice can supersede its own field at the same tick (e.g. on repeat entry).
        // Only compare the effective values when merging different voices.
        for (size_t i = 0; i < from.size(); ++i) {
            if (i + 1 == from.size() || from[i].first.time != from[i + 1].first.time)
                into.push_back(std::move(from[i]));
        }
    };
    append(all.passes, voice.passes);
    append_fields(all.tempos, voice.tempos);
    append_fields(all.meters, voice.meters);
    append_fields(all.keys, voice.keys);
    append(all.markers, voice.markers);
}

Score<Tick> score_from_tune(const Tune& tune, const TextLoader& files, Diagnostics& diagnostics) {
    auto  header    = read_header(tune, diagnostics);
    auto& settings  = header.settings;
    settings.stress = TuneStress(tune, files, diagnostics);
    const Voices        voices(tune, header, diagnostics);
    const PartItinerary itinerary(header.order, header.order_at, diagnostics);
    Performance         music;
    auto                drum_map = voices.initial_percussion_routes();
    vec<VoicePlan>      plans;
    plans.reserve(voices.all().size());
    for (const auto& voice : voices.all())
        plans.push_back(
            plan_voice(voice.music, itinerary.arrange(voice.music), voice.declaration, diagnostics)
        );
    settings.separate_conductor
        = voices.conductor_from_fields() || std::ranges::any_of(plans, &VoicePlan::uses_conductor);
    assign_midi_tracks(plans, settings.separate_conductor);
    if (settings.separate_conductor) prime_drum_map(plans.front().order, drum_map, diagnostics);
    // Share the sequence across voices, but restart it when reading a tune again.
    ChordAttackSequence chord_attacks;
    bool                has_output = false;
    for (size_t i = 0; i < plans.size(); ++i) {
        const auto& voice       = voices.all()[i];
        auto        name        = voice.name.empty() ?
                                      (has_output ? std::string{} : settings.title) :
                                      decode_text(voice.name, TextContext::VoiceName);
        auto        performance = perform(
            settings, plans[i], voice.lyrics, std::move(name), chord_attacks, diagnostics
        );
        has_output = has_output || std::ranges::any_of(performance.passes, [](const auto& pass) {
                         return !pass.tracks.empty();
                     });
        merge(music, std::move(performance));
    }
    ScoreNative<Tick> score(tpq);
    const auto        initial_meter = score_meter(settings.meter, 0);
    const auto        initial_key   = KeySignature<Tick>(
        0, static_cast<i8>(settings.key.fifths), static_cast<i8>(settings.key.minor)
    );
    score.time_signatures
        = global_events(initial_meter, music.meters, diagnostics, "meter changes");
    score.key_signatures   = global_events(initial_key, music.keys, diagnostics, "key changes");
    const int tempo_offset = PlaybackGrid::tempo_offset(settings.separate_conductor);
    for (auto& [tempo, at] : music.tempos)
        tempo.time = tick(static_cast<long double>(tempo.time) + tempo_offset, diagnostics, at);
    score.tempos
        = global_events(Tempo<Tick>(0, settings.tempo), music.tempos, diagnostics, "tempo changes");
    // A split voice can be declared after another named voice already exists.
    std::ranges::stable_sort(music.passes, {}, &PlaybackPass::declaration);
    for (auto& pass : music.passes) pass.publish(score.tracks, drum_map);
    score.markers = std::move(music.markers);
    return to_shared(std::move(score));
}

Result<Score<Tick>> read_text(
    std::string_view text, std::string_view source, const TextLoader& files = {}
) {
    Diagnostics diagnostics(source);
    const auto  tune = tokenize(text, files, diagnostics);
    return {score_from_tune(tune, files, diagnostics), diagnostics.take()};
}

std::string path_name(const std::filesystem::path& path) {
    const auto bytes = path.u8string();
    return {bytes.begin(), bytes.end()};
}

// Includes and stress profiles share the same read and byte budgets. The
// canonical path check keeps relative names and symlinks inside the score's
// directory. The caller supplies diagnostics at the referencing instruction.
class ScoreFiles {
public:
    explicit ScoreFiles(const std::filesystem::path& path) {
        std::error_code error;
        const auto      parent
            = path.has_parent_path() ? path.parent_path() : std::filesystem::path(".");
        directory = std::filesystem::canonical(parent, error);
        score     = std::filesystem::canonical(path, error);
    }

    TextRead read(std::string_view name, std::span<const std::string_view> active);

private:
    std::filesystem::path directory;
    std::filesystem::path score;
    size_t                remaining_reads = 128;
    size_t                remaining_bytes = 4 * 1024 * 1024;
};

TextRead ScoreFiles::read(std::string_view name, std::span<const std::string_view> active) {
    if (directory.empty() || remaining_reads == 0) return {};
    --remaining_reads;
    const std::filesystem::path relative(std::u8string(name.begin(), name.end()));
    if (relative.has_root_path()) return {};

    std::error_code error;
    const auto      path = std::filesystem::canonical(directory / relative, error);
    if (error || std::ranges::mismatch(directory, path).in1 != directory.end()
        || !std::filesystem::is_regular_file(path, error))
        return {};

    auto canonical = path_name(path);
    if (path == score || std::ranges::find(active, canonical) != active.end()) {
        // The back edge never opens a file and must leave the read budget intact.
        ++remaining_reads;
        return TextReadError::cycle;
    }
    std::ifstream        input(path, std::ios::binary | std::ios::ate);
    const std::streamoff size = input.tellg();
    if (size < 0 || size > static_cast<std::streamoff>(remaining_bytes) || !input.seekg(0))
        return {};
    std::string text(static_cast<size_t>(size), '\0');
    if (size != 0 && !input.read(text.data(), size)) return {};
    remaining_bytes -= text.size();
    return ExternalText{std::move(canonical), std::move(text)};
}

}   // namespace
}   // namespace detail

Result<Score<Tick>> read(std::string_view text, std::string_view source) {
    return detail::read_text(text, source);
}

Result<Score<Tick>> read_file(const std::filesystem::path& path) {
    const auto data = symusic::read_file(path);
    const auto text = data.empty() ?
                          std::string_view{} :
                          std::string_view(reinterpret_cast<const char*>(data.data()), data.size());
    detail::ScoreFiles files(path);
    return detail::read_text(text, detail::path_name(path), [&](auto name, auto active) {
        return files.read(name, active);
    });
}

}   // namespace symusic::abc

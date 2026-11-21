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
    TuneSettings settings;
    PitchChange  opening_pitch;
    std::string  order;
    Location     order_at;
};

Header read_header(const Tune& tune, Diagnostics& diagnostics) {
    Header           header{.settings = {.tuning = PitchTuning(tune, diagnostics)}};
    auto&            settings = header.settings;
    const Token*     unit     = nullptr;
    std::string_view title;
    for (const auto& token : tune.header) {
        const auto* field = std::get_if<Field>(&token.value);
        if (!field) continue;
        switch (field->kind) {
        case 'T':
            if (title.empty()) title = field->value;
            break;
        case 'M': settings.meter = meter(field->value, diagnostics, token.at); break;
        case 'L': unit = &token; break;
        case 'K': {
            const auto opening = key(field->value, settings.key, diagnostics, token.at);
            if (!opening)
                diagnostics.error(
                    token.at, "invalid-key", "expected a key signature such as C, F#m or none"
                );
            settings.key         = opening->state;
            header.opening_pitch = opening->pitch;
            break;
        }
        case 'P':
            header.order    = field->value;
            header.order_at = token.at;
            break;
        default: break;
        }
    }
    settings.title = decode_text(title, TextContext::Title);
    // The opening meter supplies the default L:. A unit-relative tempo needs the final L:.
    settings.unit = unit ? unit_length(std::get<Field>(unit->value).value, diagnostics, unit->at) :
                           (settings.meter.measure() < 3 * kTpq ? kWhole / 16 : kWhole / 8);
    for (const auto& token : tune.header) {
        if (const auto* speed = std::get_if<TempoNotation>(&token.value)) {
            if (const auto usec = tempo(*speed, settings.unit, diagnostics, token.at))
                settings.tempo = usec;
        }
    }
    return header;
}

using MusicPosition = vec<Token>::iterator;

struct Insertion {
    MusicPosition before;
    Token         token;
};

// Positions are in the original voice. Equal positions retain their input order.
void insert_in_order(vec<Token>& music, vec<Insertion>& insertions) {
    if (insertions.empty()) return;
    vec<Token> result;
    result.reserve(music.size() + insertions.size());
    auto       first      = music.begin();
    const auto move_until = [&](MusicPosition end) {
        std::move(first, end, std::back_inserter(result));
        first = end;
    };
    for (auto& [before, token] : insertions) {
        move_until(before);
        result.push_back(std::move(token));
    }
    move_until(music.end());
    music = std::move(result);
}

// Underlay counts pitched note groups. A bar skip crosses a measure containing
// music, including a rest-only measure. An opening barline starts the first one.
struct NoteSlots {
    vec<MusicPosition> notes;
    vec<size_t>        bars;
    bool               has_music = false;

    void extend(MusicPosition first, MusicPosition end) {
        for (auto token = first; token != end; ++token) {
            if (const auto* atom = std::get_if<Atom>(&token->value)) {
                has_music = true;
                if (!atom->notes.empty()) notes.push_back(token);
            } else if (
                const auto* bar = std::get_if<Bar>(&token->value); bar && bar->resets_measure
            ) {
                if (has_music) bars.push_back(notes.size());
                has_music = false;
            }
        }
    }
};

// Adjacent rows of one kind address the same passage. Source row numbers keep
// fields consumed during assembly as boundaries between passages.
class Underlay {
public:
    void add(Token token, size_t row, size_t music_end) {
        rows.push_back({std::move(token), row, music_end});
    }

    // A valid row claims the accumulated slots. A run of malformed symbol
    // rows leaves them available to the next run.
    void align(vec<Token>& music, auto place) {
        NoteSlots slots;
        auto      first = music.begin();
        for (size_t row_begin = 0; row_begin < rows.size();) {
            auto row_end = row_begin + 1;
            while (row_end < rows.size() && rows[row_end].source == rows[row_end - 1].source + 1)
                ++row_end;
            const auto end = music.begin()
                             + static_cast<vec<Token>::difference_type>(rows[row_begin].music_end);
            slots.extend(first, end);
            first = end;
            if (place(std::span(rows).subspan(row_begin, row_end - row_begin), slots)) {
                slots.notes.clear();
                slots.bars.clear();
                slots.has_music = false;
            }
            row_begin = row_end;
        }
        rows.clear();
    }

private:
    struct Row {
        Token  token;
        size_t source;
        size_t music_end;
    };
    vec<Row> rows;
};

bool part_marker(const Token& token) {
    const auto* field = std::get_if<Field>(&token.value);
    return field && field->kind == 'P';
}

bool delimits_overlay(const Bar& bar) {
    // A section bar can end a numbered ending even without repeat dots.
    return bar.section || bar.repeat_start || bar.repeat_end || !bar.endings.empty();
}

MusicPosition two_bar_end(MusicPosition first, MusicPosition end) {
    bool has_music = false;
    i32  measures  = 0;
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

    vec<Insertion>         insertions;
    std::array             starts{end, end};
    auto                   measure   = music.begin();
    bool                   has_music = false;
    std::optional<Overlay> open;
    const auto             reset = [&](MusicPosition next) {
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

    insert_in_order(music, insertions);
}

// ABC 2.1 section 7 allows numeric IDs and distinguishes 20 characters of string IDs.
struct VoiceId {
    std::string spelling;
    bool        numeric = false;

    auto               operator<=>(const VoiceId&) const = default;
    [[nodiscard]] bool empty() const { return spelling.empty(); }
};

VoiceId voice_id(std::string_view id) {
    if (!id.empty() && std::ranges::all_of(id, digit)) {
        const auto significant = id.find_first_not_of('0');
        return {std::string(id.substr(significant == id.npos ? id.size() - 1 : significant)), true};
    }
    auto end = id.begin();
    for (i32 i = 0; i < 20 && end != id.end(); ++i) utf8::unchecked::next(end);
    return {std::string(id.substr(0, static_cast<size_t>(end - id.begin())))};
}

struct Voice {
    VoiceId         id;
    std::string     name;
    vec<Token>      music;
    Location        declaration;
    WrittenLyrics   lyrics;
    WrittenPosition last_part;
    Underlay        verses;
    Underlay        symbols;

    void attach_underlay(DecorationDelimiter delimiter, Diagnostics& diagnostics);
};

void Voice::attach_underlay(DecorationDelimiter delimiter, Diagnostics& diagnostics) {
    verses.align(music, [&](auto rows, const NoteSlots& slots) {
        for (size_t verse = 0; verse < rows.size(); ++verse) {
            const auto& line = rows[verse].token;
            const auto  text = std::get<Field>(line.value).value;
            for (auto& [slot, syllable] :
                 align_lyrics(text, slots.notes.size(), slots.bars, line.at, diagnostics)) {
                auto& atom = std::get<Atom>(slots.notes[slot]->value);
                if (!atom.lyric_slot) {
                    lyrics.push_back({.verse_count = rows.size()});
                    atom.lyric_slot = lyrics.size();
                }
                lyrics[atom.lyric_slot - 1].by_verse.emplace_back(verse, std::move(syllable));
            }
        }
        return true;
    });

    vec<Insertion> prefixes;
    symbols.align(music, [&](auto rows, const NoteSlots& slots) {
        bool claimed = false;
        for (const auto& row : rows) {
            const auto& line  = row.token;
            auto        marks = align_symbols(
                std::get<Field>(line.value),
                slots.notes.size(),
                slots.bars,
                delimiter,
                line.at,
                diagnostics
            );
            if (!marks) continue;
            claimed = true;
            for (auto& [slot, at, symbol] : *marks) {
                const auto host = slots.notes[slot];
                at.written      = host->at.written;
                std::visit(
                    [&](auto& mark) {
                        prefixes.push_back({host, Token{at, std::move(mark), host->music_line}});
                    },
                    symbol
                );
            }
        }
        return claimed;
    });
    std::ranges::stable_sort(prefixes, {}, &Insertion::before);
    insert_in_order(music, prefixes);
}

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
        if (!instrument || *instrument < 1 || *instrument > kMidiMax + 1) return {};
        result.settings.patch = VoiceInstrument::Patch{*instrument - 1, 0};
        option                = word(args);
        if (option.starts_with("bank=")) {
            const auto bank = integer(option.substr(5));
            if (!bank || *bank < 1 || *bank > kMidiMaxBank + 1) return {};
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
// The input Tune must outlive this object and the plans built from it.
class Voices {
public:
    Voices(Tune& tune, Header& header, Diagnostics& diagnostics);
    vec<VoicePlan>               plan(const PartItinerary& itinerary);
    [[nodiscard]] const auto&    all() const { return voices; }
    [[nodiscard]] const DrumMap& initial_percussion_routes() const { return file_drum_map; }

private:
    Diagnostics&                  diagnostics;
    TuneSettings&                 settings;
    WrittenRhythm                 rhythm;
    ChordNames                    chord_names;
    const Location                reference;
    vec<Voice>                    voices{1};
    DrumMap                       file_drum_map;
    std::map<VoiceId, vec<Token>> pending_instruments;
    size_t                        selected           = 0;
    size_t                        opening_midi_voice = 0;
    std::optional<Token>          current_part;

    size_t find_voice(const VoiceId& id, Location at);
    void   select(std::string_view value, const Token& token);
    void   header(Token token);
    void   body(Token& token, size_t row);
    bool   written_setting(const Token& token);
    bool   set_instrument(const Token& token);
    Voice& current();
    bool   route_midi_command(const Token& token);
    void   append(Token token);
};

size_t Voices::find_voice(const VoiceId& id, Location at) {
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
    if (voices.size() >= kMaxVoiceLanes) diagnostics.error(at, "voice-limit", "too many voices");
    voices.push_back({.id = id, .declaration = at});
    return voices.size() - 1;
}

void Voices::select(std::string_view value, const Token& token) {
    auto       spec = voice_spec(value, diagnostics, token.at);
    const auto id   = voice_id(spec.id);
    selected        = find_voice(id, token.at);
    if (auto pending = pending_instruments.extract(id); !pending.empty()) {
        auto& music = voices[selected].music;
        music.insert(music.end(), pending.mapped().begin(), pending.mapped().end());
    }
    // Selection places the part label even if only another voice subsequently
    // writes MIDI commands at this voice's cursor.
    auto& voice = current();
    if (!spec.name.empty()) voice.name = std::move(spec.name);
    if (!spec.pitch.empty()) voice.music.push_back({token.at, spec.pitch, token.music_line});
}

Voice& Voices::current() {
    auto& voice = voices[selected];
    if (current_part && voice.last_part != current_part->at.written) {
        voice.music.push_back(*current_part);
        voice.last_part = current_part->at.written;
    }
    return voice;
}

void Voices::append(Token token) {
    if (auto* atom = std::get_if<Atom>(&token.value)) atom->rhythm = rhythm.at_attack();
    current().music.push_back(std::move(token));
}

bool Voices::route_midi_command(const Token& token) {
    const auto change = midi_track_command(token, diagnostics);
    if (!change) return false;
    // Opening body MIDI shifts still address the last header V:.
    auto& target = opening_midi_voice ? voices[opening_midi_voice] : current();
    target.music.push_back({token.at, *change, token.music_line});
    return true;
}

// Planning and rendering consume the same operand. Keep the field's text cost
// so playback can charge it at each performed occurrence.
void resolve_track_field(Token& token, Diagnostics& diagnostics) {
    const auto text = token.information();
    if (!text) return;
    Instruction command(token);
    const auto  work = PlaybackWork::text(*text);
    if (command.midi == "drummap") {
        token.value = DrumMapChange{DrumMap::read(command.args, token.at, diagnostics), work};
    } else if (command.midi == "makechordchannels") {
        constexpr i32 kMaxChordChannels = 9;
        const auto    count
            = std::clamp(integer(word(command.args)).value_or(0), 0, kMaxChordChannels);
        token.value = ChordChannelRequest{count, work};
    }
}

void Voices::header(Token token) {
    if (written_setting(token)) return;
    if (const auto* field = std::get_if<Field>(&token.value); field && field->kind == 'V') {
        select(field->value, token);
        return;
    }
    if (route_midi_command(token)) return;
    auto command = token.information().value_or("");
    if (word(command) != "MIDI" && !std::holds_alternative<StressCommand>(token.value)) return;
    if (token.at < reference) {
        resolve_track_field(token, diagnostics);
        if (const auto* change = std::get_if<DrumMapChange>(&token.value)) {
            if (change->route) file_drum_map.apply(*change->route);
            return;
        }
    }
    append(std::move(token));
}

void Voices::body(Token& token, size_t row) {
    if (const auto* field = std::get_if<Field>(&token.value)) {
        auto& voice = voices[selected];
        switch (field->kind) {
        case 'w':
            settings.separate_conductor = true;
            voice.verses.add(std::move(token), row, voice.music.size());
            return;
        case 's': voice.symbols.add(std::move(token), row, voice.music.size()); return;
        case 'V':
            select(field->value, token);
            opening_midi_voice          = 0;
            settings.separate_conductor = true;
            return;
        case 'P':
            current_part       = std::move(token);
            opening_midi_voice = 0;
            return;
        default: break;
        }
    }
    if (std::holds_alternative<Atom>(token.value) || std::holds_alternative<Grace>(token.value)
        || std::holds_alternative<Bar>(token.value) || std::holds_alternative<Overlay>(token.value))
        opening_midi_voice = 0;
    if (written_setting(token) || route_midi_command(token)) return;
    append(std::move(token));
}

bool Voices::written_setting(const Token& token) {
    if (rhythm.consume(token, diagnostics)) return true;
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

Voices::Voices(Tune& tune, Header& metadata, Diagnostics& diagnostics) :
    diagnostics(diagnostics), settings(metadata.settings), reference(tune.reference) {
    for (const auto& token : tune.header) header(token);
    rhythm.finish_header(settings.meter, diagnostics);
    // Opening K: pitch options belong to the selected header voice, or to
    // the shared key if the header has no named voice.
    if (voices[selected].id.empty())
        settings.key.apply(metadata.opening_pitch);
    else if (!metadata.opening_pitch.empty())
        append({tune.key, metadata.opening_pitch});
    opening_midi_voice = selected;
    selected           = 0;
    for (size_t row = 0; row < tune.body.size(); ++row) body(tune.body[row], row);
    // Voices own their tokens; remaining views borrow the tune's source text.
    vec<Token>{}.swap(tune.body);
    for (const auto& [id, commands] : pending_instruments)
        for (const auto& token : commands)
            diagnostics.warn(
                token.at, "unknown-midi-voice", fmt::format("no voice with ID '{}'", id.spelling)
            );
    for (auto& voice : voices) {
        voice.attach_underlay(tune.decoration_delimiter, diagnostics);
        for (auto& token : voice.music)
            if (auto* chord = std::get_if<Harmony>(&token.value))
                chord_names.resolve(*chord, token.at, diagnostics);
        resolve_two_bar_overlays(voice.music, diagnostics);
    }
}

vec<VoicePlan> Voices::plan(const PartItinerary& itinerary) {
    vec<VoicePlan> plans;
    plans.reserve(voices.size());
    for (auto& voice : voices) {
        auto order = itinerary.arrange(voice.music);
        // A skipped ending or part must not diagnose its MIDI fields. Replace
        // each played field in place so repeats share its operand and address.
        for (const auto& range : order) {
            if (range.flow != Flow::Token) continue;
            for (auto& token : range.tokens(std::span(voice.music)))
                resolve_track_field(token, diagnostics);
        }
        plans.push_back(
            plan_voice(std::move(voice.music), std::move(order), voice.declaration, diagnostics)
        );
    }
    return plans;
}

// Reconcile global fields after each voice has reduced its own changes to
// the last value per tick. The first voice to supply a value wins a conflict;
// an explicit field at tick zero may replace the opening default.
template<class Event>
vec<Event> global_events(
    Event initial, vec<Located<Event>>& input, Diagnostics& diagnostics, std::string_view label
) {
    std::ranges::stable_sort(input, {}, [](const auto& item) { return item.first.time; });
    if (!input.empty() && input.front().first.time == initial.time) initial = input.front().first;
    vec<Event> result{initial};
    for (auto& [event, at] : input) {
        if (result.back().time != event.time)
            result.push_back(std::move(event));
        else if (result.back() != event)
            diagnostics.warn(
                at,
                "conflicting-metadata",
                fmt::format("conflicting {} at tick {}; kept the first value", label, event.time)
            );
    }
    return result;
}

void merge(Performance& all, Performance voice) {
    const auto append_fields = [](auto& into, auto& from) {
        std::ranges::stable_sort(from, {}, [](const auto& item) { return item.first.time; });
        // A voice can supersede its own field at the same tick (e.g. on repeat entry).
        // Only compare the effective values when merging different voices.
        for (size_t i = 0; i < from.size(); ++i) {
            if (i + 1 == from.size() || from[i].first.time != from[i + 1].first.time)
                into.push_back(std::move(from[i]));
        }
    };
    std::ranges::move(voice.passes, std::back_inserter(all.passes));
    append_fields(all.tempos, voice.tempos);
    append_fields(all.meters, voice.meters);
    append_fields(all.keys, voice.keys);
}

Score<Tick> score_from_tune(Tune& tune, const TextLoader& files, Diagnostics& diagnostics) {
    auto  header    = read_header(tune, diagnostics);
    auto& settings  = header.settings;
    settings.stress = TuneStress(tune, files, diagnostics);
    Voices              voices(tune, header, diagnostics);
    const PartItinerary itinerary(header.order, header.order_at, diagnostics);
    Performance         music;
    auto                plans = voices.plan(itinerary);
    settings.separate_conductor |= std::ranges::any_of(plans, &VoicePlan::uses_conductor);
    assign_midi_tracks(plans, settings.separate_conductor, voices.initial_percussion_routes());
    // Share the sequence across voices, but restart it when reading a tune again.
    ChordAttackSequence chord_attacks;
    bool                has_output = false;
    for (size_t i = 0; i < plans.size(); ++i) {
        const auto& voice = voices.all()[i];
        auto        name  = voice.name.empty() ? (has_output ? std::string{} : settings.title) :
                                                 decode_text(voice.name, TextContext::VoiceName);
        auto        performance = perform(
            settings, std::move(plans[i]), voice.lyrics, std::move(name), chord_attacks, diagnostics
        );
        has_output = has_output || std::ranges::any_of(performance.passes, [](const auto& pass) {
                         return !pass.tracks.empty();
                     });
        merge(music, std::move(performance));
    }
    ScoreNative<Tick> score(kTpq);
    const auto        initial_meter = score_meter(settings.meter, 0);
    const auto        initial_key   = KeySignature<Tick>(
        0, static_cast<i8>(settings.key.fifths), static_cast<i8>(settings.key.minor)
    );
    score.time_signatures
        = global_events(initial_meter, music.meters, diagnostics, "meter changes");
    score.key_signatures   = global_events(initial_key, music.keys, diagnostics, "key changes");
    const i32 tempo_offset = PlaybackGrid::tempo_offset(settings.separate_conductor);
    for (auto& [tempo, at] : music.tempos)
        tempo.time = tick(static_cast<f64>(tempo.time) + tempo_offset, diagnostics, at);
    score.tempos
        = global_events(Tempo<Tick>(0, settings.tempo), music.tempos, diagnostics, "tempo changes");
    // A split voice can be declared after another named voice already exists.
    std::ranges::stable_sort(music.passes, {}, [](const PlaybackPass& pass) {
        return pass.declaration.written;
    });
    for (auto& pass : music.passes)
        std::ranges::move(pass.tracks, std::back_inserter(score.tracks));
    return to_shared(std::move(score));
}

Result<Score<Tick>> read_text(
    std::string_view text, std::string_view source, const TextLoader& files = {}
) {
    Diagnostics diagnostics(source);
    auto        tune = tokenize(text, files, diagnostics);
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
    size_t                remaining_bytes = size_t{4} * 1024 * 1024;
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
        return TextReadError::Cycle;
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

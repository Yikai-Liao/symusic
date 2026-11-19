#include "symusic/io/abc.h"

#include <map>

#include "playback.h"
#include "symusic/conversion.h"
#include "symusic/utils.h"

namespace symusic::abc {
namespace detail {
namespace {

struct Header {
    TuneSettings     settings;
    std::string_view order;
    Location         order_at;
    Location         rhythm_at;
};

void header_field(Header& header, Field field, const Token& token, Diagnostics& diagnostics) {
    const auto value = field.value;
    switch (field.kind) {
    case 'M': header.settings.meter = meter(value, diagnostics, token.at); break;
    case 'K':
        header.settings.key = *key(value, header.settings.key, diagnostics, token.at, true);
        break;
    case 'T':
        if (header.settings.title.empty()) header.settings.title = unquote_text(value);
        break;
    case 'R':
        header.settings.rhythm = rhythm_style(value);
        header.rhythm_at       = token.at;
        break;
    case 'P':
        header.order    = value;
        header.order_at = token.at;
        break;
    default: break;
    }
}

void finish_header(
    Header& header, const Token* unit, const Token* speed, Diagnostics& diagnostics
) {
    if (header.settings.rhythm == RhythmStyle::hornpipe && !hornpipe_meter(header.settings.meter)) {
        diagnostics.warn(
            header.rhythm_at,
            "hornpipe-meter",
            "disabled automatic hornpipe swing: the opening meter must be 2/4 or 4/4"
        );
        header.settings.rhythm = RhythmStyle::hornpipe_without_swing;
    }
    header.settings.unit
        = unit ? unit_length(std::get<Field>(unit->value).value, diagnostics, unit->at) :
                 (header.settings.meter.measure() < 3 * tpq ? whole / 16 : whole / 8);
    if (speed) {
        const auto usec = tempo(
            std::get<Field>(speed->value).value, header.settings.unit, diagnostics, speed->at
        );
        if (usec) header.settings.tempo = usec;
    }
}

Header read_header(const Tune& tune, Diagnostics& diagnostics) {
    Header       header{.settings = {.tuning = PitchTuning(tune, diagnostics)}};
    const Token* unit      = nullptr;
    const Token* speed     = nullptr;
    bool         saw_meter = false;
    for (const auto& token : tune.header) {
        if (const auto* field = std::get_if<Field>(&token.value)) {
            switch (field->kind) {
            case 'Q': speed = &token; break;
            case 'L': unit = &token; break;
            case 'M': saw_meter = true; [[fallthrough]];
            default: header_field(header, *field, token, diagnostics); break;
            }
        }
    }
    if (!saw_meter) diagnostics.warn(tune.key, "default-meter", "no M: field; using 4/4");
    finish_header(header, unit, speed, diagnostics);
    return header;
}

struct Voice {
    std::string_view id;
    std::string_view name;
    vec<Token>       music;
    VoiceLyrics      lyrics;
    Location         last_part{};
};

class Voices {
public:
    Voices(const Tune& tune, TuneSettings& settings, bool ordered, Diagnostics& diagnostics);
    [[nodiscard]] const auto& all() const { return voices; }
    [[nodiscard]] const auto& preamble() const { return common; }
    [[nodiscard]] bool        declares_voices() const { return !voices.front().id.empty(); }

private:
    Diagnostics&         diagnostics;
    vec<Voice>           voices;
    vec<Token>           common;
    size_t               selected = 0;
    std::optional<Token> current_part;
    size_t               lyric_source = 0;


    size_t find_voice(std::string_view id, Location at);
    bool   select(const Token& token);
    void   header(const Token& token, TuneSettings& settings);
    void   header_midi(const Token& token, std::string_view command, TuneSettings& settings);
    void   body(const Token& token, bool ordered, AccidentalPolicy& accidentals);
    bool   lyric(Field field, Location at);
    void   music_line(const Token& token);
    void   append(const Token& token);
};

size_t Voices::find_voice(std::string_view id, Location at) {
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
    voices.push_back({id, {}, {}});
    return voices.size() - 1;
}

bool Voices::select(const Token& token) {
    const auto* field = std::get_if<Field>(&token.value);
    if (!field || field->kind != 'V') return false;
    const auto spec = voice_spec(field->value, diagnostics, token.at);
    selected        = find_voice(spec.id, token.at);
    if (!spec.name.empty()) voices[selected].name = spec.name;
    append(token);
    return true;
}

void Voices::append(const Token& token) {
    auto& voice = voices[selected];
    if (current_part && voice.last_part != current_part->at) {
        voice.music.push_back(*current_part);
        voice.last_part = current_part->at;
    }
    voice.music.push_back(token);
}

void Voices::header(const Token& token, TuneSettings& settings) {
    if (select(token)) return;
    const auto information = token.information();
    if (!information) {
        if (const auto* field = std::get_if<Field>(&token.value); field && field->kind == 'U')
            common.push_back(token);
        return;
    }
    auto command = *information;
    if (word(command) != "MIDI") {
        common.push_back(token);
        return;
    }
    header_midi(token, command, settings);
}

void Voices::header_midi(const Token& token, std::string_view command, TuneSettings& settings) {
    const auto name = word(command);
    if (voices.front().id.empty()
        && midi_transposition(name, command, settings.midi_shift, diagnostics, token.at))
        return;
    append(token);
}

void Voices::music_line(const Token& token) {
    if (!token.music_line) return;
    if (std::holds_alternative<Atom>(token.value) || std::holds_alternative<Bar>(token.value))
        lyric_source = token.music_line;
    else if (lyric_source != token.music_line)
        lyric_source = 0;
}

bool Voices::lyric(Field field, Location at) {
    if (field.kind != 'w' && field.kind != '+') return false;
    auto&      lyrics   = voices[selected].lyrics;
    const auto existing = lyrics.find(lyric_source);
    if (field.kind == '+' && existing == lyrics.end()) return false;

    const bool extension = field.kind == '+' || field.value.starts_with('+');
    if (!lyric_source || (extension && existing == lyrics.end())) {
        diagnostics.warn(
            at, "orphan-lyrics", "ignored lyrics without a matching music line or verse"
        );
    } else if (extension) {
        auto& verse = existing->second.back().text;
        verse.push_back(' ');
        verse.append(field.kind == 'w' ? trim(field.value.substr(1)) : field.value);
    } else {
        lyrics[lyric_source].push_back({at, std::string(field.value)});
    }
    return true;
}

void Voices::body(const Token& token, bool ordered, AccidentalPolicy& accidentals) {
    if (select(token)) {
        lyric_source = 0;
        return;
    }
    const auto* field = std::get_if<Field>(&token.value);
    if (field && field->kind == 'P') {
        current_part = token;
        if (ordered) lyric_source = 0;
        return;
    }
    music_line(token);
    if (accidentals.consume(token, diagnostics)) return;
    if (!field || !lyric(*field, token.at)) append(token);
}

Voices::Voices(const Tune& tune, TuneSettings& settings, bool ordered, Diagnostics& diagnostics) :
    diagnostics(diagnostics) {
    voices.push_back({});
    for (const auto& token : tune.header)
        if (!settings.accidentals.consume(token, diagnostics)) header(token, settings);
    // A header K: transpose follows the global MIDI preamble. Body K:/V:
    // shifts, by contrast, are local to the current written voice.
    if (settings.key.transpose) settings.midi_shift = *settings.key.transpose;
    settings.key.transpose.reset();
    selected = 0;
    for (const auto& token : tune.body) body(token, ordered, settings.accidentals);
}

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
    all.needs_conductor = all.needs_conductor || voice.needs_conductor;
    const auto append   = [](auto& into, auto& from) {
        into.insert(
            into.end(), std::make_move_iterator(from.begin()), std::make_move_iterator(from.end())
        );
    };
    append(all.tracks, voice.tracks);
    append(all.tempos, voice.tempos);
    append(all.meters, voice.meters);
    append(all.keys, voice.keys);
    append(all.markers, voice.markers);
}

ScoreNative<Tick> score_from_tune(const Tune& tune, Diagnostics& diagnostics) {
    auto                header   = read_header(tune, diagnostics);
    auto&               settings = header.settings;
    const Voices        voices(tune, settings, !header.order.empty(), diagnostics);
    const PartItinerary itinerary(header.order, header.order_at, diagnostics);
    Performance         music;
    DrumMap             drum_map;
    for (const auto& voice : voices.all()) {
        const auto order = itinerary.arrange(voice.music);
        auto name = voice.name.empty() ? (music.tracks.empty() ? settings.title : std::string{}) :
                                         unquote_text(voice.name);
        merge(
            music,
            perform(
                settings,
                voices.preamble(),
                order,
                voice.lyrics,
                std::move(name),
                drum_map,
                diagnostics
            )
        );
    }
    ScoreNative<Tick> score(tpq);
    const auto [numerator, denominator, free] = settings.meter;
    const auto initial_meter
        = TimeSignature<Tick>(0, static_cast<u8>(numerator), static_cast<u8>(denominator));
    const auto initial_key = KeySignature<Tick>(
        0, static_cast<i8>(settings.key.fifths), static_cast<i8>(settings.key.minor)
    );
    score.time_signatures
        = global_events(initial_meter, music.meters, diagnostics, "meter changes");
    score.key_signatures = global_events(initial_key, music.keys, diagnostics, "key changes");
    const int tempo_offset
        = PlaybackGrid::tempo_offset(music.needs_conductor || voices.declares_voices());
    for (auto& [tempo, at] : music.tempos)
        tempo.time = tick(static_cast<long double>(tempo.time) + tempo_offset, diagnostics, at);
    score.tempos
        = global_events(Tempo<Tick>(0, settings.tempo), music.tempos, diagnostics, "tempo changes");
    score.tracks  = std::move(music.tracks);
    score.markers = std::move(music.markers);
    return score;
}

}   // namespace
}   // namespace detail

Result<Score<Tick>> read(std::string_view text, std::string_view source) {
    detail::Diagnostics diagnostics(source);
    const auto          tune = detail::tokenize(text, diagnostics);
    return {to_shared(detail::score_from_tune(tune, diagnostics)), diagnostics.take()};
}

}   // namespace symusic::abc

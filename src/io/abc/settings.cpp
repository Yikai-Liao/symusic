#include "settings.h"

#include <bit>
#include <cmath>
#include <limits>

namespace symusic::abc::detail {
namespace {

using namespace std::literals;

constexpr std::string_view sharp_order = "FCGDAEB";
constexpr std::string_view flat_order  = "BEADGCF";
constexpr std::array       major_names{
    "Cb", "Gb", "Db", "Ab", "Eb", "Bb", "F", "C", "G", "D", "A", "E", "B", "F#", "C#"
};
constexpr std::array minor_names{
    "Abm", "Ebm", "Bbm", "Fm", "Cm", "Gm", "Dm", "Am", "Em", "Bm", "F#m", "C#m", "G#m", "D#m", "A#m"
};
constexpr std::array<int, 7> tonic_fifths{3, 5, 0, 2, 4, -1, 1};

std::string ascii_lower(std::string_view value) {
    std::string result(value);
    for (auto& c : result) {
        if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
    }
    return result;
}

struct Mode {
    std::string_view prefix;
    int              shift;
    int              minor;
};
constexpr std::array modes{
    Mode{"maj", 0, 0},
    Mode{"ion", 0, 0},
    Mode{"min", -3, 1},
    Mode{"aeo", -3, 1},
    Mode{"dor", -2, 0},
    Mode{"phr", -4, 0},
    Mode{"lyd", 1, 0},
    Mode{"mix", -1, 0},
    Mode{"loc", -5, 0},
    Mode{"m", -3, 1},
};

const Mode* mode_of(std::string_view name) {
    const auto value = ascii_lower(name);
    for (const auto& mode : modes) {
        if (mode.prefix == "m" ? value == "m" : value.starts_with(mode.prefix)) return &mode;
    }
    return nullptr;
}

void set_signature(Key& key, int fifths, int minor = 0) {
    key.fifths = fifths;
    key.minor  = minor;
    key.alterations.fill({});
    const auto order = fifths < 0 ? flat_order : sharp_order;
    for (const auto c : order.substr(0, static_cast<size_t>(std::abs(fifths)))) {
        key.alterations[letter_index(c)] = Alteration{fifths < 0 ? -1 : 1};
    }
}

std::optional<int> tonic(std::string_view& name) {
    if (name.empty() || !note_letter(name.front())) return {};
    int fifths = tonic_fifths[letter_index(upper(name.front()))];
    name.remove_prefix(1);
    if (name.starts_with('#') || name.starts_with('b')) {
        fifths += name.front() == '#' ? 7 : -7;
        name.remove_prefix(1);
    }
    return fifths;
}

const Mode* take_mode(std::string_view& options) {
    auto        remaining = options;
    const auto* mode      = mode_of(word(remaining));
    if (mode) options = remaining;
    return mode;
}

int supported_signature(int fifths) {
    if (fifths > 7) return fifths - 12;
    if (fifths < -7) return fifths + 12;
    return fifths;
}

bool parse_named_signature(
    std::string_view  name,
    std::string_view& options,
    Key&              result,
    Diagnostics&      diagnostics,
    Location          at
) {
    // abc2midi can recover the tonic of an uppercase key with an unknown suffix.
    const bool can_recover = !name.empty() && !lower(name.front());
    const auto fifths      = tonic(name);
    if (!fifths) return false;
    const auto* mode = name.empty() ? take_mode(options) : mode_of(name);
    if (!mode && !name.empty()) {
        if (!can_recover) return false;
        diagnostics.warn(
            at, "unknown-key-mode", fmt::format("ignored key mode '{}'; used the major key", name)
        );
    }
    constexpr Mode major{"", 0, 0};
    const auto&    effective = mode ? *mode : major;
    set_signature(result, supported_signature(*fifths + effective.shift), effective.minor);
    return true;
}

bool parse_signature(
    std::string_view& options, Key& result, Diagnostics& diagnostics, Location at
) {
    const auto name    = word(options);
    const auto special = ascii_lower(name);
    if (name.empty() || special == "none") {
        set_signature(result, 0);
        return true;
    }
    if (special == "hp") {
        set_signature(result, 2);
        // HP omits the printed signature but uses the same pipe scale as Hp.
        if (name == "HP") result.fifths = 0;
        return true;
    }
    return parse_named_signature(name, options, result, diagnostics, at);
}

using WrittenAccidentals = std::array<std::optional<Alteration>, 7>;

bool custom_accidental(
    std::string_view word, WrittenAccidentals& overrides, Diagnostics& diagnostics, Location at
) {
    if (word.empty() || std::string_view("^_=").find(word.front()) == word.npos) return false;
    if (word.size() < 2 || !note_letter(word.back())) {
        diagnostics.warn(
            at, "invalid-accidental", fmt::format("key accidental '{}' needs a note A to G", word)
        );
        return true;
    }
    if (const auto value = accidental(word.substr(0, word.size() - 1), at, diagnostics))
        overrides[letter_index(upper(word.back()))] = *value;
    return true;
}

int shift_value(
    std::string_view text, Diagnostics& diagnostics, Location at, std::string_view option, int limit
) {
    const auto shift = integer(text);
    if (!shift || *shift < -limit || *shift > limit) {
        diagnostics.error(
            at,
            "invalid-transposition",
            fmt::format("{} must be between {} and {}", option, -limit, limit)
        );
    }
    return *shift;
}

std::optional<int> clef_octave(std::string_view value) {
    // ABC 2.1 section 4.6: the suffix sounds an octave away without changing the written note.
    const auto shift = value.ends_with("+8") ? 1 : value.ends_with("-8") ? -1 : 0;
    if (shift) value.remove_suffix(2);
    if (!value.empty() && value.back() >= '1' && value.back() <= '5') value.remove_suffix(1);
    constexpr std::array names{"treble"sv, "alto"sv, "tenor"sv, "bass"sv, "perc"sv, "none"sv};
    if (std::ranges::find(names, ascii_lower(value)) == names.end()) return {};
    return shift;
}

enum class PitchOption { unknown, clef, octave, transpose, engraving };

PitchOption pitch_option(std::string_view name) {
    const auto lower = ascii_lower(name);
    if (lower == "clef") return PitchOption::clef;
    if (lower == "octave") return PitchOption::octave;
    if (lower == "transpose" || lower == "t") return PitchOption::transpose;
    if (lower == "middle" || lower == "m" || lower == "stafflines") return PitchOption::engraving;
    return PitchOption::unknown;
}

void read_pitch_option(
    PitchChange&     pitch,
    std::string_view name,
    std::string_view value,
    Diagnostics&     diagnostics,
    Location         at
) {
    switch (pitch_option(name)) {
    case PitchOption::clef:
        if (const auto shift = clef_octave(value)) pitch.clef_shift = shift;
        break;
    case PitchOption::octave:
        pitch.written_octave = shift_value(value, diagnostics, at, name, 16);
        break;
    case PitchOption::transpose:
        pitch.transpose = shift_value(value, diagnostics, at, name, 127);
        break;
    case PitchOption::engraving: break;
    case PitchOption::unknown:
        if (value.empty()) {
            if (const auto shift = clef_octave(name)) pitch.clef_shift = shift;
        }
        break;
    }
}

std::string_view option_name(std::string_view text) {
    return text.substr(0, text.find_first_of("= \t\r\f\v"));
}

std::string_view quoted_option(std::string_view& options) {
    options.remove_prefix(1);
    size_t end = 0;
    while (end < options.size() && options[end] != '"') {
        end += options[end] == '\\' && end + 1 < options.size() ? 2 : 1;
    }
    const auto value = options.substr(0, end);
    options.remove_prefix(std::min(end + 1, options.size()));
    return value;
}

std::pair<std::string_view, std::string_view> option(std::string_view& options) {
    options = trim(options);
    if (options.find_first_of("=^_") == 0) return {word(options), {}};
    const auto name = option_name(options);
    const auto tail = options.substr(name.size());
    options         = trim(tail);
    if (!options.starts_with('=')
        || (!tail.starts_with('=') && (clef_octave(name) || ascii_lower(name) == "exp"))) {
        // A natural after a bare clef name or exp belongs to the key signature.
        return {name, {}};
    }
    options = trim(options.substr(1));
    return {name, options.starts_with('"') ? quoted_option(options) : word(options)};
}

// Explicit mode replaces the implied scale, not the key signature metadata.
// Accidentals anywhere in the field override the chosen scale in written order.
struct KeyAccidentals {
    bool               explicit_scale = false;
    WrittenAccidentals overrides;

    [[nodiscard]] bool sets_scale() const {
        return explicit_scale || std::ranges::any_of(overrides, [](const auto& value) {
                   return value.has_value();
               });
    }

    void apply(Key& key) const {
        if (explicit_scale) key.alterations.fill({});
        for (size_t i = 0; i < overrides.size(); ++i) {
            if (const auto& alteration = overrides[i]) key.alterations[i] = *alteration;
        }
    }
};

struct KeyOptions {
    PitchChange    pitch;
    KeyAccidentals accidentals;
};

KeyOptions key_options(std::string_view options, Diagnostics& diagnostics, Location at) {
    KeyOptions result;
    while (!trim(options).empty()) {
        const auto [name, value] = option(options);
        if (ascii_lower(name) == "exp")
            result.accidentals.explicit_scale = true;
        else if (!custom_accidental(name, result.accidentals.overrides, diagnostics, at))
            read_pitch_option(result.pitch, name, value, diagnostics, at);
    }
    return result;
}

int named_tempo(std::string_view label) {
    // Conventional text tempi use abc2midi's quarter-note metronome rates.
    struct NamedTempo {
        std::string_view name;
        int              bpm;
    };
    static constexpr NamedTempo names[]{
        {"larghissimo", 40},
        {"adagissimo", 44},
        {"lentissimo", 48},
        {"largo", 56},
        {"adagio", 59},
        {"lento", 62},
        {"larghetto", 66},
        {"adagietto", 76},
        {"andante", 88},
        {"andantino", 96},
        {"moderato", 104},
        {"allegretto", 112},
        {"allegro", 120},
        {"vivace", 168},
        {"vivo", 180},
        {"presto", 192},
        {"allegrissimo", 208},
        {"vivacissimo", 220},
        {"prestissimo", 240}
    };
    const auto found = std::ranges::find(names, ascii_lower(label), &NamedTempo::name);
    return found == std::end(names) ? 0 : found->bpm;
}

}   // namespace

void Key::apply(const PitchChange& change) {
    if (change.transpose) transpose = change.transpose;
    if (change.written_octave) written_octave = *change.written_octave;
    if (change.clef_shift) clef_shift = *change.clef_shift;
}

bool AccidentalPolicy::consume(const Token& token, Diagnostics& diagnostics) {
    const auto info = token.information();
    if (!info) return false;
    auto args = *info;
    auto name = word(args);
    if (name == "MIDI") {
        name = word(args);
        if (name == "barlines")
            args = "pitch";
        else if (name == "nobarlines")
            args = "not";
        else
            return false;
    } else if (name != "propagate-accidentals")
        return false;
    auto       scope = AccidentalScope::pitch;
    const auto value = trim(args);
    if (value == "octave")
        scope = AccidentalScope::octave;
    else if (value == "not")
        scope = AccidentalScope::none;
    else if (value != "pitch") {
        diagnostics.warn(
            token.at,
            "invalid-accidental-scope",
            "expected 'pitch', 'octave' or 'not' for propagate-accidentals"
        );
        return true;
    }
    changes.emplace_back(token.at, scope);
    return true;
}

AccidentalScope AccidentalPolicy::at(Location written) const {
    const auto next
        = std::ranges::upper_bound(changes, written, {}, &decltype(changes)::value_type::first);
    return next == changes.begin() ? AccidentalScope::pitch : std::prev(next)->second;
}

std::optional<DrumRoute> DrumMap::read(
    std::string_view args, Location at, Diagnostics& diagnostics
) {
    const auto source = drum_spelling(word(args), at, diagnostics);
    if (!source) return {};
    const auto alteration = source->accidental.value_or(Alteration{});
    const auto note       = 60 + 12 * source->octave + natural_pitch[letter_index(source->letter)]
                            + alteration.numerator;
    if (note < 0 || note > midi_max || alteration.denominator != 0)
        diagnostics.error(
            at, "invalid-drummap", "drummap source must be a MIDI pitch without a microtone"
        );
    const auto destination = midi_integer(args, 1, midi_max, diagnostics, at);
    if (destination < 35 || destination > 81)
        diagnostics.warn(
            at, "drummap-range", "destination is outside General MIDI percussion (35–81)"
        );
    return DrumRoute{note, destination};
}

bool DrumMap::consume(const Token& token, Diagnostics& diagnostics) {
    auto command = token.information().value_or("");
    if (word(command) != "MIDI" || word(command) != "drummap") return false;
    if (const auto route = read(command, token.at, diagnostics)) apply(*route);
    return true;
}

bool ProgramNumbers::consume(const Token& token, Location reference, Diagnostics& diagnostics) {
    auto args = token.information().value_or("");
    if (word(args) != "MIDI" || word(args) != "programbase") return false;
    if (token.at < reference) {
        diagnostics.warn(
            token.at,
            "program-base-placement",
            "ignored programbase before X:; place it inside the tune"
        );
    } else {
        base = midi_integer(args, 0, std::numeric_limits<int>::max(), diagnostics, token.at) != 0;
    }
    return true;
}

int ProgramNumbers::read(std::string_view& args, Diagnostics& diagnostics, Location at) const {
    return midi(midi_integer(args, 0, midi_max + base.value_or(0), diagnostics, at));
}

int ProgramNumbers::read_chord(
    std::string_view& args, Diagnostics& diagnostics, Location at
) const {
    if (base || !one_based_chords) return read(args, diagnostics, at);
    return midi_integer(args, 1, midi_max + 1, diagnostics, at) - 1;
}

int midi_integer(std::string_view& args, int low, int high, Diagnostics& diagnostics, Location at) {
    const auto number = integer(word(args));
    if (number && *number >= low && *number <= high) return *number;
    diagnostics.error(
        at, "invalid-midi-value", fmt::format("expected a MIDI value between {} and {}", low, high)
    );
}

int MidiTransposition::apply(int shift) const {
    return relative ? std::clamp(shift + amount, -127, 127) : amount;
}

void MidiTrackSettings::apply(const MidiTrackCommand& command) {
    if (const auto* change = std::get_if<MidiTransposition>(&command)) {
        shift = change->apply(shift);
    } else {
        const auto& spacing                                 = std::get<ChordAttackChange>(command);
        (spacing.random ? attack.random_limit : attack.gap) = spacing.ticks;
    }
}

std::optional<MidiTrackCommand> midi_track_command(const Token& token, Diagnostics& diagnostics) {
    auto args = token.information().value_or("");
    if (word(args) != "MIDI") return {};
    const auto name = word(args);
    if (name == "chordattack" || name == "randomchordattack")
        return ChordAttackChange{
            midi_integer(args, 0, tpq, diagnostics, token.at), name == "randomchordattack"
        };
    if (name != "transpose" && name != "rtranspose") return {};
    return MidiTransposition{
        shift_value(word(args), diagnostics, token.at, name, 127), name == "rtranspose"
    };
}

bool Meter::default_accent(long double position) const {
    constexpr auto tolerance = 0.0000001L;
    if (group_starts.none()) {
        const auto step = numerator % 3 == 0 ? 3 : numerator % 2 == 0 ? 2 : numerator;
        return std::abs(std::remainder(position, step * beat())) < tolerance;
    }
    // ABC 2.1 section 3.1.6: accent each written group, including when bar lines are omitted.
    const auto offset = std::fmod(position, measure());
    const auto pulse  = static_cast<int>(std::round(offset / beat()));
    if (std::abs(offset - pulse * beat()) >= tolerance) return false;
    return pulse == 0 || pulse == numerator || group_starts.test(pulse);
}

Length ratio(
    std::string_view text, Diagnostics& diagnostics, Location at, std::string_view context
) {
    const auto slash     = text.find('/');
    const auto numerator = integer(trim(text.substr(0, slash)));
    const auto denominator
        = slash == text.npos ? std::optional(1) : integer(trim(text.substr(slash + 1)));
    if (!numerator || *numerator < 0 || !denominator || *denominator <= 0) {
        diagnostics.error(
            at,
            "invalid-fraction",
            fmt::format("{} needs a nonnegative fraction, got '{}'", context, text)
        );
    }
    return {*numerator, *denominator};
}

long double unit_length(std::string_view text, Diagnostics& diagnostics, Location at) {
    const auto value = ratio(text, diagnostics, at, "unit note length");
    if (value.numerator == 0)
        diagnostics.error(at, "invalid-unit-length", "L: needs a positive unit note length");
    return whole * value.fraction();
}

Meter meter(std::string_view text, Diagnostics& diagnostics, Location at) {
    if (text == "C") return {};
    if (text == "C|") return {2, 2};
    if (ascii_lower(text) == "none") return {4, 4, true};
    const auto written = meter_numbers(text);
    if (!written) diagnostics.error(at, "invalid-meter", "expected a meter such as 4/4, C or none");
    const auto denominator = integer(written->denominator);
    if (!denominator || *denominator < 1 || *denominator > 128
        || !std::has_single_bit(static_cast<unsigned>(*denominator))) {
        diagnostics.error(
            at, "invalid-meter", "meter denominator must be a power of two between 1 and 128"
        );
    }
    Meter result{.numerator = 0, .denominator = *denominator};
    for (const auto group : written->groups) {
        const auto beats = integer(group);
        if (!beats || *beats < 1 || *beats > 255 - result.numerator)
            diagnostics.error(at, "invalid-meter", "meter numerator must be between 1 and 255");
        if (result.numerator) result.group_starts.set(result.numerator);
        result.numerator += *beats;
    }
    return result;
}

std::optional<KeyField> key(
    std::string_view text, const Key& previous, Diagnostics& diagnostics, Location at, bool header
) {
    text               = trim(text);
    Key        result  = previous;
    auto       options = text;
    const auto first   = option_name(text);
    // In first position, bare none is the key signature, not a clef change.
    const bool options_only = pitch_option(first) != PitchOption::unknown
                              || (clef_octave(first) && ascii_lower(first) != "none");
    if (!options_only && !parse_signature(options, result, diagnostics, at)) {
        if (header)
            diagnostics.error(at, "invalid-key", "expected a key signature such as C, F#m or none");
        diagnostics.warn(at, "invalid-inline-key", "ignored an unrecognized key change");
        return {};
    }
    const auto settings = key_options(options, diagnostics, at);
    settings.accidentals.apply(result);
    return KeyField{result, !options_only || settings.accidentals.sets_scale(), settings.pitch};
}

i32 tempo(const TempoNotation& written, long double unit, Diagnostics& diagnostics, Location at) {
    if (!written.bpm) {
        if (const auto bpm = named_tempo(written.label)) return 60'000'000 / bpm;
        diagnostics.warn(
            at, "text-tempo", "tempo text has no recognized metronome value; kept the current tempo"
        );
        return 0;
    }
    const auto beat = written.beat * (written.unit_relative ? unit : whole);
    const auto usec = 60'000'000.L * tpq / (beat * *written.bpm);
    if (usec < 1 || usec > std::numeric_limits<i32>::max())
        diagnostics.error(at, "tempo-range", "tempo is outside the score's range");
    return static_cast<i32>(usec);
}

VoiceSpec voice_spec(std::string_view text, Diagnostics& diagnostics, Location at) {
    VoiceSpec result{};
    result.id = word(text);
    if (result.id.empty()) diagnostics.error(at, "invalid-voice", "V: needs a voice identifier");
    while (!trim(text).empty()) {
        const auto [name, value] = option(text);
        if (name == "name" || name == "nm")
            result.name = value;
        else
            read_pitch_option(result.pitch, name, value, diagnostics, at);
    }
    return result;
}

std::string_view key_name(int fifths, bool minor) {
    return (minor ? minor_names : major_names).at(static_cast<size_t>(fifths + 7));
}

i32 tick(long double time, Diagnostics& diagnostics, Location at) {
    constexpr long double epsilon   = 0.0000001L;
    const auto            quantized = std::floor(time + epsilon);
    if (!(quantized >= 0 && quantized <= std::numeric_limits<i32>::max())) {
        diagnostics.error(at, "time-range", "playback exceeds the score's tick range");
    }
    return static_cast<i32>(quantized);
}

}   // namespace symusic::abc::detail

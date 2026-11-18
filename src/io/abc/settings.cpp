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
    Mode{"exp", 0, 0},
    Mode{"m", -3, 1},
};

const Mode* mode_of(std::string_view name) {
    const auto value = ascii_lower(name);
    for (const auto& mode : modes) {
        if (mode.prefix == "m" ? value == "m" : value.starts_with(mode.prefix)) return &mode;
    }
    return nullptr;
}

void set_signature(Key& key, int fifths) {
    key.fifths = fifths;
    key.alterations.fill(0);
    const auto order = fifths < 0 ? flat_order : sharp_order;
    for (const auto c : order.substr(0, static_cast<size_t>(std::abs(fifths)))) {
        key.alterations[letter_index(c)] = fifths < 0 ? -1 : 1;
    }
}

int tonic(std::string_view& name) {
    if (name.empty() || !note_letter(name.front())) return 100;
    int fifths = tonic_fifths[letter_index(upper(name.front()))];
    name.remove_prefix(1);
    if (name.starts_with('#') || name.starts_with('b')) {
        fifths += name.front() == '#' ? 7 : -7;
        name.remove_prefix(1);
    }
    return fifths;
}

bool bagpipe(std::string_view name, Key& result) {
    if (name != "HP" && name != "Hp") return false;
    if (name == "Hp") {
        result.alterations[letter_index('F')] = 1;
        result.alterations[letter_index('C')] = 1;
    }
    return true;
}

bool parse_signature(std::string_view& options, Key& result) {
    auto name = word(options);
    if (ascii_lower(name) == "none" || bagpipe(name, result)) return true;
    const auto fifths = tonic(name);
    if (fifths == 100) return false;
    auto        remaining = options;
    const auto* mode      = name.empty() ? mode_of(word(remaining)) : mode_of(name);
    if (!name.empty() && !mode) return false;
    if (name.empty() && mode) options = remaining;
    int signature = fifths + (mode ? mode->shift : 0);
    if (signature > 7) signature -= 12;
    if (signature < -7) signature += 12;
    set_signature(result, mode && mode->prefix == "exp" ? 0 : signature);
    result.minor = mode ? mode->minor : 0;
    return true;
}

bool custom_accidental(std::string_view word, Key& key) {
    if (word.size() < 2 || !note_letter(word.back())) return false;
    const auto           prefix = word.substr(0, word.size() - 1);
    constexpr std::array tokens{"__"sv, "_"sv, "="sv, "^"sv, "^^"sv};
    const auto           pos = std::ranges::find(tokens, prefix);
    if (pos == tokens.end()) return false;
    key.alterations[letter_index(upper(word.back()))] = static_cast<int>(pos - tokens.begin()) - 2;
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

// A clef is engraving-only except for explicit sounding octave shifts.
std::optional<int> clef_octave(std::string_view value) {
    if (value.ends_with("+8")) return 1;
    if (value.ends_with("-8")) return -1;
    return {};
}

std::pair<std::string_view, std::string_view> option(std::string_view& options) {
    options = trim(options);
    if (options.starts_with('=') || options.starts_with('^') || options.starts_with('_'))
        return {word(options), {}};
    const auto key = options.substr(0, options.find_first_of("= 	"));
    options.remove_prefix(key.size());
    options = trim(options);
    if (!options.starts_with('=')) return {key, {}};
    options = trim(options.substr(1));
    if (!options.starts_with('"')) return {key, word(options)};
    options.remove_prefix(1);
    size_t end = 0;
    while (end < options.size() && options[end] != '"') {
        end += options[end] == '\\' && end + 1 < options.size() ? 2 : 1;
    }
    const auto value = options.substr(0, end);
    options.remove_prefix(std::min(end + 1, options.size()));
    return {key, value};
}

void key_options(std::string_view options, Key& result, Diagnostics& diagnostics, Location at) {
    std::optional<int> from_clef;
    std::optional<int> octave;
    while (!trim(options).empty()) {
        const auto [name, value] = option(options);
        if (name == "transpose")
            result.transpose = shift_value(value, diagnostics, at, name, 127);
        else if (name == "octave")
            octave = shift_value(value, diagnostics, at, name, 16);
        else if (name == "clef")
            from_clef = clef_octave(value);
        else if (!custom_accidental(name, result) && value.empty()) {
            if (auto oct = clef_octave(name)) from_clef = oct;
        }
    }
    if (octave || from_clef) result.octave = octave.value_or(from_clef.value_or(0));
}

std::string_view tempo_label(std::string_view& text) {
    text = trim(text);
    if (!text.starts_with('"')) return {};
    const auto end = text.find('"', 1);
    if (end == text.npos) {
        text = {};
        return {};
    }
    const auto label = text.substr(1, end - 1);
    text             = trim(text.substr(end + 1));
    return label;
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

long double tempo_beat(
    std::string_view text, long double unit, Diagnostics& diagnostics, Location at
) {
    if (text.starts_with('C')) {
        text = trim(text.substr(1));
        return text.empty() ? unit : unit * ratio(text, diagnostics, at, "tempo beat").fraction();
    }
    long double beats = 0;
    while (!trim(text).empty())
        beats += ratio(word(text), diagnostics, at, "tempo beat").fraction();
    return whole * beats;
}

}   // namespace

bool midi_transposition(
    std::string_view name, std::string_view args, int& shift, Diagnostics& diagnostics, Location at
) {
    if (name != "transpose" && name != "rtranspose") return false;
    const auto amount = shift_value(word(args), diagnostics, at, name, 127);
    shift             = name == "transpose" ? amount : std::clamp(shift + amount, -127, 127);
    return true;
}

int Meter::accent_beats() const {
    if (compound()) return 3;
    if (numerator == 4) return 2;
    if (numerator == 7 && denominator == 8) return 6;
    return numerator;
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

namespace {

int meter_numerator(std::string_view numerator, Diagnostics& diagnostics, Location at) {
    numerator = trim(numerator);
    if (numerator.starts_with('(') && numerator.ends_with(')'))
        numerator = numerator.substr(1, numerator.size() - 2);
    int beats = 0;
    while (!numerator.empty()) {
        const auto plus = numerator.find('+');
        const auto part = integer(trim(numerator.substr(0, plus)));
        if (!part || *part < 1 || *part > 255 - beats)
            diagnostics.error(at, "invalid-meter", "meter numerator must be between 1 and 255");
        beats += *part;
        if (plus == numerator.npos) break;
        numerator.remove_prefix(plus + 1);
    }
    return beats;
}

}   // namespace

Meter meter(std::string_view text, Diagnostics& diagnostics, Location at) {
    if (text == "C") return {};
    if (text == "C|") return {2, 2};
    if (ascii_lower(text) == "none") return {4, 4, true};
    const auto slash = text.find('/');
    if (slash == text.npos)
        diagnostics.error(at, "invalid-meter", "expected a meter such as 4/4, C or none");
    const auto beats       = meter_numerator(text.substr(0, slash), diagnostics, at);
    const auto denominator = integer(trim(text.substr(slash + 1)));
    if (!beats || !denominator || *denominator < 1 || *denominator > 128
        || !std::has_single_bit(static_cast<unsigned>(*denominator))) {
        diagnostics.error(
            at, "invalid-meter", "meter denominator must be a power of two between 1 and 128"
        );
    }
    return {beats, *denominator};
}

std::optional<Key> key(
    std::string_view text, const Key& previous, Diagnostics& diagnostics, Location at, bool header
) {
    Key  result  = previous;
    auto options = text;
    result.alterations.fill(0);
    result.fifths = result.minor = 0;
    if (!parse_signature(options, result)) {
        const auto first = text.substr(0, text.find_first_of("= \t"));
        if (header || text.empty())
            diagnostics.error(at, "invalid-key", "expected a key signature such as C, F#m or none");
        result = previous;
        if (first != "clef" && first != "octave" && first != "transpose") {
            diagnostics.warn(at, "invalid-inline-key", "ignored an unrecognized key change");
            return {};
        }
        options = text;
    }
    key_options(options, result, diagnostics, at);
    return result;
}

i32 tempo(std::string_view text, long double unit, Diagnostics& diagnostics, Location at) {
    const auto label = tempo_label(text);
    text             = trim(text.substr(0, text.find('"')));
    if (text.empty()) {
        if (const auto bpm = named_tempo(label)) return 60'000'000 / bpm;
        diagnostics.warn(
            at, "text-tempo", "tempo text has no recognized metronome value; kept the current tempo"
        );
        return 0;
    }
    const auto eq = text.find('=');
    const auto beat
        = eq == text.npos ? tpq : tempo_beat(trim(text.substr(0, eq)), unit, diagnostics, at);
    const auto value = integer(trim(eq == text.npos ? text : text.substr(eq + 1)));
    if (!value || *value <= 0 || beat <= 0)
        diagnostics.error(at, "invalid-tempo", "tempo needs a positive metronome value");
    const auto usec = 60'000'000.L * tpq / (beat * *value);
    if (usec < 1 || usec > std::numeric_limits<i32>::max())
        diagnostics.error(at, "tempo-range", "tempo is outside the score's range");
    return static_cast<i32>(usec);
}

VoiceSpec voice_spec(std::string_view text, Diagnostics& diagnostics, Location at) {
    VoiceSpec result{};
    result.id = word(text);
    if (result.id.empty()) diagnostics.error(at, "invalid-voice", "V: needs a voice identifier");
    std::optional<int> octave;
    while (!trim(text).empty()) {
        const auto [name, value] = option(text);
        if (name == "name" || name == "nm")
            result.name = value;
        else if (name == "transpose")
            result.transpose = shift_value(value, diagnostics, at, name, 127);
        else if (name == "octave")
            result.octave = shift_value(value, diagnostics, at, name, 16);
        else if (name == "clef")
            octave = clef_octave(value);
    }
    if (!result.octave) result.octave = octave;
    return result;
}

std::string_view key_name(int fifths, bool minor) {
    return (minor ? minor_names : major_names).at(static_cast<size_t>(fifths + 7));
}

std::string unquote_text(std::string_view text) {
    std::string out;
    out.reserve(text.size());
    for (size_t i = 0; i < text.size(); ++i) {
        if (text[i] == '\\' && i + 1 < text.size()
            && std::string_view("\\\"%").find(text[i + 1]) != text.npos)
            ++i;
        out.push_back(text[i]);
    }
    return out;
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

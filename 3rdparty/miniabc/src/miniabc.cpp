#include "miniabc/miniabc.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <charconv>
#include <cmath>
#include <limits>
#include <numeric>
#include <optional>
#include <sstream>
#include <string_view>
#include <stdexcept>
#include <unordered_map>
#include <cstdlib>

#include <lexy/action/parse.hpp>
#include <lexy/callback.hpp>
#include <lexy/dsl.hpp>
#include <lexy/dsl/code_point.hpp>
#include <lexy/input_location.hpp>
#include <lexy/input/string_input.hpp>

namespace miniabc {

namespace {

using namespace std::string_view_literals;

constexpr Fraction normalize(const Fraction frac) {
    if (frac.den == 0) { throw std::runtime_error("miniabc: zero denominator"); }
    auto num = frac.num;
    auto den = frac.den;
    if (den < 0) {
        num = -num;
        den = -den;
    }
    auto abs_num = std::llabs(num);
    auto abs_den = std::llabs(den);
    auto gcd     = std::gcd(abs_num, abs_den);
    return Fraction{num / gcd, den / gcd};
}

constexpr Fraction make_fraction(const std::int64_t num, const std::int64_t den = 1) {
    return normalize(Fraction{num, den});
}

constexpr Fraction add(const Fraction a, const Fraction b) {
    return normalize(Fraction{a.num * b.den + b.num * a.den, a.den * b.den});
}

constexpr Fraction sub(const Fraction a, const Fraction b) {
    return normalize(Fraction{a.num * b.den - b.num * a.den, a.den * b.den});
}

constexpr Fraction mul(const Fraction a, const Fraction b) {
    return normalize(Fraction{a.num * b.num, a.den * b.den});
}

constexpr Fraction div(const Fraction a, const Fraction b) {
    return normalize(Fraction{a.num * b.den, a.den * b.num});
}

constexpr std::int64_t mul_div(const Fraction frac, const std::int64_t scale) {
    return (frac.num * scale) / frac.den;
}

struct RawLine {
    std::string text;
    std::size_t line = 0;
};

struct FieldLine {
    std::string id;
    std::string value;
    std::size_t line = 0;
};

struct MusicLine {
    std::string text;
    std::size_t line = 0;
};

struct TupletState {
    Fraction ratio{1, 1};
    int      remain = 0;
};

constexpr bool is_alpha(const char c) {
    return static_cast<unsigned char>(c) < 128 && std::isalpha(static_cast<unsigned char>(c)) != 0;
}

constexpr bool is_digit(const char c) {
    return static_cast<unsigned char>(c) < 128 && std::isdigit(static_cast<unsigned char>(c)) != 0;
}

constexpr bool is_space(const char c) {
    return static_cast<unsigned char>(c) < 128 && std::isspace(static_cast<unsigned char>(c)) != 0;
}

constexpr std::string_view trim(std::string_view view) {
    while (!view.empty() && std::isspace(static_cast<unsigned char>(view.front())) != 0) {
        view.remove_prefix(1);
    }
    while (!view.empty() && std::isspace(static_cast<unsigned char>(view.back())) != 0) {
        view.remove_suffix(1);
    }
    return view;
}

std::optional<int> parse_integer(std::string_view text) {
    int value = 0;
    const auto* begin = text.data();
    const auto* end   = text.data() + text.size();
    if (auto [ptr, ec] = std::from_chars(begin, end, value); ec == std::errc{}) {
        if (ptr == end) { return value; }
    }
    return std::nullopt;
}

Fraction parse_fraction(std::string_view text) {
    auto slash = text.find('/');
    if (slash == std::string::npos) {
        if (auto number = parse_integer(trim(text))) { return make_fraction(*number, 1); }
        throw std::runtime_error("miniabc: invalid fraction '" + std::string(text) + "'");
    }
    auto lhs = trim(text.substr(0, slash));
    auto rhs = trim(text.substr(slash + 1));
    auto num = parse_integer(lhs);
    auto den = parse_integer(rhs);
    if (!num || !den || *den == 0) {
        throw std::runtime_error("miniabc: invalid fraction '" + std::string(text) + "'");
    }
    return make_fraction(*num, *den);
}

constexpr std::array<int, 7> NATURAL_PITCH_CLASS = {0, 2, 4, 5, 7, 9, 11};   // C, D, E, F, G, A, B
constexpr std::array<char, 7> LETTERS           = {'C', 'D', 'E', 'F', 'G', 'A', 'B'};

int letter_index(const char letter) {
    const char upper = static_cast<char>(std::toupper(static_cast<unsigned char>(letter)));
    switch (upper) {
        case 'C': return 0;
        case 'D': return 1;
        case 'E': return 2;
        case 'F': return 3;
        case 'G': return 4;
        case 'A': return 5;
        case 'B': return 6;
        default: throw std::runtime_error("miniabc: unsupported pitch letter");
    }
}

Fraction parse_note_length(std::string_view suffix) {
    if (suffix.empty()) { return make_fraction(1, 1); }
    // handle // style
    if (suffix.front() == '/') {
        int slash_count = 0;
        std::size_t idx = 0;
        while (idx < suffix.size() && suffix[idx] == '/') {
            ++slash_count;
            ++idx;
        }
        if (idx == suffix.size()) {
            return make_fraction(1, 1 << slash_count);
        }
        const auto denom = parse_integer(suffix.substr(idx));
        if (!denom || *denom == 0) { throw std::runtime_error("miniabc: invalid length " + std::string(suffix)); }
        return make_fraction(1, *denom * (1 << slash_count));
    }

    std::size_t idx = 0;
    while (idx < suffix.size() && std::isdigit(static_cast<unsigned char>(suffix[idx]))) { ++idx; }

    auto integer_value = parse_integer(suffix.substr(0, idx));
    if (!integer_value) { throw std::runtime_error("miniabc: invalid length " + std::string(suffix)); }
    auto numerator = *integer_value;

    if (idx >= suffix.size() || suffix[idx] != '/') { return make_fraction(numerator, 1); }
    ++idx;
    if (idx == suffix.size()) { return make_fraction(numerator, 2); }
    const auto denom = parse_integer(suffix.substr(idx));
    if (!denom || *denom == 0) { throw std::runtime_error("miniabc: invalid length " + std::string(suffix)); }
    return make_fraction(numerator, *denom);
}

const std::array<char, 7> SHARP_ORDER = {'F', 'C', 'G', 'D', 'A', 'E', 'B'};
const std::array<char, 7> FLAT_ORDER  = {'B', 'E', 'A', 'D', 'G', 'C', 'F'};

std::array<int, 7> build_key_signature(const KeySignature& ksig) {
    std::array<int, 7> table{};
    table.fill(0);
    if (ksig.accidentals == 0) { return table; }
    const bool sharp = ksig.accidentals > 0;
    const auto count = std::min<int>(7, std::abs(ksig.accidentals));
    for (auto idx = 0; idx < count; ++idx) {
        const char letter = sharp ? SHARP_ORDER[idx] : FLAT_ORDER[idx];
        const auto pos    = letter_index(letter);
        table[pos]        = sharp ? 1 : -1;
    }
    return table;
}

struct DiagnosticSink {
    std::vector<Diagnostic> diagnostics;

    void add(std::size_t line, std::size_t column, std::string message) {
        diagnostics.push_back(Diagnostic{line, column, std::move(message)});
    }
};

std::string format_parse_error_message(const std::vector<Diagnostic>& diagnostics) {
    if (diagnostics.empty()) { return "miniabc: parse failure"; }
    const auto& diag = diagnostics.front();
    std::ostringstream oss;
    oss << "miniabc: parse failure at line " << diag.line << ", column " << diag.column << ": " << diag.message;
    return oss.str();
}

}  // namespace

ParseError::ParseError(std::vector<Diagnostic> diagnostics) :
    std::runtime_error(format_parse_error_message(diagnostics)), diagnostics_{std::move(diagnostics)} {}

const char* describe() noexcept { return "miniabc: standalone build helper"; }

std::vector<RawLine> parse_lines(std::string_view input, DiagnosticSink& sink) {
    (void)sink;
    std::vector<RawLine> lines;
    lines.reserve(128);

    std::size_t line_no = 1;
    std::size_t pos     = 0;
    while (pos <= input.size()) {
        const auto next = input.find('\n', pos);
        if (next == std::string_view::npos) {
            auto segment = input.substr(pos);
            lines.push_back(RawLine{std::string(segment), line_no});
            break;
        }
        auto segment = input.substr(pos, next - pos);
        lines.push_back(RawLine{std::string(segment), line_no});
        pos = next + 1;
        ++line_no;
    }
    if (input.empty()) { lines.push_back(RawLine{"", line_no}); }
    return lines;
}

bool looks_like_field(std::string_view line) {
    line = trim(line);
    if (line.size() < 2) { return false; }
    if (!std::isalpha(static_cast<unsigned char>(line.front()))) { return false; }
    return line[1] == ':';
}

std::vector<FieldLine> collect_fields(const std::vector<RawLine>& lines, std::vector<MusicLine>& body) {
    std::vector<FieldLine> fields;
    bool                   in_header = true;
    std::string            pending;
    std::size_t            pending_line = 0;

    auto flush_pending = [&]() {
        if (pending.empty()) { return; }
        body.push_back(MusicLine{pending, pending_line});
        pending.clear();
    };

    for (const auto& line : lines) {
        auto trimmed = trim(line.text);
        if (trimmed.empty()) { continue; }
        if (!trimmed.empty() && trimmed.front() == '%') { continue; }
        const bool is_field = in_header && looks_like_field(trimmed);
        if (is_field) {
            const auto colon = trimmed.find(':');
            FieldLine field{
                std::string(trimmed.substr(0, colon)),
                std::string(trimmed.substr(colon + 1)),
                line.line
            };
            fields.push_back(std::move(field));
        } else {
            in_header = false;
            auto comment = trimmed.find('%');
            if (comment != std::string::npos) { trimmed = trimmed.substr(0, comment); }
            auto content = std::string(trimmed);
            bool continuation = false;
            while (!content.empty() && std::isspace(static_cast<unsigned char>(content.back())) != 0) {
                content.pop_back();
            }
            if (!content.empty() && content.back() == '\\') {
                continuation = true;
                content.pop_back();
            }
            if (!content.empty()) {
                if (pending.empty()) { pending_line = line.line; }
                pending += content;
            }
            if (!continuation) {
                flush_pending();
            }
        }
    }
    flush_pending();
    return fields;
}

Meter parse_meter(const std::string& value) {
    auto trimmed = trim(value);
    if (trimmed.empty() || trimmed == "C"sv) { return Meter{4, 4}; }
    if (trimmed == "C|"sv) { return Meter{2, 2}; }
    auto slash = trimmed.find('/');
    if (slash == std::string::npos) {
        if (auto num = parse_integer(trimmed)) { return Meter{*num, 1}; }
        return Meter{4, 4};
    }
    auto numerator_str = trim(trimmed.substr(0, slash));
    auto denominator_str = trim(trimmed.substr(slash + 1));
    const int numerator = parse_integer(numerator_str).value_or(4);
    int denominator = parse_integer(denominator_str).value_or(4);
    if (denominator == 0) { denominator = 1; }
    return Meter{numerator, denominator};
}

Fraction infer_unit_length(const Meter meter) {
    const auto decimal = static_cast<double>(meter.numerator) / static_cast<double>(meter.denominator);
    if (meter.numerator == 0 || meter.denominator == 0) { return make_fraction(1, 8); }
    if (decimal < 0.75) { return make_fraction(1, 16); }
    return make_fraction(1, 8);
}

KeySignature parse_key(const std::string& value) {
    KeySignature key{};
    auto         trimmed = trim(value);
    if (trimmed.empty()) { return key; }

    std::size_t pos = 0;
    while (pos < trimmed.size() && std::isspace(static_cast<unsigned char>(trimmed[pos])) != 0) { ++pos; }
    if (pos >= trimmed.size() || !std::isalpha(static_cast<unsigned char>(trimmed[pos]))) { return key; }

    std::string root;
    root.push_back(static_cast<char>(std::toupper(static_cast<unsigned char>(trimmed[pos]))));
    ++pos;

    std::string accidental;
    while (pos < trimmed.size() && (trimmed[pos] == 'b' || trimmed[pos] == '#')) {
        accidental.push_back(trimmed[pos]);
        ++pos;
    }

    auto mode_view = trim(trimmed.substr(pos));
    std::string mode;
    mode.reserve(mode_view.size());
    for (char ch : mode_view) { mode.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(ch)))); }
    key.is_minor = (mode == "m" || mode == "min" || mode == "minor");

    const std::unordered_map<std::string, int> major{
        {"C", 0},  {"G", 1},  {"D", 2},  {"A", 3},  {"E", 4},   {"B", 5},   {"F#", 6}, {"C#", 7},
        {"F", -1}, {"Bb", -2}, {"Eb", -3}, {"Ab", -4}, {"Db", -5}, {"Gb", -6}, {"Cb", -7}
    };
    const std::unordered_map<std::string, int> minor{
        {"A", 0},  {"E", 1},  {"B", 2},  {"F#", 3}, {"C#", 4}, {"G#", 5}, {"D#", 6}, {"A#", 7},
        {"D", -1}, {"G", -2}, {"C", -3}, {"F", -4}, {"Bb", -5}, {"Eb", -6}, {"Ab", -7}
    };

    std::string key_name = root + accidental;
    if (key_name.empty()) { key_name = "C"; }

    if (!key.is_minor) {
        auto it = major.find(key_name);
        key.accidentals = it == major.end() ? 0 : it->second;
    } else {
        auto it = minor.find(key_name);
        key.accidentals = it == minor.end() ? 0 : it->second;
    }
    return key;
}

TempoMark parse_tempo(const std::string& value) {
    TempoMark tempo{};
    auto trimmed = trim(value);
    if (trimmed.empty()) { return tempo; }
    auto eq_pos = trimmed.find('=');
    if (eq_pos == std::string::npos) {
        if (auto bpm = parse_integer(trimmed)) {
            tempo.qpm = static_cast<double>(*bpm);
        }
        return tempo;
    }
    auto lhs = trim(trimmed.substr(0, eq_pos));
    auto rhs = trim(trimmed.substr(eq_pos + 1));
    tempo.beat = parse_fraction(lhs);
    if (auto bpm = parse_integer(rhs)) { tempo.qpm = static_cast<double>(*bpm); }
    return tempo;
}

Fraction default_note_length(const std::vector<FieldLine>& fields, const Meter meter) {
    for (const auto& field : fields) {
        if (field.id == "L") {
            return parse_fraction(field.value);
        }
    }
    return infer_unit_length(meter);
}

std::optional<int> parse_reference_number(const std::vector<FieldLine>& fields) {
    for (const auto& field : fields) {
        if (field.id == "X") {
            return parse_integer(field.value);
        }
    }
    return std::nullopt;
}

std::string first_title(const std::vector<FieldLine>& fields) {
    for (const auto& field : fields) {
        if (field.id == "T") { return std::string(trim(field.value)); }
    }
    return {};
}

struct ParseState {
    const std::vector<MusicLine>& lines;
    std::size_t                   line_index = 0;
    std::size_t                   column     = 0;
    DiagnosticSink&               diagnostics;
    const Header&                 header;
    const ParseOptions&           options;
    std::array<int, 7>            key_table;
    std::array<std::optional<int>, 7> measure_accidentals;
    TupletState                   tuplet;
    std::vector<BarEvent>         pending_bars;

    ParseState(const std::vector<MusicLine>& lines_, DiagnosticSink& sink, const Header& header_, const ParseOptions& options_) :
        lines{lines_}, diagnostics{sink}, header{header_}, options{options_} {
        key_table.fill(0);
        measure_accidentals.fill(std::nullopt);
        key_table = build_key_signature(header.key);
    }

    [[nodiscard]] bool eof() const {
        return line_index >= lines.size();
    }

    void next_line() {
        ++line_index;
        column = 0;
    }

    [[nodiscard]] char peek() const {
        if (eof()) { return '\0'; }
        const auto& text = lines[line_index].text;
        if (column >= text.size()) { return '\n'; }
        return text[column];
    }

    [[nodiscard]] char peek_next() const {
        if (eof()) { return '\0'; }
        const auto& text = lines[line_index].text;
        if (column < text.size()) {
            if (column + 1 < text.size()) { return text[column + 1]; }
            return '\n';
        }
        if (line_index + 1 >= lines.size()) { return '\0'; }
        const auto& next_line = lines[line_index + 1].text;
        if (next_line.empty()) { return '\n'; }
        return next_line[0];
    }

    void advance(std::size_t count = 1) {
        while (count-- > 0) {
            if (eof()) { return; }
            const auto& text = lines[line_index].text;
            if (column >= text.size()) {
                next_line();
            } else {
                ++column;
            }
        }
    }

    void skip_space() {
        while (true) {
            if (eof()) { return; }
            auto ch = peek();
            if (ch == '\n') {
                next_line();
                continue;
            }
            if (std::isspace(static_cast<unsigned char>(ch)) != 0) {
                advance();
                continue;
            }
            break;
        }
    }

    [[nodiscard]] Diagnostic location_diag(std::string message) const {
        if (eof()) {
            if (lines.empty()) { return Diagnostic{0, 0, std::move(message)}; }
            const auto& last = lines.back();
            return Diagnostic{last.line, last.text.size() + 1, std::move(message)};
        }
        return Diagnostic{lines[line_index].line, column + 1, std::move(message)};
    }

    void report(std::string message) { diagnostics.diagnostics.push_back(location_diag(std::move(message))); }

    void reset_measure() {
        measure_accidentals.fill(std::nullopt);
    }
};

int compute_midi_pitch(ParseState& state, const char letter, int octave_delta, int accidental_delta, const bool explicit_natural) {
    const bool lower = std::islower(static_cast<unsigned char>(letter)) != 0;
    const char upper = static_cast<char>(std::toupper(static_cast<unsigned char>(letter)));
    const auto idx   = letter_index(letter);
    const int  natural_pc = NATURAL_PITCH_CLASS[idx];

    const int base_octave_pitch = [&]() {
        int base = 0;
        switch (upper) {
            case 'C': base = 48; break;
            case 'D': base = 50; break;
            case 'E': base = 52; break;
            case 'F': base = 53; break;
            case 'G': base = 55; break;
            case 'A': base = 57; break;
            case 'B': base = 59; break;
            default: base = 48; break;
        }
        if (lower) { base += 12; }
        return base;
    }();

    int shift = state.key_table[idx];
    if (state.measure_accidentals[idx].has_value()) {
        shift = state.measure_accidentals[idx].value();
    }
    if (accidental_delta != 0) {
        shift = accidental_delta;
    } else if (explicit_natural) {
        shift = 0;
    }
    const int final_pitch = base_octave_pitch + octave_delta * 12 + shift;

    if (final_pitch < 0 || final_pitch > 127) {
        throw std::runtime_error("miniabc: midi pitch out of range");
    }
    return final_pitch;
}

Fraction apply_tuplet(ParseState& state, Fraction length) {
    if (state.tuplet.remain <= 0) { return length; }
    length = mul(length, state.tuplet.ratio);
    if (--state.tuplet.remain == 0) {
        state.tuplet = TupletState{};
    }
    return length;
}

TupletState parse_tuplet_spec(ParseState& state) {
    state.advance();  // skip '('
    std::string buffer;
    while (std::isdigit(static_cast<unsigned char>(state.peek())) != 0) {
        buffer.push_back(state.peek());
        state.advance();
    }
    if (buffer.empty()) {
        state.report("tuplet missing count");
        return {};
    }
    const int p = std::stoi(buffer);

    int q = 0;
    int r = 0;
    if (state.peek() == ':') {
        state.advance();
        buffer.clear();
        while (std::isdigit(static_cast<unsigned char>(state.peek())) != 0) {
            buffer.push_back(state.peek());
            state.advance();
        }
        if (!buffer.empty()) { q = std::stoi(buffer); }
        if (state.peek() == ':') {
            state.advance();
            buffer.clear();
            while (std::isdigit(static_cast<unsigned char>(state.peek())) != 0) {
                buffer.push_back(state.peek());
                state.advance();
            }
            if (!buffer.empty()) { r = std::stoi(buffer); }
        }
    }

    if (q == 0) {
        switch (p) {
            case 2: q = 3; break;
            case 3: q = 2; break;
            case 4: q = 3; break;
            case 6: q = 2; break;
            case 8: q = 3; break;
            case 5:
            case 7:
            case 9: {
                const bool compound = (state.header.meter.denominator == 8 && state.header.meter.numerator % 3 == 0);
                q = compound ? 3 : 2;
                break;
            }
            default: q = p - 1; break;
        }
    }
    if (r == 0) { r = p; }

    TupletState tuplet;
    tuplet.ratio  = normalize(Fraction{q, p});
    tuplet.remain = r;
    return tuplet;
}

Fraction parse_length_suffix(ParseState& state) {
    std::string suffix;
    while (true) {
        const auto ch = state.peek();
        if (std::isdigit(static_cast<unsigned char>(ch)) != 0 || ch == '/') {
            suffix.push_back(ch);
            state.advance();
        } else {
            break;
        }
    }
    return parse_note_length(suffix);
}

Fraction resolve_duration(ParseState& state, const Fraction ratio) {
    Fraction base = mul(state.header.unit_note_length, ratio);
    base          = apply_tuplet(state, base);
    return base;
}

std::optional<RestEvent> parse_rest(ParseState& state) {
    const char rest = state.peek();
    if (rest != 'z' && rest != 'x' && rest != 'Z') { return std::nullopt; }
    state.advance();
    const auto ratio = parse_length_suffix(state);
    RestEvent  rest_event;
    rest_event.length = resolve_duration(state, ratio);
    return rest_event;
}

std::optional<BarEvent> parse_bar(ParseState& state) {
    const char ch = state.peek();
    if (!(ch == '|' || ch == ':' || ch == ']')) { return std::nullopt; }
    std::string token;
    while (true) {
        const char cur = state.peek();
        if (cur == '|' || cur == ':' || cur == '[' || cur == ']') {
            token.push_back(cur);
            state.advance();
        } else {
            break;
        }
    }

    BarEvent bar;
    if (token.find(":|") != std::string::npos) { bar.repeat_end = true; }
    if (token.find("|:") != std::string::npos) { bar.repeat_start = true; }
    if (token == "||" || token == "|]" || token == "[|" || token == ":||" || token == "||:" || token == "|[|"sv) {
        bar.double_bar = true;
    }
    if (token.find("|]") != std::string::npos || token.find(":|]") != std::string::npos) { bar.final_bar = true; }
    return bar;
}

bool decoration_char(const char c) {
    switch (c) {
        case '~':
        case '.':
        case 'H':
        case 'L':
        case 'M':
        case 'O':
        case 'P':
        case 'S':
        case 'T':
        case 'u':
        case 'v':
        case '\'':
        case '"':
        case '{':
            return true;
        default: return false;
    }
}

void skip_decoration(ParseState& state) {
    const char ch = state.peek();
    if (ch == '"') {
        state.advance();
        while (!state.eof() && state.peek() != '"') { state.advance(); }
        if (state.peek() == '"') { state.advance(); }
        return;
    }
    if (ch == '!') {
        state.advance();
        while (!state.eof() && state.peek() != '!') { state.advance(); }
        if (state.peek() == '!') { state.advance(); }
        return;
    }
    if (ch == '{') {
        state.advance();
        while (!state.eof() && state.peek() != '}') { state.advance(); }
        if (state.peek() == '}') { state.advance(); }
        return;
    }
    state.advance();
}

std::optional<NoteEvent> parse_note(ParseState& state) {
    auto ch = state.peek();
    if (!(is_alpha(ch) || ch == '^' || ch == '_' || ch == '=')) { return std::nullopt; }

    int accidental_delta = 0;
    bool explicit_natural = false;
    while (ch == '^' || ch == '_' || ch == '=') {
        if (ch == '^') {
            ++accidental_delta;
        } else if (ch == '_') {
            --accidental_delta;
        } else {
            explicit_natural = true;
            accidental_delta = 0;
        }
        state.advance();
        ch = state.peek();
    }

    if (!is_alpha(ch)) { return std::nullopt; }

    const char letter = ch;
    state.advance();

    int octave_delta = 0;
    while (state.peek() == '\'' || state.peek() == ',') {
        if (state.peek() == '\'') {
            ++octave_delta;
        } else {
            --octave_delta;
        }
        state.advance();
    }

    const auto ratio = parse_length_suffix(state);
    NoteEvent  note;
    try {
        note.midi_pitch = compute_midi_pitch(state, letter, octave_delta, accidental_delta, explicit_natural);
    } catch (const std::exception& ex) {
        state.report(ex.what());
        return std::nullopt;
    }
    state.measure_accidentals[letter_index(letter)] = accidental_delta;

    note.length    = resolve_duration(state, ratio);
    note.velocity  = state.options.default_velocity;
    note.tie_continue = false;
    return note;
}

void duplicate_section(std::vector<Element>& events, const std::size_t start) {
    if (start >= events.size()) { return; }
    std::vector<Element> slice(events.begin() + static_cast<std::ptrdiff_t>(start), events.end());
    events.insert(events.end(), slice.begin(), slice.end());
}

std::vector<Element> flatten_events(const std::vector<Element>& raw) {
    std::vector<Element> final;
    std::vector<std::size_t> repeat_stack;
    for (const auto& elem : raw) {
        if (std::holds_alternative<BarEvent>(elem)) {
            const auto& bar = std::get<BarEvent>(elem);
            if (bar.repeat_start) { repeat_stack.push_back(final.size()); }
            if (bar.repeat_end) {
                if (!repeat_stack.empty()) {
                    const auto start = repeat_stack.back();
                    repeat_stack.pop_back();
                    duplicate_section(final, start);
                }
            }
            final.push_back(bar);
            continue;
        }
        final.push_back(elem);
    }
    return final;
}

Voice parse_voice(const std::vector<MusicLine>& lines, DiagnosticSink& diagnostics, const Header& header, const ParseOptions& options) {
    ParseState state(lines, diagnostics, header, options);
    std::vector<Element> events;
    std::optional<Fraction> pending_length_multiplier;
    auto apply_to_last_note = [&](const Fraction& multiplier) {
        for (auto it = events.rbegin(); it != events.rend(); ++it) {
            if (auto* note = std::get_if<NoteEvent>(&*it)) {
                note->length = mul(note->length, multiplier);
                break;
            }
        }
    };
    while (!state.eof()) {
        state.skip_space();
        if (state.eof()) { break; }

        if (auto rest = parse_rest(state)) {
            events.emplace_back(*rest);
            continue;
        }
        if (auto bar = parse_bar(state)) {
            events.emplace_back(*bar);
            state.reset_measure();
            continue;
        }
        if (state.peek() == '(') {
            const auto next = state.peek_next();
            if (std::isdigit(static_cast<unsigned char>(next)) != 0) {
                state.tuplet = parse_tuplet_spec(state);
            } else {
                state.advance();
            }
            continue;
        }
        if (state.peek() == '-' ) {
            state.advance();
            continue;
        }
        if (state.peek() == ')' || state.peek() == ']') {
            state.advance();
            continue;
        }
        if (state.peek() == '>' || state.peek() == '<') {
            const bool is_greater = state.peek() == '>';
            state.advance();
            const Fraction prev_mult = is_greater ? Fraction{3, 2} : Fraction{1, 2};
            const Fraction next_mult = is_greater ? Fraction{1, 2} : Fraction{3, 2};
            apply_to_last_note(prev_mult);
            if (pending_length_multiplier.has_value()) {
                pending_length_multiplier = mul(*pending_length_multiplier, next_mult);
            } else {
                pending_length_multiplier = next_mult;
            }
            continue;
        }
        if (state.peek() == '!') {
            skip_decoration(state);
            continue;
        }
        if (decoration_char(state.peek())) {
            skip_decoration(state);
            continue;
        }
        if (auto note = parse_note(state)) {
            if (pending_length_multiplier.has_value()) {
                note->length = mul(note->length, *pending_length_multiplier);
                pending_length_multiplier.reset();
            }
            events.emplace_back(*note);
            continue;
        }
        state.report(std::string("unexpected character '") + state.peek() + "'");
        state.advance();
    }

    auto flattened = flatten_events(events);
    Voice voice;
    voice.name = header.title;
    voice.elements = std::move(flattened);
    return voice;
}

Document parse_document(std::string_view input, const ParseOptions& options) {
    DiagnosticSink sink;
    const auto lines = parse_lines(input, sink);
    std::vector<MusicLine> music;
    auto fields = collect_fields(lines, music);

    Document doc;
    doc.header.reference_number = parse_reference_number(fields).value_or(1);
    doc.header.title            = first_title(fields);
    doc.header.meter            = parse_meter([&]() -> std::string {
        for (const auto& field : fields) {
            if (field.id == "M") { return field.value; }
        }
        return "4/4";
    }());
    doc.header.unit_note_length = default_note_length(fields, doc.header.meter);
    doc.header.tempo            = [&]() {
        for (const auto& field : fields) {
            if (field.id == "Q") { return parse_tempo(field.value); }
        }
        return TempoMark{};
    }();
    doc.header.key = [&]() {
        for (const auto& field : fields) {
            if (field.id == "K") { return parse_key(field.value); }
        }
        return parse_key("C");
    }();
    doc.header.ticks_per_quarter = options.ticks_per_quarter;

    auto voice = parse_voice(music, sink, doc.header, options);
    doc.voices.push_back(std::move(voice));
    if (!sink.diagnostics.empty() && options.strict) {
        throw ParseError(std::move(sink.diagnostics));
    }
    return doc;
}

std::string render_duration_token(const Fraction length, const Fraction unit) {
    const auto ratio = div(length, unit);
    if (ratio.num == ratio.den) { return {}; }
    if (ratio.den == 1) {
        if (ratio.num == 1) { return {}; }
        return std::to_string(ratio.num);
    }
    if (ratio.num == 1) {
        if (ratio.den == 2) { return "/"; }
        if (ratio.den == 4) { return "//"; }
    }
    std::ostringstream oss;
    oss << ratio.num << '/' << ratio.den;
    return oss.str();
}

std::string render_note(const NoteEvent& note, const Fraction unit) {
    static const std::array<std::string, 12> names = {
        "=C", "^C", "=D", "^D", "=E", "=F", "^F", "=G", "^G", "=A", "^A", "=B"
    };
    const int midi = note.midi_pitch;
    const int pc = midi % 12;
    auto       token = names[pc];

    if (midi >= 60) {
        token.back() = static_cast<char>(std::tolower(static_cast<unsigned char>(token.back())));
        const int extra = (midi - 60) / 12;
        if (extra > 0) { token.append(static_cast<std::size_t>(extra), '\''); }
    } else if (midi >= 48) {
        token.back() = static_cast<char>(std::toupper(static_cast<unsigned char>(token.back())));
    } else {
        token.back() = static_cast<char>(std::toupper(static_cast<unsigned char>(token.back())));
        const int commas = (48 - midi + 11) / 12;
        if (commas > 0) { token.append(static_cast<std::size_t>(commas), ','); }
    }
    token += render_duration_token(note.length, unit);
    return token;
}

std::string render_rest(const RestEvent& rest, const Fraction unit) {
    return "z" + render_duration_token(rest.length, unit);
}

std::string render_document(const Document& doc) {
    std::ostringstream out;
    out << "X: " << doc.header.reference_number << '\n';
    if (!doc.header.title.empty()) { out << "T: " << doc.header.title << '\n'; }
    out << "M: " << doc.header.meter.numerator << '/' << doc.header.meter.denominator << '\n';
    out << "L: " << doc.header.unit_note_length.num << '/' << doc.header.unit_note_length.den << '\n';
    out << "Q: " << static_cast<int>(doc.header.tempo.qpm) << '\n';
    out << "K: ";
    const auto accidentals = doc.header.key.accidentals;
    const auto is_minor    = doc.header.key.is_minor;
    if (accidentals > 0) {
        static const std::array<std::string, 8> names = {"C", "G", "D", "A", "E", "B", "F#", "C#"};
        out << names[static_cast<std::size_t>(accidentals)];
    } else if (accidentals < 0) {
        static const std::array<std::string, 8> names = {"C", "F", "Bb", "Eb", "Ab", "Db", "Gb", "Cb"};
        out << names[static_cast<std::size_t>(-accidentals)];
    } else {
        out << 'C';
    }
    if (is_minor) { out << "min"; }
    out << "\n";

    if (doc.voices.empty()) { return out.str(); }
    const auto unit = doc.header.unit_note_length;
    std::size_t beat_count = 0;
    const auto   per_measure = doc.header.meter.numerator;
    for (const auto& elem : doc.voices.front().elements) {
        if (const auto* note = std::get_if<NoteEvent>(&elem)) {
            out << render_note(*note, unit);
            beat_count += note->length.num * unit.den / note->length.den / unit.num;
        } else if (const auto* rest = std::get_if<RestEvent>(&elem)) {
            out << render_rest(*rest, unit);
            beat_count += rest->length.num * unit.den / rest->length.den / unit.num;
        } else {
            out << '|';
            beat_count = 0;
        }
        if (beat_count >= static_cast<std::size_t>(per_measure)) {
            out << '|';
            beat_count = 0;
        }
    }
    out << '\n';
    return out.str();
}

}  // namespace miniabc

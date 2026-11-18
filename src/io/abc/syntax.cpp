#include "syntax.h"

#include <bit>
#include <limits>
#include <span>
#include <stdexcept>
#include <utility>

#include <tao/pegtl.hpp>
#include <tao/pegtl/extra/parse_tree.hpp>
#include <utf8.h>

namespace symusic::abc::detail {

std::string Diagnostics::format(
    Location at, std::string_view code, std::string_view message
) const {
    return fmt::format("{}:{}:{}: ABC [{}]: {}", source, at.line, at.column, code, message);
}

void Diagnostics::error(Location at, std::string_view code, std::string_view message) const {
    throw std::invalid_argument(format(at, code, message));
}

void Diagnostics::warn(Location at, std::string_view code, std::string_view message) {
    warnings.emplace(at, code, message);
}

vec<std::string> Diagnostics::take() {
    vec<std::string> result;
    result.reserve(warnings.size());
    for (const auto& [at, code, message] : warnings) result.push_back(format(at, code, message));
    return result;
}

void Diagnostics::spend(size_t work, Location at) {
    if (work > playback_budget)
        error(at, "expansion-limit", "expanded tune exceeds the playback work limit");
    playback_budget -= work;
}

namespace {

namespace peg = tao::pegtl;
namespace grammar {

using namespace peg;

template<char... Cs>
using chars = one<Cs...>;
template<char C>
using repeated = plus<chars<C>>;
template<char End>
using until   = star<not_one<End>>;
using ws      = chars<' ', '\t', '\r', '\f', '\v'>;
using escaped = seq<chars<'\\'>, any>;

struct number : plus<peg::digit> {};
struct slashes : repeated<'/'> {};
struct fraction : seq<slashes, opt<number>> {};
struct length : sor<seq<number, opt<fraction>>, fraction> {};
struct octave : plus<chars<',', '\''>> {};
using shift_sign = chars<'^', '_'>;
using accidental_sign
    = sor<string<'^', '^'>, string<'_', '_'>, seq<chars<'='>, opt<shift_sign>>, shift_sign>;
struct accidental : seq<accidental_sign, opt<length>> {};
struct letter : ranges<'a', 'g', 'A', 'G'> {};
struct tie : chars<'-'> {};
struct pitch : seq<opt<accidental>, letter, opt<octave>, opt<length>, opt<tie>> {};

struct rest_letter : chars<'x', 'z', 'X', 'Z'> {};
struct rest : seq<rest_letter, opt<length>> {};
struct chord : seq<chars<'['>, star<sor<pitch, rest, ws>>, chars<']'>, opt<length>, opt<tie>> {};
struct grace : seq<chars<'{'>, opt<chars<'/'>>, star<sor<pitch, ws>>, chars<'}'>> {};

using field_key = seq<sor<alpha, chars<'+'>>, chars<':'>>;
struct field : seq<field_key, until<']'>> {};
struct bad_field : seq<chars<'['>, field_key, star<any>> {};
using inline_field = seq<chars<'['>, field, chars<']'>>;
struct endings : seq<number, star<seq<chars<',', '-'>, number>>> {};
struct repeat_both : sor<string<':', ':'>, string<':', '|', ':'>, string<':', '|', '|', ':'>> {};
struct repeat_start : sor<string<'|', '|', ':'>, string<'|', ':'>> {};
struct repeat_end : sor<string<':', '|', '|'>, string<':', '|'>, string<':', ']'>> {};
struct section : sor<string<'|', ']'>, string<'|', '|'>> {};
struct plain_bar : sor<string<'[', '|'>, chars<'|'>> {};
struct bar : seq<sor<repeat_both, repeat_end, repeat_start, section, plain_bar>,
                 opt<seq<opt<chars<'['>>, endings>>> {};
struct quoted_ending : seq<chars<'"'>, star<sor<escaped, not_one<'"', '\\'>>>, chars<'"'>> {};
struct ending : seq<chars<'['>, sor<endings, quoted_ending>, opt<chars<']'>>> {};

struct tuplet : seq<chars<'('>, number, rep_max<2, chars<':'>, opt<number>>> {};
struct broken : sor<repeated<'>'>, repeated<'<'>> {};
struct overlay : sor<string<'(', '&'>, string<'&', ')'>, chars<'&'>> {};
struct harmony : seq<chars<'"'>, star<sor<escaped, not_one<'"', '\\'>>>, chars<'"'>> {};
struct named_decoration : sor<seq<chars<'!'>, plus<not_one<'!'>>, chars<'!'>>,
                              seq<chars<'+'>, plus<not_one<'+'>>, chars<'+'>>> {};
struct decoration : sor<named_decoration, chars<'.', '~'>, alpha> {};
struct slur : chars<'(', ')'> {};
struct invalid : any {};
using layout = sor<chars<'$', '`'>, seq<chars<'\\', '!'>, eof>, ws>;
using token  = sor<
    inline_field,
    bad_field,
    ending,
    bar,
    chord,
    grace,
    pitch,
    rest,
    tuplet,
    broken,
    overlay,
    harmony,
    decoration,
    slur,
    tie,
    layout,
    invalid>;
using music = seq<star<token>, eof>;

}   // namespace grammar

// Select only rules that carry values; PEGTL owns matching, disambiguation and
// discarding partial matches. The playback engine never sees the parse tree.
template<class Rule>
using Select = peg::parse_tree::selector<
    Rule,
    peg::parse_tree::store_content::on<
        grammar::pitch,
        grammar::rest,
        grammar::chord,
        grammar::grace,
        grammar::accidental,
        grammar::letter,
        grammar::octave,
        grammar::tie,
        grammar::length,
        grammar::field,
        grammar::bad_field,
        grammar::quoted_ending,
        grammar::bar,
        grammar::repeat_both,
        grammar::repeat_start,
        grammar::repeat_end,
        grammar::section,
        grammar::endings,
        grammar::ending,
        grammar::tuplet,
        grammar::broken,
        grammar::overlay,
        grammar::harmony,
        grammar::decoration,
        grammar::slur,
        grammar::invalid>>;

using Input = peg::view_input<>;
using Node  = peg::parse_tree::node_t<Input>;

size_t field_separator(std::string_view text) {
    if (text.empty() || !(letter(text.front()) || text.front() == '+')) return text.npos;
    const auto colon = text.find_first_not_of(whitespace, 1);
    return colon != text.npos && text[colon] == ':' ? colon : text.npos;
}

std::string_view without_comment(std::string_view line) {
    bool quoted = false;
    for (size_t i = 0; i < line.size(); ++i) {
        if (line[i] == '"') quoted = !quoted;
        if (line[i] == '%' && !quoted) return line.substr(0, i);
        if (line[i] == '\\' && i + 1 < line.size() && (quoted || line[i + 1] == '%')) ++i;
    }
    return line;
}

Field field(std::string_view text) {
    return {text[0], trim(text.substr(field_separator(text) + 1))};
}

struct Fragment {
    size_t offset;
    size_t line;
};

// A logical line keeps its full byte view so every token kind uses the same
// source map after trimming, comment removal and continuation assembly.
struct LogicalLine {
    std::string_view          bytes;
    size_t                    first_line;
    std::span<const Fragment> fragments{};

    [[nodiscard]] Location where(std::string_view part) const {
        const auto offset = static_cast<size_t>(part.data() - bytes.data());
        if (fragments.empty()) return {first_line, offset + 1};
        const auto fragment
            = std::ranges::upper_bound(fragments, offset, {}, &Fragment::offset) - 1;
        return {fragment->line, offset - fragment->offset + 1};
    }
};

class MusicLine {
public:
    MusicLine(
        LogicalLine source, std::string_view text, Diagnostics& diagnostics, vec<Token>& output
    ) : source(source), text(text), diagnostics(diagnostics), output(output) {}

    void read();

private:
    LogicalLine      source;
    std::string_view text;
    Diagnostics&     diagnostics;
    vec<Token>&      output;

    Location location(const Node& node) const { return source.where(node.data); }

    template<class Rule>
    static const Node* child(const Node& node) {
        const auto found = std::ranges::find_if(node.children, [](const auto& candidate) {
            return candidate->template is_type<Rule>();
        });
        return found == node.children.end() ? nullptr : found->get();
    }
    int        number(std::string_view value, Location at, int fallback = 0) const;
    Length     length(const Node* node) const;
    Pitch      pitch(const Node& node) const;
    int        octave(const Node& node) const;
    Bar        bar(const Node& node) const;
    Tuplet     tuplet(const Node& node) const;
    vec<Pitch> members(const Node& node) const;
    void       invalid(const Node& node);

    template<class T>
    void push(const Node& node, T value) {
        if constexpr (std::is_same_v<T, Atom>) value.music_line = source.first_line;
        output.push_back({location(node), std::move(value)});
    }

    void emit(const Node& node, grammar::pitch);
    void emit(const Node& node, grammar::rest);
    void emit(const Node& node, grammar::chord);
    void emit(const Node& node, grammar::grace) { push(node, Grace{members(node)}); }
    void emit(const Node& node, grammar::field) { push(node, field(node.data)); }
    void emit(const Node& node, grammar::bad_field) {
        diagnostics.error(
            location(node), "unclosed-inline-field", "expected ']' after the inline field"
        );
    }
    void emit(const Node& node, grammar::bar) { push(node, bar(node)); }
    void emit(const Node& node, grammar::ending) { push(node, bar(node)); }
    void emit(const Node& node, grammar::tuplet) { push(node, tuplet(node)); }
    void emit(const Node& node, grammar::broken);
    void emit(const Node& node, grammar::overlay);
    void emit(const Node& node, grammar::harmony) {
        push(node, Harmony{node.data.substr(1, node.data.size() - 2)});
    }
    void emit(const Node& node, grammar::decoration);
    void emit(const Node& node, grammar::slur) { push(node, Slur{node.data.front() == '('}); }
    void emit(const Node& node, grammar::tie) { push(node, Tie{}); }
    void emit(const Node& node, grammar::invalid) { invalid(node); }
};

int MusicLine::number(std::string_view value, Location at, int fallback) const {
    if (value.empty()) return fallback;
    if (const auto parsed = integer(value)) return *parsed;
    diagnostics.error(at, "number-range", "expected an integer in range");
}

struct FractionFields {
    std::string_view numerator;
    std::string_view denominator;
    size_t           slashes;
};

FractionFields fraction_fields(std::string_view text) {
    const auto first = text.find('/');
    if (first == text.npos) return {text, {}, 0};
    const auto end = std::min(text.size(), text.find_first_not_of('/', first));
    return {text.substr(0, first), text.substr(end), end - first};
}

Length MusicLine::length(const Node* node) const {
    if (!node) return {};
    const auto at        = location(*node);
    const auto parts     = fraction_fields(node->data);
    const auto numerator = number(parts.numerator, at, 1);
    if (parts.slashes == 0) return {numerator, 1};
    const auto denominator = number(parts.denominator, at, 2);
    if (denominator == 0) diagnostics.error(at, "zero-length-divisor", "duration divides by zero");
    const auto power         = std::bit_ceil(static_cast<uint64_t>(denominator));
    const auto extra_slashes = parts.slashes - 1;
    if (extra_slashes > 30 || (power << extra_slashes) > std::numeric_limits<int>::max()) {
        diagnostics.error(at, "length-range", "duration denominator is too large");
    }
    if (denominator != static_cast<int64_t>(power)) {
        diagnostics.warn(
            at,
            "non-binary-length",
            fmt::format("rounded duration denominator {} up to {}", denominator, power)
        );
    }
    return {numerator, static_cast<int>(power << extra_slashes)};
}

int MusicLine::octave(const Node& node) const {
    auto value = child<grammar::octave>(node);
    if (!value) return 0;
    if (value->data.size() > 32)
        diagnostics.error(location(*value), "pitch-range", "too many octave marks");
    return static_cast<int>(
        2 * std::ranges::count(value->data, '\'') - static_cast<ptrdiff_t>(value->data.size())
    );
}

Pitch MusicLine::pitch(const Node& node) const {
    const auto                name = child<grammar::letter>(node)->data.front();
    std::optional<Alteration> alteration;
    if (const auto prefix = child<grammar::accidental>(node))
        alteration = accidental(prefix->data, location(*prefix), diagnostics);
    return {
        upper(name),
        octave(node) + lower(name),
        alteration,
        child<grammar::tie>(node) != nullptr,
        length(child<grammar::length>(node))
    };
}

vec<Pitch> MusicLine::members(const Node& node) const {
    vec<Pitch> notes;
    for (const auto& member : node.children) {
        if (!member->is_type<grammar::pitch>()) continue;
        if (notes.size() >= 128)
            diagnostics.error(
                location(*member),
                "group-limit",
                "chords and grace groups support at most 128 notes"
            );
        notes.push_back(pitch(*member));
    }
    return notes;
}

Bar MusicLine::bar(const Node& node) const {
    const auto  ending = child<grammar::endings>(node);
    const auto  both   = child<grammar::repeat_both>(node);
    const auto* label  = child<grammar::quoted_ending>(node);
    return {
        bool(both || child<grammar::repeat_start>(node)),
        bool(both || child<grammar::repeat_end>(node)),
        !node.is_type<grammar::ending>(),
        child<grammar::section>(node) != nullptr,
        ending ? ending->data : std::string_view{}
    };
}

Tuplet MusicLine::tuplet(const Node& node) const {
    auto       text     = node.data.substr(1);
    const auto argument = [&](int fallback) {
        const auto colon = text.find(':');
        const auto count = number(text.substr(0, colon), location(node), fallback);
        text.remove_prefix(colon == text.npos ? text.size() : colon + 1);
        return count;
    };
    const auto count      = argument(0);
    const auto in_time_of = argument(0);
    return {count, in_time_of, argument(0)};
}

void MusicLine::invalid(const Node& node) {
    const auto at = location(node);
    const char c  = node.data.front();
    using namespace std::literals;
    if ("[{\"!+"sv.find(c) != std::string_view::npos) {
        diagnostics.error(
            at, "malformed-group", fmt::format("unclosed or malformed group starting with '{}'", c)
        );
    }
    if (c == ']')
        diagnostics.warn(at, "stray-chord-close", "ignored ']' outside a chord");
    else if (c == '\\')
        diagnostics.warn(at, "stray-continuation", "ignored a line continuation inside music");
    else
        diagnostics.warn(
            at,
            "unexpected-character",
            fmt::format("ignored unexpected byte {:#04x}", static_cast<unsigned char>(c))
        );
}

void MusicLine::emit(const Node& node, grammar::pitch) {
    const auto note = pitch(node);
    push(node, Atom{{note}, note.length, false, note.tie});
}

void MusicLine::emit(const Node& node, grammar::rest) {
    push(node, Atom{{}, length(child<grammar::length>(node)), !lower(node.data.front())});
}

void MusicLine::emit(const Node& node, grammar::chord) {
    auto       notes  = members(node);
    const auto suffix = child<grammar::length>(node);
    const auto duration
        = suffix ? length(suffix) : (notes.empty() ? Length{} : notes.front().length);
    if (notes.empty())
        diagnostics.warn(location(node), "empty-chord", "empty chord treated as a rest");
    push(node, Atom{std::move(notes), duration, false, child<grammar::tie>(node) != nullptr});
}

void MusicLine::emit(const Node& node, grammar::broken) {
    if (node.data.size() > 30) {
        diagnostics.error(location(node), "rhythm-range", "too many broken rhythm marks");
    }
    push(node, Broken{node.data.front() == '>' ? 1 : -1, static_cast<int>(node.data.size())});
}

void MusicLine::emit(const Node& node, grammar::overlay) {
    push(
        node,
        node.data == "(&" ? Overlay::start : (node.data == "&)" ? Overlay::end : Overlay::split)
    );
}

void MusicLine::emit(const Node& node, grammar::decoration) {
    const auto value = node.data;
    const auto name  = value.size() == 1 ? value : value.substr(1, value.size() - 2);
    push(node, Decoration{name, value.data() + value.size() == text.data() + text.size()});
}

void MusicLine::read() {
    Input      input(text);
    const auto root = peg::parse_tree::parse<grammar::music, Select>(input);
    if (!root)
        diagnostics.error(source.where(text), "invalid-music", "could not read the music line");
    for (const auto& node : root->children) {
        const auto dispatch = [&]<class... Rule>(std::tuple<Rule...>) {
            return ((node->is_type<Rule>() && (emit(*node, Rule{}), true)) || ...);
        };
        dispatch(
            std::tuple<
                grammar::pitch,
                grammar::rest,
                grammar::chord,
                grammar::grace,
                grammar::field,
                grammar::bad_field,
                grammar::bar,
                grammar::ending,
                grammar::tuplet,
                grammar::broken,
                grammar::overlay,
                grammar::harmony,
                grammar::decoration,
                grammar::slur,
                grammar::tie,
                grammar::invalid>{}
        );
    }
}

class TuneLines {
public:
    explicit TuneLines(Diagnostics& diagnostics) : diagnostics(diagnostics) {}
    bool line(LogicalLine line);
    bool joined(std::string text, std::span<const Fragment> fragments);
    Tune finish(Location end);

private:
    Diagnostics& diagnostics;
    Tune         tune;
    bool         saw_reference = false;
    bool         in_body       = false;
    bool         saw_music     = false;
    bool         ended         = false;

    bool metadata(Field value, Location at);
    bool content(LogicalLine line, std::string_view text);
};

bool TuneLines::metadata(Field value, Location at) {
    if (value.kind == 'X') {
        if (in_body) {
            diagnostics.warn(at, "extra-tunes", "only the first tune was read");
            return false;
        }
        if (saw_reference) diagnostics.error(at, "missing-key", "first tune has no K: field");
        saw_reference = true;
        if (!integer(value.value))
            diagnostics.warn(at, "bad-reference", "expected an integer after X:");
    }
    (in_body ? tune.body : tune.header).push_back({at, value});
    if (value.kind == 'K' && !in_body) {
        in_body  = true;
        tune.key = at;
    }
    return true;
}

bool TuneLines::joined(std::string text, std::span<const Fragment> fragments) {
    return line(
        {tune.continued_lines.emplace_back(std::move(text)), fragments.front().line, fragments}
    );
}

bool TuneLines::line(LogicalLine line) {
    const auto text = trim(line.bytes);
    if (text.empty()) {
        ended = ended || saw_music;
        return true;
    }
    if (ended && !text.starts_with("X:")) return true;
    if (text.starts_with("%%")) {
        (in_body ? tune.body : tune.header)
            .push_back({line.where(text), Directive{trim(without_comment(text.substr(2)))}});
        return true;
    }
    const auto content = trim(without_comment(text));
    return content.empty() || this->content(line, content);
}

bool TuneLines::content(LogicalLine line, std::string_view content) {
    const auto at          = line.where(content);
    const auto tail        = content.substr(1);
    const bool note_repeat = in_body && note_letter(content.front())
                             && (tail.starts_with(":|") || tail.starts_with("::"));
    if (note_repeat)
        diagnostics.warn(
            at, "field-repeat-ambiguity", "read a note followed by a repeat bar rather than a field"
        );
    else if (field_separator(content) != content.npos)
        return metadata(field(content), at);
    if (!in_body) diagnostics.error(at, "missing-key", "expected K: before the music");
    MusicLine(line, content, diagnostics, tune.body).read();
    saw_music = true;
    return true;
}

Tune TuneLines::finish(Location end) {
    if (!in_body) diagnostics.error(end, "missing-key", "expected a K: field in the tune header");
    if (!saw_reference)
        diagnostics.error(tune.key, "missing-reference", "expected an X: field before K:");
    return std::move(tune);
}

}   // namespace

namespace {

struct PhysicalLine {
    std::string_view text;
    bool             has_newline;
};

PhysicalLine take_line(std::string_view& text) {
    const auto end  = text.find_first_of("\r\n");
    const auto line = text.substr(0, end);
    if (end == text.npos)
        text = {};
    else
        text.remove_prefix(end + (text.substr(end).starts_with("\r\n") ? 2 : 1));
    return {line, end != text.npos};
}

bool continuation(std::string_view part) {
    size_t count = 0;
    while (part.ends_with(char(92))) {
        part.remove_suffix(1);
        ++count;
    }
    return count % 2 != 0;
}

void validate_line(std::string_view line, size_t number, Diagnostics& diagnostics) {
    const auto invalid  = utf8::find_invalid(line.begin(), line.end());
    const auto bad_byte = static_cast<size_t>(invalid - line.begin());
    const auto nul      = line.find('\0');
    if (nul <= bad_byte && nul != line.npos)
        diagnostics.error({number, nul + 1}, "invalid-text", "ABC text cannot contain a NUL byte");
    if (invalid != line.end())
        diagnostics.error({number, bad_byte + 1}, "invalid-text", "ABC text must be valid UTF-8");
}

bool read_logical_line(
    TuneLines& tune, std::string_view& text, size_t& line, Diagnostics& diagnostics
) {
    vec<Fragment> fragments;
    std::string   joined;
    do {
        const auto [raw, newline] = take_line(text);
        validate_line(raw, line, diagnostics);
        const auto part      = trim(raw.starts_with("%%") ? raw : without_comment(raw));
        const bool continues = newline && continuation(part);
        if (!continues && fragments.empty()) return tune.line({raw, line++});
        fragments.push_back({joined.size(), line++});
        if (continues)
            joined.append(
                raw.substr(0, static_cast<size_t>(part.data() - raw.data()) + part.size() - 1)
            );
        else
            joined.append(raw);
        if (!continues) break;
    } while (!text.empty());
    return tune.joined(std::move(joined), fragments);
}

}   // namespace

namespace {

int accidental_quantity(
    std::string_view text, int fallback, Location at, Diagnostics& diagnostics
) {
    if (text.empty()) return fallback;
    const auto number = integer(text);
    if (!number || *number < 0)
        diagnostics.error(at, "invalid-microtone", "expected an accidental such as ^/ or _3/4");
    return *number;
}

Alteration microtone(std::string_view text, int direction, Location at, Diagnostics& diagnostics) {
    const auto parts     = fraction_fields(text);
    const auto numerator = accidental_quantity(parts.numerator, 1, at, diagnostics);
    if (parts.slashes > 31)
        diagnostics.error(at, "microtone-range", "microtone denominator is too large");
    const auto denominator
        = parts.slashes == 0 ?
              1 :
              static_cast<int64_t>(accidental_quantity(parts.denominator, 2, at, diagnostics))
                  << (parts.slashes - 1);
    if (denominator == 0) diagnostics.error(at, "invalid-microtone", "microtone divides by zero");
    if (denominator > std::numeric_limits<int>::max() || numerator / denominator > 127)
        diagnostics.error(at, "microtone-range", "microtone quantity is too large");
    return {direction * numerator, static_cast<int>(denominator)};
}

}   // namespace

std::optional<Alteration> accidental(std::string_view text, Location at, Diagnostics& diagnostics) {
    using namespace std::literals;
    constexpr std::array normal{"__"sv, "_"sv, "="sv, "^"sv, "^^"sv};
    if (const auto found = std::ranges::find(normal, text); found != normal.end())
        return Alteration{static_cast<int>(found - normal.begin()) - 2};
    if (text.find_first_not_of("^_=") == 1 && text.front() != '=')
        return microtone(text.substr(1), text.front() == '_' ? -1 : 1, at, diagnostics);
    diagnostics.warn(
        at, "invalid-accidental", "ignored ambiguous accidental; use a single sign for microtones"
    );
    return {};
}

Tune tokenize(std::string_view text, Diagnostics& diagnostics) {
    if (text.starts_with("\xef\xbb\xbf")) text.remove_prefix(3);
    TuneLines tune(diagnostics);
    size_t    line = 1;
    while (!text.empty() && read_logical_line(tune, text, line, diagnostics)) {}
    return tune.finish({line, 1});
}

}   // namespace symusic::abc::detail

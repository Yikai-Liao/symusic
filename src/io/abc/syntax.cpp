#include "syntax.h"

#include <algorithm>
#include <array>
#include <charconv>
#include <cmath>
#include <deque>
#include <iterator>
#include <limits>
#include <map>
#include <optional>
#include <ranges>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <variant>

#include <fmt/format.h>
#include <tao/pegtl.hpp>
#include <utf8.h>

namespace symusic::abc::detail {

std::strong_ordering Location::operator<=>(const Location& rhs) const {
    if (source == rhs.source && include_path.data() == rhs.include_path.data())
        return point() <=> rhs.point();
    // Compare each path as enclosing include sites followed by the physical point.
    const auto depth = std::min(include_path.size(), rhs.include_path.size());
    for (size_t level = 0; level <= depth; ++level) {
        const auto left  = level == include_path.size() ? point() : include_path[level];
        const auto right = level == rhs.include_path.size() ? rhs.point() : rhs.include_path[level];
        if (const auto order = left <=> right; order != 0) return order;
    }
    return include_path.size() <=> rhs.include_path.size();
}

Location Diagnostics::register_include(std::string_view source, Location at) {
    vec<Location::Point> path(at.include_path.begin(), at.include_path.end());
    path.push_back(at.point());
    sources.emplace_back(source, std::move(path));
    return {1, 1, sources.size() - 1, sources.back().include_path};
}

std::string_view Diagnostics::record_line(size_t source, std::string_view raw) {
    auto& entry = sources[source];
    if (entry.blocks.empty()
        || raw.size() > entry.blocks.back().capacity() - entry.blocks.back().size()) {
        auto& block = entry.blocks.emplace_back();
        try {
            block.reserve(std::max<size_t>(4096, raw.size()));
        } catch (...) {
            entry.blocks.pop_back();
            throw;
        }
    }
    auto&      block = entry.blocks.back();
    const auto start = block.size();
    entry.lines.emplace_back(entry.blocks.size() - 1, start + raw.size());
    try {
        block.insert(block.end(), raw.begin(), raw.end());
    } catch (...) {
        entry.lines.pop_back();
        throw;
    }
    // Appends stay within capacity, so earlier line and token views remain valid.
    return {block.data() + start, raw.size()};
}

std::string Diagnostics::format(
    Location at, std::string_view code, std::string_view message
) const {
    const auto& source = sources[at.source];
    auto        result
        = fmt::format("{}:{}:{}: ABC [{}]: {}", source.name, at.line, at.column, code, message);
    if (at.line == 0 || at.line > source.lines.size()) return result;

    const auto [block_index, line_end] = source.lines[at.line - 1];
    const auto       start  = at.line > 1 && source.lines[at.line - 2].first == block_index ?
                                  source.lines[at.line - 2].second :
                                  0;
    const auto&      block  = source.blocks[block_index];
    const auto       line   = std::string_view(block.data() + start, line_end - start);
    constexpr size_t kWidth = 96;
    const auto       pos    = std::min(at.column - 1, line.size());
    size_t           begin  = std::min(
        pos > kWidth / 2 ? pos - kWidth / 2 : 0, line.size() > kWidth ? line.size() - kWidth : 0
    );
    size_t end = begin + std::min(kWidth, line.size() - begin);
    // Include a complete UTF-8 character when a window edge falls inside it.
    const auto trail = [&](size_t i) { return (static_cast<u8>(line[i]) & 0xc0U) == 0x80U; };
    for (i32 n = 0; n < 3 && begin && trail(begin); ++n) --begin;
    for (i32 n = 0; n < 3 && end < line.size() && trail(end); ++n) ++end;
    return result
           + fmt::format(
               "\n{}{:?}{}",
               begin ? "..." : "",
               line.substr(begin, end - begin),
               end < line.size() ? "..." : ""
           );
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

RepeatEndings::RepeatEndings(vec<EndingRange> written) : ranges(std::move(written)) {
    std::ranges::sort(ranges, {}, &EndingRange::first);
    auto next = ranges.begin();
    for (const auto range : ranges) {
        if (next != ranges.begin() && std::prev(next)->last >= range.first - 1) {
            auto& last = std::prev(next)->last;
            last       = std::max(last, range.last);
        } else {
            *next++ = range;
        }
    }
    ranges.erase(next, ranges.end());
}

bool RepeatEndings::contains(size_t pass) const {
    if (pass > last_pass()) return false;
    const auto range
        = std::ranges::lower_bound(ranges, static_cast<i32>(pass), {}, &EndingRange::last);
    return range != ranges.end() && range->first <= static_cast<i32>(pass);
}

f64 Broken::first_factor(f64 single_short) const {
    const auto short_factor = count == 1 ? single_short : std::ldexp(1.0, -count);
    return direction > 0 ? 2 - short_factor : short_factor;
}

namespace {

namespace peg = tao::pegtl;
namespace grammar {

using namespace peg;

template<char... Cs>
using chars      = one<Cs...>;
using ws         = chars<' ', '\t', '\r', '\f', '\v'>;
using escaped    = seq<chars<'\\'>, any>;
using number     = plus<peg::digit>;
using fraction   = seq<plus<chars<'/'>>, opt<number>>;
using duration   = sor<seq<number, opt<fraction>>, fraction>;
using octave     = plus<chars<',', '\''>>;
using shift_sign = chars<'^', '_'>;
using accidental_sign
    = sor<string<'^', '^'>, string<'_', '_'>, seq<chars<'='>, opt<shift_sign>>, shift_sign>;
using accidental     = seq<accidental_sign, opt<duration>>;
using letter         = ranges<'a', 'g', 'A', 'G'>;
using bare_pitch     = seq<letter, opt<octave>>;
using pitch_spelling = seq<opt<accidental>, bare_pitch>;

// Only the alternatives of the choice being scanned emit a span. Matching a
// chord or grace group can backtrack without emitting any of its members.
enum class Item {
    Pitch,
    Chord,
    Rest,
    Spacer,
    Grace,
    Broken,
    Field,
    BadField,
    Bar,
    Ending,
    Tuplet,
    Harmony,
    Slur,
    Tie,
    Overlay,
    Decoration,
    Bang,
    Layout,
    Invalid
};
template<Item Kind, class Rule>
struct item : Rule {
    static void apply(const auto& in, const auto& visit) { visit(Kind, in.string_view()); }
};
using tie         = item<Item::Tie, seq<opt<chars<'.'>>, chars<'-'>>>;
using pitch       = item<Item::Pitch, seq<pitch_spelling, opt<duration>, opt<tie>>>;
using rest_letter = chars<'x', 'z', 'X', 'Z'>;
using rest        = item<Item::Rest, seq<rest_letter, opt<duration>>>;
using spacer      = item<Item::Spacer, seq<chars<'y'>, opt<duration>>>;
using space       = item<Item::Layout, plus<ws>>;
// ABC 2.1 section 4.14: a decoration cannot consume a bar, chord boundary or field separator.
template<char Delimiter>
using delimited_decoration = seq<
    chars<Delimiter>,
    plus<not_one<Delimiter, ' ', '\t', '\r', '\n', '\f', '\v', '[', ']', '|', ':'>>,
    chars<Delimiter>>;
using plus_decoration  = delimited_decoration<'+'>;
using named_decoration = sor<delimited_decoration<'!'>, plus_decoration>;
using decoration     = item<Item::Decoration, sor<named_decoration, chars<'.', '~', '\''>, alpha>>;
using bang_break     = item<Item::Bang, chars<'!'>>;
using linebreak_mark = sor<bang_break, chars<'$'>, string<'<', 'E', 'O', 'L', '>'>>;
using linebreak_setting = sor<string<'<', 'n', 'o', 'n', 'e', '>'>, list<linebreak_mark, plus<ws>>>;
using reserved_music    = chars<'#', '*', ';', '?', '@'>;
using chord_member      = sor<pitch, rest, spacer, space, decoration>;
using chord
    = item<Item::Chord, seq<chars<'['>, star<chord_member>, chars<']'>, opt<duration>, opt<tie>>>;
using broken       = item<Item::Broken, sor<plus<chars<'>'>>, plus<chars<'<'>>>>;
using grace_member = sor<chord, pitch, broken, spacer, space>;
using grace = item<Item::Grace, seq<chars<'{'>, opt<chars<'/'>>, star<grace_member>, chars<'}'>>>;

using field_key        = seq<sor<alpha, chars<'+'>>, chars<':'>>;
using quote_body       = star<sor<escaped, not_one<'"', '\\'>>>;
using quoted           = seq<chars<'"'>, quote_body, chars<'"'>>;
using symbol_shorthand = sor<ranges<'H', 'W', 'h', 'w'>, chars<'~'>>;
using symbol_expansion = sor<named_decoration, quoted>;
using equal            = pad<chars<'='>, ws>;

using text_escape   = seq<chars<'\\'>, chars<'\\', '%', '"', '&'>>;
using text_mnemonic = seq<chars<'\\'>, sor<chars<'`', '\'', '^', '~', '"', '/'>, alpha>, alpha>;
using text_unicode
    = sor<seq<string<'\\', 'u'>, rep<4, xdigit>>, seq<string<'\\', 'U'>, rep<8, xdigit>>>;
using text_entity      = seq<chars<'&'>, plus<alpha>, chars<';'>>;
using text_font_switch = seq<chars<'$'>, ranges<'0', '4'>>;
using text_dollar      = string<'$', '$'>;
using text_lyric_dash  = string<'\\', '-'>;
using lyric_escape     = seq<chars<'\\'>, chars<'-', '\\', '%', '&'>>;

using beat         = seq<number, opt<chars<'/'>, number>>;
using tempo_beats  = sor<seq<letter, opt<star<ws>, beat>>, seq<beat, rep_opt<3, plus<ws>, beat>>>;
using tempo_prefix = seq<tempo_beats, equal>;
using bpm          = seq<opt<chars<'+'>>, number>;
using part_item    = seq<sor<alpha, chars<')'>>, opt<number>>;

using lyric_character        = sor<lyric_escape, not_one<' ', '\t', '-', '|', '*', '_'>>;
using styled_lyric_character = sor<text_dollar, seq<not_at<text_font_switch>, lyric_character>>;
template<class Item>
using styled         = seq<Item, star<text_font_switch>>;
using lyric_syllable = seq<star<text_font_switch>, plus<styled<styled_lyric_character>>>;
// Section 5.3: the numeric prefix can include punctuation such as the escaped
// hyphen in 1\-~Si. Keep alignment marks and encoded letters for the verse.
using lyric_stanza_mark = sor<
    text_font_switch,
    text_dollar,
    text_lyric_dash,
    text_escape,
    seq<not_at<sor<text_entity, chars<'\\', '|', '*', '_'>>>, peg::punct>>;
using lyric_stanza = seq<
    star<sor<ws, text_font_switch>>,
    number,
    star<sor<peg::digit, lyric_stanza_mark>>,
    star<sor<ws, text_font_switch, chars<'~'>>>>;
// A held syllable takes trailing punctuation in the section 13.4 example.
using hold_punctuation = seq<
    plus<styled<chars<'.', ',', '!', '?', ':', ';'>>>,
    at<sor<ws, chars<'-', '*', '|'>, eof>>>;

using inline_field_start = seq<chars<'['>, field_key>;
using inline_field
    = item<Item::Field, seq<inline_field_start, star<sor<quoted, not_one<']', '"'>>>, chars<']'>>>;
using bad_field = item<Item::BadField, seq<inline_field_start, star<any>>>;
// Sections 2.2.5 and 8.1: elide these even inside a note, but not within text or fields.
using inline_remark      = seq<string<'[', 'r', ':'>, star<not_one<']'>>, chars<']'>>;
using playback_elision   = sor<inline_remark, reserved_music>;
using music_text         = sor<inline_field, bad_field, quoted, seq<chars<'"'>, star<any>>>;
using macro_spaced_equal = seq<ws, chars<'='>, at<ws>>;
template<class Separator>
using macro_to_separator = peg::until<at<Separator>, sor<music_text, named_decoration, any>>;
// Section 9 limits the written text in characters. An annotation in a macro
// can contain UTF-8, and header charset validation may still be pending.
template<size_t Max>
using macro_text = seq<peg::utf8::any, rep_opt<Max - 1, peg::utf8::any>>;

using ending_range = seq<number, opt<chars<'-'>, number>>;
using endings
    = seq<list<ending_range, chars<','>>, not_at<sor<chars<','>, seq<chars<'-'>, number>>>>;
// Section 4.8 permits continuous thick/thin patterns such as |[|.
// A bracket needs its own pipe so a following chord or ending stays separate.
using bar_piece = sor<string<'[', '|', ']'>, string<'[', '|'>, string<'|', ']'>, chars<'|'>>;
using barline   = plus<bar_piece>;
using colons    = plus<chars<':'>>;
// Consume colon runs once, including malformed bare runs without a divider.
using repeat_close = seq<
    at<seq<chars<':'>, sor<chars<':', ']'>, bar_piece>>>,
    colons,
    opt<sor<barline, chars<']'>>, opt<colons>>>;
using repeat_bar     = sor<repeat_close, seq<barline, colons>>;
using bar_sign       = sor<repeat_bar, barline>;
using bar            = item<Item::Bar, seq<opt<chars<'.'>>, bar_sign, opt<endings>>>;
using ending         = item<Item::Ending, seq<chars<'['>, sor<endings, quoted>, opt<chars<']'>>>>;
using tuplet_arg     = seq<chars<':'>, opt<number>>;
using tuplet         = item<Item::Tuplet, seq<chars<'('>, number, rep_opt<2, tuplet_arg>>>;
using overlay_splits = plus<seq<chars<'&'>, not_at<chars<')'>>>>;
using overlay        = item<Item::Overlay, sor<string<'(', '&'>, string<'&', ')'>, overlay_splits>>;
using harmony        = item<Item::Harmony, quoted>;
using slur           = item<Item::Slur, seq<opt<chars<'.'>>, chars<'(', ')'>>>;
using layout         = item<Item::Layout, sor<chars<'$', '`'>, seq<chars<'\\'>, eof>, plus<ws>>>;
using invalid        = item<Item::Invalid, any>;
// A dialect may give a bare bang precedence over named decorations at top level.
template<class... Prefix>
using music_token = sor<
    Prefix...,
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
    spacer,
    slur,
    tie,
    decoration,
    bang_break,
    layout,
    invalid>;
}   // namespace grammar

using Input = peg::view_input<>;

// Visit successive complete alternatives and return the unconsumed suffix.
template<class... Items>
std::string_view scan(std::string_view text, peg::sor<Items...>, const auto& visit) {
    Input input(text);
    using Choice = peg::sor<peg::if_apply<Items, Items>...>;
    peg::parse<peg::star<Choice>>(input, visit);
    return text.substr(text.size() - input.size());
}

template<class Rule>
bool matches(std::string_view text) {
    return peg::parse<peg::seq<Rule, peg::eof>>(Input(text));
}

// Consume one nonempty lexical production. Failure leaves the caller's view intact.
template<class Rule>
std::string_view take(std::string_view& text) {
    Input input(text);
    if (!peg::parse<Rule>(input)) return {};
    const auto match = text.substr(0, text.size() - input.size());
    text.remove_prefix(match.size());
    return match;
}

// ABC 2.1 sections 8.2 and 14.1: mnemonic and entity spellings of the same character.
struct TextCharacter {
    std::string_view mnemonic;
    std::string_view entity;
    u32              code;
};
constexpr TextCharacter kTextCharacters[]{
    {"`A", "Agrave", 0x00C0}, {"`a", "agrave", 0x00E0}, {"`E", "Egrave", 0x00C8},
    {"`e", "egrave", 0x00E8}, {"`I", "Igrave", 0x00CC}, {"`i", "igrave", 0x00EC},
    {"`O", "Ograve", 0x00D2}, {"`o", "ograve", 0x00F2}, {"`U", "Ugrave", 0x00D9},
    {"`u", "ugrave", 0x00F9}, {"'A", "Aacute", 0x00C1}, {"'a", "aacute", 0x00E1},
    {"'E", "Eacute", 0x00C9}, {"'e", "eacute", 0x00E9}, {"'I", "Iacute", 0x00CD},
    {"'i", "iacute", 0x00ED}, {"'O", "Oacute", 0x00D3}, {"'o", "oacute", 0x00F3},
    {"'U", "Uacute", 0x00DA}, {"'u", "uacute", 0x00FA}, {"'Y", "Yacute", 0x00DD},
    {"'y", "yacute", 0x00FD}, {"^A", "Acirc", 0x00C2},  {"^a", "acirc", 0x00E2},
    {"^E", "Ecirc", 0x00CA},  {"^e", "ecirc", 0x00EA},  {"^I", "Icirc", 0x00CE},
    {"^i", "icirc", 0x00EE},  {"^O", "Ocirc", 0x00D4},  {"^o", "ocirc", 0x00F4},
    {"^U", "Ucirc", 0x00DB},  {"^u", "ucirc", 0x00FB},  {"^Y", "Ycirc", 0x0176},
    {"^y", "ycirc", 0x0177},  {"~A", "Atilde", 0x00C3}, {"~a", "atilde", 0x00E3},
    {"~N", "Ntilde", 0x00D1}, {"~n", "ntilde", 0x00F1}, {"~O", "Otilde", 0x00D5},
    {"~o", "otilde", 0x00F5}, {"\"A", "Auml", 0x00C4},  {"\"a", "auml", 0x00E4},
    {"\"E", "Euml", 0x00CB},  {"\"e", "euml", 0x00EB},  {"\"I", "Iuml", 0x00CF},
    {"\"i", "iuml", 0x00EF},  {"\"O", "Ouml", 0x00D6},  {"\"o", "ouml", 0x00F6},
    {"\"U", "Uuml", 0x00DC},  {"\"u", "uuml", 0x00FC},  {"\"Y", "Yuml", 0x0178},
    {"\"y", "yuml", 0x00FF},  {"cC", "Ccedil", 0x00C7}, {"cc", "ccedil", 0x00E7},
    {"AA", "Aring", 0x00C5},  {"aa", "aring", 0x00E5},  {"/O", "Oslash", 0x00D8},
    {"/o", "oslash", 0x00F8}, {"uA", "Abreve", 0x0102}, {"ua", "abreve", 0x0103},
    {"uE", "", 0x0114},       {"ue", "", 0x0115},       {"vS", "Scaron", 0x0160},
    {"vs", "scaron", 0x0161}, {"vZ", "Zcaron", 0x017D}, {"vz", "zcaron", 0x017E},
    {"HO", "", 0x0150},       {"Ho", "", 0x0151},       {"HU", "", 0x0170},
    {"Hu", "", 0x0171},       {"AE", "AElig", 0x00C6},  {"ae", "aelig", 0x00E6},
    {"OE", "OElig", 0x0152},  {"oe", "oelig", 0x0153},  {"ss", "szlig", 0x00DF},
    {"DH", "ETH", 0x00D0},    {"dh", "eth", 0x00F0},    {"TH", "THORN", 0x00DE},
    {"th", "thorn", 0x00FE},  {"", "quot", '"'},        {"", "amp", '&'},
    {"", "copy", 0x00A9},
};

// Unknown names and invalid Unicode scalars retain the entire written escape.
bool decode_character(std::string_view& text, TextContext context, std::string& out) {
    using namespace grammar;
    if (text.starts_with("\\n")) {
        out.append(context == TextContext::VoiceName ? "\n" : "\\n");
        text.remove_prefix(2);
    } else if (auto raw = take<text_unicode>(text); !raw.empty()) {
        u32 value = 0;
        std::from_chars(raw.data() + 2, raw.data() + raw.size(), value, 16);
        if (value > 0x10ffff || (value >= 0xd800 && value <= 0xdfff))
            out.append(raw);
        else
            ::utf8::append(value, out);
    } else if (auto raw = take<sor<text_mnemonic, text_entity>>(text); !raw.empty()) {
        const bool entity = raw.front() == '&';
        const auto name   = entity ? raw.substr(1, raw.size() - 2) : raw.substr(1);
        const auto found  = std::ranges::find(
            kTextCharacters, name, entity ? &TextCharacter::entity : &TextCharacter::mnemonic
        );
        if (found == std::end(kTextCharacters))
            out.append(raw);
        else
            ::utf8::append(found->code, out);
    } else if (auto raw = take<text_escape>(text); !raw.empty()) {
        out.push_back(raw.back());
    } else {
        return false;
    }
    return true;
}

std::string decoded_text(std::string_view text, TextContext context) {
    using namespace grammar;
    std::string out;
    out.reserve(text.size());
    while (!text.empty()) {
        if (decode_character(text, context, out)) continue;
        if (context != TextContext::ChordSymbol && !take<text_dollar>(text).empty()) {
            out.push_back('$');
        } else if (context != TextContext::ChordSymbol && !take<text_font_switch>(text).empty()) {
            continue;
        } else if (context == TextContext::Lyric && !take<text_lyric_dash>(text).empty()) {
            out.push_back('-');
        } else if (context == TextContext::Lyric && text.front() == '~') {
            out.push_back(' ');
            text.remove_prefix(1);
        } else {
            const auto end
                = text.find_first_of(context == TextContext::Lyric ? "\\&$~" : "\\&$", 1);
            out.append(text.substr(0, end));
            text.remove_prefix(std::min(end, text.size()));
        }
    }
    return out;
}

struct NoteAlignment {
    size_t                  notes;
    std::span<const size_t> bars;
    size_t                  next   = 0;
    bool                    excess = false;

    // Whitespace, a blank slot and a bar jump have the same meaning on w: and s: rows.
    bool skip_layout(std::string_view& text) {
        if (text.front() == '|') {
            const auto bar = std::ranges::lower_bound(bars, next);
            next           = bar == bars.end() ? notes : *bar;
            if (bar != bars.end())
                bars = bars.subspan(static_cast<size_t>(bar - bars.begin()) + 1);
            else
                bars = {};
        } else if (text.front() == '*') {
            take_note();
        } else if (kWhitespace.find(text.front()) == kWhitespace.npos) {
            return false;
        }
        text.remove_prefix(1);
        return true;
    }

    std::optional<size_t> take_note() {
        if (next < notes) return next++;
        excess = true;
        return {};
    }
};

Location locate_fragment(std::span<const SourceFragment> fragments, size_t offset, Location start) {
    if (fragments.empty()) {
        start.column += offset;
        return start;
    }
    const auto  after    = std::ranges::upper_bound(fragments, offset, {}, &SourceFragment::offset);
    const auto& fragment = *std::prev(after);
    auto        at       = fragment.at;
    if (fragment.generated)
        at.expansion_offset += offset - fragment.offset;
    else
        at.column += offset - fragment.offset;
    return at;
}

bool written_before(Location left, Location right) {
    if (const auto order = left <=> right; order != 0) return order < 0;
    return left.expansion_offset < right.expansion_offset;
}

void assign_written(std::span<Token> tokens, size_t& next) {
    // A completed batch contains interrupted fields and every member of its groups.
    // Their emission order can differ from written order, including at a group host.
    struct Occurrence {
        Location* destination;
        Location  order;
    };
    vec<Occurrence> occurrences;
    const auto      collect_atom = [&](Atom& atom) {
        for (auto& note : atom.notes)
            for (auto& mark : note.decorations) occurrences.push_back({&mark.at, mark.at});
    };
    for (auto& token : tokens) {
        occurrences.push_back({&token.at, token.written_end.value_or(token.at)});
        if (auto* atom = std::get_if<Atom>(&token.value)) collect_atom(*atom);
        if (auto* grace = std::get_if<Grace>(&token.value))
            for (auto& attack : grace->attacks) collect_atom(attack.atom);
    }
    std::ranges::stable_sort(occurrences, [](const Occurrence& left, const Occurrence& right) {
        return written_before(left.order, right.order);
    });
    for (auto& occurrence : occurrences) occurrence.destination->written = WrittenPosition{++next};
}

void check_decoration(
    std::string_view text, DecorationDelimiter delimiter, Location at, Diagnostics& diagnostics
) {
    if (text.size() == 1 || delimiter == DecorationDelimiter::Either) return;
    const char expected = delimiter == DecorationDelimiter::Bang ? '!' : '+';
    if (text.front() != expected)
        diagnostics.error(
            at, "decoration-delimiter", fmt::format("expected '{}' decoration delimiters", expected)
        );
}

enum class BangRole { Decoration, Layout, Legacy };

struct MusicDialect {
    DecorationDelimiter delimiter;
    BangRole            bang;
    bool                strict;
};

std::optional<AbcVersion> abc_version(std::string_view text) {
    const auto dot = text.find('.');
    if (dot == text.npos) return {};
    const auto major = integer(text.substr(0, dot));
    const auto minor = integer(text.substr(dot + 1));
    if (!major || !minor || *major < 0 || *minor < 0) return {};
    return AbcVersion{*major, *minor};
}


// Apply U: definitions at the written site as each music token is assembled.
// Fields inside a continued chord are emitted before the chord. Its earlier
// members still see the definition in effect at their own physical position.
struct Shorthands {
    DecorationDelimiter delimiter;
    Diagnostics&        diagnostics;

    void                           define(Token& token);
    [[nodiscard]] std::string_view decoration(std::string_view raw, Location at) const {
        check_decoration(raw, delimiter, at, diagnostics);
        if (raw.size() != 1) return raw;
        const auto& history = definitions[static_cast<u8>(raw.front())];
        const auto  after   = std::upper_bound(
            history.begin(), history.end(), at, [](Location key, const Binding& binding) {
                return written_before(key, binding.at);
            }
        );
        return after == history.begin() ? raw : std::prev(after)->spelling;
    }

    struct Binding {
        Location         at;
        std::string_view spelling;
    };
    // Single-character decorations are ASCII by grammar::decoration.
    std::array<vec<Binding>, 128> definitions{};
};

void Shorthands::define(Token& token) {
    const auto* field = std::get_if<Field>(&token.value);
    if (!field || field->kind != 'U') return;
    auto       spelling  = field->value;
    const auto shorthand = take<grammar::symbol_shorthand>(spelling);
    if (shorthand.empty() || take<grammar::equal>(spelling).empty()
        || !matches<grammar::symbol_expansion>(spelling)) {
        diagnostics.warn(
            token.at,
            "invalid-symbol",
            "U: needs a shorthand followed by '=' and a decoration or annotation"
        );
        return;
    }
    if (!spelling.starts_with('"')) check_decoration(spelling, delimiter, token.at, diagnostics);
    definitions[static_cast<u8>(shorthand.front())].push_back({token.at, spelling});
    token.value = SymbolDefinition{shorthand.front(), spelling};
}

struct DialectSettings {
    std::optional<std::pair<Location, bool>>                linebreak;
    std::optional<std::pair<Location, DecorationDelimiter>> decoration;

    void read(const Token& token, Diagnostics& diagnostics) {
        auto args = token.information();
        if (!args) return;
        const auto name  = word(*args);
        const auto value = trim(*args);
        if (name == "decoration") {
            if (value == "!" || value == "+")
                decoration = std::pair{
                    token.at, value == "!" ? DecorationDelimiter::Bang : DecorationDelimiter::Plus
                };
            else
                diagnostics.warn(
                    token.at, "invalid-decoration-dialect", "expected '!' or '+' after decoration"
                );
        } else if (name == "linebreak") {
            if (matches<grammar::linebreak_setting>(value))
                linebreak = std::pair{token.at, value.find('!') != value.npos};
            else
                diagnostics.warn(
                    token.at,
                    "invalid-linebreak",
                    "expected <none> or one or more of <EOL>, '$' and '!'"
                );
        } else if (
            name == "continueall" && std::holds_alternative<Directive>(token.value)
            && (value == "true" || value == "false")
        ) {
            linebreak = std::pair{token.at, value == "true"};
        }
    }
};

MusicDialect read_music_dialect(const Tune& tune, Diagnostics& diagnostics) {
    DialectSettings settings;
    for (const auto& token : tune.header) settings.read(token, diagnostics);
    const auto& [linebreak, decoration] = settings;
    const bool strict                   = tune.version >= AbcVersion{2, 1};
    const auto default_delimiter = strict ? DecorationDelimiter::Bang : DecorationDelimiter::Either;
    // A current bang line break implies '+'; replacing that setting removes its implication.
    if (linebreak && linebreak->second) {
        if (decoration && decoration->second == DecorationDelimiter::Bang
            && decoration->first > linebreak->first) {
            diagnostics.warn(
                decoration->first,
                "conflicting-decoration-dialect",
                "'!' is already a line break; kept '+' decoration delimiters"
            );
        }
        return {DecorationDelimiter::Plus, BangRole::Layout, strict};
    }
    return {
        decoration ? decoration->second : default_delimiter,
        !linebreak && !strict ? BangRole::Legacy : BangRole::Decoration,
        strict
    };
}

bool is_include(std::string_view name) { return name == "abc-include" || name == "include"; }

bool header_only_instruction(std::string_view value, Location at, Diagnostics& diagnostics) {
    const auto name = word(value);
    if (name == "abc-charset") {
        diagnostics.warn(
            at, "header-only-instruction", "ignored abc-charset outside the file header"
        );
        return true;
    }
    if (name != "abc-version" && name != "linebreak" && name != "decoration"
        && name != "continueall" && !is_include(name))
        return false;
    diagnostics.warn(
        at, "header-only-instruction", fmt::format("ignored '{}' outside the header", name)
    );
    return true;
}

enum class FieldSite { FileHeader, TuneHeader, Body, InlineMusic };

bool field_allowed(char kind, FieldSite site) {
    // ABC 2.1 section 3. Accept deprecated E: in headers to give its own warning.
    constexpr std::string_view kFileFields   = "ABCDEFGHILMmNORrSUZ";
    constexpr std::string_view kTuneFields   = "KPQTVWX";
    constexpr std::string_view kInlineFields = "IKLMmNPQRrUV";
    const auto has = [kind](std::string_view fields) { return fields.find(kind) != fields.npos; };
    switch (site) {
    case FieldSite::FileHeader: return has(kFileFields);
    case FieldSite::TuneHeader: return has(kFileFields) || has(kTuneFields);
    case FieldSite::Body: return has(kInlineFields) || has("sTWw");
    case FieldSite::InlineMusic: return has(kInlineFields);
    }
    return false;
}

// Both physical and inline fields use grammar::field_key. Keep the original
// value whitespace until any physical field continuations have been assembled.
Field split_field(std::string_view text) { return {text.front(), text.substr(2)}; }

std::optional<Field> line_field(std::string_view text) {
    if (!peg::parse<grammar::field_key>(Input(text))) return {};
    return split_field(text);
}

bool starts_with_repeat(std::string_view text) {
    Input input(text);
    return peg::parse<peg::seq<
        peg::sor<grammar::letter, grammar::rest_letter>,
        peg::at<peg::one<':'>>,
        grammar::repeat_bar>>(input);
}

// A backslash quotes the next byte. A bare percent starts a comment even inside quotes.
std::string_view without_comment(std::string_view line, bool& quoted) {
    for (size_t i = 0; (i = line.find_first_of("\\\"%", i)) != line.npos; ++i) {
        if (line[i] == '%') return line.substr(0, i);
        if (line[i] == '"')
            quoted = !quoted;
        else if (i + 1 < line.size())
            ++i;
    }
    return line;
}

std::string_view without_comment(std::string_view line) {
    bool quoted = false;
    return without_comment(line, quoted);
}

// Joined and rewritten music keeps its opening lyric line and the physical origin of each span.
struct LogicalLine {
    std::string_view                bytes;
    size_t                          first_line;
    std::span<const SourceFragment> fragments{};

    [[nodiscard]] Location where(std::string_view part) const {
        const auto offset = static_cast<size_t>(part.data() - bytes.data());
        return locate_fragment(fragments, offset, {first_line, 1});
    }
};

// Both music rewrites retain physical positions for every copied span.
struct RewrittenMusic {
    std::string         text;
    vec<SourceFragment> fragments;

    void append(LogicalLine source, size_t begin, size_t end) {
        if (begin == end) return;
        const auto piece = source.bytes.substr(begin, end - begin);
        const auto start = text.size();
        const auto after
            = std::ranges::upper_bound(source.fragments, begin, {}, &SourceFragment::offset);
        const bool generated = after != source.fragments.begin() && std::prev(after)->generated;
        fragments.push_back({start, source.where(piece), generated});
        for (auto it = after; it != source.fragments.end() && it->offset < end; ++it)
            fragments.push_back({start + it->offset - begin, it->at, it->generated});
        text.append(piece);
    }
};

// Match the bytes to remove only outside quoted text, fields and decorations.
// A physical continuation can start inside a quote opened on the preceding line.
template<class Removed>
std::optional<RewrittenMusic> elide_music(
    LogicalLine line, BangRole bang, bool quoted_prefix = false
) {
    using namespace grammar;
    auto rest = line.bytes;
    if (rest.find("[r:") == rest.npos
        && (std::is_same_v<Removed, inline_remark> || rest.find_first_of("#*;?@") == rest.npos))
        return {};
    if (quoted_prefix && take<seq<quote_body, chars<'"'>>>(rest).empty()) return {};
    RewrittenMusic result;
    size_t         copied = 0;
    while (!rest.empty()) {
        const auto next = rest.find_first_of("[\"!+#*;?@");
        if (next == rest.npos) break;
        rest.remove_prefix(next);
        if (const auto removed = take<Removed>(rest); !removed.empty()) {
            const auto end = line.bytes.size() - rest.size();
            result.append(line, copied, end - removed.size());
            copied = end;
            continue;
        }
        if (!take<music_text>(rest).empty()) continue;
        if ((bang != BangRole::Layout || rest.front() == '+')
            && !take<named_decoration>(rest).empty())
            continue;
        rest.remove_prefix(1);
    }
    if (!copied) return {};
    result.append(line, copied, line.bytes.size());
    return result;
}

const Field* macro_definition(const Token& token) {
    const auto* field = std::get_if<Field>(&token.value);
    return field && field->kind == 'm' ? field : nullptr;
}

// Section 9 expands music text before the grammar binds U: shorthands.
// Definitions are local to a tune; replacement text is not expanded again.
class MusicMacros {
public:
    explicit MusicMacros(Diagnostics& diagnostics) : diagnostics(diagnostics) {}

    void define(std::string_view value, Location at);

    [[nodiscard]] std::optional<RewrittenMusic> expand(
        LogicalLine line, MusicDialect dialect, std::span<const Token> entries
    );

private:
    struct Match;
    struct Rule {
        std::string_view target;
        std::string_view replacement;
        size_t           note;

        Match match(std::string_view text, Location at, Diagnostics& diagnostics) const;
    };
    struct Match {
        const Rule* rule   = nullptr;
        size_t      length = 0;
        i64         step   = 0;
    };

    Diagnostics&              diagnostics;
    std::map<char, vec<Rule>> rules;

    Match find(std::string_view text, Location at);
    void  replace(const Match& match, MusicDialect dialect, Location at, std::string& text);
};

size_t macro_separator(std::string_view value) {
    // Section 9 allows '=' in the target, for example as a natural sign.
    // A separate '=' disambiguates it from the compact target=value form.
    Input spaced(value);
    if (peg::parse<grammar::macro_to_separator<grammar::macro_spaced_equal>>(spaced))
        return value.size() - spaced.size() + 1;

    using Equal   = grammar::chars<'='>;
    using ToEqual = grammar::macro_to_separator<Equal>;
    Input compact(value);
    if (!peg::parse<ToEqual>(compact)) return value.npos;
    const auto first  = value.size() - compact.size();
    const auto prefix = trim(value.substr(0, first));
    // A compact turn target may start with a natural, e.g. ~=a3=a3.
    if (compact.size() > 1 && note_letter(value[first + 1])
        && (prefix.empty() || (prefix.size() == first && prefix.ends_with('~')))) {
        return peg::parse<peg::seq<Equal, ToEqual>>(compact) ? value.size() - compact.size() :
                                                               value.npos;
    }
    return first;
}

void MusicMacros::define(std::string_view value, Location at) {
    value             = trim(value);
    const auto equal  = macro_separator(value);
    const auto target = trim(value.substr(0, equal));
    const auto replacement
        = equal == value.npos ? std::string_view{} : trim(value.substr(equal + 1));
    const auto note = target.find('n');
    if (!matches<grammar::macro_text<31>>(target) || !matches<grammar::macro_text<200>>(replacement)
        || (note != target.npos && !matches<grammar::number>(target.substr(note + 1)))) {
        diagnostics.warn(at, "invalid-macro", "ignored malformed m: definition");
        return;
    }
    auto&      bucket = rules[target.front()];
    const auto old    = std::ranges::find(bucket, target, &Rule::target);
    if (old != bucket.end()) bucket.erase(old);
    bucket.push_back({target, replacement, note});
}

// A macro may match a whole quoted or decorated token, but does not match
// inside one. Fields and their payloads are not part of the music text.
size_t macro_opaque_prefix(std::string_view text, MusicDialect dialect) {
    Input input(text);
    if (text.starts_with('"')) {
        if (!peg::parse<grammar::quoted>(input)) return text.size();
    } else if (
        text.starts_with('+') || (text.starts_with('!') && dialect.bang != BangRole::Layout)
    ) {
        peg::parse<grammar::named_decoration>(input);
    } else if (text.starts_with('[')) {
        peg::parse<peg::opt<grammar::inline_field>>(input);
    } else if (std::string_view("^_=").find(text.front()) != text.npos) {
        // A transposing placeholder matches a pitch without an accidental.
        peg::parse<grammar::pitch>(input);
    }
    return text.size() - input.size();
}

constexpr std::string_view kDiatonicLetters = "CDEFGABcdefgab";

i64 octave_shift(std::string_view marks) {
    i64 shift = 0;
    for (char mark : marks) shift += mark == '\'' ? 1 : -1;
    return shift;
}

MusicMacros::Match MusicMacros::Rule::match(
    std::string_view text, Location at, Diagnostics& diagnostics
) const {
    diagnostics.spend(1, at);
    const auto prefix = target.substr(0, note);
    if (!text.starts_with(prefix)) return {};
    auto tail = text.substr(prefix.size());
    i64  step = 0;
    if (note != target.npos) {
        const auto spelling = take<grammar::bare_pitch>(tail);
        if (spelling.empty()) return {};
        diagnostics.spend(PlaybackWork::text(spelling), at);
        const auto suffix = target.substr(note + 1);
        if (!tail.starts_with(suffix)) return {};
        tail.remove_prefix(suffix.size());
        step = static_cast<i64>(kDiatonicLetters.find(spelling.front()))
               + 7 * octave_shift(spelling.substr(1));
    }
    // A target such as ~G3 must not match the written lengths 30 or 3/2.
    if (digit(target.back()) && !tail.empty() && (digit(tail.front()) || tail.front() == '/'))
        return {};
    return {this, text.size() - tail.size(), step};
}

MusicMacros::Match MusicMacros::find(std::string_view text, Location at) {
    Match      best;
    const auto consider = [&](char key) {
        const auto bucket = rules.find(key);
        if (bucket == rules.end()) return;
        for (const auto& rule : bucket->second) {
            const auto candidate = rule.match(text, at, diagnostics);
            if (candidate.length >= best.length) best = candidate;
        }
    };
    if (note_letter(text.front())) consider('n');
    consider(text.front());
    return best;
}

void MusicMacros::replace(
    const Match& match, MusicDialect dialect, Location at, std::string& text
) {
    auto value = match.rule->replacement;
    diagnostics.spend(value.size(), at);
    if (match.rule->note == match.rule->target.npos) {
        text.append(value);
        return;
    }
    while (!value.empty()) {
        if (const auto opaque = macro_opaque_prefix(value, dialect); opaque != 0) {
            text.append(value.substr(0, opaque));
            value.remove_prefix(opaque);
            continue;
        }
        const auto ch = value.front();
        value.remove_prefix(1);
        if (ch < 'h' || ch > 'z') {
            text.push_back(ch);
            continue;
        }
        const auto step   = match.step + (ch - 'n');
        const auto degree = (step % 7 + 7) % 7;
        const auto octave = (step - degree) / 7;
        text.push_back(kDiatonicLetters[static_cast<size_t>(degree + (octave > 0 ? 7 : 0))]);
        const auto marks
            = static_cast<size_t>(octave < 0 ? -octave : (octave > 0 ? octave - 1 : 0));
        diagnostics.spend(marks, at);
        text.append(marks, octave < 0 ? ',' : '\'');
    }
}

std::optional<RewrittenMusic> MusicMacros::expand(
    LogicalLine line, MusicDialect dialect, std::span<const Token> entries
) {
    if (rules.empty() && line.bytes.find("[m:") == line.bytes.npos
        && std::ranges::none_of(entries, macro_definition))
        return {};

    auto       next              = entries.begin();
    const auto apply_definitions = [&](auto stop) {
        while (next != entries.end() && stop(*next)) {
            if (const auto* field = macro_definition(*next)) define(field->value, next->at);
            ++next;
        }
    };

    RewrittenMusic result;
    size_t         copied = 0;

    for (size_t offset = 0; offset < line.bytes.size();) {
        const auto text = line.bytes.substr(offset);
        const auto at   = line.where(text);
        apply_definitions([at](const Token& entry) { return entry.at < at; });
        if (text.starts_with('[')) {
            auto tail = text;
            if (const auto field = take<grammar::inline_field>(tail); !field.empty()) {
                if (field.starts_with("[m:")) define(field.substr(3, field.size() - 4), at);
                offset += field.size();
                continue;
            }
            if (peg::parse<grammar::inline_field_start>(Input(text))) break;
        }
        const auto match = find(text, at);
        if (match.rule) {
            result.append(line, copied, offset);
            result.fragments.push_back({result.text.size(), at, true});
            replace(match, dialect, at, result.text);
            offset += match.length;
            copied = offset;
        } else {
            offset += std::max<size_t>(1, macro_opaque_prefix(text, dialect));
        }
    }
    apply_definitions([](const Token&) { return true; });
    if (result.fragments.empty()) return {};
    result.append(line, copied, line.bytes.size());
    return result;
}

// Resolve aliases at token emission, in written order. Callers see ordinary
// MIDI directives at the use site, or no token if nothing expands.
struct MidiAliases {
    std::deque<std::string>&                     text;
    const std::optional<Location>&               reference;
    Diagnostics&                                 diagnostics;
    std::map<std::string_view, std::string_view> definitions{};

    void append(Token token, vec<Token>& output);
};

void MidiAliases::append(Token token, vec<Token>& output) {
    const auto instruction = trim(token.information().value_or(""));
    const auto end         = std::ranges::find_if_not(instruction, letter);
    const auto name        = instruction.substr(0, static_cast<size_t>(end - instruction.begin()));
    if (name != "MIDIdef" && name != "MIDIx") {
        output.push_back(std::move(token));
        return;
    }
    if (!reference && (name == "MIDIx" || std::holds_alternative<Field>(token.value))) {
        diagnostics.warn(
            token.at,
            "midi-alias-placement",
            "ignored MIDI alias before X:; only %%MIDIdef can go in the file header"
        );
        return;
    }
    auto args = instruction.substr(name.size());
    if (name == "MIDIdef") {
        const auto alias = word(args);
        args             = trim(args);
        if (alias.empty() || args.empty())
            diagnostics.warn(
                token.at, "invalid-midi-alias", "ignored MIDIdef without a name and command"
            );
        else if (!definitions.contains(alias))
            definitions.emplace(alias, text.emplace_back(fmt::format("MIDI {}", args)));
        return;
    }
    if (trim(args).empty()) {
        diagnostics.warn(token.at, "invalid-midi-alias", "expected one or more names after MIDIx");
        return;
    }
    bool have_controller = false;
    for (auto name = word(args); !name.empty(); name = word(args)) {
        const auto match = definitions.find(name);
        if (match == definitions.end()) {
            diagnostics.warn(
                token.at,
                "undefined-midi-alias",
                fmt::format("stopped MIDIx at '{}': no preceding MIDIdef for that name", name)
            );
            return;
        }
        auto command = match->second;
        word(command);   // The stored alias starts with MIDI.
        const bool controller = word(command) == "controlstring";
        const bool combine    = controller && have_controller;
        if (controller) have_controller = true;
        diagnostics.spend(combine ? 2 : 1, token.at);
        if (combine) output.push_back({token.at, Directive{"MIDI controlcombo"}, token.music_line});
        output.push_back({token.at, Directive{match->second}, token.music_line});
    }
}

i32 written_number(std::string_view text, i32 fallback, Location at, Diagnostics& diagnostics) {
    if (text.empty()) return fallback;
    if (const auto parsed = integer(text)) return *parsed;
    diagnostics.error(at, "number-range", "expected an integer in range");
}

struct FractionFields {
    std::string_view numerator;
    std::string_view denominator;
    size_t           slashes;

    // The first slash introduces the divisor. Each of up to 30 more doubles it.
    [[nodiscard]] i64 scaled_divisor(i32 base) const {
        return static_cast<i64>(base) * (u32{1} << (slashes - 1));
    }
};

FractionFields fraction_fields(std::string_view text) {
    const auto first = text.find('/');
    if (first == text.npos) return {text, {}, 0};
    const auto end = std::min(text.size(), text.find_first_not_of('/', first));
    return {text.substr(0, first), text.substr(end), end - first};
}

i32 accidental_quantity(
    std::string_view text, i32 fallback, Location at, Diagnostics& diagnostics
) {
    if (text.empty()) return fallback;
    const auto number = integer(text);
    if (!number || *number < 0)
        diagnostics.error(at, "invalid-microtone", "expected an accidental such as ^/ or _3/4");
    return *number;
}

Alteration microtone(std::string_view text, i32 direction, Location at, Diagnostics& diagnostics) {
    const auto parts     = fraction_fields(text);
    const auto numerator = accidental_quantity(parts.numerator, 1, at, diagnostics);
    if (parts.slashes > 31)
        diagnostics.error(at, "microtone-range", "microtone denominator is too large");
    const auto denominator
        = parts.slashes == 0 ?
              1 :
              parts.scaled_divisor(accidental_quantity(parts.denominator, 2, at, diagnostics));
    if (denominator == 0) diagnostics.error(at, "invalid-microtone", "microtone divides by zero");
    if (denominator > std::numeric_limits<i32>::max() || numerator / denominator > 127)
        diagnostics.error(at, "microtone-range", "microtone quantity is too large");
    return {direction * numerator, static_cast<i32>(denominator)};
}

std::optional<Alteration> written_accidental(
    std::string_view text, Location at, Diagnostics& diagnostics
) {
    using namespace std::literals;
    constexpr std::array kNormal{"__"sv, "_"sv, "="sv, "^"sv, "^^"sv};
    if (const auto found = std::ranges::find(kNormal, text); found != kNormal.end())
        return Alteration{static_cast<i32>(found - kNormal.begin()) - 2};
    if (text.find_first_not_of("^_=") == 1 && text.front() != '=')
        return microtone(text.substr(1), text.front() == '_' ? -1 : 1, at, diagnostics);
    diagnostics.warn(
        at, "invalid-accidental", "ignored ambiguous accidental; use a single sign for microtones"
    );
    return {};
}

// Consume the spelling from a matched pitch, leaving its written length and tie.
Pitch read_spelling(std::string_view& text, const auto& locate, Diagnostics& diagnostics) {
    const auto prefix = take<grammar::accidental>(text);
    const auto name   = text.front();
    text.remove_prefix(1);
    Pitch pitch{upper(name), lower(name), {}, text.ends_with('-')};
    if (!prefix.empty()) pitch.accidental = written_accidental(prefix, locate(prefix), diagnostics);
    const auto octave = take<grammar::octave>(text);
    if (octave.size() > 32)
        diagnostics.error(locate(octave), "pitch-range", "too many octave marks");
    pitch.octave += static_cast<i32>(octave_shift(octave));
    return pitch;
}

RepeatEndings ending_passes(std::string_view text, LogicalLine line, Diagnostics& diagnostics) {
    const auto endpoint = [&] {
        const auto raw  = take<grammar::number>(text);
        const auto pass = integer(raw);
        if (!pass || *pass < 1)
            diagnostics.error(line.where(raw), "ending-range", "expected a positive ending number");
        return *pass;
    };
    vec<EndingRange> ranges;
    while (!text.empty() && digit(text.front())) {
        const auto start = text;
        const auto first = endpoint();
        const auto last  = take<grammar::chars<'-'>>(text).empty() ? first : endpoint();
        if (first > last)
            diagnostics.error(
                line.where(start), "invalid-ending", "repeat ending range is reversed"
            );
        ranges.push_back({first, last});
        if (take<grammar::chars<','>>(text).empty()) break;
    }
    return RepeatEndings(std::move(ranges));
}

Bar written_bar(std::string_view text, LogicalLine source, Diagnostics& diagnostics) {
    take<grammar::chars<'.'>>(text);
    auto       sign = take<grammar::bar_sign>(text);
    Bar        bar{.endings = ending_passes(text, source, diagnostics)};
    const auto closing = take<grammar::colons>(sign);
    if (sign.empty()) {
        // ABC 2.1 section 4.8 treats bare :: as :| followed by |:.
        if (closing.size() != 2)
            diagnostics.warn(
                source.where(closing), "ambiguous-repeat", "treated bare colon run as ::"
            );
        bar.repeat_start = bar.repeat_end = 2;
    } else {
        const auto opening = sign.find(':');
        if (!closing.empty()) bar.repeat_end = closing.size() + 1;
        if (opening != sign.npos) bar.repeat_start = sign.size() - opening + 1;
        bar.section = !bar.repeat_end && !bar.repeat_start && sign != "|" && sign != "[|]";
    }
    return bar;
}

Tuplet written_tuplet(std::string_view text, Location at, Diagnostics& diagnostics) {
    text.remove_prefix(1);
    const auto argument = [&] {
        const auto number = take<grammar::number>(text);
        take<grammar::chars<':'>>(text);
        return written_number(number, 0, at, diagnostics);
    };
    return {argument(), argument(), argument()};
}

void invalid_byte(char c, Location at, Diagnostics& diagnostics) {
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
            fmt::format("ignored unexpected byte {:#04x}", static_cast<u8>(c))
        );
}

// Notes in music, chords and grace groups share lengths, ties and accidentals.
struct WrittenNotes {
    LogicalLine       source;
    const Shorthands& shorthands;
    Diagnostics&      diagnostics;

    [[nodiscard]] Length length(std::string_view raw) const {
        if (raw.empty()) return {};
        const auto at        = source.where(raw);
        const auto parts     = fraction_fields(raw);
        const auto numerator = written_number(parts.numerator, 1, at, diagnostics);
        if (parts.slashes == 0) return {numerator, 1};
        const auto denominator = written_number(parts.denominator, 2, at, diagnostics);
        if (denominator == 0)
            diagnostics.error(at, "zero-length-divisor", "duration divides by zero");
        if (parts.slashes > 31
            || parts.scaled_divisor(denominator) > std::numeric_limits<i32>::max())
            diagnostics.error(at, "length-range", "duration denominator is too large");
        // ABC 2.1 section 4.3 permits ratios such as 3/2 and 1/3. Quantize only at playback output.
        return {numerator, parts.scaled_divisor(denominator)};
    }

    [[nodiscard]] std::pair<Pitch, Length> pitched_member(std::string_view text) const {
        const auto pitch
            = read_spelling(text, [this](auto raw) { return source.where(raw); }, diagnostics);
        return {pitch, length(take<grammar::duration>(text))};
    }

    [[nodiscard]] Atom note(std::string_view text) const {
        const auto [pitch, duration] = pitched_member(text);
        return {{{pitch}}, duration, false, pitch.tie};
    }

    void rests(std::string_view raw, const auto& emit) const;

    [[nodiscard]] Atom chord(std::string_view text) const {
        using enum grammar::Item;
        Atom                 atom{.tie = text.ends_with('-'), .chord = true};
        vec<ChordDecoration> pending;
        const auto tail = scan(text.substr(1), grammar::chord_member{}, [&](auto kind, auto raw) {
            if (kind == Decoration) {
                const auto at = source.where(raw);
                pending.emplace_back(at, shorthands.decoration(raw, at));
            } else if (kind == Rest || kind == Spacer) {
                // Silent members still have to carry a valid written rest length.
                if (kind == Rest) (void)length(raw.substr(1));
                pending.clear();
            } else if (kind == Pitch) {
                if (atom.notes.size() >= 128)
                    diagnostics.error(
                        source.where(raw),
                        "group-limit",
                        "chords and grace groups support at most 128 notes"
                    );
                const auto [pitch, written] = pitched_member(raw);
                if (atom.notes.empty()) atom.length = written;
                auto& note
                    = atom.notes.emplace_back(pitch, std::nullopt, std::exchange(pending, {}));
                if (written.numerator * atom.length.denominator
                    != atom.length.numerator * written.denominator)
                    note.own_length = written;
            }
        });
        for (const auto& mark : pending)
            diagnostics.warn(
                mark.at, "orphan-decoration", "chord decoration has no following note"
            );
        auto       suffix_text = tail.substr(1);   // After the closing bracket.
        const auto suffix      = length(take<grammar::duration>(suffix_text));
        // ABC 2.1 section 4.17 multiplies each written length by the suffix.
        const auto apply_suffix = [suffix](Length& duration) {
            duration.numerator *= suffix.numerator;
            duration.denominator *= suffix.denominator;
        };
        apply_suffix(atom.length);
        for (auto& note : atom.notes)
            if (note.own_length) apply_suffix(*note.own_length);
        return atom;
    }

    [[nodiscard]] Broken broken_mark(std::string_view raw) const {
        if (raw.size() > 30)
            diagnostics.error(source.where(raw), "rhythm-range", "too many broken rhythm marks");
        return {raw.front() == '>' ? 1 : -1, static_cast<i32>(raw.size())};
    }

    [[nodiscard]] Grace grace(std::string_view text) const {
        Grace                                      group;
        size_t                                     count = 0;
        std::optional<std::pair<Broken, Location>> pending;
        text.remove_prefix(text.starts_with("{/") ? 2 : 1);
        scan(text, grammar::grace_member{}, [&](auto kind, auto raw) {
            if (kind == grammar::Item::Layout || kind == grammar::Item::Spacer) return;
            const auto at = source.where(raw);
            if (kind == grammar::Item::Broken) {
                const auto mark = broken_mark(raw);
                if (!group.attacks.empty() && !pending)
                    pending = {mark, at};
                else
                    diagnostics.warn(
                        at,
                        "unexpected-broken-rhythm",
                        "ignored a broken rhythm mark without a preceding grace attack"
                    );
                return;
            }
            auto attack = kind == grammar::Item::Chord ? chord(raw) : note(raw);
            if (attack.notes.empty()) {
                diagnostics.warn(at, "empty-chord", "ignored empty grace chord");
                return;
            }
            count += attack.notes.size();
            if (count > 128)
                diagnostics.error(at, "group-limit", "grace groups support at most 128 notes");
            f64 scale = 1;
            if (pending) {
                const auto [mark, at] = *pending;
                auto& previous        = group.attacks.back();
                if (previous.weight() == attack.length.fraction()) {
                    const auto first = mark.first_factor();
                    previous.rhythm_scale *= first;
                    scale = 2 - first;
                } else {
                    diagnostics.warn(
                        at,
                        "unequal-broken-rhythm",
                        "ignored broken rhythm between grace attacks of unequal length"
                    );
                }
                pending.reset();
            }
            group.attacks.push_back({std::move(attack), scale});
        });
        if (pending)
            diagnostics.warn(
                pending->second,
                "unfinished-broken-rhythm",
                "broken rhythm did not reach a following grace attack"
            );
        return group;
    }
};

void WrittenNotes::rests(std::string_view raw, const auto& emit) const {
    const auto suffix = raw.substr(1);
    if (lower(raw.front())) {
        emit(Atom{{}, length(suffix)});
        return;
    }
    const auto count = suffix.empty() ? std::optional{1} : integer(suffix);
    if (!count || *count < 1) {
        diagnostics.error(
            source.where(suffix),
            "invalid-measure-rest",
            "expected a positive integer measure count"
        );
    }

    // ABC 2.1 section 4.5: the internal bars have the same semantics as written bars.
    const auto at = source.where(raw);
    diagnostics.spend(2 * static_cast<size_t>(*count - 1) * PlaybackWork::kToken, at);
    for (i32 measure = 1; measure < *count; ++measure) {
        emit(Atom{.measure_rest = true});
        emit(Bar{});
    }
    emit(Atom{.measure_rest = true});
}

bool continuation(std::string_view part) {
    size_t count = 0;
    while (part.ends_with('\\')) {
        part.remove_suffix(1);
        ++count;
    }
    return count % 2 != 0;
}

// A continued music line owns its physical fragments and the fields that interrupt it.
// Macros consume definitions in written order before those fields reach token emission.
struct MusicBody {
    Tune&                   tune;
    Diagnostics&            diagnostics;
    MusicMacros&            macros;
    MidiAliases&            aliases;
    size_t&                 written_count;
    MusicDialect            dialect;
    Shorthands              shorthands{dialect.delimiter, diagnostics};
    RewrittenMusic          joined;
    vec<Token>              entries;
    std::optional<Location> continuation_at;
    bool                    quoted    = false;
    bool                    saw_music = false;

    void emit_field(Token token) {
        if (auto info = token.information();
            info && header_only_instruction(*info, token.at, diagnostics))
            return;
        shorthands.define(token);
        aliases.append(std::move(token), tune.body);
    }

    void field(Token token) {
        if (!joined.fragments.empty()) {
            entries.push_back(std::move(token));
            return;
        }
        if (const auto* definition = macro_definition(token))
            macros.define(definition->value, token.at);
        const auto first = tune.body.size();
        emit_field(std::move(token));
        assign_written(std::span(tune.body).subspan(first), written_count);
    }

    void emit_token(
        grammar::Item kind, std::string_view raw, const WrittenNotes& notes, bool last_on_line
    );
    void read_tokens(LogicalLine line);

    void emit_music(LogicalLine line) {
        vec<SourceFragment> positions;
        const auto          rewrite = [&](std::optional<RewrittenMusic> replacement) {
            if (!replacement) return;
            line.bytes     = tune.owned_text.emplace_back(std::move(replacement->text));
            positions      = std::move(replacement->fragments);
            line.fragments = positions;
        };
        // Macros see literal reserved characters, but remarks have already disappeared.
        rewrite(elide_music<grammar::inline_remark>(line, dialect.bang));
        rewrite(macros.expand(line, dialect, entries));
        rewrite(elide_music<grammar::playback_elision>(line, dialect.bang));
        read_tokens(line);
    }

    void flush() {
        if (joined.fragments.empty()) return;
        const auto& bytes = tune.owned_text.emplace_back(std::move(joined.text));
        emit_music({bytes, joined.fragments.front().at.line, joined.fragments});
        joined = {};
    }

    bool blank_line() {
        flush();
        if (continuation_at)
            diagnostics.warn(
                *continuation_at, "stray-continuation", "cannot continue across a blank line"
            );
        continuation_at.reset();
        quoted = false;
        return saw_music || dialect.strict;
    }

    void music(std::string_view bytes, size_t line, bool has_newline, bool ends_quoted) {
        continuation_at.reset();
        if (has_newline && bytes.find('\\') != bytes.npos) {
            // Section 2.2.5 removes remarks before testing for a continuation.
            LogicalLine physical{bytes, line};
            const auto  clean = elide_music<grammar::inline_remark>(physical, dialect.bang, quoted);
            if (clean) physical = {clean->text, line, clean->fragments};
            if (continuation(trim(physical.bytes))) {
                const auto last = physical.bytes.find_last_not_of(kWhitespace);
                continuation_at = physical.where(physical.bytes.substr(last, 1));
                if (clean)
                    without_comment(physical.bytes.substr(0, last), quoted);
                else
                    quoted = ends_quoted;
            }
        }
        if (!continuation_at && joined.fragments.empty()) {
            emit_music({bytes, line});
            return;
        }
        joined.fragments.push_back({joined.text.size(), {line, 1}});
        joined.text.append(continuation_at ? bytes.substr(0, continuation_at->column - 1) : bytes);
        if (!continuation_at) {
            flush();
            quoted = false;
        }
    }
};

std::optional<Overlay> written_overlay(
    std::string_view raw, Location at, Diagnostics& diagnostics
) {
    if (raw == "(&") return Overlay::Start;
    if (raw == "&)") return Overlay::End;
    if (raw == "&&") return Overlay::PreviousTwo;
    if (raw == "&") return Overlay::Split;
    diagnostics.warn(at, "unsupported-overlay", "ignored a run longer than '&&'");
    return {};
}

void MusicBody::emit_token(
    grammar::Item kind, std::string_view raw, const WrittenNotes& notes, bool last_on_line
) {
    using Item       = grammar::Item;
    const auto& line = notes.source;
    const auto  at   = line.where(raw);
    const auto  emit
        = [&](auto value) { tune.body.push_back({at, std::move(value), line.first_line}); };
    switch (kind) {
    case Item::Pitch: emit(notes.note(raw)); break;
    case Item::Chord: {
        auto atom = notes.chord(raw);
        if (atom.notes.empty())
            diagnostics.warn(at, "empty-chord", "empty chord treated as a rest");
        emit(std::move(atom));
        tune.body.back().written_end = line.where(raw.substr(raw.size() - 1));
        break;
    }
    case Item::Rest: notes.rests(raw, emit); break;
    case Item::Grace:
        emit(notes.grace(raw));
        tune.body.back().written_end = line.where(raw.substr(raw.size() - 1));
        break;
    case Item::Bar: emit(written_bar(raw, line, diagnostics)); break;
    case Item::Ending:
        emit(
            Bar{.resets_measure = false, .endings = ending_passes(raw.substr(1), line, diagnostics)}
        );
        break;
    case Item::Field: {
        auto value = split_field(raw);
        if (value.kind == 'm' || value.kind == 'r') break;
        if (!field_allowed(value.kind, FieldSite::InlineMusic)) {
            diagnostics.warn(at, "invalid-inline-field", "ignored a non-inline field");
        } else {
            value.value = trim(value.value);
            emit_field({at, value, line.first_line});
        }
        break;
    }
    case Item::BadField:
        diagnostics.error(at, "unclosed-inline-field", "expected ']' after the inline field");
    case Item::Tuplet: emit(written_tuplet(raw, at, diagnostics)); break;
    case Item::Broken: emit(notes.broken_mark(raw)); break;
    case Item::Harmony: emit(Harmony{raw.substr(1, raw.size() - 2)}); break;
    case Item::Spacer: emit(Spacer{}); break;
    case Item::Slur: emit(Slur{raw.back() == '('}); break;
    case Item::Decoration: emit(Decoration{shorthands.decoration(raw, at), last_on_line}); break;
    case Item::Tie:
        if (!dialect.strict)
            emit(Tie{});
        else
            diagnostics.warn(at, "detached-tie", "a tie must immediately follow its note or chord");
        break;
    case Item::Overlay:
        if (const auto overlay = written_overlay(raw, at, diagnostics)) emit(*overlay);
        break;
    case Item::Bang:
        if (dialect.bang != BangRole::Decoration) break;
        [[fallthrough]];
    case Item::Invalid: invalid_byte(raw.front(), at, diagnostics); break;
    case Item::Layout: break;
    }
}

void MusicBody::read_tokens(LogicalLine line) {
    using Item         = grammar::Item;
    const auto   text  = trim(line.bytes);
    const auto   first = tune.body.size();
    auto         entry = entries.begin();
    WrittenNotes notes{line, shorthands, diagnostics};
    const auto   read = [&](Item kind, std::string_view raw) {
        if (kind == Item::Layout) return;
        if (kind == Item::Field) raw = raw.substr(1, raw.size() - 2);
        // Fields inside a continued token take effect before it sounds. For
        // inline fields the matched contents exclude both brackets.
        if (entry != entries.end()) {
            const auto end = line.where(raw.substr(raw.size() - 1));
            while (entry != entries.end() && entry->at < end) emit_field(std::move(*entry++));
        }
        emit_token(kind, raw, notes, raw.data() + raw.size() == text.data() + text.size());
    };
    if (dialect.bang == BangRole::Layout)
        scan(text, grammar::music_token<grammar::bang_break>{}, read);
    else
        scan(text, grammar::music_token<>{}, read);
    while (entry != entries.end()) emit_field(std::move(*entry++));
    entries.clear();
    if (!text.empty() && first == tune.body.size())
        tune.body.push_back({line.where(text), EmptyMusicLine{}, line.first_line});
    assign_written(std::span(tune.body).subspan(first), written_count);
    if (!text.empty()) saw_music = true;
}

// ABC 2.1 section 3.1.17 gives each physical file one charset. Keep both
// possible failures in the preamble until a declaration (including a
// continued I: field) or the end of the file header fixes the encoding.
class SourceCharset {
    struct TextError {
        Location at;
        bool     is_nul;
    };
    std::string              declared;
    std::optional<Location>  first_non_ascii;
    std::optional<TextError> first_utf8_error;

    void validate(Diagnostics& diagnostics) const {
        if (declared == "us-ascii" && first_non_ascii
            && (!first_utf8_error || *first_non_ascii <= first_utf8_error->at))
            diagnostics.error(*first_non_ascii, "invalid-text", "ABC text must be US-ASCII");
        if (first_utf8_error)
            diagnostics.error(
                first_utf8_error->at,
                "invalid-text",
                first_utf8_error->is_nul ? "ABC text cannot contain a NUL byte" :
                                           "ABC text must be valid UTF-8"
            );
    }

public:
    void declare(std::string_view name, Location at, Diagnostics& diagnostics) {
        const bool supported = name == "utf-8" || name == "us-ascii";
        const auto iso_part  = name.starts_with("iso-8859-") ? name.substr(9) : std::string_view{};
        const bool iso
            = iso_part == "10"
              || (iso_part.size() == 1 && iso_part.front() >= '1' && iso_part.front() <= '9');
        if (!supported && !iso) {
            diagnostics.warn(
                at, "invalid-charset", "expected utf-8, us-ascii or iso-8859-1 through iso-8859-10"
            );
        } else if (!declared.empty() && declared != name) {
            diagnostics.warn(
                at, "changed-charset", fmt::format("kept the file charset '{}'", declared)
            );
        } else if (declared.empty()) {
            declared = name;
            if (!supported)
                diagnostics.warn(
                    at, "unsupported-charset", fmt::format("reading '{}' as UTF-8", name)
                );
            validate(diagnostics);
        }
    }

    void check_line(std::string_view raw, Location at, Diagnostics& diagnostics) {
        if (!first_non_ascii && (declared.empty() || declared == "us-ascii")) {
            const auto byte = std::ranges::find_if(raw, [](u8 ch) { return ch > 127; });
            if (byte != raw.end()) {
                auto high_byte   = at;
                high_byte.column = static_cast<size_t>(byte - raw.begin()) + 1;
                first_non_ascii  = high_byte;
            }
        }
        if (!first_utf8_error) {
            const auto invalid = utf8::find_invalid(raw.begin(), raw.end());
            const auto offset  = static_cast<size_t>(invalid - raw.begin());
            const auto nul     = raw.find('\0');
            if (nul <= offset) {
                at.column        = nul + 1;
                first_utf8_error = {at, true};
            } else if (invalid != raw.end()) {
                at.column        = offset + 1;
                first_utf8_error = {at, false};
            }
        }
        if (!declared.empty()) validate(diagnostics);
    }

    void finish_header(Diagnostics& diagnostics) {
        if (declared.empty()) declared = "utf-8";
        validate(diagnostics);
    }
};

// Classifies physical lines and assembles fields across the file header, tune header and body.
class TuneLines {
public:
    TuneLines(const TextLoader& headers, Diagnostics& diagnostics) :
        diagnostics(diagnostics), headers(headers),
        aliases{tune.owned_text, reference, diagnostics}, macros(diagnostics) {}
    Tune read(std::string_view text) { return finish(read_source(text, {})); }

private:
    Location read_source(std::string_view text, Location at);
    bool     line(std::string_view raw, Location at, bool has_newline);
    Tune     finish(Location end);
    void     compile_fields();

    Diagnostics&      diagnostics;
    const TextLoader& headers;
    // Canonical names owned by the include calls currently being parsed.
    vec<std::string_view>     active_headers;
    Tune                      tune;
    size_t                    written_count = 0;
    std::optional<AbcVersion> file_version;
    SourceCharset             charset;
    std::optional<Location>   reference;
    std::optional<Location>   key_without_reference;
    MidiAliases               aliases;
    MusicMacros               macros;
    std::optional<MusicBody>  body;
    bool                      ended = false;
    struct LayoutBlock {
        std::string_view end;
        Location         opened_at;
    };
    std::optional<LayoutBlock> layout;

    struct HeaderEntry {
        char     kind;
        Location at;
    };
    std::optional<HeaderEntry> first_tune_entry;

    struct PendingField {
        Field                      field;
        Location                   at;
        std::optional<std::string> extended;
        vec<Token>                 directives;
        bool                       accepts_bare_history = false;
    };
    std::optional<PendingField> pending_field;
    vec<Location>               legacy_history_fields;

    void append_entry(Token token);
    void include_header(std::string_view name, Location at);
    void directive(std::string_view value, Location at);
    bool consume_layout(std::string_view text, Location at);
    void close_layout();
    void music_line(std::string_view raw, Location at, bool has_newline);
    void observe_header_entry(char kind, Location at);
    bool start_field(Field value, Location at);
    void extend_field(std::string_view value, Location at);
    void flush_field();
};

void TuneLines::observe_header_entry(char kind, Location at) {
    if (body || at.source || first_tune_entry) return;
    // Directives may arrive while X: is still open to a continuation.
    if (reference || (pending_field && pending_field->field.kind == 'X'))
        first_tune_entry = HeaderEntry{kind, at};
}

bool TuneLines::start_field(Field value, Location at) {
    if (value.kind == 'X') {
        charset.finish_header(diagnostics);
        if (body) {
            body->flush();
            diagnostics.warn(at, "extra-tunes", "only the first tune was read");
            return false;
        }
        if (reference) diagnostics.error(at, "missing-key", "first tune has no K: field");
    } else {
        // Keep the written order even when an include or macro consumes the field.
        observe_header_entry(value.kind, at);
    }
    constexpr std::string_view kForbiddenInBody = "ABCDEFGXYZabcdefgxyz";
    if (body
        && (body->dialect.strict ? !field_allowed(value.kind, FieldSite::Body) :
                                   kForbiddenInBody.find(value.kind) != kForbiddenInBody.npos)) {
        diagnostics.warn(at, "invalid-body-field", "ignored a field not allowed in the tune body");
        return true;
    }
    value.value = without_comment(value.value);
    pending_field.emplace(
        PendingField{.field = value, .at = at, .accepts_bare_history = !body && value.kind == 'H'}
    );
    return true;
}

void TuneLines::extend_field(std::string_view value, Location at) {
    if (!pending_field) {
        diagnostics.warn(at, "orphan-field-continuation", "ignored +: without a preceding field");
        return;
    }
    auto&       pending = *pending_field;
    const auto& field   = pending.field;
    // Only string fields insert an implicit space (ABC 2.1 section 3.3).
    const bool string_field = std::string_view("ABCDFGHNORSTWwZ").find(field.kind) != value.npos;
    const auto append       = [&](std::string_view part, Location start) {
        if (field.kind == 's') {
            start.column += 2;
            pending.field.fragments.push_back({pending.extended->size(), start});
        }
        pending.extended->append(string_field ? trim(part) : part);
    };
    if (!pending.extended) {
        pending.extended.emplace();
        append(field.value, pending.at);
    }
    if (string_field) pending.extended->push_back(' ');
    append(without_comment(value), at);
}

void TuneLines::flush_field() {
    if (!pending_field) return;
    auto [field, at, extended, directives, accepts_bare_history] = std::move(*pending_field);
    pending_field.reset();   // An include can parse more fields while emitting this one.
    if (extended) field.value = tune.owned_text.emplace_back(std::move(*extended));
    if (field.kind != 's') field.value = trim(field.value);
    if (field.kind == 'X') reference = at;
    const bool outside_tune = field.kind == 'K' && !reference;
    const bool opens_body   = field.kind == 'K' && reference && !body;
    if (outside_tune) {
        if (!key_without_reference) key_without_reference = at;
        diagnostics.warn(at, "invalid-file-header-field", "ignored K: before the first tune");
    } else {
        append_entry({at, std::move(field)});
    }
    if (opens_body) {
        const auto dialect        = read_music_dialect(tune, diagnostics);
        tune.decoration_delimiter = dialect.delimiter;
        assign_written(tune.header, written_count);
        body.emplace(tune, diagnostics, macros, aliases, written_count, dialect);
        for (auto& token : tune.header) body->shorthands.define(token);
        tune.key = at;
    }
    // A continued field is emitted at its opening, before any intervening directives.
    for (auto& token : directives) append_entry(std::move(token));
}

constexpr std::pair<std::string_view, std::string_view> kLayoutBlocks[]{
    {"begintext", "endtext"}, {"beginps", "endps"}
};

void TuneLines::directive(std::string_view value, Location at) {
    auto       args    = value;
    const auto command = word(args);
    if (layout) {
        if (command == layout->end) layout.reset();
        return;
    }
    if (!command.empty()) observe_header_entry('I', at);
    // Section 3.1.17 gives these instructions both an I: and a %% spelling.
    // Other stylesheet directives, like comments, may interrupt the prose without ending it.
    constexpr std::string_view kHistoryBoundaries[]{
        "abc-charset", "abc-version", "abc-creator", "linebreak", "decoration"
    };
    if (pending_field
        && (is_include(command)
            || std::ranges::find(kHistoryBoundaries, command)
                   != std::ranges::end(kHistoryBoundaries)))
        pending_field->accepts_bare_history = false;
    for (const auto& [begin, end] : kLayoutBlocks) {
        if (command == begin) {
            layout = {end, at};
            return;
        }
    }
    Token token{at, Directive{value}};
    if (pending_field)
        pending_field->directives.push_back(std::move(token));
    else
        append_entry(std::move(token));
}

void TuneLines::append_entry(Token token) {
    if (body) {
        body->field(std::move(token));
        return;
    }
    if (const auto* field = macro_definition(token)) {
        macros.define(field->value, token.at);
        return;
    }
    if (auto args = token.information()) {
        const auto name = word(*args);
        if (name == "abc-charset") {
            if (reference && token.at.source == 0)
                header_only_instruction("abc-charset", token.at, diagnostics);
            else
                charset.declare(trim(*args), token.at, diagnostics);
            return;
        }
        if (name == "abc-version") {
            if (const auto version = abc_version(trim(*args))) {
                tune.version = version;
                // A tune-local override cannot change how the file header is read.
                if (!reference) file_version = version;
            } else {
                diagnostics.warn(
                    token.at, "invalid-version", "expected an ABC version such as 2.1"
                );
            }
        } else if (is_include(name)) {
            include_header(trim(*args), token.at);
            return;
        }
    }
    aliases.append(std::move(token), tune.header);
}

bool TuneLines::consume_layout(std::string_view text, Location at) {
    // I: owns its field continuations, except for standalone layout delimiters.
    if (!ended && !(body && body->quoted) && (text.starts_with("%%") || text.starts_with("I:"))) {
        const auto instruction = trim(without_comment(text.substr(2)));
        if (text.starts_with('%') || std::ranges::any_of(kLayoutBlocks, [&](const auto& block) {
                return instruction == block.first || instruction == block.second;
            })) {
            directive(instruction, at);
            return true;
        }
    }
    return layout || text.starts_with('%');
}

bool TuneLines::line(std::string_view raw, Location at, bool has_newline) {
    charset.check_line(raw, at, diagnostics);
    const auto first = raw.find_first_not_of(kWhitespace);
    const auto text  = raw.substr(std::min(first, raw.size()));
    at.column        = first == raw.npos ? 1 : first + 1;
    if (consume_layout(text, at)) return true;

    auto value = body && body->quoted ? std::nullopt : line_field(text);
    if (ended) return !value || value->kind != 'X' || start_field(*value, at);
    if (value && value->kind == 'r') return true;
    if (value && value->kind == '+') {
        extend_field(value->value, at);
        return true;
    }
    // Sections 3.1.13 and 10.1: bare lines continue a header H: until the next
    // field or blank line. The score does not retain the history prose.
    if (!value && !text.empty() && pending_field && pending_field->accepts_bare_history) {
        const auto opening = pending_field->at;
        if (legacy_history_fields.empty() || legacy_history_fields.back() != opening)
            legacy_history_fields.push_back(opening);
        return true;
    }

    flush_field();
    // A note or rest followed immediately by a repeat bar is music, not a field.
    if (value && body && starts_with_repeat(text)) value.reset();
    if (at.source && !text.empty()
        && (!value || value->kind == 'X' || (value->kind == 'K' && !reference))) {
        diagnostics.warn(
            at, "invalid-include-content", "ignored non-header ABC in an included file"
        );
        return true;
    }
    if (value) return start_field(*value, at);
    music_line(raw, at, has_newline);
    return true;
}

void TuneLines::music_line(std::string_view raw, Location at, bool has_newline) {
    if (trim(raw).empty()) {
        if (body) {
            if (body->blank_line() && !at.source) ended = true;
        } else if (!at.source && reference && tune.version >= AbcVersion{2, 1}) {
            diagnostics.error(at, "missing-key", "expected K: before the blank line");
        }
        return;
    }
    if (!reference) return;   // Free text before the first tune.
    if (!body) diagnostics.error(at, "missing-key", "expected K: before the music");
    // Quoted music can span continued physical lines. Keep leading bytes for diagnostics.
    bool       quoted  = body->quoted;
    const auto content = without_comment(raw, quoted);
    body->music(content, at.line, has_newline, quoted);
}

void TuneLines::close_layout() {
    if (layout)
        diagnostics.warn(
            layout->opened_at,
            "unclosed-layout",
            fmt::format("ignored layout through end of file; expected %%{}", layout->end)
        );
    layout.reset();
}

TempoNotation tempo_notation(std::string_view text, Location at, Diagnostics& diagnostics) {
    text                  = trim(text);
    auto label            = take<grammar::quoted>(text);
    text                  = trim(text);
    const bool label_only = !label.empty() && text.empty();
    auto       beats      = take<grammar::tempo_prefix>(text);
    const auto bpm        = take<grammar::bpm>(text);
    text                  = trim(text);
    if (label.empty()) label = take<grammar::quoted>(text);
    if (!text.empty() || (bpm.empty() && !label_only))
        diagnostics.error(
            at, "invalid-tempo", "expected a tempo label or up to four beats and a metronome value"
        );

    TempoNotation result;
    if (!label.empty()) result.label = label.substr(1, label.size() - 2);
    if (label_only) return result;
    result.bpm = integer(bpm);
    if (!result.bpm || *result.bpm <= 0)
        diagnostics.error(at, "invalid-tempo", "tempo needs a positive metronome value");
    if (beats.empty()) return result;
    // The prefix includes the '=', and a relative beat can omit its factor.
    beats                = trim(beats.substr(0, beats.find('=')));
    result.unit_relative = note_letter(beats.front());
    if (result.unit_relative) beats = trim(beats.substr(1));
    if (beats.empty()) return result;
    result.beat = 0;
    while (!beats.empty()) {
        auto       beat        = word(beats);
        const auto numerator   = integer(take<grammar::number>(beat));
        const auto denominator = beat.empty() ? std::optional{1} : integer(beat.substr(1));
        if (!numerator || !*numerator || !denominator || !*denominator)
            diagnostics.error(at, "invalid-fraction", "tempo beats need a positive fraction");
        result.beat += static_cast<f64>(*numerator) / *denominator;
    }
    return result;
}

void check_deprecated_directive(std::string_view value, Location at, Diagnostics& diagnostics) {
    const auto name = word(value);
    if (name == "continueall" && trim(value) == "true")
        diagnostics.warn(
            at, "deprecated-directive", "use I:linebreak ! instead of %%continueall true"
        );
    else if (name == "abc-copyright" || name == "abc-edited-by")
        diagnostics.warn(at, "deprecated-directive", fmt::format("use Z: instead of %%{}", name));
}

void compile_entry(Token& token, bool strict, Diagnostics& diagnostics) {
    const auto* field = std::get_if<Field>(&token.value);
    if (!field) {
        if (const auto* directive = std::get_if<Directive>(&token.value); strict && directive)
            check_deprecated_directive(directive->value, token.at, diagnostics);
        return;
    }
    const auto value = field->value;
    switch (field->kind) {
    case 'X': {
        if (value.empty()) break;
        // The reference identifies a tune and can have arbitrarily many digits in ABC 2.1.
        const bool valid = strict ? std::ranges::all_of(value, digit)
                                        && value.find_first_not_of('0') != value.npos :
                                    integer(value).has_value();
        if (!valid)
            diagnostics.warn(
                token.at,
                "bad-reference",
                strict ? "expected a positive integer or an empty X: field" :
                         "expected an integer after X:"
            );
        break;
    }
    case 'Q': {
        const auto tempo = tempo_notation(value, token.at, diagnostics);
        if (strict && tempo.bpm && tempo.unit_relative)
            diagnostics.warn(
                token.at,
                "deprecated-tempo",
                "unit-relative Q: is deprecated; write an explicit beat, such as 1/4=120"
            );
        token.value = tempo;
        break;
    }
    case 'A':
        if (strict) diagnostics.warn(token.at, "deprecated-field", "use O: instead of A:");
        break;
    case 'E':
        if (strict) diagnostics.warn(token.at, "deprecated-field", "E: spacing is deprecated");
        break;
    default: break;
    }
}

Tune TuneLines::finish(Location end) {
    flush_field();
    charset.finish_header(diagnostics);
    if (body) body->flush();
    close_layout();
    if (!reference && key_without_reference)
        diagnostics.error(
            *key_without_reference, "missing-reference", "expected an X: field before K:"
        );
    if (!reference || !body)
        diagnostics.error(end, "missing-key", "expected a K: field in the tune header");
    tune.reference    = *reference;
    const bool strict = tune.version >= AbcVersion{2, 1};
    if (strict && (!first_tune_entry || first_tune_entry->kind != 'T')) {
        diagnostics.warn(
            first_tune_entry ? first_tune_entry->at : tune.key,
            "title-order",
            "expected a T: field immediately after X:"
        );
    }
    compile_fields();
    return std::move(tune);
}

void TuneLines::compile_fields() {
    const bool strict      = tune.version >= AbcVersion{2, 1};
    const auto file_header = [&](Location at) { return at < tune.reference; };
    const auto strict_header
        = [&](Location at) { return file_header(at) ? file_version >= AbcVersion{2, 1} : strict; };
    for (const auto at : legacy_history_fields) {
        if (strict_header(at))
            diagnostics.warn(at, "deprecated-history", "use +: for history continuation lines");
    }
    // A late abc-version instruction applies to the entire header that contains it.
    std::erase_if(tune.header, [&](const Token& token) {
        const auto* field = std::get_if<Field>(&token.value);
        if (!field || !strict_header(token.at)) return false;
        const bool file = file_header(token.at);
        if (field_allowed(field->kind, file ? FieldSite::FileHeader : FieldSite::TuneHeader))
            return false;
        diagnostics.warn(
            token.at,
            file ? "invalid-file-header-field" : "invalid-tune-header-field",
            fmt::format("ignored {}: in the {} header", field->kind, file ? "file" : "tune")
        );
        return true;
    });
    for (auto& token : tune.header) compile_entry(token, strict_header(token.at), diagnostics);
    for (auto& token : tune.body) compile_entry(token, strict, diagnostics);
}

size_t part_count(
    std::string_view digits, size_t max_parts, Location at, Diagnostics& diagnostics
) {
    if (digits.empty()) return 1;
    const auto count = integer(digits);
    if (!count || *count < 1 || static_cast<size_t>(*count) > max_parts)
        diagnostics.error(
            at,
            "part-count-range",
            fmt::format("part repeat count must be between 1 and {}", max_parts)
        );
    return static_cast<size_t>(*count);
}

}   // namespace

std::optional<MeterNumbers> meter_numbers(std::string_view text) {
    using namespace grammar;
    text                 = trim(text);
    const bool   grouped = !take<chars<'('>>(text).empty();
    MeterNumbers result;
    do {
        text             = trim(text);
        const auto group = take<number>(text);
        if (group.empty() || result.groups.size() == std::numeric_limits<u8>::max()) return {};
        // Zero is invalid for a meter component, including an out-of-range integer.
        result.groups.push_back(integer(group).value_or(0));
        text = trim(text);
    } while (!take<chars<'+'>>(text).empty());
    if (grouped && take<chars<')'>>(text).empty()) return {};
    if (take<pad<chars<'/'>, ws>>(text).empty()
        || !matches<seq<opt<chars<'+', '-'>>, number>>(text))
        return {};
    result.denominator = integer(text).value_or(0);
    return result;
}

std::string part_order(
    std::string_view text, size_t max_parts, Location at, Diagnostics& diagnostics
) {
    // ABC 2.1 section 3.1.9 ignores dots anywhere, including inside a repeat count.
    std::string undotted(text);
    std::erase(undotted, '.');
    using namespace grammar;
    text = undotted;
    // Diagnose malformed notation before interpreting any repeat counts.
    if (!matches<star<sor<part_item, chars<'('>, ws>>>(text))
        diagnostics.error(
            at, "invalid-parts", "expected part letters, groups or repeat counts in P:"
        );

    // A group remembers where its first copy begins. No recursive parse or
    // evaluation is needed, and the limit applies to playback, not nesting.
    std::string labels;
    vec<size_t> groups;
    while (!text.empty()) {
        if (!take<plus<ws>>(text).empty()) continue;
        if (!take<chars<'('>>(text).empty()) {
            groups.push_back(labels.size());
            continue;
        }
        size_t     start = labels.size();
        const auto item  = take<part_item>(text);
        if (item.starts_with(')')) {
            if (groups.empty()) diagnostics.error(at, "invalid-parts", "unmatched ')' in P: order");
            start = groups.back();
            groups.pop_back();
        } else {
            if (labels.size() == max_parts)
                diagnostics.error(
                    at, "expansion-limit", "part order exceeds the playback expansion limit"
                );
            labels.push_back(upper(item.front()));
        }

        const auto   count = part_count(item.substr(1), max_parts, at, diagnostics);
        const size_t width = labels.size() - start;
        if (width == 0) continue;
        if (count - 1 > (max_parts - labels.size()) / width)
            diagnostics.error(
                at, "expansion-limit", "part order exceeds the playback expansion limit"
            );
        for (size_t copy = 1; copy < count; ++copy) labels.append(labels, start, width);
    }
    if (!groups.empty()) diagnostics.error(at, "invalid-parts", "unclosed '(' in P: order");
    return labels;
}

vec<LyricPlacement> align_lyrics(
    std::string_view        text,
    size_t                  notes,
    std::span<const size_t> bars,
    Location                at,
    Diagnostics&            diagnostics
) {
    diagnostics.spend(PlaybackWork::text(text), at);
    using namespace grammar;
    NoteAlignment       alignment{notes, bars};
    vec<LyricPlacement> placements;
    take<lyric_stanza>(text);
    while (!text.empty()) {
        if (alignment.skip_layout(text)) continue;
        if (!take<chars<'_', '-'>>(text).empty()) {
            alignment.take_note();
            continue;
        }
        const auto syllable = take<lyric_syllable>(text);
        if (syllable.empty()) {
            if (take<plus<text_font_switch>>(text).empty()) break;
            continue;
        }
        const auto slot = alignment.take_note();
        if (slot) placements.push_back({*slot, decoded_text(syllable, TextContext::Lyric)});
        // One hyphen ends the syllable. Each following hold consumes another slot.
        take<styled<chars<'-'>>>(text);
        const auto holds = take<plus<styled<chars<'_'>>>>(text);
        if (!holds.empty()) {
            for (const char mark : holds)
                if (mark == '_') alignment.take_note();
            const auto punctuation = take<hold_punctuation>(text);
            if (slot) placements.back().text += decoded_text(punctuation, TextContext::Lyric);
        }
    }
    if (alignment.excess)
        diagnostics.warn(at, "extra-lyrics", "more lyric syllables than available notes");
    return placements;
}

std::string encode_text(std::string_view text, TextContext context) {
    std::string result;
    for (const char c : text) {
        if (c == '\n' && context == TextContext::VoiceName) {
            result += "\\n";
            continue;
        }
        const auto byte = static_cast<u8>(c);
        if (byte < 0x20 || byte == 0x7f) {
            fmt::format_to(std::back_inserter(result), "\\u{:04x}", byte);
            continue;
        }
        if (context == TextContext::Lyric) {
            if (c == ' ') {
                result += '~';
                continue;
            }
            if (c == '-') {
                result += "\\-";
                continue;
            }
            // Escape literal alignment marks before the verse parser sees them.
            if (std::string_view("*_|~+").find(c) != std::string_view::npos
                || (result.empty() && digit(c))) {
                fmt::format_to(std::back_inserter(result), "\\u{:04x}", byte);
                continue;
            }
        }
        // ABC 2.1 section 8.2: a backslash followed by a quote can introduce an accent.
        switch (c) {
        case '"': result += "&quot;"; break;
        case '$': result += "$$"; break;
        case '\\':
        case '%':
        case '&':
            result += '\\';
            result += c;
            break;
        default: result += c;
        }
    }
    return result;
}

std::string decode_text(std::string_view text, TextContext context) {
    // Only underlay alignment interprets lyric slot escapes.
    return decoded_text(text, context == TextContext::Lyric ? TextContext::Title : context);
}

std::optional<vec<SymbolPlacement>> align_symbols(
    const Field&            field,
    size_t                  notes,
    std::span<const size_t> bars,
    DecorationDelimiter     delimiter,
    Location                at,
    Diagnostics&            diagnostics
) {
    diagnostics.spend(PlaybackWork::text(field.value), at);
    NoteAlignment        alignment{notes, bars};
    vec<SymbolPlacement> placements;
    auto                 start = at;
    start.column += 2;
    auto text = field.value;
    while (!text.empty()) {
        if (alignment.skip_layout(text)) continue;
        const auto offset = static_cast<size_t>(text.data() - field.value.data());
        const auto where  = locate_fragment(field.fragments, offset, start);
        if (const auto quoted = take<grammar::quoted>(text); !quoted.empty()) {
            if (const auto slot = alignment.take_note())
                placements.push_back({*slot, where, Harmony{quoted.substr(1, quoted.size() - 2)}});
        } else if (const auto mark = take<grammar::named_decoration>(text); !mark.empty()) {
            check_decoration(mark, delimiter, where, diagnostics);
            if (const auto slot = alignment.take_note())
                placements.push_back({*slot, where, Decoration{mark}});
        } else {
            diagnostics.warn(
                where, "invalid-symbol-line", "expected a decoration, quoted text, '*' or '|'"
            );
            return {};
        }
    }
    if (alignment.excess)
        diagnostics.warn(at, "extra-symbols", "more symbol slots than available notes");
    return placements;
}

namespace {

Location TuneLines::read_source(std::string_view text, Location at) {
    if (text.starts_with("\xef\xbb\xbf")) text.remove_prefix(3);
    bool invalid_version = false;
    while (!text.empty()) {
        const auto end = text.find_first_of("\r\n");
        const auto raw = diagnostics.record_line(at.source, text.substr(0, end));
        if (!at.source && at.line == 1 && raw.starts_with("%abc-")) {
            file_version = tune.version = abc_version(trim(without_comment(raw.substr(5))));
            invalid_version             = !tune.version;
        }
        const bool more = line(raw, at, end != text.npos);
        ++at.line;
        if (!more || end == text.npos) break;
        text.remove_prefix(end + (text.substr(end).starts_with("\r\n") ? 2 : 1));
    }
    if (invalid_version)
        diagnostics.warn({1, 6}, "invalid-version", "expected an ABC version such as 2.1");
    return at;
}

void TuneLines::include_header(std::string_view name, Location at) {
    if (name.size() >= 2 && name.starts_with('"') && name.ends_with('"'))
        name = name.substr(1, name.size() - 2);
    if (name.empty()) {
        diagnostics.warn(at, "invalid-include", "expected a file name after abc-include");
        return;
    }
    if (!headers) {
        diagnostics.warn(at, "external-include", "reading ABC text does not open referenced files");
        return;
    }
    auto  loaded = headers(name, active_headers);
    auto* header = std::get_if<ExternalText>(&loaded);
    if (!header) {
        if (std::get<TextReadError>(loaded) == TextReadError::Cycle) {
            diagnostics.warn(
                at, "cyclic-include", fmt::format("ignored recursive include '{}'", name)
            );
            return;
        }
        diagnostics.warn(
            at, "unavailable-include", fmt::format("could not read ABC header '{}'", name)
        );
        return;
    }
    active_headers.push_back(header->name);
    auto       position       = diagnostics.register_include(header->name, at);
    const auto parent_layout  = std::exchange(layout, std::nullopt);
    auto       parent_charset = std::exchange(charset, SourceCharset{});
    read_source(header->text, position);
    flush_field();
    charset.finish_header(diagnostics);
    charset = std::move(parent_charset);
    close_layout();
    layout = parent_layout;
    active_headers.pop_back();
}

}   // namespace

std::optional<Alteration> accidental(std::string_view text, Location at, Diagnostics& diagnostics) {
    if (!matches<grammar::accidental>(text)) {
        diagnostics.warn(
            at, "invalid-accidental", fmt::format("ignored malformed accidental '{}'", text)
        );
        return {};
    }
    return written_accidental(text, at, diagnostics);
}

std::optional<Pitch> drum_spelling(std::string_view text, Location at, Diagnostics& diagnostics) {
    if (!matches<grammar::pitch_spelling>(text))
        diagnostics.error(
            at, "invalid-drummap", "drummap needs a pitch without a duration, such as ^F,"
        );
    const bool marked = !note_letter(text.front());
    const auto pitch  = read_spelling(text, [at](auto) { return at; }, diagnostics);
    if (marked && !pitch.accidental) return {};
    return pitch;
}

Tune tokenize(std::string_view text, const TextLoader& headers, Diagnostics& diagnostics) {
    return TuneLines(headers, diagnostics).read(text);
}


}   // namespace symusic::abc::detail

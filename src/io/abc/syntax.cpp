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
#include <utility>
#include <variant>

#include <fmt/format.h>
#include <tao/pegtl.hpp>
#include <tao/pegtl/extra/parse_tree.hpp>
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
    sources.push_back({std::string(source), {}, std::move(path)});
    return {1, 1, sources.size() - 1, sources.back().include_path};
}

void Diagnostics::record_line(size_t source, std::string_view raw) {
    sources[source].lines.push_back(raw);
}

std::string Diagnostics::format(
    Location at, std::string_view code, std::string_view message
) const {
    const auto& source = sources[at.source];
    auto        result
        = fmt::format("{}:{}:{}: ABC [{}]: {}", source.name, at.line, at.column, code, message);
    if (at.line == 0 || at.line > source.lines.size()) return result;

    const auto       line   = source.lines[at.line - 1];
    constexpr size_t kWidth = 96;
    const auto       pos    = std::min(at.column - 1, line.size());
    size_t           begin  = std::min(
        pos > kWidth / 2 ? pos - kWidth / 2 : 0, line.size() > kWidth ? line.size() - kWidth : 0
    );
    size_t end = begin + std::min(kWidth, line.size() - begin);
    // Include a complete UTF-8 character when a window edge falls inside it.
    const auto trail = [&](size_t i) { return (static_cast<u8>(line[i]) & 0xc0U) == 0x80U; };
    for (int n = 0; n < 3 && begin && trail(begin); ++n) --begin;
    for (int n = 0; n < 3 && end < line.size() && trail(end); ++n) ++end;
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
        = std::ranges::lower_bound(ranges, static_cast<int>(pass), {}, &EndingRange::last);
    return range != ranges.end() && range->first <= static_cast<int>(pass);
}

long double Broken::first_factor(long double single_short) const {
    const auto short_factor = count == 1 ? single_short : std::ldexp(1.L, -count);
    return direction > 0 ? 2 - short_factor : short_factor;
}

namespace {

namespace peg = tao::pegtl;
namespace grammar {

using namespace peg;

template<char... Cs>
using chars = one<Cs...>;
template<char C>
using repeated = plus<chars<C>>;
using ws       = chars<' ', '\t', '\r', '\f', '\v'>;
using escaped  = seq<chars<'\\'>, any>;

struct number : plus<peg::digit> {};
struct slashes : repeated<'/'> {};
struct fraction : seq<slashes, opt<number>> {};
using duration = sor<seq<number, opt<fraction>>, fraction>;
struct length : duration {};
struct octave : plus<chars<',', '\''>> {};
using shift_sign = chars<'^', '_'>;
using accidental_sign
    = sor<string<'^', '^'>, string<'_', '_'>, seq<chars<'='>, opt<shift_sign>>, shift_sign>;
struct accidental : seq<accidental_sign, opt<length>> {};
struct letter : ranges<'a', 'g', 'A', 'G'> {};
struct tie : seq<opt<chars<'.'>>, chars<'-'>> {};
using pitch_spelling = seq<opt<accidental>, letter, opt<octave>>;
struct pitch : seq<pitch_spelling, opt<length>, opt<tie>> {};

struct rest_letter : chars<'x', 'z', 'X', 'Z'> {};
struct rest : seq<rest_letter, opt<length>> {};
struct spacing : seq<chars<'y'>, opt<duration>> {};
// ABC 2.1 section 4.14: a decoration cannot consume a bar, chord boundary or field separator.
template<char Delimiter>
using delimited_decoration = seq<
    chars<Delimiter>,
    plus<not_one<Delimiter, ' ', '\t', '\r', '\n', '\f', '\v', '[', ']', '|', ':'>>,
    chars<Delimiter>>;
struct bang_decoration : delimited_decoration<'!'> {};
struct named_decoration : sor<bang_decoration, delimited_decoration<'+'>> {};
struct decoration : sor<named_decoration, chars<'.', '~', '\''>, alpha> {};
struct bang_break : chars<'!'> {};
using linebreak_mark = sor<bang_break, chars<'$'>, string<'<', 'E', 'O', 'L', '>'>>;
using linebreak_setting
    = seq<sor<string<'<', 'n', 'o', 'n', 'e', '>'>, list<linebreak_mark, plus<ws>>>, eof>;
using reserved_music = chars<'#', '*', ';', '?', '@'>;
struct chord_spacing : spacing {};
struct chord : seq<chars<'['>,
                   star<sor<pitch, rest, chord_spacing, ws, decoration>>,
                   chars<']'>,
                   opt<length>,
                   opt<tie>> {};
struct broken : sor<repeated<'>'>, repeated<'<'>> {};
struct grace
    : seq<chars<'{'>, opt<chars<'/'>>, star<sor<chord, pitch, broken, spacing, ws>>, chars<'}'>> {};

using field_key  = seq<sor<alpha, chars<'+'>>, chars<':'>>;
using quote_body = star<sor<escaped, not_one<'"', '\\'>>>;
using quoted     = seq<chars<'"'>, quote_body, chars<'"'>>;
struct symbol_shorthand : sor<ranges<'H', 'W', 'h', 'w'>, chars<'~'>> {};
struct symbol_expansion : sor<named_decoration, quoted> {};
using symbol_definition = seq<symbol_shorthand, pad<chars<'='>, ws>, symbol_expansion, eof>;

struct legacy_text_escape : seq<chars<'\\'>, chars<'\\', '%', '"'>> {};
struct text_amp_escape : string<'\\', '&'> {};
using text_escape = sor<legacy_text_escape, text_amp_escape>;
struct text_voice_break : string<'\\', 'n'> {};
struct text_unicode16 : seq<string<'\\', 'u'>, rep<4, xdigit>> {};
struct text_unicode32 : seq<string<'\\', 'U'>, rep<8, xdigit>> {};
struct text_mnemonic : seq<chars<'\\'>, sor<chars<'`', '\'', '^', '~', '"', '/'>, alpha>, alpha> {};
struct text_entity : seq<chars<'&'>, plus<alpha>, chars<';'>> {};
// ABC 2.1 sections 8.2 and 11.4.2: font changes do not contribute text.
using text_font_switch = seq<chars<'$'>, ranges<'0', '4'>>;
using text_font_run    = plus<text_font_switch>;
struct text_dollar : string<'$', '$'> {};
using text_styling = sor<text_font_switch, text_dollar>;
struct text_run : sor<plus<not_one<'\\', '&', '$'>>, any> {};
using text_encoded = sor<
    text_voice_break,
    text_unicode16,
    text_unicode32,
    text_mnemonic,
    text_escape,
    text_entity>;
using text_string        = star<sor<text_encoded, text_run>>;
using styled_text_string = star<sor<text_styling, text_encoded, text_run>>;
using legacy_text_string = star<sor<text_voice_break, legacy_text_escape, text_run>>;

struct text_lyric_dash : string<'\\', '-'> {};
struct text_lyric_space : chars<'~'> {};
struct legacy_lyric_escape : seq<chars<'\\'>, chars<'-', '\\', '%', '&'>> {};
struct lyric_text_run : sor<plus<not_one<'\\', '&', '~', '$'>>, any> {};
using lyric_text
    = star<sor<text_styling, text_encoded, text_lyric_dash, text_lyric_space, lyric_text_run>>;
using legacy_lyric_text = star<sor<legacy_lyric_escape, text_lyric_space, lyric_text_run>>;

// ABC 2.1 sections 2.2.5 and 8.2: quotes do not protect a bare percent.
struct line_quote : chars<'"'> {};
using comment_prefix = star<sor<escaped, line_quote, plus<not_one<'\\', '"', '%'>>, chars<'\\'>>>;

struct tempo_label : quoted {};
struct tempo_integer : number {};
struct tempo_beat : seq<tempo_integer, opt<chars<'/'>, tempo_integer>> {};
struct tempo_whole_notes : seq<tempo_beat, rep_opt<3, plus<ws>, tempo_beat>> {};
struct tempo_unit_notes : seq<letter, opt<star<ws>, tempo_beat>> {};
struct tempo_bpm : seq<opt<chars<'+'>>, number> {};
using tempo_rate  = seq<sor<tempo_unit_notes, tempo_whole_notes>, pad<chars<'='>, ws>, tempo_bpm>;
using tempo_value = sor<tempo_rate, tempo_bpm>;
using tempo_field = seq<
    sor<seq<tempo_label, opt<star<ws>, tempo_value>>, seq<tempo_value, opt<star<ws>, tempo_label>>>,
    eof>;

// ABC 2.1 section 3.1.6 allows either (2+3+2)/8 or 2+3+2/8.
struct meter_group : number {};
struct meter_denominator : seq<opt<chars<'+', '-'>>, number> {};
using meter_groups = seq<
    meter_group,
    rep_opt<std::numeric_limits<u8>::max() - 1, pad<chars<'+'>, ws>, meter_group>>;
using meter_field = seq<
    sor<seq<chars<'('>, pad<meter_groups, ws>, chars<')'>>, meter_groups>,
    pad<chars<'/'>, ws>,
    meter_denominator,
    eof>;

struct part_count : number {};
struct part_label : seq<alpha, opt<part_count>> {};
struct part_open : chars<'('> {};
struct part_close : seq<chars<')'>, opt<part_count>> {};
using part_field = seq<star<sor<part_label, part_open, part_close, ws>>, eof>;

struct alignment_bar : chars<'|'> {};
struct blank_slot : chars<'*'> {};
struct lyric_skip : chars<'_', '-'> {};
struct lyric_hold : chars<'_'> {};
using lyric_character = sor<legacy_lyric_escape, not_one<' ', '\t', '-', '|', '*', '_'>>;
struct lyric_syllable : plus<lyric_character> {};
using styled_lyric_character = sor<text_dollar, seq<not_at<text_font_switch>, lyric_character>>;
template<class Item>
using styled_lyric_item = seq<Item, star<text_font_switch>>;
// A font change alone takes no slot and is transparent to a following hyphen.
struct styled_lyric_syllable
    : seq<star<text_font_switch>, plus<styled_lyric_item<styled_lyric_character>>> {};
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
    star<sor<ws, text_font_switch, text_lyric_space>>>;
// One hyphen ends a syllable. A second hyphen or one after whitespace
// is a separate blank syllable (ABC 2.1 section 5.1).
template<class Syllable, class Hyphen = chars<'-'>, class... Suffix>
using lyric_word = seq<Syllable, opt<Hyphen>, Suffix...>;
// The held syllable takes trailing punctuation in the section 13.4 example.
struct lyric_hold_punctuation : seq<plus<styled_lyric_item<chars<'.', ',', '!', '?', ':', ';'>>>,
                                    at<sor<ws, chars<'-', '*', '|'>, eof>>> {};
using styled_lyric_word = lyric_word<
    styled_lyric_syllable,
    styled_lyric_item<chars<'-'>>,
    opt<seq<plus<styled_lyric_item<lyric_hold>>, opt<lyric_hold_punctuation>>>>;
template<class Word, class... Standalone>
using lyric_verse = star<sor<ws, alignment_bar, blank_slot, lyric_skip, Word, Standalone...>>;

struct field : seq<field_key, star<sor<quoted, not_one<']', '"'>>>> {};
using inline_field_start = seq<chars<'['>, field_key>;
using inline_field_ahead = at<inline_field_start>;
struct bad_field : seq<inline_field_start, star<any>> {};
using inline_field = seq<chars<'['>, field, chars<']'>>;
// Sections 2.2.5 and 8.1: elide these even inside a note, but not within text or fields.
enum class Elision { Remarks, Playback };
using inline_remark = seq<string<'[', 'r', ':'>, star<not_one<']'>>, chars<']'>>;
template<Elision Phase>
struct elided_music : std::conditional_t<
                          Phase == Elision::Remarks,
                          inline_remark,
                          sor<inline_remark, reserved_music>> {};
using music_text         = sor<inline_field, bad_field, quoted, seq<chars<'"'>, star<any>>>;
using macro_spaced_equal = seq<ws, chars<'='>, at<ws>>;
template<class Separator>
using macro_to_separator = peg::until<at<Separator>, sor<music_text, named_decoration, any>>;
// Section 9 limits the written text in characters. An annotation in a macro
// can contain UTF-8, and header charset validation may still be pending.
template<size_t Max>
using macro_text      = seq<peg::utf8::any, rep_opt<Max - 1, peg::utf8::any>, eof>;
using music_run       = plus<seq<not_at<sor<chars<'[', '"', '!', '+'>, reserved_music>>, any>>;
using plus_decoration = delimited_decoration<'+'>;
template<Elision Phase, class Decoration>
using music_elisions
    = seq<star<sor<elided_music<Phase>, music_text, Decoration, music_run, any>>, eof>;
struct ending_number : number {};
struct ending_range : seq<ending_number, opt<chars<'-'>, ending_number>> {};
struct endings
    : seq<list<ending_range, chars<','>>, not_at<sor<chars<','>, seq<chars<'-'>, number>>>> {};
// Section 4.8 permits continuous thick/thin patterns such as |[|.
// A bracket needs its own pipe so a following chord or ending stays separate.
using bar_piece = sor<string<'[', '|', ']'>, string<'[', '|'>, string<'|', ']'>, chars<'|'>>;
struct barline : plus<bar_piece> {};
struct opening_colons : repeated<':'> {};
struct closing_colons : repeated<':'> {};
// Consume colon runs once, including malformed bare runs without a divider.
using repeat_close = seq<
    at<seq<chars<':'>, sor<chars<':', ']'>, bar_piece>>>,
    closing_colons,
    opt<seq<sor<barline, chars<']'>>, opt<opening_colons>>>>;
struct repeat_bar : sor<repeat_close, seq<barline, opening_colons>> {};
// An explicit [1 stays separate so a continued field can precede it.
struct bar : seq<opt<chars<'.'>>, sor<repeat_bar, barline>, opt<endings>> {};
struct ending : seq<chars<'['>, sor<endings, quoted>, opt<chars<']'>>> {};

struct tuplet_count : number {};
struct tuplet_time : opt<number> {};
struct tuplet_notes : opt<number> {};
struct tuplet
    : seq<chars<'('>, tuplet_count, opt<chars<':'>, tuplet_time, opt<chars<':'>, tuplet_notes>>> {};
using overlay_splits = plus<seq<chars<'&'>, not_at<chars<')'>>>>;
struct overlay : sor<string<'(', '&'>, string<'&', ')'>, overlay_splits> {};
struct harmony : quoted {};
using symbol_line = star<sor<ws, alignment_bar, blank_slot, harmony, named_decoration>>;
struct slur : seq<opt<chars<'.'>>, chars<'(', ')'>> {};
struct invalid : any {};
struct music_spacing : spacing {};
using layout = sor<chars<'$', '`'>, seq<chars<'\\'>, eof>, ws>;
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
    music_spacing,
    slur,
    tie,
    decoration,
    bang_break,
    layout,
    invalid>;
using music = seq<star<token>, eof>;
// In this dialect a top-level '!' is layout, even when another '!' follows it.
using bang_layout_music = seq<star<sor<bang_break, token>>, eof>;

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
        grammar::chord_spacing,
        grammar::music_spacing,
        grammar::grace,
        grammar::accidental,
        grammar::letter,
        grammar::octave,
        grammar::tie,
        grammar::length,
        grammar::field,
        grammar::bad_field,
        grammar::bar,
        grammar::repeat_bar,
        grammar::opening_colons,
        grammar::closing_colons,
        grammar::barline,
        grammar::ending_number,
        grammar::ending_range,
        grammar::endings,
        grammar::ending,
        grammar::tuplet,
        grammar::tuplet_count,
        grammar::tuplet_time,
        grammar::tuplet_notes,
        grammar::broken,
        grammar::overlay,
        grammar::harmony,
        grammar::decoration,
        grammar::bang_break,
        grammar::slur,
        grammar::invalid>>;

using Input = peg::view_input<>;
using Node  = peg::parse_tree::node_t<Input>;

template<class Rule>
struct TextAction : peg::nothing<Rule> {};

struct TextValue {
    bool        line_breaks;
    std::string text;
};

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

template<>
struct TextAction<grammar::legacy_text_escape> {
    static void apply(const auto& input, TextValue& out) {
        out.text.push_back(input.string_view().back());
    }
};

template<>
struct TextAction<grammar::text_amp_escape> : TextAction<grammar::legacy_text_escape> {};

template<>
struct TextAction<grammar::text_dollar> : TextAction<grammar::legacy_text_escape> {};

template<>
struct TextAction<grammar::text_lyric_dash> : TextAction<grammar::legacy_text_escape> {};

template<>
struct TextAction<grammar::legacy_lyric_escape> : TextAction<grammar::legacy_text_escape> {};

template<>
struct TextAction<grammar::text_lyric_space> {
    static void apply0(TextValue& out) { out.text.push_back(' '); }
};

template<>
struct TextAction<grammar::text_voice_break> {
    static void apply(const auto& input, TextValue& out) {
        if (out.line_breaks)
            out.text.push_back('\n');
        else
            out.text.append(input.string_view());
    }
};

struct UnicodeTextAction {
    static void apply(const auto& input, TextValue& out) {
        const auto spelling = input.string_view();
        u32        value    = 0;
        std::from_chars(spelling.data() + 2, spelling.data() + spelling.size(), value, 16);
        if (value > 0x10FFFF || (value >= 0xD800 && value <= 0xDFFF))
            out.text.append(spelling);
        else
            utf8::append(value, out.text);
    }
};

template<>
struct TextAction<grammar::text_unicode16> : UnicodeTextAction {};
template<>
struct TextAction<grammar::text_unicode32> : UnicodeTextAction {};

template<>
struct TextAction<grammar::text_entity> {
    static void apply(const auto& input, TextValue& out) {
        const auto spelling = input.string_view();
        const auto name     = spelling.substr(1, spelling.size() - 2);
        const auto match    = std::ranges::find(kTextCharacters, name, &TextCharacter::entity);
        if (match != std::end(kTextCharacters))
            utf8::append(match->code, out.text);
        else
            out.text.append(spelling);
    }
};

template<>
struct TextAction<grammar::text_mnemonic> {
    static void apply(const auto& input, TextValue& out) {
        const auto spelling = input.string_view();
        const auto match
            = std::ranges::find(kTextCharacters, spelling.substr(1), &TextCharacter::mnemonic);
        if (match != std::end(kTextCharacters))
            utf8::append(match->code, out.text);
        else
            out.text.append(spelling);
    }
};

template<>
struct TextAction<grammar::text_run> {
    static void apply(const auto& input, TextValue& out) { out.text.append(input.string_view()); }
};

template<>
struct TextAction<grammar::lyric_text_run> : TextAction<grammar::text_run> {};

template<class Grammar>
std::string decode_text_as(std::string_view text, bool line_breaks = false) {
    TextValue out{line_breaks, {}};
    out.text.reserve(text.size());
    Input input(text);
    peg::parse<Grammar, TextAction>(input, out);
    return std::move(out.text);
}

template<class Rule>
using TempoSelect = peg::parse_tree::selector<
    Rule,
    peg::parse_tree::store_content::on<
        grammar::tempo_label,
        grammar::tempo_integer,
        grammar::tempo_beat,
        grammar::tempo_whole_notes,
        grammar::tempo_unit_notes,
        grammar::tempo_bpm>>;

template<class Rule>
using PartSelect = peg::parse_tree::selector<
    Rule,
    peg::parse_tree::store_content::
        on<grammar::part_label, grammar::part_count, grammar::part_open, grammar::part_close>>;

template<class Rule>
using MeterSelect = peg::parse_tree::selector<
    Rule,
    peg::parse_tree::store_content::on<grammar::meter_group, grammar::meter_denominator>>;

struct NoteAlignment {
    size_t                  notes;
    std::span<const size_t> bars;
    size_t                  next   = 0;
    bool                    excess = false;

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
    if (fragment.generated) return fragment.at;
    auto at = fragment.at;
    at.column += offset - fragment.offset;
    return at;
}

template<class Rule>
struct AlignmentAction : peg::nothing<Rule> {};

template<>
struct AlignmentAction<grammar::alignment_bar> {
    static void apply0(NoteAlignment& state, auto&) {
        const auto bar      = std::ranges::lower_bound(state.bars, state.next);
        state.next          = bar == state.bars.end() ? state.notes : *bar;
        const auto consumed = bar == state.bars.end() ? bar : std::next(bar);
        state.bars = state.bars.subspan(static_cast<size_t>(consumed - state.bars.begin()));
    }
};

template<>
struct AlignmentAction<grammar::blank_slot> {
    static void apply0(NoteAlignment& state, auto&) { state.take_note(); }
};

template<class Rule>
struct LyricAction : AlignmentAction<Rule> {};

template<>
struct LyricAction<grammar::lyric_skip> : AlignmentAction<grammar::blank_slot> {};

template<>
struct LyricAction<grammar::lyric_hold> : AlignmentAction<grammar::blank_slot> {};

struct LyricScan {
    LyricEncoding       encoding;
    vec<LyricPlacement> placements;
    bool                placed_syllable = false;
};

template<>
struct LyricAction<grammar::lyric_syllable> {
    static void apply(const auto& input, NoteAlignment& state, LyricScan& scan) {
        const auto slot      = state.take_note();
        scan.placed_syllable = slot.has_value();
        if (!slot) return;
        // Decoded punctuation is text, not an alignment command.
        const auto raw  = input.string_view();
        auto       text = scan.encoding == LyricEncoding::AbcText ?
                              decode_text_as<grammar::lyric_text>(raw) :
                              decode_text_as<grammar::legacy_lyric_text>(raw);
        scan.placements.push_back({*slot, std::move(text)});
    }
};

template<>
struct LyricAction<grammar::styled_lyric_syllable> : LyricAction<grammar::lyric_syllable> {};

template<>
struct LyricAction<grammar::lyric_hold_punctuation> {
    static void apply(const auto& input, NoteAlignment&, LyricScan& scan) {
        if (scan.placed_syllable)
            scan.placements.back().text += decode_text_as<grammar::lyric_text>(input.string_view());
    }
};

template<class Rule>
struct SymbolAction : AlignmentAction<Rule> {};

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

struct SymbolScan {
    const Field&         field;
    Location             start;
    DecorationDelimiter  delimiter;
    Diagnostics&         diagnostics;
    vec<SymbolPlacement> placements;

    Location where(const char* position) const {
        const auto offset = static_cast<size_t>(position - field.value.data());
        return locate_fragment(field.fragments, offset, start);
    }
};

template<>
struct SymbolAction<grammar::harmony> {
    static void apply(const auto& input, NoteAlignment& state, SymbolScan& scan) {
        if (const auto slot = state.take_note()) {
            const auto quoted = input.string_view();
            scan.placements.push_back(
                {*slot, scan.where(input.begin()), Harmony{quoted.substr(1, quoted.size() - 2)}}
            );
        }
    }
};

template<>
struct SymbolAction<grammar::named_decoration> {
    static void apply(const auto& input, NoteAlignment& state, SymbolScan& scan) {
        const auto at = scan.where(input.begin());
        check_decoration(input.string_view(), scan.delimiter, at, scan.diagnostics);
        if (const auto slot = state.take_note())
            scan.placements.push_back({*slot, at, Decoration{input.string_view()}});
    }
};

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

template<class Rule>
using DefinitionSelect = peg::parse_tree::selector<
    Rule,
    peg::parse_tree::store_content::on<grammar::symbol_shorthand, grammar::symbol_expansion>>;

void parse_symbol_definition(
    Token& token, DecorationDelimiter delimiter, Diagnostics& diagnostics
) {
    const auto* field = std::get_if<Field>(&token.value);
    if (!field || field->kind != 'U') return;
    Input      input(field->value);
    const auto root = peg::parse_tree::parse<grammar::symbol_definition, DefinitionSelect>(input);
    if (!root) {
        diagnostics.warn(
            token.at,
            "invalid-symbol",
            "U: needs a shorthand followed by '=' and a decoration or annotation"
        );
        return;
    }
    const char shorthand = root->children.front()->data.front();
    const auto spelling  = root->children.back()->data;
    if (!spelling.starts_with('"')) check_decoration(spelling, delimiter, token.at, diagnostics);
    token.value = SymbolDefinition{shorthand, spelling};
}

template<class Rule>
struct LinebreakAction : peg::nothing<Rule> {};
template<>
struct LinebreakAction<grammar::bang_break> {
    static void apply0(bool& contains_bang) { contains_bang = true; }
};

MusicDialect read_music_dialect(const Tune& tune, Diagnostics& diagnostics) {
    std::optional<std::pair<Location, bool>>                linebreak;
    std::optional<std::pair<Location, DecorationDelimiter>> decoration;
    for (const auto& token : tune.header) {
        auto args = token.information();
        if (!args) continue;
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
            Input input(value);
            bool  bang = false;
            if (peg::parse<grammar::linebreak_setting, LinebreakAction>(input, bang))
                linebreak = std::pair{token.at, bang};
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
    const bool strict            = tune.version >= AbcVersion{2, 1};
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

enum class FieldSite { file_header, tune_header, body, inline_music };

bool field_allowed(char kind, FieldSite site) {
    // ABC 2.1 section 3. Accept deprecated E: in headers to give its own warning.
    constexpr std::string_view file_fields   = "ABCDEFGHILMmNORrSUZ";
    constexpr std::string_view tune_fields   = "KPQTVWX";
    constexpr std::string_view inline_fields = "IKLMmNPQRrUV";
    const auto has = [kind](std::string_view fields) { return fields.find(kind) != fields.npos; };
    switch (site) {
    case FieldSite::file_header: return has(file_fields);
    case FieldSite::tune_header: return has(file_fields) || has(tune_fields);
    case FieldSite::body: return has(inline_fields) || has("sTWw");
    case FieldSite::inline_music: return has(inline_fields);
    }
    return false;
}

// Both physical and inline fields use grammar::field_key. Keep the original
// value whitespace until any physical field continuations have been assembled.
Field field(std::string_view text) { return {text.front(), text.substr(2)}; }

std::optional<Field> line_field(std::string_view text) {
    Input input(text);
    if (!peg::parse<grammar::field_key>(input)) return {};
    return field(text);
}

bool starts_with_repeat(std::string_view text) {
    Input input(text);
    return peg::parse<peg::seq<
        peg::sor<grammar::letter, grammar::rest_letter>,
        peg::at<peg::one<':'>>,
        grammar::repeat_bar>>(input);
}

template<class Rule>
struct CommentAction : peg::nothing<Rule> {};

template<>
struct CommentAction<grammar::line_quote> {
    static void apply0(bool& quoted) { quoted = !quoted; }
};

std::string_view without_comment(std::string_view line, bool& quoted) {
    Input input(line);
    peg::parse<grammar::comment_prefix, CommentAction>(input, quoted);
    return line.substr(0, line.size() - input.size());
}

std::string_view without_comment(std::string_view line) {
    bool quoted = false;
    return without_comment(line, quoted);
}

// A continued music line keeps one lyric identity and a map to physical byte columns.
// A logical music line may span physical lines and generated macro fragments.
// Convert views into bytes back to source positions before emitting tokens.
struct LogicalLine {
    std::string_view                bytes;
    size_t                          first_line;
    size_t                          music_line;
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

template<class Rule>
struct MusicElision : peg::nothing<Rule> {};
template<grammar::Elision Phase>
struct MusicElision<grammar::elided_music<Phase>> {
    static void apply(const auto& input, LogicalLine line, RewrittenMusic& result, size_t& copied) {
        const auto offset = static_cast<size_t>(input.begin() - line.bytes.data());
        result.append(line, copied, offset);
        copied = offset + input.size();
    }
};

template<grammar::Elision Phase>
std::optional<RewrittenMusic> elide_music(
    LogicalLine line, BangRole bang, bool quoted_prefix = false
) {
    if (line.bytes.find("[r:") == line.bytes.npos
        && (Phase == grammar::Elision::Remarks
            || line.bytes.find_first_of("#*;?@") == line.bytes.npos))
        return {};

    RewrittenMusic result;
    size_t         copied = 0;
    Input          input(line.bytes);
    if (quoted_prefix && !peg::parse<peg::seq<grammar::quote_body, grammar::chars<'"'>>>(input))
        return {};
    if (bang == BangRole::Layout)
        peg::parse<grammar::music_elisions<Phase, grammar::plus_decoration>, MusicElision>(
            input, line, result, copied
        );
    else
        peg::parse<grammar::music_elisions<Phase, grammar::named_decoration>, MusicElision>(
            input, line, result, copied
        );
    if (copied == 0) return {};
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
    struct Rule {
        std::string_view target;
        std::string_view replacement;
        size_t           note;
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
    Input      target_text(target);
    Input      replacement_text(replacement);
    Input note_length(note == target.npos ? target.substr(target.size()) : target.substr(note + 1));
    if (!peg::parse<grammar::macro_text<31>>(target_text)
        || !peg::parse<grammar::macro_text<200>>(replacement_text)
        || (note != target.npos && !peg::parse<peg::seq<grammar::number, peg::eof>>(note_length))) {
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

MusicMacros::Match MusicMacros::find(std::string_view text, Location at) {
    Match      best;
    const auto consider = [&](char key) {
        const auto bucket = rules.find(key);
        if (bucket == rules.end()) return;
        for (const auto& rule : bucket->second) {
            diagnostics.spend(1, at);
            size_t length = rule.target.size();
            i64    step   = 0;
            if (rule.note == rule.target.npos) {
                if (!text.starts_with(rule.target)) continue;
            } else {
                if (!text.starts_with(rule.target.substr(0, rule.note))) continue;
                auto pitch = text.substr(rule.note);
                if (pitch.empty() || !note_letter(pitch.front())) continue;
                step = (upper(pitch.front()) - 'A' + 5) % 7 + (lower(pitch.front()) ? 7 : 0);
                pitch.remove_prefix(1);
                while (!pitch.empty() && (pitch.front() == '\'' || pitch.front() == ',')) {
                    step += pitch.front() == '\'' ? 7 : -7;
                    pitch.remove_prefix(1);
                }
                const auto spelling
                    = text.substr(rule.note, text.size() - pitch.size() - rule.note);
                diagnostics.spend(PlaybackWork::text(spelling), at);
                const auto suffix = rule.target.substr(rule.note + 1);
                if (!pitch.starts_with(suffix)) continue;
                length = text.size() - pitch.size() + suffix.size();
            }
            // A target such as ~G3 must not match the written lengths 30 or 3/2.
            if (digit(rule.target.back()) && length < text.size()
                && (digit(text[length]) || text[length] == '/'))
                continue;
            if (length >= best.length) best = {&rule, length, step};
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
    constexpr std::string_view kLetters = "CDEFGABcdefgab";
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
        text.push_back(kLetters[static_cast<size_t>(degree + (octave > 0 ? 7 : 0))]);
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
            Input input(text);
            if (peg::parse<grammar::inline_field_ahead>(input)) {
                if (!peg::parse<grammar::inline_field>(input)) break;
                const auto length = text.size() - input.size();
                if (text.starts_with("[m:")) define(text.substr(3, length - 4), at);
                offset += length;
                continue;
            }
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
class MidiAliases {
public:
    MidiAliases(
        std::deque<std::string>&       text,
        const std::optional<Location>& reference,
        Diagnostics&                   diagnostics
    ) : text(text), reference(reference), diagnostics(diagnostics) {}

    void append(Token token, vec<Token>& output);

private:
    std::deque<std::string>&                     text;
    const std::optional<Location>&               reference;
    Diagnostics&                                 diagnostics;
    std::map<std::string_view, std::string_view> definitions;

    void define(std::string_view args, Location at);
    void use(std::string_view args, const Token& token, vec<Token>& output);
};

void MidiAliases::define(std::string_view args, Location at) {
    const auto name = word(args);
    args            = trim(args);
    if (name.empty() || args.empty()) {
        diagnostics.warn(at, "invalid-midi-alias", "ignored MIDIdef without a name and command");
        return;
    }
    if (!definitions.contains(name))
        definitions.emplace(name, text.emplace_back(fmt::format("MIDI {}", args)));
}

void MidiAliases::use(std::string_view args, const Token& token, vec<Token>& output) {
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

void MidiAliases::append(Token token, vec<Token>& output) {
    const auto text = trim(token.information().value_or(""));
    const auto end  = std::ranges::find_if_not(text, letter);
    const auto name = text.substr(0, static_cast<size_t>(end - text.begin()));
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
    const auto args = text.substr(name.size());
    if (name == "MIDIdef")
        define(args, token.at);
    else
        use(args, token, output);
}

template<class Rule>
const Node* child(const Node& node) {
    const auto found = std::ranges::find_if(node.children, [](const auto& candidate) {
        return candidate->template is_type<Rule>();
    });
    return found == node.children.end() ? nullptr : found->get();
}

// Lowers PEG nodes to rhythmic tokens, with no playback state. Physical
// fields interleaved in a continued line reenter at their source positions.
class MusicLine {
public:
    MusicLine(
        LogicalLine      source,
        std::string_view text,
        MusicDialect     dialect,
        Diagnostics&     diagnostics,
        MidiAliases&     aliases,
        vec<Token>&      output
    ) :
        source(source), text(text), dialect(dialect), diagnostics(diagnostics), aliases(aliases),
        output(output) {}

    void read(std::span<Token> entries, const auto& emit_entry);

private:
    LogicalLine      source;
    std::string_view text;
    MusicDialect     dialect;
    Diagnostics&     diagnostics;
    MidiAliases&     aliases;
    vec<Token>&      output;

    Location location(const Node& node) const { return source.where(node.data); }

    int           number(std::string_view value, Location at, int fallback = 0) const;
    Length        length(const Node* node) const;
    Pitch         pitch(const Node& node) const;
    Atom          note_atom(const Node& node) const;
    Atom          chord_atom(const Node& node) const;
    RepeatEndings ending_passes(const Node& node) const;
    Bar           bar(const Node& node) const;
    Tuplet        tuplet(const Node& node) const;
    Broken        broken_mark(const Node& node) const;
    void          invalid(const Node& node);

    template<class T>
    void push(const Node& node, T value) {
        output.push_back({location(node), std::move(value), source.music_line});
    }

    void emit(const Node& node, grammar::pitch);
    void emit(const Node& node, grammar::rest);
    void emit(const Node& node, grammar::chord);
    void emit(const Node& node, grammar::grace);
    void emit(const Node& node, grammar::field) {
        auto value = field(node.data);
        // MusicMacros has already applied inline definitions in written order.
        if (value.kind == 'm' || value.kind == 'r') return;
        // ABC 2.1 section 3: only body fields with an inline form may be used here.
        if (!field_allowed(value.kind, FieldSite::inline_music)) {
            diagnostics.warn(location(node), "invalid-inline-field", "ignored a non-inline field");
            return;
        }
        value.value = trim(value.value);
        if (value.kind == 'I' && header_only_instruction(value.value, location(node), diagnostics))
            return;
        Token token{location(node), value, source.music_line};
        parse_symbol_definition(token, dialect.delimiter, diagnostics);
        aliases.append(std::move(token), output);
    }
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
    void emit(const Node& node, grammar::music_spacing) {
        if (dialect.strict) push(node, Spacer{});
    }
    void emit(const Node& node, grammar::bang_break) {
        if (dialect.bang == BangRole::Decoration) invalid(node);
    }
    void emit(const Node& node, grammar::slur) { push(node, Slur{node.data.back() == '('}); }
    void emit(const Node& node, grammar::tie) {
        // A tie attached to a note or chord is already part of its atom (§4.11).
        if (dialect.strict)
            diagnostics.warn(
                location(node), "detached-tie", "a tie must immediately follow its note or chord"
            );
        else
            push(node, Tie{});
    }
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
    const auto extra_slashes = parts.slashes - 1;
    if (extra_slashes > 30) {
        diagnostics.error(at, "length-range", "duration denominator is too large");
    }
    const auto divisor = static_cast<i64>(denominator) << extra_slashes;
    if (divisor > std::numeric_limits<int>::max()) {
        diagnostics.error(at, "length-range", "duration denominator is too large");
    }
    // ABC 2.1 section 4.3 permits ratios such as 3/2 and 1/3. Quantize only at playback output.
    return {numerator, divisor};
}

int written_octave(const Node& node, const auto& locate, Diagnostics& diagnostics) {
    const auto value = child<grammar::octave>(node);
    if (!value) return 0;
    if (value->data.size() > 32)
        diagnostics.error(locate(*value), "pitch-range", "too many octave marks");
    return static_cast<int>(
        2 * std::ranges::count(value->data, '\'') - static_cast<ptrdiff_t>(value->data.size())
    );
}

Pitch read_spelling(const Node& node, const auto& locate, Diagnostics& diagnostics) {
    const auto                name = child<grammar::letter>(node)->data.front();
    std::optional<Alteration> alteration;
    if (const auto prefix = child<grammar::accidental>(node))
        alteration = accidental(prefix->data, locate(*prefix), diagnostics);
    return {
        upper(name), written_octave(node, locate, diagnostics) + lower(name), alteration, false
    };
}

Pitch MusicLine::pitch(const Node& node) const {
    auto result = read_spelling(node, [this](const Node& n) { return location(n); }, diagnostics);
    result.tie  = child<grammar::tie>(node) != nullptr;
    return result;
}

RepeatEndings MusicLine::ending_passes(const Node& node) const {
    const auto parse_pass = [&](const Node& endpoint) {
        const auto pass = integer(endpoint.data);
        if (!pass || *pass < 1)
            diagnostics.error(
                location(endpoint), "ending-range", "expected a positive ending number"
            );
        return *pass;
    };
    vec<EndingRange> ranges;
    ranges.reserve(node.children.size());
    for (const auto& range : node.children) {
        const auto first = parse_pass(*range->children.front());
        const auto last  = parse_pass(*range->children.back());
        if (first > last)
            diagnostics.error(
                location(*range), "invalid-ending", "repeat ending range is reversed"
            );
        ranges.push_back({first, last});
    }
    return RepeatEndings(std::move(ranges));
}

Bar MusicLine::bar(const Node& node) const {
    const auto* ending = child<grammar::endings>(node);
    const auto* line   = child<grammar::barline>(node);
    Bar         result{
        .resets_measure = !node.is_type<grammar::ending>(),
        // The invisible bar closes a measure like a single visible pipe.
        .section = line && line->data != "|" && line->data != "[|]",
        .endings = ending ? ending_passes(*ending) : RepeatEndings{}
    };
    if (const auto* repeat = child<grammar::repeat_bar>(node)) {
        const auto* start = child<grammar::opening_colons>(*repeat);
        const auto* end   = child<grammar::closing_colons>(*repeat);
        // ABC 2.1 section 4.8: each colon adds a pass. Bare :: is :| followed by |:.
        if (end && end->data.size() == repeat->data.size()) {
            if (end->data.size() != 2)
                diagnostics.warn(
                    location(*repeat), "ambiguous-repeat", "treated bare colon run as ::"
                );
            result.repeat_start = result.repeat_end = 2;
        } else {
            if (start) result.repeat_start = start->data.size() + 1;
            if (end) result.repeat_end = end->data.size() + 1;
        }
    }
    return result;
}

Tuplet MusicLine::tuplet(const Node& node) const {
    const auto value = [&](const Node* argument) {
        return argument ? number(argument->data, location(node)) : 0;
    };
    return {
        value(child<grammar::tuplet_count>(node)),
        value(child<grammar::tuplet_time>(node)),
        value(child<grammar::tuplet_notes>(node))
    };
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

Atom MusicLine::note_atom(const Node& node) const {
    const auto note = pitch(node);
    return {{note}, length(child<grammar::length>(node)), false, note.tie};
}

Atom MusicLine::chord_atom(const Node& node) const {
    Atom   atom{.tie = child<grammar::tie>(node) != nullptr, .chord = true};
    size_t attached = 0;
    for (const auto& member : node.children) {
        if (member->is_type<grammar::decoration>()) {
            check_decoration(member->data, dialect.delimiter, location(*member), diagnostics);
            atom.decorations.emplace_back(atom.pitches.size(), location(*member), member->data);
            continue;
        }
        if (member->is_type<grammar::rest>()) {
            // Silent members still have to carry a valid written length.
            (void)length(child<grammar::length>(*member));
            atom.decorations.resize(attached);
            continue;
        }
        if (member->is_type<grammar::chord_spacing>()) atom.decorations.resize(attached);
        if (!member->is_type<grammar::pitch>()) continue;
        if (atom.pitches.size() >= 128)
            diagnostics.error(
                location(*member),
                "group-limit",
                "chords and grace groups support at most 128 notes"
            );
        atom.pitches.push_back(pitch(*member));
        attached           = atom.decorations.size();
        const auto written = length(child<grammar::length>(*member));
        const auto index   = atom.pitches.size() - 1;
        if (index == 0)
            atom.length = written;
        else if (
            written.numerator * atom.length.denominator
            != atom.length.numerator * written.denominator
        )
            atom.member_lengths.push_back({index, written});
    }
    for (const auto& mark : std::span(atom.decorations).subspan(attached))
        diagnostics.warn(mark.at, "orphan-decoration", "chord decoration has no following note");
    atom.decorations.resize(attached);
    auto suffix = length(child<grammar::length>(node));
    if (!dialect.strict) {
        // abc2midi treats 1/1 as the default. Other suffixes replace all lengths.
        suffix.numerator = std::max(i64{1}, suffix.numerator);
        if (suffix.numerator != 1 || suffix.denominator != 1) {
            atom.length = suffix;
            atom.member_lengths.clear();
            atom.chord_gate_override = true;
        }
        return atom;
    }
    // ABC 2.1 section 4.17 multiplies each written length by the suffix.
    const auto apply_suffix = [suffix](Length& duration) {
        duration.numerator *= suffix.numerator;
        duration.denominator *= suffix.denominator;
    };
    apply_suffix(atom.length);
    for (auto& member : atom.member_lengths) apply_suffix(member.length);
    return atom;
}

void MusicLine::emit(const Node& node, grammar::pitch) { push(node, note_atom(node)); }

void MusicLine::emit(const Node& node, grammar::rest) {
    const auto* suffix = child<grammar::length>(node);
    if (lower(node.data.front())) {
        push(node, Atom{{}, length(suffix)});
        return;
    }

    const auto count = suffix ? integer(suffix->data) : std::optional{1};
    if (!count || *count < 1) {
        diagnostics.error(
            location(*suffix), "invalid-measure-rest", "expected a positive integer measure count"
        );
    }

    // ABC 2.1 section 4.5: the internal bars have the same semantics as written bars.
    const auto at = location(node);
    diagnostics.spend(2 * static_cast<size_t>(*count - 1) * PlaybackWork::token, at);
    for (int measure = 1; measure < *count; ++measure) {
        output.push_back({at, Atom{.measure_rest = true}, source.music_line});
        output.push_back({at, Bar{}, source.music_line});
    }
    output.push_back({at, Atom{.measure_rest = true}, source.music_line});
}

void MusicLine::emit(const Node& node, grammar::chord) {
    auto atom = chord_atom(node);
    if (atom.pitches.empty())
        diagnostics.warn(location(node), "empty-chord", "empty chord treated as a rest");
    push(node, std::move(atom));
}

void MusicLine::emit(const Node& node, grammar::grace) {
    Grace                                      group;
    size_t                                     count = 0;
    std::optional<std::pair<Broken, Location>> pending;
    for (const auto& member : node.children) {
        if (member->is_type<grammar::broken>()) {
            const auto mark = broken_mark(*member);
            const auto at   = location(*member);
            if (!group.attacks.empty() && !pending)
                pending = {mark, at};
            else
                diagnostics.warn(
                    at,
                    "unexpected-broken-rhythm",
                    "ignored a broken rhythm mark without a preceding grace attack"
                );
            continue;
        }
        auto atom = member->is_type<grammar::chord>() ? chord_atom(*member) : note_atom(*member);
        if (atom.pitches.empty()) {
            diagnostics.warn(location(*member), "empty-chord", "ignored empty grace chord");
            continue;
        }
        count += atom.pitches.size();
        if (count > 128)
            diagnostics.error(
                location(*member), "group-limit", "grace groups support at most 128 notes"
            );
        long double scale = 1;
        if (pending) {
            const auto [mark, at] = *pending;
            auto& previous        = group.attacks.back();
            if (previous.weight() == atom.length.fraction()) {
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
        group.attacks.push_back({std::move(atom), scale});
    }
    if (pending)
        diagnostics.warn(
            pending->second,
            "unfinished-broken-rhythm",
            "broken rhythm did not reach a following grace attack"
        );
    push(node, std::move(group));
}

Broken MusicLine::broken_mark(const Node& node) const {
    if (node.data.size() > 30) {
        diagnostics.error(location(node), "rhythm-range", "too many broken rhythm marks");
    }
    return {node.data.front() == '>' ? 1 : -1, static_cast<int>(node.data.size())};
}

void MusicLine::emit(const Node& node, grammar::broken) { push(node, broken_mark(node)); }

void MusicLine::emit(const Node& node, grammar::overlay) {
    if (node.data == "(&")
        push(node, Overlay::Start);
    else if (node.data == "&)")
        push(node, Overlay::End);
    else if (dialect.strict && node.data.size() > 1) {
        if (node.data == "&&")
            push(node, Overlay::PreviousTwo);
        else
            diagnostics.warn(
                location(node), "unsupported-overlay", "ignored a run longer than '&&'"
            );
    } else {
        for (auto mark = node.data; !mark.empty(); mark.remove_prefix(1))
            output.push_back({source.where(mark), Overlay::Split, source.music_line});
    }
}

void MusicLine::emit(const Node& node, grammar::decoration) {
    const auto value = node.data;
    check_decoration(value, dialect.delimiter, location(node), diagnostics);
    push(node, Decoration{value, value.data() + value.size() == text.data() + text.size()});
}

void MusicLine::read(std::span<Token> entries, const auto& emit_entry) {
    Input      input(text);
    const auto root = dialect.bang == BangRole::Layout ?
                          peg::parse_tree::parse<grammar::bang_layout_music, Select>(input) :
                          peg::parse_tree::parse<grammar::music, Select>(input);
    if (!root)
        diagnostics.error(source.where(text), "invalid-music", "could not read the music line");
    auto entry = entries.begin();
    for (const auto& node : root->children) {
        // A field encountered inside a continued atom takes effect before that atom sounds.
        if (entry != entries.end()) {
            const auto end = source.where(node->data.substr(node->data.size() - 1));
            while (entry != entries.end() && entry->at < end) emit_entry(std::move(*entry++));
        }
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
                grammar::music_spacing,
                grammar::bang_break,
                grammar::slur,
                grammar::tie,
                grammar::invalid>{}
        );
    }
    while (entry != entries.end()) emit_entry(std::move(*entry++));
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

// Owns the file/tune header and body state while scanning physical lines.
// It assembles continuations and rewrites before passing music to MusicLine;
// the source fragments preserve locations for both tokens and diagnostics.
class TuneLines {
public:
    TuneLines(
        std::optional<AbcVersion> version, const TextLoader& headers, Diagnostics& diagnostics
    ) :
        diagnostics(diagnostics), headers(headers), file_version(version),
        aliases(tune.owned_text, reference, diagnostics), macros(diagnostics) {
        tune.version = version;
    }
    bool line(std::string_view raw, Location at, bool has_newline);
    Tune finish(Location end);

private:
    Diagnostics&      diagnostics;
    const TextLoader& headers;
    // Canonical names owned by the include calls currently being parsed.
    vec<std::string_view>     active_headers;
    Tune                      tune;
    std::optional<AbcVersion> file_version;
    SourceCharset             charset;
    std::optional<Location>   reference;
    std::optional<Location>   key_without_reference;
    MidiAliases               aliases;
    MusicMacros               macros;
    MusicDialect              dialect{};
    bool                      in_body   = false;
    bool                      saw_music = false;
    bool                      ended     = false;
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
        vec<SourceFragment>        fragments;
        bool                       accepts_bare_history = false;
    };
    std::optional<PendingField> pending_field;
    vec<Location>               legacy_history_fields;

    size_t                  music_line = 0;
    std::string             joined_music;
    vec<SourceFragment>     fragments;
    vec<Token>              continued_entries;
    std::optional<Location> continuation_at;
    bool                    music_quoted = false;

    vec<Token>& output() { return in_body ? tune.body : tune.header; }
    void        append(Token token);
    void        append_entry(Token token);
    void        include_header(std::string_view name, Location at);
    void        directive(std::string_view value, Location at);
    void        close_layout();
    void        observe_header_entry(char kind, Location at);
    bool        start_field(Field value, Location at);
    void        extend_field(std::string_view value, Location at);
    void        flush_field();
    void        music(std::string_view bytes, size_t line, bool has_newline, bool ends_quoted);
    void        emit_music(LogicalLine line);
    void        flush_music();
};

void TuneLines::observe_header_entry(char kind, Location at) {
    if (in_body || at.source || first_tune_entry) return;
    // Directives may arrive while X: is still open to a continuation.
    if (reference || (pending_field && pending_field->field.kind == 'X'))
        first_tune_entry = HeaderEntry{kind, at};
}

bool TuneLines::start_field(Field value, Location at) {
    if (value.kind == 'X') {
        charset.finish_header(diagnostics);
        flush_music();
        if (in_body) {
            diagnostics.warn(at, "extra-tunes", "only the first tune was read");
            return false;
        }
        if (reference) diagnostics.error(at, "missing-key", "first tune has no K: field");
    } else {
        // Keep the written order even when an include or macro consumes the field.
        observe_header_entry(value.kind, at);
    }
    constexpr std::string_view kForbiddenInBody = "ABCDEFGXYZabcdefgxyz";
    if (in_body
        && (dialect.strict ? !field_allowed(value.kind, FieldSite::body) :
                             kForbiddenInBody.find(value.kind) != kForbiddenInBody.npos)) {
        diagnostics.warn(at, "invalid-body-field", "ignored a field not allowed in the tune body");
        return true;
    }
    value.value = without_comment(value.value);
    pending_field.emplace(
        PendingField{
            .field = value, .at = at, .accepts_bare_history = !in_body && value.kind == 'H'
        }
    );
    return true;
}

void TuneLines::extend_field(std::string_view value, Location at) {
    if (!pending_field) {
        diagnostics.warn(at, "orphan-field-continuation", "ignored +: without a preceding field");
        return;
    }
    auto&      pending = *pending_field;
    const auto field   = pending.field;
    value              = without_comment(value);
    // Only string fields insert an implicit space (ABC 2.1 section 3.3).
    const bool string_field = std::string_view("ABCDFGHNORSTWwZ").find(field.kind) != value.npos;
    if (!pending.extended) {
        pending.extended.emplace(string_field ? trim(field.value) : field.value);
        if (field.kind == 's') {
            auto start = pending.at;
            start.column += 2;
            pending.fragments.push_back({0, start});
        }
    }
    if (string_field) pending.extended->push_back(' ');
    if (field.kind == 's') {
        at.column += 2;
        pending.fragments.push_back({pending.extended->size(), at});
    }
    pending.extended->append(string_field ? trim(value) : value);
}

void TuneLines::flush_field() {
    if (!pending_field) return;
    auto [field, at, extended, directives, positions, accepts_bare_history]
        = std::move(*pending_field);
    pending_field.reset();   // An include can parse more fields while emitting this one.
    if (extended) field.value = tune.owned_text.emplace_back(std::move(*extended));
    if (field.kind == 's') {
        field.fragments = std::move(positions);
    } else {
        field.value = trim(field.value);
    }
    if (field.kind == 'X') { reference = at; }
    const bool outside_tune = field.kind == 'K' && !reference;
    const bool opens_body   = field.kind == 'K' && reference && !in_body;
    if (outside_tune) {
        if (!key_without_reference) key_without_reference = at;
        diagnostics.warn(at, "invalid-file-header-field", "ignored K: before the first tune");
    } else {
        append_entry({at, std::move(field)});
    }
    if (opens_body) {
        dialect                   = read_music_dialect(tune, diagnostics);
        tune.decoration_delimiter = dialect.delimiter;
        for (auto& token : tune.header)
            parse_symbol_definition(token, dialect.delimiter, diagnostics);
        in_body  = true;
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
    if (fragments.empty())
        append(std::move(token));
    else
        continued_entries.push_back(std::move(token));
}

void TuneLines::append(Token token) {
    if (const auto* field = macro_definition(token)) {
        macros.define(field->value, token.at);
        // A body definition separates adjacent lyrics or symbol rows.
        if (in_body) tune.body.push_back(std::move(token));
        return;
    }
    if (auto args = token.information()) {
        if (in_body && header_only_instruction(*args, token.at, diagnostics)) return;
        const auto name = word(*args);
        if (name == "abc-charset") {
            if (reference && token.at.source == 0)
                header_only_instruction("abc-charset", token.at, diagnostics);
            else
                charset.declare(trim(*args), token.at, diagnostics);
            return;
        }
        if (!in_body && name == "abc-version") {
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
    if (in_body) { parse_symbol_definition(token, dialect.delimiter, diagnostics); }
    aliases.append(std::move(token), output());
}

bool continuation(std::string_view part) {
    size_t count = 0;
    while (part.ends_with('\\')) {
        part.remove_suffix(1);
        ++count;
    }
    return count % 2 != 0;
}

bool layout_delimiter(std::string_view name) {
    return std::ranges::any_of(kLayoutBlocks, [&](const auto& block) {
        return name == block.first || name == block.second;
    });
}

bool TuneLines::line(std::string_view raw, Location at, bool has_newline) {
    diagnostics.record_line(at.source, raw);
    charset.check_line(raw, at, diagnostics);
    const auto first = raw.find_first_not_of(whitespace);
    const auto text  = raw.substr(std::min(first, raw.size()));
    at.column        = first == raw.npos ? 1 : first + 1;
    if (!ended && !music_quoted) {
        // Section 2.2.6 lets I: instructions own their field continuations.
        // Standalone layout delimiters must be recognized at the physical line.
        if (text.starts_with("%%")) {
            directive(trim(without_comment(text.substr(2))), at);
            return true;
        }
        if (text.starts_with("I:")) {
            const auto instruction = trim(without_comment(text.substr(2)));
            if (layout_delimiter(instruction)) {
                directive(instruction, at);
                return true;
            }
        }
    }
    if (layout || text.starts_with('%')) return true;

    const auto value = music_quoted ? std::nullopt : line_field(text);
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
    const bool field_line = value && !(in_body && starts_with_repeat(text));
    if (at.source && !text.empty()
        && (!field_line || value->kind == 'X' || (value->kind == 'K' && !reference))) {
        diagnostics.warn(
            at, "invalid-include-content", "ignored non-header ABC in an included file"
        );
        return true;
    }
    // A note or rest followed immediately by a repeat bar is music, not a field.
    if (field_line) return start_field(*value, at);

    // A quoted music token can itself span consecutive continued lines.
    auto       quoted  = music_quoted;
    const auto content = without_comment(text, quoted);
    if (content.empty()) {
        flush_music();
        if (continuation_at)
            diagnostics.warn(
                *continuation_at, "stray-continuation", "cannot continue across a blank line"
            );
        continuation_at.reset();
        music_line   = 0;
        music_quoted = false;
        if (!at.source && reference) {
            // A blank line ends a strict tune, even before any music is written.
            // An included header's blank lines do not end the enclosing tune.
            const bool strict = tune.version >= AbcVersion{2, 1};
            if (strict && !in_body)
                diagnostics.error(at, "missing-key", "expected K: before the blank line");
            if (in_body && (saw_music || strict)) ended = true;
        }
        return true;
    }

    // Free text in a tunebook is not music until the first tune has started.
    if (!reference && !in_body) return true;
    if (!in_body) diagnostics.error(at, "missing-key", "expected K: before the music");
    music(raw.substr(0, first + content.size()), at.line, has_newline, quoted);
    return true;
}

void TuneLines::emit_music(LogicalLine line) {
    const auto use_rewrite = [&](RewrittenMusic& music) {
        line.bytes     = tune.owned_text.emplace_back(std::move(music.text));
        line.fragments = music.fragments;
    };
    // Remarks are absent from the input text. Section 9.1 macros still match literal
    // targets before the playback grammar ignores reserved music characters.
    auto unremarked = elide_music<grammar::Elision::Remarks>(line, dialect.bang);
    if (unremarked) use_rewrite(*unremarked);
    auto expansion = macros.expand(line, dialect, continued_entries);
    if (expansion) use_rewrite(*expansion);
    auto normalized = elide_music<grammar::Elision::Playback>(line, dialect.bang);
    if (normalized) use_rewrite(*normalized);
    const auto content    = trim(line.bytes);
    const auto first      = tune.body.size();
    const auto emit_entry = [this](Token token) {
        // Macro expansion has already applied continued definitions in written order.
        if (macro_definition(token))
            tune.body.push_back(std::move(token));
        else
            append(std::move(token));
    };
    MusicLine(line, content, dialect, diagnostics, aliases, tune.body)
        .read(continued_entries, emit_entry);
    continued_entries.clear();
    if (content.empty()) return;
    if (first == tune.body.size())
        tune.body.push_back({line.where(content), EmptyMusicLine{}, line.music_line});
    saw_music = true;
}

void TuneLines::flush_music() {
    if (fragments.empty()) return;
    const auto& bytes = tune.owned_text.emplace_back(std::move(joined_music));
    emit_music({bytes, fragments.front().at.line, music_line, fragments});
    joined_music.clear();
    fragments.clear();
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
    Input      input(trim(text));
    const auto root = peg::parse_tree::parse<grammar::tempo_field, TempoSelect>(input);
    if (!root)
        diagnostics.error(
            at, "invalid-tempo", "expected a tempo label or up to four beats and a metronome value"
        );

    const auto ratio = [&](const Node& beat) {
        const auto numerator = integer(beat.children.front()->data);
        const auto denominator
            = beat.children.size() == 1 ? std::optional(1) : integer(beat.children.back()->data);
        if (!numerator || *numerator == 0 || !denominator || *denominator == 0)
            diagnostics.error(at, "invalid-fraction", "tempo beats need a positive fraction");
        return static_cast<long double>(*numerator) / *denominator;
    };

    TempoNotation result;
    if (const auto* label = child<grammar::tempo_label>(*root))
        result.label = label->data.substr(1, label->data.size() - 2);
    if (const auto* bpm = child<grammar::tempo_bpm>(*root)) {
        result.bpm = integer(bpm->data);
        if (!result.bpm || *result.bpm <= 0)
            diagnostics.error(at, "invalid-tempo", "tempo needs a positive metronome value");
    }
    if (const auto* beats = child<grammar::tempo_whole_notes>(*root)) {
        result.unit_relative = false;
        result.beat          = 0;
        for (const auto& beat : beats->children) result.beat += ratio(*beat);
    } else if (const auto* relative = child<grammar::tempo_unit_notes>(*root)) {
        if (const auto* beat = child<grammar::tempo_beat>(*relative)) result.beat = ratio(*beat);
    }
    return result;
}

void compile_fields(vec<Token>& tokens, const auto& strict_at, Diagnostics& diagnostics) {
    for (auto& token : tokens) {
        const bool strict = strict_at(token.at);
        if (const auto* field = std::get_if<Field>(&token.value)) {
            switch (field->kind) {
            case 'X': {
                const auto value = field->value;
                if (value.empty()) break;
                // The number identifies a tune, not an arithmetic operand.
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
                const auto tempo = tempo_notation(field->value, token.at, diagnostics);
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
                if (strict)
                    diagnostics.warn(token.at, "deprecated-field", "E: spacing is deprecated");
                break;
            }
        } else if (strict) {
            const auto* directive = std::get_if<Directive>(&token.value);
            if (!directive) continue;
            auto       args = directive->value;
            const auto name = word(args);
            if (name == "continueall" && trim(args) == "true")
                diagnostics.warn(
                    token.at,
                    "deprecated-directive",
                    "use I:linebreak ! instead of %%continueall true"
                );
            else if (name == "abc-copyright" || name == "abc-edited-by")
                diagnostics.warn(
                    token.at, "deprecated-directive", fmt::format("use Z: instead of %%{}", name)
                );
        }
    }
}

// ABC 2.1 sections 3.2, 4.16 and 9: substitute after macro expansion and
// before voices and navigation can change the order of the written tokens.
void expand_shorthand(Tune& tune) {
    struct Binding {
        Location         at;
        std::string_view expansion;
    };
    std::array<vec<Binding>, 128> definitions;
    const auto expansion = [&](std::string_view spelling, Location at) -> std::string_view {
        if (spelling.size() != 1) return {};
        const auto index = static_cast<unsigned char>(spelling.front());
        if (index >= definitions.size()) return {};
        const auto& history = definitions[index];
        // Field tokens between continued fragments precede the composite
        // atom. Earlier chord members still use the definition at their own site.
        const auto after = std::ranges::upper_bound(history, at, {}, &Binding::at);
        return after == history.begin() ? std::string_view{} : std::prev(after)->expansion;
    };
    const auto expand_members = [&](Atom& atom) {
        for (auto& mark : atom.decorations)
            if (const auto text = expansion(mark.spelling, mark.at); !text.empty())
                mark.spelling = text;
    };
    for (auto* tokens : {&tune.header, &tune.body}) {
        for (auto& token : *tokens) {
            if (const auto* definition = std::get_if<SymbolDefinition>(&token.value))
                definitions[static_cast<unsigned char>(definition->shorthand)].push_back(
                    {token.at, definition->spelling}
                );
            else if (auto* decoration = std::get_if<Decoration>(&token.value)) {
                if (const auto text = expansion(decoration->spelling, token.at); !text.empty())
                    decoration->spelling = text;
            } else if (auto* atom = std::get_if<Atom>(&token.value))
                expand_members(*atom);
            else if (auto* grace = std::get_if<Grace>(&token.value))
                for (auto& attack : grace->attacks) expand_members(attack.atom);
        }
    }
}

Tune TuneLines::finish(Location end) {
    flush_field();
    charset.finish_header(diagnostics);
    flush_music();
    close_layout();
    if (!reference && key_without_reference)
        diagnostics.error(
            *key_without_reference, "missing-reference", "expected an X: field before K:"
        );
    if (!in_body) diagnostics.error(end, "missing-key", "expected a K: field in the tune header");
    tune.reference    = *reference;
    const bool strict = tune.version >= AbcVersion{2, 1};
    if (strict && (!first_tune_entry || first_tune_entry->kind != 'T')) {
        diagnostics.warn(
            first_tune_entry ? first_tune_entry->at : tune.key,
            "title-order",
            "expected a T: field immediately after X:"
        );
    }
    const auto file_header = [&](Location at) { return at < *reference; };
    const auto strict_header
        = [&](Location at) { return file_header(at) ? file_version >= AbcVersion{2, 1} : strict; };
    for (const auto at : legacy_history_fields) {
        if (strict_header(at))
            diagnostics.warn(at, "deprecated-history", "use +: for history continuation lines");
    }
    // A late abc-version instruction applies to the entire header that contains it.
    std::erase_if(tune.header, [&](const Token& token) {
        const auto* field = std::get_if<Field>(&token.value);
        if (!field) return false;
        if (!strict_header(token.at)) return false;
        const bool file = file_header(token.at);
        if (field_allowed(field->kind, file ? FieldSite::file_header : FieldSite::tune_header))
            return false;
        diagnostics.warn(
            token.at,
            file ? "invalid-file-header-field" : "invalid-tune-header-field",
            fmt::format("ignored {}: in the {} header", field->kind, file ? "file" : "tune")
        );
        return true;
    });
    compile_fields(tune.header, strict_header, diagnostics);
    compile_fields(tune.body, [strict](Location) { return strict; }, diagnostics);
    expand_shorthand(tune);
    return std::move(tune);
}

}   // namespace

std::optional<MeterNumbers> meter_numbers(std::string_view text) {
    Input      input(trim(text));
    const auto root = peg::parse_tree::parse<grammar::meter_field, MeterSelect>(input);
    if (!root) return std::nullopt;
    MeterNumbers result;
    result.denominator     = root->children.back()->data;
    const auto group_count = root->children.size() - 1;
    result.groups.reserve(group_count);
    for (const auto& group : root->children | std::views::take(group_count))
        result.groups.push_back(group->data);
    return result;
}

std::string part_order(
    std::string_view text, size_t max_parts, Location at, Diagnostics& diagnostics
) {
    // ABC 2.1 section 3.1.9 ignores dots anywhere, including inside a repeat count.
    std::string undotted(text);
    std::erase(undotted, '.');
    Input      input(undotted);
    const auto root = peg::parse_tree::parse<grammar::part_field, PartSelect>(input);
    if (!root)
        diagnostics.error(
            at, "invalid-parts", "expected part letters, groups or repeat counts in P:"
        );

    // A group remembers where its first copy begins. No recursive parse or
    // evaluation is needed, and the limit applies to playback, not nesting.
    std::string labels;
    vec<size_t> groups;
    for (const auto& item : root->children) {
        if (item->is_type<grammar::part_open>()) {
            groups.push_back(labels.size());
            continue;
        }
        size_t start = labels.size();
        if (item->is_type<grammar::part_close>()) {
            if (groups.empty()) diagnostics.error(at, "invalid-parts", "unmatched ')' in P: order");
            start = groups.back();
            groups.pop_back();
        } else {
            if (labels.size() == max_parts)
                diagnostics.error(
                    at, "expansion-limit", "part order exceeds the playback expansion limit"
                );
            labels.push_back(upper(item->data.front()));
        }

        size_t count = 1;
        if (const auto* repeats = child<grammar::part_count>(*item)) {
            const auto written = integer(repeats->data);
            if (!written || *written < 1 || static_cast<size_t>(*written) > max_parts)
                diagnostics.error(
                    at,
                    "part-count-range",
                    fmt::format("part repeat count must be between 1 and {}", max_parts)
                );
            count = static_cast<size_t>(*written);
        }
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
    LyricEncoding           encoding,
    Location                at,
    Diagnostics&            diagnostics
) {
    diagnostics.spend(PlaybackWork::text(text), at);
    Input         input(text);
    NoteAlignment alignment{notes, bars};
    LyricScan     scan{encoding, {}};
    if (encoding == LyricEncoding::AbcText)
        peg::parse<
            peg::seq<
                peg::opt<grammar::lyric_stanza>,
                grammar::lyric_verse<grammar::styled_lyric_word, grammar::text_font_run>>,
            LyricAction>(input, alignment, scan);
    else
        peg::parse<grammar::lyric_verse<grammar::lyric_word<grammar::lyric_syllable>>, LyricAction>(
            input, alignment, scan
        );
    if (alignment.excess)
        diagnostics.warn(at, "extra-lyrics", "more lyric syllables than available notes");
    return std::move(scan.placements);
}

std::string encode_text(std::string_view text, TextContext context) {
    std::string result;
    for (const char c : text) {
        if (c == '\n' && context == TextContext::VoiceName) {
            result += "\\n";
            continue;
        }
        const auto byte = static_cast<unsigned char>(c);
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

std::string decode_text(
    std::string_view text, TextContext context, std::optional<AbcVersion> version
) {
    // Unversioned exports escaped quotes but left entities and mnemonic-like text literal.
    if (context == TextContext::ChordSymbol) return decode_text_as<grammar::text_string>(text);
    const bool line_breaks = context == TextContext::VoiceName;
    if (version >= AbcVersion{2, 1})
        return decode_text_as<grammar::styled_text_string>(text, line_breaks);
    return decode_text_as<grammar::legacy_text_string>(text, line_breaks);
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
    Input         input(field.value);
    NoteAlignment alignment{notes, bars};
    auto          start = at;
    start.column += 2;
    SymbolScan scan{field, start, delimiter, diagnostics, {}};
    peg::parse<grammar::symbol_line, SymbolAction>(input, alignment, scan);
    if (!input.empty()) {
        diagnostics.warn(
            scan.where(input.current()),
            "invalid-symbol-line",
            "expected a decoration, quoted text, '*' or '|'"
        );
        return {};
    }
    if (alignment.excess)
        diagnostics.warn(at, "extra-symbols", "more symbol slots than available notes");
    return std::move(scan.placements);
}

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
        if (std::get<TextReadError>(loaded) == TextReadError::cycle) {
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
    auto             position       = diagnostics.register_include(header->name, at);
    const auto       parent_layout  = std::exchange(layout, std::nullopt);
    auto             parent_charset = std::exchange(charset, SourceCharset{});
    std::string_view text           = tune.owned_text.emplace_back(std::move(header->text));
    if (text.starts_with("\xef\xbb\xbf")) text.remove_prefix(3);
    while (!text.empty()) {
        const auto physical = take_line(text);
        line(physical.text, position, physical.has_newline);
        ++position.line;
    }
    flush_field();
    charset.finish_header(diagnostics);
    charset = std::move(parent_charset);
    close_layout();
    layout = parent_layout;
    active_headers.pop_back();
}

void TuneLines::music(std::string_view bytes, size_t line, bool has_newline, bool ends_quoted) {
    if (music_line == 0) music_line = line;
    if (has_newline && bytes.find('\\') != bytes.npos) {
        // Section 2.2.5 elides remarks before testing for a continuation.
        // Keep the original slices for joining and for physical diagnostics.
        LogicalLine physical{bytes, line, music_line};
        const auto  unremarked
            = elide_music<grammar::Elision::Remarks>(physical, dialect.bang, music_quoted);
        if (unremarked) {
            physical.bytes     = unremarked->text;
            physical.fragments = unremarked->fragments;
        }
        if (continuation(trim(physical.bytes))) {
            const auto last = physical.bytes.find_last_not_of(whitespace);
            continuation_at = physical.where(physical.bytes.substr(last, 1));
            if (unremarked)
                without_comment(physical.bytes.substr(0, last), music_quoted);
            else
                music_quoted = ends_quoted;
            fragments.push_back({joined_music.size(), {line, 1}});
            joined_music.append(bytes.substr(0, continuation_at->column - 1));
            return;
        }
    }
    if (fragments.empty()) {
        emit_music({bytes, line, music_line});
    } else {
        fragments.push_back({joined_music.size(), {line, 1}});
        joined_music.append(bytes);
        flush_music();
    }
    continuation_at.reset();
    music_line   = 0;
    music_quoted = false;
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
              static_cast<i64>(accidental_quantity(parts.denominator, 2, at, diagnostics))
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
    Input input(text);
    if (!peg::parse<peg::seq<grammar::accidental, peg::eof>>(input)) {
        diagnostics.warn(
            at, "invalid-accidental", fmt::format("ignored malformed accidental '{}'", text)
        );
        return {};
    }
    if (text.find_first_not_of("^_=") == 1 && text.front() != '=')
        return microtone(text.substr(1), text.front() == '_' ? -1 : 1, at, diagnostics);
    diagnostics.warn(
        at, "invalid-accidental", "ignored ambiguous accidental; use a single sign for microtones"
    );
    return {};
}

std::optional<Pitch> drum_spelling(std::string_view text, Location at, Diagnostics& diagnostics) {
    Input      input(text);
    const auto root
        = peg::parse_tree::parse<peg::seq<grammar::pitch_spelling, peg::eof>, Select>(input);
    if (!root)
        diagnostics.error(
            at, "invalid-drummap", "drummap needs a pitch without a duration, such as ^F,"
        );
    const auto pitch = read_spelling(*root, [at](const Node&) { return at; }, diagnostics);
    if (child<grammar::accidental>(*root) && !pitch.accidental) return {};
    return pitch;
}

Tune tokenize(std::string_view text, const TextLoader& headers, Diagnostics& diagnostics) {
    if (text.starts_with("\xef\xbb\xbf")) text.remove_prefix(3);
    const auto first_line       = text.substr(0, text.find_first_of("\r\n"));
    const bool declares_version = first_line.starts_with("%abc-");
    const auto version          = declares_version ?
                                      abc_version(trim(without_comment(first_line.substr(5)))) :
                                      std::nullopt;
    TuneLines  tune(version, headers, diagnostics);
    size_t     line = 1;
    while (!text.empty()) {
        const auto physical = take_line(text);
        if (!tune.line(physical.text, {line++, 1}, physical.has_newline)) break;
    }
    if (declares_version && !version)
        diagnostics.warn({1, 6}, "invalid-version", "expected an ABC version such as 2.1");
    return tune.finish({line, 1});
}


}   // namespace symusic::abc::detail

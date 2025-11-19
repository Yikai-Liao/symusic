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
#include <type_traits>
#include <utility>
#include <type_traits>

#include <lexy/action/parse.hpp>
#include <lexy/callback.hpp>
#include <lexy/callback/container.hpp>
#include <lexy/dsl.hpp>
#include <lexy/dsl/code_point.hpp>
#include <lexy/input_location.hpp>
#include <lexy/input/string_input.hpp>

namespace miniabc {

namespace {

using namespace std::string_view_literals;

thread_local const lexy::string_input<lexy::utf8_encoding>* g_current_input = nullptr;

struct SourceLocation {
    std::size_t line   = 0;
    std::size_t column = 0;
};

template <typename Iterator>
SourceLocation make_location(Iterator pos) {
    if (!g_current_input) { return {}; }
    const auto loc = lexy::get_input_location(*g_current_input, pos);
    return SourceLocation{loc.line_nr(), loc.column_nr()};
}

template <typename Lexeme>
int lexeme_to_int(const Lexeme& lex) {
    int value = 0;
    for (auto ch : lex) { value = value * 10 + (ch - '0'); }
    return value;
}

template <typename T>
struct ListSink {
    std::vector<T> items;
    using return_type = std::vector<T>;

    void operator()(T value) { items.push_back(std::move(value)); }
    std::vector<T> finish() && { return std::move(items); }
};

template <typename T>
struct ListBuilder {
    using return_type = std::vector<T>;
    static constexpr auto sink() { return ListSink<T>{}; }
    std::vector<T> operator()(std::vector<T> values) const { return values; }
};

template <typename T>
constexpr bool has_value_like(const T& value) {
    if constexpr (requires { value.has_value(); }) {
        return value.has_value();
    } else {
        return !std::is_same_v<std::decay_t<T>, lexy::nullopt>;
    }
}

namespace ast {

struct Field {
    std::string   id;
    std::string   value;
    SourceLocation loc;
};

struct Directive {
    std::string   text;
    SourceLocation loc;
};

struct Lyric {
    bool          block = false;
    std::string   text;
    SourceLocation loc;
};

struct Tuplet {
    int           p   = 0;
    int           q   = 0;
    int           r   = 0;
    SourceLocation loc;
};

struct Note {
    std::string           accidentals;
    char                  letter = 'C';
    std::string           octave_mod;
    std::string           length_text;
    bool                  tie = false;
    bool                  explicit_natural = false;
    bool                  grace = false;
    std::vector<std::string> decorations;
    std::vector<std::string> chord_symbols;
    SourceLocation         loc;
};

struct Rest {
    char            symbol = 'z';
    std::string     length_text;
    bool            invisible = false;
    SourceLocation  loc;
};

struct Bar {
    std::string     token;
    SourceLocation  loc;
};

struct InlineField {
    Field field;
};

struct Chord {
    std::vector<Note> notes;
    std::string       length_text;
    bool              grace = false;
    SourceLocation    loc;
};

struct GraceGroup {
    std::vector<Note> notes;
    SourceLocation    loc;
};

struct BrokenRhythm {
    bool           greater = true;  // '>' when true, '<' when false
    SourceLocation loc;
};

struct Continuation {
    SourceLocation loc;
};

using MusicItem = std::variant<Note, Rest, Chord, Bar, Tuplet, InlineField, GraceGroup, BrokenRhythm, Continuation>;

struct MusicLine {
    std::vector<MusicItem> items;
    SourceLocation         loc;
};

struct Statement {
    std::variant<Field, Directive, Lyric, MusicLine> payload;
};

struct Document {
    std::vector<Statement> statements;
};

}  // namespace ast

namespace grammar {

namespace dsl = lexy::dsl;

struct ParsingState {
    const lexy::string_input<lexy::utf8_encoding>* input = nullptr;
};

struct comment {
    static constexpr auto rule = dsl::lit_c<'%'> >> dsl::while_(dsl::peek_not(dsl::newline) >> dsl::any);
    static constexpr auto value = lexy::noop;
};

struct line_end {
    static constexpr auto rule = dsl::opt(dsl::p<comment>) + (dsl::newline | dsl::eof);
    static constexpr auto value = lexy::noop;
};

struct identifier : lexy::token_production {
    static constexpr auto rule  = dsl::identifier(dsl::ascii::alpha);
    static constexpr auto value = lexy::as_string<std::string>;
};

struct field_line {
    static constexpr auto rule = [] {
        auto id    = dsl::p<identifier>;
        auto value = dsl::capture(dsl::token(dsl::while_(dsl::peek_not(dsl::newline) >> dsl::any)));
        return dsl::position + id + dsl::lit_c<':'> + value + dsl::p<line_end>;
    }();

    static constexpr auto value = lexy::callback<ast::Field>(
        [](auto pos, std::string id, auto val) {
            return ast::Field{
                std::move(id),
                std::string(val.begin(), val.end()),
                make_location(pos)
            };
        }
    );
};

struct directive_line {
    static constexpr auto rule = dsl::position
                                 + dsl::lit_c<'%'> + dsl::lit_c<'%'>
                                 + dsl::capture(dsl::token(dsl::while_(dsl::peek_not(dsl::newline) >> dsl::any)))
                                 + dsl::p<line_end>;

    static constexpr auto value = lexy::callback<ast::Directive>(
        [](auto pos, auto lex) {
            std::string text = "%%";
            text.append(lex.begin(), lex.end());
            return ast::Directive{std::move(text), make_location(pos)};
        }
    );
};

struct lyric_line {
    static constexpr auto rule = dsl::position
                                 + dsl::capture(dsl::token(dsl::lit_c<'w'> | dsl::lit_c<'W'>))
                                 + dsl::lit_c<':'>
                                 + dsl::capture(dsl::token(dsl::while_(dsl::peek_not(dsl::newline) >> dsl::any)))
                                 + dsl::p<line_end>;

    static constexpr auto value = lexy::callback<ast::Lyric>(
        [](auto pos, auto prefix, auto text) {
            const bool block = prefix.begin()[0] == 'W';
            return ast::Lyric{
                block,
                std::string(text.begin(), text.end()),
                make_location(pos)
            };
        }
    );
};

struct inline_field_token {
    static constexpr auto rule = dsl::capture(dsl::ascii::upper) + dsl::lit_c<':'> + dsl::capture(dsl::token(dsl::while_(dsl::peek_not(dsl::ascii::blank | dsl::newline | dsl::lit_c<'%'>) >> dsl::any)));

    static constexpr auto value = lexy::callback<ast::InlineField>(
        [](auto id, auto value) {
            ast::Field field;
            field.id    = std::string(id.begin(), id.end());
            field.value = std::string(value.begin(), value.end());
            field.loc   = make_location(id.begin());
            return ast::InlineField{std::move(field)};
        }
    );
};

struct bar_token {
    static constexpr auto rule = dsl::capture(dsl::token(dsl::while_one(dsl::lit_c<'|'> | dsl::lit_c<':' > | dsl::lit_c<'['> | dsl::lit_c<']'>)));

    static constexpr auto value = lexy::callback<ast::Bar>(
        [](auto lex) {
            return ast::Bar{std::string(lex.begin(), lex.end()), make_location(lex.begin())};
        }
    );
};

struct tuplet_token {
    static constexpr auto rule = dsl::lit_c<'('>
                                 + dsl::position
                                 + dsl::capture(dsl::token(dsl::digits<>))
                                 + dsl::opt(dsl::lit_c<':'> >> dsl::capture(dsl::token(dsl::digits<>))
                                            >> dsl::opt(dsl::lit_c<':'> >> dsl::capture(dsl::token(dsl::digits<>))));

    static constexpr auto value = lexy::callback<ast::Tuplet>(
        [](auto pos, auto p, lexy::nullopt) {
            ast::Tuplet tuplet;
            tuplet.loc = make_location(pos);
            tuplet.p   = lexeme_to_int(p);
            return tuplet;
        },
        [](auto pos, auto p, auto q, lexy::nullopt) {
            ast::Tuplet tuplet;
            tuplet.loc = make_location(pos);
            tuplet.p   = lexeme_to_int(p);
            tuplet.q   = lexeme_to_int(q);
            return tuplet;
        },
        [](auto pos, auto p, auto q, auto r) {
            ast::Tuplet tuplet;
            tuplet.loc = make_location(pos);
            tuplet.p   = lexeme_to_int(p);
            tuplet.q   = lexeme_to_int(q);
            tuplet.r   = lexeme_to_int(r);
            return tuplet;
        }
    );
};

struct length_suffix : lexy::token_production {
    static constexpr auto rule  = dsl::capture(dsl::token(dsl::while_(dsl::digit<> | dsl::lit_c<'/'>)));
    static constexpr auto value = lexy::as_string<std::string>;
};

struct accidental_token : lexy::token_production {
    static constexpr auto rule  = dsl::capture(dsl::token(dsl::while_(dsl::lit_c<'^'> | dsl::lit_c<'_'> | dsl::lit_c<'='>)));
    static constexpr auto value = lexy::as_string<std::string>;
};

struct octave_modifier : lexy::token_production {
    static constexpr auto rule  = dsl::capture(dsl::token(dsl::while_(dsl::lit_c<'\''> | dsl::lit_c<','>)));
    static constexpr auto value = lexy::as_string<std::string>;
};

struct decoration_token : lexy::token_production {
    static constexpr auto rule = dsl::lit_c<'!'>
                                 >> dsl::capture(dsl::token(dsl::while_(dsl::peek_not(dsl::lit_c<'!'>) >> dsl::any)))
                                 >> dsl::lit_c<'!'>;
    static constexpr auto value = lexy::callback<std::string>([](auto lex) {
        return std::string(lex.begin(), lex.end());
    });
};

struct simple_decoration : lexy::token_production {
    static constexpr auto rule
        = dsl::capture(dsl::token(dsl::lit_c<'~'> | dsl::lit_c<'`'> | dsl::lit_c<'\''> | dsl::lit_c<'^'> | dsl::lit_c<'v'> | dsl::lit_c<'u'> | dsl::lit_c<'.'> | dsl::lit_c<'H'> | dsl::lit_c<'L'> | dsl::lit_c<'M'> | dsl::lit_c<'O'> | dsl::lit_c<'P'> | dsl::lit_c<'S'> | dsl::lit_c<'T'>));
    static constexpr auto value = lexy::as_string<std::string>;
};

struct note_start : lexy::token_production {
    static constexpr auto rule = dsl::lit_c<'!'>
                                 | dsl::lit_c<'~'> | dsl::lit_c<'`'> | dsl::lit_c<'\''> | dsl::lit_c<'^'> | dsl::lit_c<'v'> | dsl::lit_c<'u'> | dsl::lit_c<'.'> | dsl::lit_c<'H'> | dsl::lit_c<'L'> | dsl::lit_c<'M'> | dsl::lit_c<'O'> | dsl::lit_c<'P'> | dsl::lit_c<'S'> | dsl::lit_c<'T'>
                                 | dsl::lit_c<'\"'>
                                 | dsl::lit_c<'^'> | dsl::lit_c<'_'> | dsl::lit_c<'='>
                                 | dsl::ascii::alpha;
};

struct chord_symbol_token : lexy::token_production {
    static constexpr auto rule  = dsl::lit_c<'\"'> >> dsl::capture(dsl::token(dsl::while_(dsl::peek_not(dsl::lit_c<'\"'>) >> dsl::any))) >> dsl::lit_c<'\"'>;
    static constexpr auto value = lexy::as_string<std::string>;
};

struct decoration_list {
    static constexpr auto rule = dsl::list(dsl::p<decoration_token> | dsl::p<simple_decoration>);
    static constexpr auto value = ListBuilder<std::string>{};
};

struct chord_symbol_list {
    static constexpr auto rule = dsl::list(dsl::p<chord_symbol_token>);
    static constexpr auto value = ListBuilder<std::string>{};
};

struct note_token {
    static constexpr auto rule = [] {
        auto decorations = dsl::opt(dsl::p<decoration_list>);
        auto chord_sym   = dsl::opt(dsl::p<chord_symbol_list>);
        auto accidentals = dsl::p<accidental_token>;
        auto letter      = dsl::capture(dsl::token(dsl::lit_c<'A'> | dsl::lit_c<'B'> | dsl::lit_c<'C'> | dsl::lit_c<'D'> | dsl::lit_c<'E'> | dsl::lit_c<'F'> | dsl::lit_c<'G'>
                                        | dsl::lit_c<'a'> | dsl::lit_c<'b'> | dsl::lit_c<'c'> | dsl::lit_c<'d'> | dsl::lit_c<'e'> | dsl::lit_c<'f'> | dsl::lit_c<'g'>));
        auto octave      = dsl::p<octave_modifier>;
        auto length      = dsl::opt(dsl::p<length_suffix>);
        auto tie         = dsl::opt(dsl::lit_c<'-'>);
        return dsl::position
               + decorations
               + chord_sym
               + accidentals
               + letter
               + octave
               + length
               + tie;
    }();
    struct builder {
        using return_type = ast::Note;

        template <typename Pos,
                  typename Decorations,
                  typename ChordSymbols,
                  typename Letter,
                  typename Length,
                  typename Tie>
        ast::Note operator()(Pos pos,
                             Decorations decorations,
                             ChordSymbols chord_syms,
                             std::string accidentals,
                             Letter letter,
                             std::string octave,
                             Length length,
                             Tie tie) const {
            ast::Note note;
            note.loc          = make_location(pos);
            note.accidentals  = std::move(accidentals);
            note.letter       = letter.empty() ? 'C' : letter.begin()[0];
            note.octave_mod   = std::move(octave);
            if constexpr (!std::is_same_v<std::decay_t<Decorations>, lexy::nullopt>) {
                note.decorations = std::move(decorations);
            }
            if constexpr (!std::is_same_v<std::decay_t<ChordSymbols>, lexy::nullopt>) {
                note.chord_symbols = std::move(chord_syms);
            }
            if constexpr (!std::is_same_v<std::decay_t<Length>, lexy::nullopt>) {
                note.length_text = std::move(length);
            }
            note.tie = has_value_like(tie);
            note.explicit_natural = note.accidentals.find('=') != std::string::npos;
            return note;
        }

        template <typename Pos,
                  typename Decorations,
                  typename ChordSymbols,
                  typename Letter,
                  typename Length>
        ast::Note operator()(Pos pos,
                             Decorations decorations,
                             ChordSymbols chord_syms,
                             std::string accidentals,
                             Letter letter,
                             std::string octave,
                             Length length) const {
            return (*this)(
                pos,
                std::move(decorations),
                std::move(chord_syms),
                std::move(accidentals),
                std::move(letter),
                std::move(octave),
                std::move(length),
                lexy::nullopt{}
            );
        }
    };

    static constexpr auto value = lexy::bind(builder{}, lexy::values);
};

struct note_list {
    static constexpr auto rule = dsl::list(dsl::peek(dsl::p<note_start>) >> dsl::p<note_token>);
    static constexpr auto value = ListBuilder<ast::Note>{};
};

struct rest_token {
    static constexpr auto rule = dsl::capture(dsl::token(dsl::lit_c<'z'> | dsl::lit_c<'x'> | dsl::lit_c<'Z'>))
                                 + dsl::position
                                 + dsl::opt(dsl::p<length_suffix>);

    static constexpr auto value = lexy::callback<ast::Rest>(
        [](auto sym, auto pos, std::optional<std::string> length) {
            ast::Rest rest;
            rest.loc       = make_location(pos);
            rest.symbol    = sym.begin()[0];
            rest.invisible = rest.symbol == 'x';
            if (length.has_value()) { rest.length_text = std::move(*length); }
            return rest;
        }
    );
};

struct grace_group_token {
    static constexpr auto rule = dsl::lit_c<'{'> + dsl::position + dsl::p<note_list> + dsl::lit_c<'}'>;

    static constexpr auto value = lexy::callback<ast::GraceGroup>(
        [](auto pos, std::vector<ast::Note> notes) {
            for (auto& note : notes) { note.grace = true; }
            ast::GraceGroup group;
            group.loc   = make_location(pos);
            group.notes = std::move(notes);
            return group;
        }
    );
};

struct chord_token {
    static constexpr auto rule = dsl::lit_c<'['>
                                 + dsl::position
                                 + dsl::opt(dsl::peek(dsl::p<note_start>) >> dsl::p<note_list>)
                                 + dsl::lit_c<']'>
                                 + dsl::opt(dsl::p<length_suffix>);

    static constexpr auto value = lexy::callback<ast::Chord>(
        [](auto pos, std::optional<std::vector<ast::Note>> notes, std::optional<std::string> length) {
            ast::Chord chord;
            chord.loc = make_location(pos);
            if (notes.has_value()) { chord.notes = std::move(*notes); }
            if (length.has_value()) { chord.length_text = std::move(*length); }
            return chord;
        }
    );
};

struct broken_rhythm_token {
    static constexpr auto rule = dsl::capture(dsl::token(dsl::lit_c<'>'> | dsl::lit_c<'<'>)) + dsl::position;

    static constexpr auto value = lexy::callback<ast::BrokenRhythm>(
        [](auto lex, auto pos) {
            const bool greater = lex.begin()[0] == '>';
            return ast::BrokenRhythm{greater, make_location(pos)};
        }
    );
};

struct continuation_token {
    static constexpr auto rule = dsl::lit_c<'\\'> + dsl::position;

    static constexpr auto value = lexy::callback<ast::Continuation>(
        [](auto pos) { return ast::Continuation{make_location(pos)}; }
    );
};

struct music_token {
    static constexpr auto rule = [] {
        auto inline_field = dsl::peek(dsl::lit_c<'['> + dsl::p<identifier> + dsl::lit_c<':'>)
                            >> dsl::p<inline_field_token>;
        auto repeat_bar = dsl::peek(dsl::lit_c<'['> + dsl::lit_c<'|'>) >> dsl::p<bar_token>;
        auto chord      = dsl::peek(dsl::lit_c<'['>) >> dsl::p<chord_token>;
        auto tuplet     = dsl::peek(dsl::lit_c<'('> + dsl::digit<>) >> dsl::p<tuplet_token>;
        auto grace      = dsl::peek(dsl::lit_c<'{'>) >> dsl::p<grace_group_token>;
        auto bar        = dsl::peek(dsl::lit_c<'|'> | dsl::lit_c<':'>) >> dsl::p<bar_token>;
        auto rest = dsl::peek(dsl::lit_c<'z'> | dsl::lit_c<'x'> | dsl::lit_c<'Z'>) >> dsl::p<rest_token>;
        auto broken
            = dsl::peek(dsl::lit_c<'>'> | dsl::lit_c<'<'>) >> dsl::p<broken_rhythm_token>;
        auto continuation = dsl::peek(dsl::lit_c<'\\'>) >> dsl::p<continuation_token>;
        auto note         = dsl::peek(dsl::p<note_start>) >> dsl::p<note_token>;
        return dsl::while_(dsl::ascii::blank)
               + (inline_field | repeat_bar | chord | tuplet | grace | bar | rest | broken
                  | continuation | dsl::else_ >> note);
    }();

    static constexpr auto value = lexy::callback<ast::MusicItem>(
        [](ast::InlineField field) { return ast::MusicItem{std::move(field)}; },
        [](ast::Bar bar) { return ast::MusicItem{std::move(bar)}; },
        [](ast::Chord chord) { return ast::MusicItem{std::move(chord)}; },
        [](ast::Tuplet tuplet) { return ast::MusicItem{std::move(tuplet)}; },
        [](ast::GraceGroup group) { return ast::MusicItem{std::move(group)}; },
        [](ast::Rest rest) { return ast::MusicItem{std::move(rest)}; },
        [](ast::BrokenRhythm br) { return ast::MusicItem{std::move(br)}; },
        [](ast::Continuation cont) { return ast::MusicItem{std::move(cont)}; },
        [](ast::Note note) { return ast::MusicItem{std::move(note)}; }
    );
};

struct music_line_value {
    using return_type = ast::MusicLine;

    struct sink_t {
        std::vector<ast::MusicItem> items;
        using return_type = std::vector<ast::MusicItem>;

        void operator()(ast::MusicItem item) { items.push_back(std::move(item)); }
        std::vector<ast::MusicItem> finish() && { return std::move(items); }
    };

    static constexpr auto sink() { return sink_t{}; }

    template <typename Iterator>
    ast::MusicLine operator()(Iterator pos, lexy::nullopt) const
    {
        const auto loc = lexy::get_input_location(*g_current_input, pos);
        ast::MusicLine line;
        line.loc = SourceLocation{loc.line_nr(), loc.column_nr()};
        return line;
    }

    template <typename Iterator>
    ast::MusicLine operator()(Iterator pos, std::vector<ast::MusicItem> tokens) const
    {
        const auto loc = lexy::get_input_location(*g_current_input, pos);
        ast::MusicLine line;
        line.loc   = SourceLocation{loc.line_nr(), loc.column_nr()};
        line.items = std::move(tokens);
        return line;
    }
};

struct music_line {
    static constexpr auto rule = dsl::position
                                 + dsl::while_(dsl::ascii::space)
                                 + dsl::opt(dsl::list(dsl::peek_not(dsl::newline) >> dsl::p<music_token>))
                                 + dsl::p<line_end>;

    static constexpr auto value = music_line_value{};
};

struct blank_line {
    static constexpr auto rule  = dsl::while_(dsl::ascii::space) + dsl::p<line_end>;
    static constexpr auto value = lexy::callback<std::optional<ast::Statement>>([] { return std::nullopt; });
};

struct statement {
    static constexpr auto rule = [] {
        auto directive_branch = dsl::peek(dsl::lit_c<'%'> + dsl::lit_c<'%'>) >> dsl::p<directive_line>;
        auto lyric_branch     = dsl::peek(dsl::lit_c<'w'> | dsl::lit_c<'W'>) >> dsl::p<lyric_line>;
        auto field_branch     = dsl::peek(dsl::p<identifier> + dsl::lit_c<':'>) >> dsl::p<field_line>;
        return directive_branch | lyric_branch | field_branch | dsl::else_ >> dsl::p<music_line>;
    }();

    static constexpr auto value = lexy::callback<std::optional<ast::Statement>>(
        [](ast::Directive directive) { return std::optional<ast::Statement>{ast::Statement{std::move(directive)}}; },
        [](ast::Lyric lyric) { return std::optional<ast::Statement>{ast::Statement{std::move(lyric)}}; },
        [](ast::Field field) { return std::optional<ast::Statement>{ast::Statement{std::move(field)}}; },
        [](ast::MusicLine line) {
            if (line.items.empty()) return std::optional<ast::Statement>{std::nullopt};
            return std::optional<ast::Statement>{ast::Statement{std::move(line)}};
        }
    );
};

struct document {
    static constexpr auto whitespace = dsl::ascii::blank;

    static constexpr auto rule = dsl::terminator(dsl::eof).list(dsl::p<statement>);

    struct value_cb {
        using return_type = ast::Document;

        static constexpr auto sink()
        {
            struct sink_t {
                ast::Document doc;
                using return_type = ast::Document;

                void operator()(std::optional<ast::Statement> stmt)
                {
                    if (stmt.has_value()) {
                        doc.statements.push_back(std::move(*stmt));
                    }
                }

                ast::Document finish() && { return std::move(doc); }
            };
            return sink_t{};
        }
    };

    static constexpr auto value = value_cb{};
};

}  // namespace grammar

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

std::string fraction_to_text(const Fraction frac) {
    auto norm = normalize(frac);
    if (norm.den == 1) { return std::to_string(norm.num); }
    return std::to_string(norm.num) + "/" + std::to_string(norm.den);
}

std::string render_meter(const Meter& meter) {
    if (meter.numerator == 4 && meter.denominator == 4) { return "C"; }
    if (meter.numerator == 2 && meter.denominator == 2) { return "C|"; }
    if (meter.denominator == 0) { return "C"; }
    return std::to_string(meter.numerator) + "/" + std::to_string(meter.denominator);
}

std::string render_tempo(const TempoMark& tempo) {
    std::ostringstream oss;
    oss << fraction_to_text(tempo.beat) << "=" << static_cast<int>(std::lround(tempo.qpm));
    return oss.str();
}

std::string render_key(const KeySignature& key) {
    static const std::unordered_map<int, std::string> major = {
        {0, "C"},  {1, "G"},  {2, "D"},  {3, "A"},  {4, "E"},  {5, "B"},  {6, "F#"}, {7, "C#"},
        {-1, "F"}, {-2, "Bb"}, {-3, "Eb"}, {-4, "Ab"}, {-5, "Db"}, {-6, "Gb"}, {-7, "Cb"}
    };
    static const std::unordered_map<int, std::string> minor = {
        {0, "A"},  {1, "E"},  {2, "B"},  {3, "F#"}, {4, "C#"}, {5, "G#"}, {6, "D#"}, {7, "A#"},
        {-1, "D"}, {-2, "G"}, {-3, "C"}, {-4, "F"}, {-5, "Bb"}, {-6, "Eb"}, {-7, "Ab"}
    };
    const auto& map = key.is_minor ? minor : major;
    auto        it  = map.find(key.accidentals);
    if (it != map.end()) { return it->second + (key.is_minor ? "m" : ""); }
    return key.is_minor ? "Am" : "C";
}

std::string render_length_suffix(const Fraction length, const Fraction unit) {
    auto ratio = div(length, unit);
    if (ratio.num == 0) { return "0"; }
    if (ratio.den == 1) {
        if (ratio.num == 1) { return {}; }
        return std::to_string(ratio.num);
    }
    if (ratio.num == 1) { return "/" + std::to_string(ratio.den); }
    return std::to_string(ratio.num) + "/" + std::to_string(ratio.den);
}

int floor_div(const int value, const int divisor) {
    if (divisor == 0) { return 0; }
    int quot = value / divisor;
    int rem  = value % divisor;
    if ((rem != 0) && ((rem < 0) != (divisor < 0))) { --quot; }
    return quot;
}

struct NoteRenderParts {
    std::string accidental;
    char        letter = 'C';
    bool        lowercase = false;
    std::string octave_mod;
};

NoteRenderParts compute_note_render_parts(
    int midi_pitch, const std::array<int, 7>& key_table
) {
    const auto pitch_class = static_cast<int>((midi_pitch % 12 + 12) % 12);
    int        best_idx    = 0;
    int        best_delta  = 999;

    for (int idx = 0; idx < static_cast<int>(LETTERS.size()); ++idx) {
        int delta = pitch_class - NATURAL_PITCH_CLASS[idx];
        while (delta > 6) { delta -= 12; }
        while (delta < -6) { delta += 12; }
        if (std::abs(delta) < std::abs(best_delta)
            || (std::abs(delta) == std::abs(best_delta) && delta > best_delta)) {
            best_delta = delta;
            best_idx   = idx;
        }
    }

    int accidental_delta = best_delta == 999 ? 0 : best_delta;
    int natural_pitch    = 48 + NATURAL_PITCH_CLASS[best_idx];
    int midi_no_acc      = midi_pitch - accidental_delta;
    int octave_steps     = floor_div(midi_no_acc - natural_pitch, 12);

    NoteRenderParts parts;
    if (accidental_delta > 0) {
        parts.accidental.assign(accidental_delta, '^');
    } else if (accidental_delta < 0) {
        parts.accidental.assign(-accidental_delta, '_');
    } else if (key_table[best_idx] != 0) {
        parts.accidental = "=";
    }

    if (octave_steps >= 1) {
        parts.lowercase = true;
        --octave_steps;
        while (octave_steps-- > 0) { parts.octave_mod.push_back('\''); }
    } else if (octave_steps < 0) {
        parts.lowercase = false;
        while (octave_steps++ < 0) { parts.octave_mod.push_back(','); }
    }
    parts.letter = parts.lowercase ? static_cast<char>(std::tolower(LETTERS[best_idx]))
                                   : LETTERS[best_idx];
    return parts;
}

std::string render_note_event(
    const NoteEvent& note, const Fraction unit, const std::array<int, 7>& key_table
) {
    const auto parts = compute_note_render_parts(note.midi_pitch, key_table);
    std::string token;
    auto render_decoration = [](const std::string& value) -> std::string {
        static constexpr std::string_view simple = "~`'^vu.HLMOPST";
        if (value.size() == 1 && simple.find(value[0]) != std::string::npos) {
            return value;
        }
        std::string wrapped = "!";
        wrapped += value;
        wrapped.push_back('!');
        return wrapped;
    };

    for (const auto& deco : note.decorations) {
        token += render_decoration(deco);
    }
    for (const auto& chord : note.chord_symbols) {
        token.push_back('"');
        token += chord;
        token.push_back('"');
    }
    token += parts.accidental;
    token.push_back(parts.letter);
    token += parts.octave_mod;
    token += render_length_suffix(note.length, unit);
    if (note.tie_continue) { token.push_back('-'); }
    return token;
}

std::string render_chord_event(
    const ChordEvent& chord, const Fraction unit, const std::array<int, 7>& key_table
) {
    std::string token;
    for (const auto& symbol : chord.chord_symbols) {
        token.push_back('"');
        token += symbol;
        token.push_back('"');
    }
    token.push_back('[');
    for (const auto& note : chord.notes) { token += render_note_event(note, unit, key_table); }
    token.push_back(']');
    return token;
}

std::string render_bar(const BarEvent& bar) {
    if (bar.final_bar && bar.repeat_end) { return ":|]"; }
    if (bar.final_bar) { return "|]"; }
    if (bar.repeat_start && bar.repeat_end) { return ":|:"; }
    if (bar.repeat_start) { return "|:"; }
    if (bar.repeat_end) { return ":|"; }
    if (bar.double_bar) { return "||"; }
    return "|";
}

std::string render_voice_line(
    const Voice& voice, const Fraction unit, const std::array<int, 7>& key_table
) {
    std::vector<std::string> tokens;
    tokens.reserve(voice.elements.size());
    bool in_grace = false;
    std::string grace_block;
    auto flush_grace = [&]() {
        if (!in_grace) { return; }
        grace_block.push_back('}');
        tokens.push_back(grace_block);
        grace_block.clear();
        in_grace = false;
    };

    for (const auto& element : voice.elements) {
        if (const auto* note = std::get_if<NoteEvent>(&element)) {
            auto rendered = render_note_event(*note, unit, key_table);
            if (note->is_grace) {
                if (!in_grace) {
                    in_grace = true;
                    grace_block = "{";
                } else {
                    grace_block.push_back(' ');
                }
                grace_block += rendered;
            } else {
                flush_grace();
                tokens.push_back(std::move(rendered));
            }
        } else if (const auto* rest = std::get_if<RestEvent>(&element)) {
            flush_grace();
            auto suffix = render_length_suffix(rest->length, unit);
            std::string token(1, rest->symbol);
            token += suffix;
            tokens.push_back(std::move(token));
        } else if (const auto* bar = std::get_if<BarEvent>(&element)) {
            flush_grace();
            tokens.push_back(render_bar(*bar));
        } else if (const auto* chord = std::get_if<ChordEvent>(&element)) {
            flush_grace();
            tokens.push_back(render_chord_event(*chord, unit, key_table));
        }
    }
    flush_grace();

    std::ostringstream oss;
    for (std::size_t i = 0; i < tokens.size(); ++i) {
        if (i) { oss << ' '; }
        oss << tokens[i];
    }
    return oss.str();
}

std::string render_document(const Document& document) {
    std::ostringstream oss;
    oss << "X: " << document.header.reference_number << '\n';
    if (!document.header.title.empty()) { oss << "T: " << document.header.title << '\n'; }
    oss << "M: " << render_meter(document.header.meter) << '\n';
    oss << "L: " << fraction_to_text(document.header.unit_note_length) << '\n';
    oss << "Q: " << render_tempo(document.header.tempo) << '\n';
    oss << "K: " << render_key(document.header.key) << '\n';
    for (const auto& directive : document.directives) {
        if (!directive.empty()) { oss << directive << '\n'; }
    }

    const bool multi_voice = document.voices.size() > 1;
    auto        key_table  = build_key_signature(document.header.key);

    for (std::size_t idx = 0; idx < document.voices.size(); ++idx) {
        const auto& voice = document.voices[idx];
        if (multi_voice) { oss << "V: V" << (idx + 1) << '\n'; }
        oss << render_voice_line(voice, document.header.unit_note_length, key_table) << '\n';
        for (const auto& line : voice.lyrics) {
            if (line.empty()) { continue; }
            oss << "w: " << line << '\n';
        }
        if (idx + 1 < document.voices.size()) { oss << '\n'; }
    }
    return oss.str();
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

Fraction default_note_length(const std::vector<ast::Field>& fields, const Meter meter) {
    for (const auto& field : fields) {
        if (field.id == "L") {
            return parse_fraction(field.value);
        }
    }
    return infer_unit_length(meter);
}

std::optional<int> parse_reference_number(const std::vector<ast::Field>& fields) {
    for (const auto& field : fields) {
        if (field.id == "X") {
            return parse_integer(field.value);
        }
    }
    return std::nullopt;
}

std::string first_title(const std::vector<ast::Field>& fields) {
    for (const auto& field : fields) {
        if (field.id == "T") { return std::string(trim(field.value)); }
    }
    return {};
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




class SemanticParser {
public:
    SemanticParser(ast::Document ast, const ParseOptions& options, DiagnosticSink& sink)
        : ast_(std::move(ast)), options_(options), sink_(sink) {
        header_.ticks_per_quarter = options_.ticks_per_quarter;
        // Initialize default voice
        get_voice_context("default");
    }

    Document parse() {
        // 1. Extract Header
        process_header();

        // 2. Process Body
        process_body();

        // 3. Finalize
        Document doc;
        doc.header = header_;
        doc.directives = std::move(directives_);
        
        for (const auto& id : voice_order_) {
            auto& ctx = voices_[id];
            Voice voice;
            voice.name = ctx.id == "default" ? "" : ctx.id;
            voice.elements = flatten_events(ctx.elements);
            voice.lyrics = std::move(ctx.lyrics);
            doc.voices.push_back(std::move(voice));
        }
        
        return doc;
    }

private:
    ast::Document ast_;
    const ParseOptions& options_;
    DiagnosticSink& sink_;
    Header header_;
    std::vector<std::string> directives_;
    bool header_unit_length_explicit_ = false;
    
    struct VoiceContext {
        std::string id;
        std::vector<Element> elements;
        std::vector<std::string> lyrics;
        
        TupletState tuplet;
        std::array<std::optional<int>, 7> measure_accidentals;
        Fraction pending_length_multiplier{1, 1};
        std::array<int, 7> key_table;
        Fraction unit_note_length{1, 8};
        bool     explicit_unit_length = false;
        Meter    meter{};
        KeySignature key_signature{};
        std::vector<ast::Note> pending_grace_notes;
        
        VoiceContext() {
            measure_accidentals.fill(std::nullopt);
            key_table.fill(0);
        }
    };
    
    std::unordered_map<std::string, VoiceContext> voices_;
    std::vector<std::string> voice_order_;
    std::string current_voice_id_ = "default";
    
    VoiceContext& get_voice_context(const std::string& id) {
        auto it = voices_.find(id);
        if (it == voices_.end()) {
            voices_[id].id = id;
            voice_order_.push_back(id);
            voices_[id].key_table = build_key_signature(header_.key);
            voices_[id].key_signature = header_.key;
            voices_[id].unit_note_length = header_.unit_note_length;
            voices_[id].explicit_unit_length = header_unit_length_explicit_;
            voices_[id].meter = header_.meter;
            return voices_[id];
        }
        return it->second;
    }
    
    VoiceContext& current_voice() {
        return get_voice_context(current_voice_id_);
    }

    void process_header() {
        for (const auto& stmt : ast_.statements) {
            if (std::holds_alternative<ast::Field>(stmt.payload)) {
                const auto& field = std::get<ast::Field>(stmt.payload);
                if (field.id == "K") {
                    process_header_field(field);
                    break; 
                }
                process_header_field(field);
            } else if (std::holds_alternative<ast::MusicLine>(stmt.payload)) {
                break;
            }
        }
        
        if (header_.meter.numerator == 0) { header_.meter = Meter{4, 4}; }
        if (header_.unit_note_length.num == 0) { header_.unit_note_length = infer_unit_length(header_.meter); }
        
        auto key_table = build_key_signature(header_.key);
        for (auto& [id, ctx] : voices_) {
            ctx.key_table = key_table;
            ctx.key_signature = header_.key;
            ctx.unit_note_length = header_.unit_note_length;
            ctx.explicit_unit_length = header_unit_length_explicit_;
            ctx.meter = header_.meter;
        }
    }
    
    void process_header_field(const ast::Field& field) {
        if (field.id == "X") {
            if (auto val = parse_integer(field.value)) header_.reference_number = *val;
        } else if (field.id == "T") {
            if (header_.title.empty()) header_.title = field.value;
        } else if (field.id == "M") {
            header_.meter = parse_meter(field.value);
            if (!header_unit_length_explicit_) {
                header_.unit_note_length = infer_unit_length(header_.meter);
            }
        } else if (field.id == "L") {
            header_.unit_note_length = parse_fraction(field.value);
            header_unit_length_explicit_ = true;
        } else if (field.id == "Q") {
            header_.tempo = parse_tempo(field.value);
        } else if (field.id == "K") {
            header_.key = parse_key(field.value);
        } else if (field.id == "V") {
            auto id = parse_voice_id(field.value);
            get_voice_context(id);
        } else if (field.id == "I") {
            directives_.push_back("I:" + field.value);
        } else {
            directives_.push_back(field.id + ":" + field.value);
        }
    }
    
    std::string parse_voice_id(const std::string& value) {
        std::stringstream ss(value);
        std::string id;
        ss >> id;
        return id;
    }

    void process_body() {
        bool header_passed = false;
        for (const auto& stmt : ast_.statements) {
            if (!header_passed) {
                if (std::holds_alternative<ast::Field>(stmt.payload)) {
                    const auto& field = std::get<ast::Field>(stmt.payload);
                    if (field.id == "K") {
                        header_passed = true;
                    }
                } else if (std::holds_alternative<ast::MusicLine>(stmt.payload)) {
                    header_passed = true;
                    process_music_line(std::get<ast::MusicLine>(stmt.payload));
                }
                continue;
            }
            
            std::visit([this](auto&& arg) {
                using T = std::decay_t<decltype(arg)>;
                if constexpr (std::is_same_v<T, ast::Field>) {
                    process_body_field(arg);
                } else if constexpr (std::is_same_v<T, ast::Directive>) {
                    directives_.push_back(arg.text);
                } else if constexpr (std::is_same_v<T, ast::Lyric>) {
                    process_lyric(arg);
                } else if constexpr (std::is_same_v<T, ast::MusicLine>) {
                    process_music_line(arg);
                }
            }, stmt.payload);
        }
    }
    
    void process_body_field(const ast::Field& field) {
        if (field.id == "V") {
            current_voice_id_ = parse_voice_id(field.value);
            get_voice_context(current_voice_id_);
        } else if (field.id == "K") {
            auto key = parse_key(field.value);
            auto& ctx = current_voice();
            ctx.key_table = build_key_signature(key);
            ctx.key_signature = key;
            ctx.measure_accidentals.fill(std::nullopt);
        } else if (field.id == "M") {
            auto meter = parse_meter(field.value);
            auto& ctx = current_voice();
            ctx.meter = meter;
            if (!ctx.explicit_unit_length) {
                ctx.unit_note_length = infer_unit_length(meter);
            }
        } else if (field.id == "L") {
            auto length = parse_fraction(field.value);
            auto& ctx = current_voice();
            ctx.unit_note_length = length;
            ctx.explicit_unit_length = true;
        } else if (field.id == "I") {
            directives_.push_back("I:" + field.value);
        } else {
            directives_.push_back(field.id + ":" + field.value);
        }
    }
    
    void process_lyric(const ast::Lyric& lyric) {
        current_voice().lyrics.push_back(lyric.text);
    }
    
    void process_music_line(const ast::MusicLine& line) {
        for (const auto& item : line.items) {
            std::visit([this](auto&& arg) { process_music_item(arg); }, item);
        }
    }

    std::optional<NoteEvent> build_note_event(const ast::Note& note,
                                              VoiceContext& ctx,
                                              std::optional<Fraction> explicit_length = std::nullopt,
                                              bool mark_grace = false) {
        int midi_pitch = 60;
        try {
            midi_pitch = compute_pitch(note, ctx);
        } catch (const std::exception& e) {
            sink_.add(note.loc.line, note.loc.column, e.what());
            return std::nullopt;
        }

        Fraction length = explicit_length.value_or(resolve_duration(parse_note_length(note.length_text), ctx));

        NoteEvent event;
        event.midi_pitch = midi_pitch;
        event.length = length;
        event.velocity = options_.default_velocity;
        event.tie_continue = note.tie;
        event.is_grace = mark_grace || note.grace;
        event.decorations = note.decorations;
        event.chord_symbols = note.chord_symbols;
        return event;
    }

    void append_note_event(const ast::Note& note,
                           VoiceContext& ctx,
                           std::optional<Fraction> explicit_length = std::nullopt,
                           bool mark_grace = false) {
        auto event = build_note_event(note, ctx, explicit_length, mark_grace);
        if (!event.has_value()) { return; }
        ctx.elements.emplace_back(*event);
        apply_pending_multiplier(ctx);
    }

    void append_rest_event(const ast::Rest& rest, VoiceContext& ctx) {
        Fraction length = resolve_duration(parse_note_length(rest.length_text), ctx);
        RestEvent event;
        event.length = length;
        event.symbol = rest.symbol;
        event.invisible = rest.invisible;
        ctx.elements.emplace_back(event);
        apply_pending_multiplier(ctx);
    }

    void flush_grace_notes(VoiceContext& ctx) {
        if (ctx.pending_grace_notes.empty()) { return; }
        for (const auto& note : ctx.pending_grace_notes) {
            auto base = resolve_duration(parse_note_length(note.length_text), ctx);
            base = mul(base, Fraction{1, 4});
            append_note_event(note, ctx, base, true);
        }
        ctx.pending_grace_notes.clear();
    }
    
    void process_music_item(const ast::Note& note) {
        auto& ctx = current_voice();
        flush_grace_notes(ctx);
        append_note_event(note, ctx);
    }
    
    void process_music_item(const ast::Rest& rest) {
        auto& ctx = current_voice();
        flush_grace_notes(ctx);
        append_rest_event(rest, ctx);
    }
    
    void process_music_item(const ast::Bar& bar) {
        auto& ctx = current_voice();
        flush_grace_notes(ctx);
        BarEvent event;
        if (bar.token.find(":|") != std::string::npos) { event.repeat_end = true; }
        if (bar.token.find("|:") != std::string::npos) { event.repeat_start = true; }
        if (bar.token == "||" || bar.token == "|]" || bar.token == "[|" || bar.token == ":||" || bar.token == "||:" || bar.token == "|[|") {
            event.double_bar = true;
        }
        if (bar.token.find("|]") != std::string::npos || bar.token.find(":|]") != std::string::npos) { event.final_bar = true; }
        
        ctx.elements.emplace_back(event);
        ctx.measure_accidentals.fill(std::nullopt);
    }
    
    void process_music_item(const ast::Tuplet& tuplet) {
        auto& ctx = current_voice();
        int p = tuplet.p;
        int q = tuplet.q;
        int r = tuplet.r;
        
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
                    const bool compound = (header_.meter.denominator == 8 && header_.meter.numerator % 3 == 0);
                    q = compound ? 3 : 2;
                    break;
                }
                default: q = p - 1; break;
            }
        }
        if (r == 0) { r = p; }
        
        ctx.tuplet.ratio = normalize(Fraction{q, p});
        ctx.tuplet.remain = r;
    }
    
    void process_music_item(const ast::Chord& chord) {
        auto& ctx = current_voice();
        flush_grace_notes(ctx);
        std::vector<NoteEvent> notes;
        Fraction chord_length = parse_note_length(chord.length_text);
        for (const auto& note : chord.notes) {
            Fraction inner = parse_note_length(note.length_text);
            Fraction resolved = resolve_duration(mul(inner, chord_length), ctx);
            auto event = build_note_event(note, ctx, resolved);
            if (event.has_value()) {
                event->length = resolved;
                notes.push_back(*event);
            }
        }
        if (notes.empty()) { return; }

        ChordEvent event;
        event.notes = std::move(notes);
        ctx.elements.emplace_back(event);
        apply_pending_multiplier(ctx);
    }
    
    void process_music_item(const ast::GraceGroup& group) {
        auto& ctx = current_voice();
        ctx.pending_grace_notes.insert(ctx.pending_grace_notes.end(), group.notes.begin(), group.notes.end());
    }
    
    void process_music_item(const ast::BrokenRhythm& br) {
        auto& ctx = current_voice();
        Fraction prev_mult = br.greater ? Fraction{3, 2} : Fraction{1, 2};
        Fraction next_mult = br.greater ? Fraction{1, 2} : Fraction{3, 2};
        
        if (!ctx.elements.empty()) {
            auto& last = ctx.elements.back();
            if (auto* note = std::get_if<NoteEvent>(&last)) {
                note->length = mul(note->length, prev_mult);
            } else if (auto* chord = std::get_if<ChordEvent>(&last)) {
                for (auto& n : chord->notes) {
                    n.length = mul(n.length, prev_mult);
                }
            }
        }
        
        if (ctx.pending_length_multiplier.num != 1 || ctx.pending_length_multiplier.den != 1) {
            ctx.pending_length_multiplier = mul(ctx.pending_length_multiplier, next_mult);
        } else {
            ctx.pending_length_multiplier = next_mult;
        }
    }
    
    void process_music_item(const ast::Continuation& cont) {}
    
    void process_music_item(const ast::InlineField& field) {
        process_body_field(field.field);
    }

    int compute_pitch(const ast::Note& note, VoiceContext& ctx) {
        const char letter = note.letter;
        const char upper = static_cast<char>(std::toupper(static_cast<unsigned char>(letter)));
        const bool lower = std::islower(static_cast<unsigned char>(letter)) != 0;
        
        int accidental_delta = 0;
        bool explicit_natural = note.explicit_natural;
        
        for (char c : note.accidentals) {
            if (c == '^') accidental_delta++;
            else if (c == '_') accidental_delta--;
            else if (c == '=') { explicit_natural = true; accidental_delta = 0; }
        }
        
        int octave_delta = 0;
        for (char c : note.octave_mod) {
            if (c == '\'') octave_delta++;
            else if (c == ',') octave_delta--;
        }
        
        const int idx = letter_index(letter);
        
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
        
        int shift = ctx.key_table[idx];
        if (ctx.measure_accidentals[idx].has_value()) {
            shift = ctx.measure_accidentals[idx].value();
        }
        
        if (accidental_delta != 0) {
            shift = accidental_delta;
        } else if (explicit_natural) {
            shift = 0;
        }
        
        if (accidental_delta != 0 || explicit_natural) {
            ctx.measure_accidentals[idx] = shift;
        }
        
        const int final_pitch = base_octave_pitch + octave_delta * 12 + shift;
        return final_pitch;
    }
    
    Fraction resolve_duration(Fraction length, VoiceContext& ctx) {
        Fraction base = mul(ctx.unit_note_length, length);
        
        if (ctx.tuplet.remain > 0) {
            base = mul(base, ctx.tuplet.ratio);
            ctx.tuplet.remain--;
            if (ctx.tuplet.remain == 0) {
                ctx.tuplet = TupletState{};
            }
        }
        
        return base;
    }

    void apply_pending_multiplier(VoiceContext& ctx) {
        if (ctx.pending_length_multiplier.num == 1 && ctx.pending_length_multiplier.den == 1) {
            return;
        }
        if (!ctx.elements.empty()) {
            std::visit(
                [&](auto& elem) {
                    using ElemT = std::decay_t<decltype(elem)>;
                    if constexpr (std::is_same_v<ElemT, NoteEvent>) {
                        elem.length = mul(elem.length, ctx.pending_length_multiplier);
                    } else if constexpr (std::is_same_v<ElemT, RestEvent>) {
                        elem.length = mul(elem.length, ctx.pending_length_multiplier);
                    } else if constexpr (std::is_same_v<ElemT, ChordEvent>) {
                        for (auto& note : elem.notes) {
                            note.length = mul(note.length, ctx.pending_length_multiplier);
                        }
                    }
                },
                ctx.elements.back()
            );
        }
        ctx.pending_length_multiplier = Fraction{1, 1};
    }
};

Document parse_document(std::string_view input, const ParseOptions& options) {
    DiagnosticSink sink;
    lexy::string_input<lexy::utf8_encoding> lexy_input(input);
    grammar::ParsingState state{};
    state.input = &lexy_input;
    g_current_input = &lexy_input;
    auto result = lexy::parse<grammar::document>(lexy_input, state, lexy::noop);
    g_current_input = nullptr;
    
    if (!result) {
        // TODO: Better error reporting from lexy
        sink.add(0, 0, "Failed to parse ABC document");
        if (options.strict) {
            throw ParseError(sink.diagnostics);
        }
        return Document{};
    }
    
    SemanticParser parser(std::move(result.value()), options, sink);
    Document doc = parser.parse();
    
    if (!sink.diagnostics.empty() && options.strict) {
        throw ParseError(sink.diagnostics);
    }
    return doc;
}

} // namespace miniabc

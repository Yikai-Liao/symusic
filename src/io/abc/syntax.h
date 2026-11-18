#pragma once

#include <algorithm>
#include <charconv>
#include <deque>
#include <optional>
#include <set>
#include <string_view>
#include <tuple>
#include <variant>

#include <fmt/format.h>

#include "playback_rules.h"
#include "symusic/mtype.h"

namespace symusic::abc::detail {

struct Location {
    size_t line                               = 1;
    size_t column                             = 1;
    auto   operator<=>(const Location&) const = default;
};

class Diagnostics {
public:
    explicit Diagnostics(std::string_view source) : source(source) {}

    [[noreturn]] void error(Location at, std::string_view code, std::string_view message) const;
    void              warn(Location at, std::string_view code, std::string_view message);
    vec<std::string>  take();
    void              spend(size_t work, Location at);

private:
    std::string_view                                         source;
    size_t                                                   playback_budget = PlaybackWork::limit;
    std::set<std::tuple<Location, std::string, std::string>> warnings;
    std::string format(Location at, std::string_view code, std::string_view message) const;
};

inline constexpr std::string_view whitespace = " \t\r\f\v";

inline std::string_view trim(std::string_view text) {
    const auto first = text.find_first_not_of(whitespace);
    if (first == text.npos) return {};
    return text.substr(first, text.find_last_not_of(whitespace) - first + 1);
}

inline std::string_view word(std::string_view& text) {
    text             = trim(text);
    const auto first = text.substr(0, text.find_first_of(whitespace));
    text.remove_prefix(first.size());
    return first;
}

inline std::optional<int> integer(std::string_view text) {
    if (text.starts_with('+')) text.remove_prefix(1);
    if (text.empty()) return {};
    int value               = 0;
    const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), value);
    if (error != std::errc{} || end != text.data() + text.size()) return {};
    return value;
}

inline constexpr bool digit(char c) { return c >= '0' && c <= '9'; }
inline constexpr bool lower(char c) { return c >= 'a' && c <= 'z'; }
inline constexpr char upper(char c) { return lower(c) ? static_cast<char>(c - 'a' + 'A') : c; }
inline constexpr bool note_letter(char c) { return upper(c) >= 'A' && upper(c) <= 'G'; }
inline constexpr bool letter(char c) { return upper(c) >= 'A' && upper(c) <= 'Z'; }

struct Length {
    int                       numerator   = 1;
    int                       denominator = 1;
    [[nodiscard]] long double fraction() const {
        return static_cast<long double>(numerator) / denominator;
    }
};

// A denominator of zero denotes a conventional accidental. A positive
// denominator retains ABC's explicit fractional notation, even for ^1.
struct Alteration {
    int numerator   = 0;
    int denominator = 0;
};

// Parse an accidental prefix (without the note letter), including ABC's
// fractional shortcuts. An ambiguous prefix warns and leaves the pitch unchanged.
std::optional<Alteration> accidental(std::string_view text, Location at, Diagnostics& diagnostics);

struct Pitch {
    char                      letter;
    int                       octave;
    std::optional<Alteration> accidental;
    bool                      tie;
    Length                    length;
};

struct Atom {
    vec<Pitch> pitches;
    Length     length;
    bool       measure_rest = false;
    bool       tie          = false;
    size_t     music_line   = 0;   // identity shared across physical continuations
};

struct Grace {
    vec<Pitch> pitches;
};
struct Field {
    char             kind;
    std::string_view value;
};
struct Directive {
    std::string_view value;
};
struct Harmony {
    std::string_view symbol;
};
struct Decoration {
    std::string_view spelling;
    bool             last_on_line = false;
};
struct Tuplet {
    int count;
    int in_time_of;
    int notes;
};
struct Broken {
    int direction;
    int count;
};
struct Slur {
    bool start;
};
struct Tie {};
enum class Overlay { split, start, end };

struct Bar {
    bool             repeat_start   = false;
    bool             repeat_end     = false;
    bool             resets_measure = true;
    bool             section        = false;
    std::string_view endings;
};

struct Token {
    Location at;
    std::variant<
        Atom,
        Grace,
        Field,
        Directive,
        Harmony,
        Decoration,
        Tuplet,
        Broken,
        Slur,
        Tie,
        Overlay,
        Bar>
        value;
};

// Views in the token stream refer to the input; locations are one-based UTF-8 byte columns.
// Header ends at K:, and body contains only the first tune's music and fields.
struct Tune {
    std::deque<std::string> continued_lines;
    vec<Token>              header;
    vec<Token>              body;
    Location                key;
};

Tune tokenize(std::string_view text, Diagnostics& diagnostics);

}   // namespace symusic::abc::detail

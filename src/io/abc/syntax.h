#pragma once

#include <algorithm>
#include <charconv>
#include <compare>
#include <deque>
#include <functional>
#include <optional>
#include <set>
#include <span>
#include <string>
#include <string_view>
#include <tuple>
#include <variant>

#include <fmt/format.h>

#include "playback_rules.h"
#include "symusic/mtype.h"

namespace symusic::abc::detail {

// A physical source position with one-based line and UTF-8 byte column.
// An included file borrows its enclosing include sites from Diagnostics,
// so comparisons follow insertion order while diagnostics use physical lines.
struct Location {
    using Point = std::tuple<size_t, size_t, size_t>;

    size_t                 line   = 1;
    size_t                 column = 1;
    size_t                 source = 0;
    std::span<const Point> include_path{};

    Point                point() const { return {line, column, source}; }
    bool                 operator==(const Location& rhs) const { return (*this <=> rhs) == 0; }
    std::strong_ordering operator<=>(const Location& rhs) const;
};

// Collects one tune's diagnostics and charges its shared playback budget.
// Registered line views must remain valid through formatting. Warnings are
// deduplicated by location, code and message; error() throws immediately.
class Diagnostics {
public:
    explicit Diagnostics(std::string_view source) : sources{{std::string(source), {}, {}}} {}
    Diagnostics(const Diagnostics&)            = delete;
    Diagnostics& operator=(const Diagnostics&) = delete;

    Location          register_include(std::string_view source, Location at);
    void              record_line(size_t source, std::string_view raw);
    [[noreturn]] void error(Location at, std::string_view code, std::string_view message) const;
    void              warn(Location at, std::string_view code, std::string_view message);
    vec<std::string>  take();
    void              spend(size_t work, Location at);

private:
    struct Source {
        std::string           name;
        vec<std::string_view> lines;
        vec<Location::Point>  include_path;
    };
    std::deque<Source>                                       sources;
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
    if (text.starts_with('+')) {
        text.remove_prefix(1);
        if (text.starts_with('-')) return {};
    }
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
    // Exact ratio until the caller supplies its reference unit. For an Atom
    // that unit is L:, or the current measure for a measure rest. A chord can
    // multiply two lengths parsed from int-sized components.
    i64                       numerator   = 1;
    i64                       denominator = 1;
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
// fractional shortcuts. An ambiguous prefix warns and returns no alteration;
// an invalid fractional divisor or an out-of-range value is fatal.
std::optional<Alteration> accidental(std::string_view text, Location at, Diagnostics& diagnostics);

// Letter is uppercase A-G; octave zero is the octave of written uppercase C.
// Key signature, carried accidentals and MIDI transposition are not applied here.
struct Pitch {
    char                      letter;
    int                       octave;
    std::optional<Alteration> accidental;
    bool                      tie;
};

struct ChordDecoration {
    size_t           member;
    Location         at;
    std::string_view spelling;
};

struct ChordMemberLength {
    size_t member;
    Length length;
};

struct AttackRhythm {
    int  longer  = 3;
    int  shorter = 1;
    bool swing   = false;
};

// One written rhythmic slot. Empty pitches denote a rest, which still advances
// playback. A chord shares one slot, with member-local marks.
struct Atom {
    vec<Pitch>           pitches;
    Length               length;
    bool                 measure_rest = false;
    bool                 tie          = false;
    bool                 chord        = false;
    vec<ChordDecoration> decorations{};
    AttackRhythm         rhythm{};
    // Voice-local lyric entry, assigned during underlay alignment. Macro
    // expansions can have multiple atoms at the same source position.
    size_t lyric_slot = 0;   // One-based; zero means no written syllables.
    // Only lengths that differ from the first member's, in pitch order.
    vec<ChordMemberLength> member_lengths{};
};

struct GraceAttack {
    Atom        atom;
    long double rhythm_scale = 1;

    [[nodiscard]] long double weight() const { return atom.length.fraction() * rhythm_scale; }
};
struct Grace {
    vec<GraceAttack> attacks;
};
struct SourceFragment {
    size_t   offset;
    Location at;
    bool     generated = false;
};
struct Field {
    char             kind;
    std::string_view value;
    // Symbol lines keep the physical positions of their continued payload.
    vec<SourceFragment> fragments{};
};

struct TempoNotation {
    std::string_view   label;
    std::optional<int> bpm;
    long double        beat          = 1;
    bool               unit_relative = true;
};

struct MeterNumbers {
    vec<std::string_view> groups;
    std::string_view      denominator;
};

// The numeric M: forms share one denominator and may wrap the numerator in parentheses.
[[nodiscard]] std::optional<MeterNumbers> meter_numbers(std::string_view text);

// Decode the header P: itinerary into uppercase labels in playback order.
// Invalid syntax or expansion beyond max_parts throws a located error.
[[nodiscard]] std::string part_order(
    std::string_view text, size_t max_parts, Location at, Diagnostics& diagnostics
);

struct LyricPlacement {
    size_t      slot;
    std::string text;
};

// Decode one verse against its available note slots and bar boundaries.
[[nodiscard]] vec<LyricPlacement> align_lyrics(
    std::string_view        text,
    size_t                  notes,
    std::span<const size_t> bars,
    Location                at,
    Diagnostics&            diagnostics
);

struct Directive {
    std::string_view value;
};

// Voice assembly emits the pitch options from V: at the voice's written cursor.
struct PitchChange {
    std::optional<int> transpose;
    std::optional<int> written_octave;
    std::optional<int> clef_shift;

    [[nodiscard]] bool empty() const { return !transpose && !written_octave && !clef_shift; }
};

struct MidiTransposition {
    int  amount;
    bool relative;

    [[nodiscard]] int apply(int shift) const;
};

struct ChordAttackChange {
    int  ticks;
    bool random;
};

using MidiTrackCommand = std::variant<MidiTransposition, ChordAttackChange>;

struct VoiceInstrument {
    struct Patch {
        int program;
        int bank;
    };
    std::optional<Patch> patch;
    bool                 mute = false;
};
struct Harmony {
    struct Voicing {
        i64      bass;
        vec<i64> pitches;
        bool     declares_backing;
    };

    std::string_view symbol;
    // Resolved once in written order before discovering or playing backing tracks.
    std::optional<Voicing> voicing;
};
struct Decoration {
    std::string_view spelling;
    bool             last_on_line = false;
};

enum class DecorationDelimiter { Either, Bang, Plus };

struct SymbolDefinition {
    char             shorthand;
    std::string_view spelling;
};

struct SymbolPlacement {
    size_t                            slot;
    Location                          at;
    std::variant<Harmony, Decoration> symbol;
};

// Decode an s: field into inline symbols addressed by written note slot.
// A malformed row warns and claims no passage; a valid empty row does.
[[nodiscard]] std::optional<vec<SymbolPlacement>> align_symbols(
    const Field&            field,
    size_t                  notes,
    std::span<const size_t> bars,
    DecorationDelimiter     delimiter,
    Location                at,
    Diagnostics&            diagnostics
);

struct Tuplet {
    int count;
    int in_time_of;
    int notes;
};
struct Broken {
    int direction;
    int count;
    // ABC's dotted pair by default; playback can override the single-mark ratio.
    [[nodiscard]] long double first_factor(long double single_short = 0.5L) const;
};
struct Slur {
    bool start;
};
struct Tie {};
enum class Overlay { Split, Start, StartAtCursor, End, PreviousTwo };

struct EndingRange {
    int first;
    int last;
};

// A set of one-based repeat passes built from validated inclusive ranges.
// The empty set means no numbered ending was written on this bar.
class RepeatEndings {
public:
    RepeatEndings() = default;
    explicit RepeatEndings(vec<EndingRange> written);

    [[nodiscard]] bool   empty() const { return ranges.empty(); }
    [[nodiscard]] bool   contains(size_t pass) const;
    [[nodiscard]] size_t last_pass() const { return empty() ? 0 : ranges.back().last; }

private:
    vec<EndingRange> ranges;
};

struct Bar {
    // Requested passes for the adjacent region, or zero when that side does not repeat.
    size_t        repeat_start   = 0;
    size_t        repeat_end     = 0;
    bool          resets_measure = true;
    bool          section        = false;
    RepeatEndings endings;
};

// ABC 2.1 section 6.1.2: y can carry decorations but has no rhythmic length.
struct Spacer {};
struct EmptyMusicLine {};

struct Token {
    Location at;
    std::variant<
        Atom,
        Grace,
        Field,
        TempoNotation,
        Directive,
        PitchChange,
        MidiTrackCommand,
        VoiceInstrument,
        Harmony,
        Decoration,
        SymbolDefinition,
        Tuplet,
        Broken,
        Slur,
        Tie,
        Overlay,
        Bar,
        Spacer,
        EmptyMusicLine>
           value;
    size_t music_line = 0;   // Opening physical line of this music line, across continuations.

    // Only rhythmic atoms and bars establish a lyric cursor in playback.
    [[nodiscard]] bool anchors_lyrics() const {
        return std::holds_alternative<Atom>(value) || std::holds_alternative<Bar>(value);
    }

    // Both information fields and percent directives carry commands.
    [[nodiscard]] std::optional<std::string_view> information() const {
        if (const auto* directive = std::get_if<Directive>(&value)) return directive->value;
        if (const auto* field = std::get_if<Field>(&value); field && field->kind == 'I')
            return field->value;
        return {};
    }
};

struct AbcVersion {
    int  major;
    int  minor;
    auto operator<=>(const AbcVersion&) const = default;
};

enum class TextContext { Title, VoiceName, Lyric, ChordSymbol };

// Encode ABC text for its field. A lyric is encoded as one written syllable.
[[nodiscard]] std::string encode_text(std::string_view text, TextContext context);
// Decode ABC text without its surrounding delimiters.
[[nodiscard]] std::string decode_text(std::string_view text, TextContext context);

// Header token order includes file-level settings before X: and ends at K:.
// The body contains only the first tune. Token views reference the input or
// owned_text (for included and rewritten text); consumers must keep both alive.
struct Tune {
    std::deque<std::string>   owned_text;
    vec<Token>                header;
    vec<Token>                body;
    Location                  reference;
    Location                  key;
    std::optional<AbcVersion> version;
    DecorationDelimiter       decoration_delimiter = DecorationDelimiter::Either;
};

// Parse a bare pitch for a MIDI drummap directive, using the music spelling rules.
std::optional<Pitch> drum_spelling(std::string_view text, Location at, Diagnostics& diagnostics);

struct ExternalText {
    std::string name;
    std::string text;
};

enum class TextReadError { unavailable, cycle };
using TextRead = std::variant<TextReadError, ExternalText>;

// Resolve a name under the score's directory. Reject canonical paths already
// present in the include stack before reading bytes. The loader supplies the
// canonical name alongside the text. An empty loader disables external reads.
using TextLoader = std::function<TextRead(std::string_view, std::span<const std::string_view>)>;

// Check the source charset and tokenize the first tune. In the result, music-line identity
// survives line joins and source positions survive rewrites. Locations borrow
// the source registry in diagnostics. A structural error that prevents
// tokenization is reported through Diagnostics::error.
Tune tokenize(std::string_view text, const TextLoader& headers, Diagnostics& diagnostics);

}   // namespace symusic::abc::detail

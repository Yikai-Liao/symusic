// miniabc: header-only abstraction for ABC parsing/rendering.
#pragma once

#include <cstdint>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

#include "miniabc/support.h"

namespace miniabc {


inline constexpr std::size_t version_major = 0;
inline constexpr std::size_t version_minor = 2;

struct Meter {
    int numerator   = 4;
    int denominator = 4;
};

struct TempoMark {
    Fraction beat{1, 4};
    double   qpm = 120.0;
};

struct KeySignature {
    int  accidentals = 0;   // -7 .. 7
    bool is_minor    = false;
};

struct Header {
    int          reference_number = 1;
    std::string  title;
    Meter        meter{};
    Fraction     unit_note_length{1, 8};
    TempoMark    tempo{};
    KeySignature key{};
    int          ticks_per_quarter = 960;
};

struct NoteEvent {
    // Symbolic information
    std::string accidental;   // "^", "^^", "=", "_", "__" or ""
    char        letter = 'C';
    int         octave = 0;
    Fraction    duration_factor{1, 1};
    bool        has_symbolic_info = false;

    // Derived / MIDI information
    Fraction                 length{};
    int                      midi_pitch   = 60;
    int                      velocity     = 96;
    bool                     tie_continue = false;
    bool                     is_grace     = false;
    std::vector<std::string> decorations;
    std::vector<std::string> chord_symbols;
};

struct RestEvent {
    Fraction length{};
    char     symbol    = 'z';
    bool     invisible = false;
};

struct BarEvent {
    bool repeat_start = false;
    bool repeat_end   = false;
    bool double_bar   = false;
    bool final_bar    = false;
};

struct ChordEvent {
    std::vector<NoteEvent>   notes;
    std::vector<std::string> chord_symbols;
    bool                     is_grace = false;
};

using Element = std::variant<NoteEvent, RestEvent, BarEvent, ChordEvent>;

struct Voice {
    std::string              name;
    std::vector<Element>     elements;
    std::vector<std::string> lyrics;
};

struct Document {
    Header                   header{};
    std::vector<Voice>       voices;
    std::vector<std::string> directives;
};

struct ParseOptions {
    int  ticks_per_quarter = 480;
    int  default_velocity  = 96;
    bool strict            = true;
};

class ParseError : public std::runtime_error {
public:
    explicit ParseError(std::vector<Diagnostic> diagnostics);

    [[nodiscard]] const std::vector<Diagnostic>& diagnostics() const noexcept {
        return diagnostics_;
    }

private:
    std::vector<Diagnostic> diagnostics_;
};

[[nodiscard]] Document    parse_document(std::string_view input, const ParseOptions& options = {});
[[nodiscard]] std::string render_document(const Document& document);
[[nodiscard]] const char* describe() noexcept;

}   // namespace miniabc

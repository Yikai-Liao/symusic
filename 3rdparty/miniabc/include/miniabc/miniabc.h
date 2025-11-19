// miniabc: header-only abstraction for ABC parsing/rendering.
#pragma once

#include <cstdint>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace miniabc {

struct Fraction {
    std::int64_t num = 0;
    std::int64_t den = 1;
};

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
    Fraction length{};
    int      midi_pitch   = 60;
    int      velocity     = 96;
    bool     tie_continue = false;
};

struct RestEvent {
    Fraction length{};
};

struct BarEvent {
    bool repeat_start = false;
    bool repeat_end   = false;
    bool double_bar   = false;
    bool final_bar    = false;
};

struct ChordEvent {
    std::vector<NoteEvent> notes;
};

using Element = std::variant<NoteEvent, RestEvent, BarEvent, ChordEvent>;

struct Voice {
    std::string           name;
    std::vector<Element>  elements;
    std::vector<std::string> lyrics;
};

struct Document {
    Header              header{};
    std::vector<Voice>  voices;
    std::vector<std::string> directives;
};

struct Diagnostic {
    std::size_t line   = 0;
    std::size_t column = 0;
    std::string message;
};

struct ParseOptions {
    int  ticks_per_quarter = 960;
    int  default_velocity  = 96;
    bool strict            = true;
};

class ParseError : public std::runtime_error {
public:
    explicit ParseError(std::vector<Diagnostic> diagnostics);

    [[nodiscard]] const std::vector<Diagnostic>& diagnostics() const noexcept { return diagnostics_; }

private:
    std::vector<Diagnostic> diagnostics_;
};

[[nodiscard]] Document parse_document(std::string_view input, const ParseOptions& options = {});
[[nodiscard]] std::string render_document(const Document& document);
[[nodiscard]] const char* describe() noexcept;

}  // namespace miniabc

#pragma once

#include <array>

#include "syntax.h"

namespace symusic::abc::detail {

// Playback uses fractional ticks; only emitted events are quantized.
inline constexpr i32 tpq               = 480;
inline constexpr i32 whole             = 4 * tpq;
inline constexpr int midi_bend_center  = 8192;
inline constexpr int midi_channels     = 16;
inline constexpr int midi_drum_channel = 10;
inline constexpr int midi_max          = 127;
inline constexpr int max_voice_lanes   = 64;

inline constexpr std::array<int, 7> natural_pitch{9, 11, 0, 2, 4, 5, 7};
inline constexpr size_t letter_index(char note) { return static_cast<size_t>(note - 'A'); }

struct Meter {
    int                       numerator   = 4;
    int                       denominator = 4;
    bool                      free        = false;
    [[nodiscard]] long double beat() const { return static_cast<long double>(whole) / denominator; }
    [[nodiscard]] long double measure() const { return numerator * beat(); }
    [[nodiscard]] int         accent_beats() const;
};

struct Key {
    int                       fifths = 0;
    int                       minor  = 0;
    std::optional<int>        transpose;
    int                       octave = 0;
    std::array<Alteration, 7> alterations{};
};

struct VoiceSpec {
    std::string_view   id;
    std::string_view   name;
    std::optional<int> transpose;
    std::optional<int> octave;
};

enum class AccidentalScope { pitch, octave, none };

// A tune-wide notation policy. Consume directives in source order, before
// splitting voices or expanding repeats; queries use the original note location.
class AccidentalPolicy {
public:
    bool                          consume(const Token& token, Diagnostics& diagnostics);
    [[nodiscard]] AccidentalScope at(Location written) const;

private:
    vec<std::pair<Location, AccidentalScope>> changes;
};

// abc2midi applies the last programbase inside a tune to every program it emits.
// Collect it before rendering, then use the same conversion for every instrument.
class ProgramNumbers {
public:
    bool              consume(const Token& token, Location reference, Diagnostics& diagnostics);
    [[nodiscard]] int read(std::string_view& args, Diagnostics& diagnostics, Location at) const;
    [[nodiscard]] int midi(int written) const { return std::max(0, written - base); }

private:
    int base = 0;
};

// A tune's percussion routes persist across MIDI tracks in render order.
// Both melody and backing resolve the destination once, at attack time.
class DrumMap {
public:
    void              assign(std::string_view args, Location at, Diagnostics& diagnostics);
    [[nodiscard]] int pitch(int source) const { return routes[source].value_or(source); }

private:
    std::array<std::optional<int>, midi_max + 1> routes{};
};

int midi_integer(std::string_view& args, int low, int high, Diagnostics& diagnostics, Location at);

// MIDI transpose shifts sounding pitches independently of K:/V: transposition.
// A handled directive updates the current MIDI shift; other commands are untouched.
bool midi_transposition(
    std::string_view name, std::string_view args, int& shift, Diagnostics& diagnostics, Location at
);

Length ratio(
    std::string_view text, Diagnostics& diagnostics, Location at, std::string_view context
);
Meter              meter(std::string_view text, Diagnostics& diagnostics, Location at);
long double        unit_length(std::string_view text, Diagnostics& diagnostics, Location at);
std::optional<Key> key(
    std::string_view text, const Key& previous, Diagnostics& diagnostics, Location at, bool header
);
i32       tempo(std::string_view text, long double unit, Diagnostics& diagnostics, Location at);
VoiceSpec voice_spec(std::string_view text, Diagnostics& diagnostics, Location at);
std::string_view key_name(int fifths, bool minor);
std::string      unquote_text(std::string_view text);
i32              tick(long double time, Diagnostics& diagnostics, Location at);

}   // namespace symusic::abc::detail

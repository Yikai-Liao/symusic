#pragma once

#include <array>
#include <bitset>

#include "syntax.h"

namespace symusic::abc::detail {

// Playback uses fractional ticks; only emitted events are quantized.
inline constexpr i32 tpq               = 480;
inline constexpr i32 whole             = 4 * tpq;
inline constexpr int midi_bend_center  = 8192;
inline constexpr int midi_channels     = 16;
inline constexpr int midi_drum_channel = 10;
inline constexpr int midi_max          = 127;
inline constexpr int midi_max_bank     = (midi_max + 1) * (midi_max + 1) - 1;
inline constexpr int max_voice_lanes   = 64;

inline constexpr std::array<int, 7> natural_pitch{9, 11, 0, 2, 4, 5, 7};
inline constexpr size_t letter_index(char note) { return static_cast<size_t>(note - 'A'); }

// The ratio also supplies the playback pulse; beat() and measure() return
// fractional ticks. Free meter retains a pulse but imposes no bar length.
struct Meter {
    int  numerator   = 4;
    int  denominator = 4;
    bool free        = false;
    // Additional group starts, in denominator units from the start of the bar.
    std::bitset<256>          group_starts{};
    [[nodiscard]] long double beat() const { return static_cast<long double>(whole) / denominator; }
    [[nodiscard]] long double measure() const { return numerator * beat(); }
    [[nodiscard]] bool        default_accent(long double position) const;
    bool                      operator==(const Meter&) const = default;
};

// Key signature and pitch settings are independent: octave and clef shift
// alter written pitch in octaves; transpose alters sounding pitch in semitones.
struct Key {
    // MIDI signature metadata; explicit and bagpipe keys can use a different sounding scale.
    int                       fifths = 0;
    int                       minor  = 0;
    std::optional<int>        transpose;
    int                       written_octave = 0;
    int                       clef_shift     = 0;
    std::array<Alteration, 7> alterations{};

    void              apply(const PitchChange& change);
    [[nodiscard]] int effective_octave(int octave) const {
        return octave + written_octave + clef_shift;
    }
};

struct KeyField {
    Key  state;
    bool sets_signature;
    // Resolve the destination voice before applying these explicit options.
    PitchChange pitch;
};

struct VoiceSpec {
    std::string_view id;
    std::string_view name;
    PitchChange      pitch;
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

// Collect the last explicit programbase before rendering. ABC 2.1 uses
// General MIDI instrument numbers for chordprog in the absence of an override.
class ProgramNumbers {
public:
    bool              consume(const Token& token, Location reference, Diagnostics& diagnostics);
    [[nodiscard]] int read(std::string_view& args, Diagnostics& diagnostics, Location at) const;
    [[nodiscard]] int read_chord(
        std::string_view& args, Diagnostics& diagnostics, Location at
    ) const;
    [[nodiscard]] int midi(int written) const { return std::max(0, written - base.value_or(0)); }

private:
    std::optional<int> base;
};

struct DrumRoute {
    int source;
    int destination;
};

// The tune's percussion routes persist across MIDI passes. Read validates a
// written route; applying it replaces only the route for its source pitch.
class DrumMap {
public:
    static std::optional<DrumRoute> read(
        std::string_view args, Location at, Diagnostics& diagnostics
    );
    bool consume(const Token& token, Diagnostics& diagnostics);
    void apply(DrumRoute route) { routes[route.source] = route.destination; }
    void apply(const DrumMap& changes) {
        for (size_t i = 0; i < routes.size(); ++i)
            if (changes.routes[i]) routes[i] = changes.routes[i];
    }
    [[nodiscard]] std::optional<int> destination(int source) const { return routes[source]; }
    [[nodiscard]] int pitch(int source) const { return routes[source].value_or(source); }

private:
    std::array<std::optional<int>, midi_max + 1> routes{};
};

int midi_integer(std::string_view& args, int low, int high, Diagnostics& diagnostics, Location at);

struct ChordAttackSettings {
    static constexpr int default_gap  = 10;
    int                  gap          = default_gap;
    int                  random_limit = 0;
};

// These MIDI settings survive from the end of one generated track to the next.
// Written K:/V: pitch options and one-shot decorations are separate.
struct MidiTrackSettings {
    int                 shift = 0;
    ChordAttackSettings attack;

    void apply(const MidiTrackCommand& command);
};

std::optional<MidiTrackCommand> midi_track_command(const Token& token, Diagnostics& diagnostics);

Length ratio(
    std::string_view text, Diagnostics& diagnostics, Location at, std::string_view context
);
Meter       meter(std::string_view text, Diagnostics& diagnostics, Location at);
long double unit_length(std::string_view text, Diagnostics& diagnostics, Location at);
// A clef or pitch option alone updates the voice without setting a new signature.
// An invalid opening field raises; an invalid body field warns and returns no change.
std::optional<KeyField> key(
    std::string_view text, const Key& previous, Diagnostics& diagnostics, Location at, bool header
);
i32 tempo(const TempoNotation& written, long double unit, Diagnostics& diagnostics, Location at);
VoiceSpec        voice_spec(std::string_view text, Diagnostics& diagnostics, Location at);
std::string_view key_name(int fifths, bool minor);

// Floor a fractional playback tick, allowing for arithmetic noise just below
// an integer boundary. A result outside the nonnegative int32 domain is fatal.
i32 tick(long double time, Diagnostics& diagnostics, Location at);

}   // namespace symusic::abc::detail

#pragma once

#include <algorithm>
#include <array>
#include <bitset>
#include <cassert>
#include <iterator>
#include <string>
#include <utility>

#include "syntax.h"

namespace symusic::abc::detail {

// Playback uses fractional ticks; only emitted events are quantized.
inline constexpr i32 kTpq             = 480;
inline constexpr i32 kWhole           = 4 * kTpq;
inline constexpr i32 kMidiBendCenter  = 8192;
inline constexpr i32 kMidiChannels    = 16;
inline constexpr i32 kMidiDrumChannel = 10;
inline constexpr i32 kMidiMax         = 127;
inline constexpr i32 kMidiMaxBank     = (kMidiMax + 1) * (kMidiMax + 1) - 1;
inline constexpr i32 kMaxVoiceLanes   = 64;
inline constexpr f64 kTickTolerance   = 1e-7;

inline constexpr std::array<i32, 7> kNaturalPitch{9, 11, 0, 2, 4, 5, 7};
inline constexpr std::array<i32, 7> kTonicFifths{3, 5, 0, 2, 4, -1, 1};
inline constexpr size_t letter_index(char note) { return static_cast<size_t>(note - 'A'); }

// I: and %% share an instruction syntax. For MIDI, args starts after the subcommand.
struct Instruction {
    explicit Instruction(const Token& token) : args(token.information().value_or("")) {
        name = word(args);
        if (name == "MIDI") midi = word(args);
    }

    std::string_view name;
    std::string_view midi;
    std::string_view args;
};

// Record in written order, then query in any playback order.
template<typename T>
class WrittenSetting {
public:
    explicit WrittenSetting(T initial = {}) : initial(std::move(initial)) {}

    void record(WrittenPosition at, T value) {
        assert(changes.empty() || changes.back().at <= at);
        changes.push_back({at, std::move(value)});
    }
    [[nodiscard]] const T& current() const {
        return changes.empty() ? initial : changes.back().value;
    }
    [[nodiscard]] const T& at(WrittenPosition written) const {
        const auto next = std::ranges::upper_bound(changes, written, {}, &Change::at);
        return next == changes.begin() ? initial : std::prev(next)->value;
    }

private:
    struct Change {
        WrittenPosition at;
        T               value;
    };
    T           initial;
    vec<Change> changes;
};

// The ratio also supplies the playback pulse; beat() and measure() return
// fractional ticks. Free meter retains a pulse but imposes no bar length.
struct Meter {
    i32  numerator   = 4;
    i32  denominator = 4;
    bool free        = false;
    // Additional group starts, in denominator units from the start of the bar.
    std::bitset<256>   group_starts{};
    [[nodiscard]] f64  beat() const { return static_cast<f64>(kWhole) / denominator; }
    [[nodiscard]] f64  measure() const { return numerator * beat(); }
    [[nodiscard]] bool default_accent(f64 position) const;
    bool               operator==(const Meter&) const = default;
};

// Key signature and pitch settings are independent: octave and clef shift
// alter written pitch in octaves; transpose alters sounding pitch in semitones.
struct Key {
    // MIDI signature metadata; explicit and bagpipe keys can use a different sounding scale.
    i32                       fifths = 0;
    i32                       minor  = 0;
    std::optional<i32>        transpose;
    i32                       written_octave = 0;
    i32                       clef_shift     = 0;
    std::array<Alteration, 7> alterations{};

    void              apply(const PitchChange& change);
    [[nodiscard]] i32 effective_octave(i32 octave) const {
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
    std::string id;
    std::string name;
    PitchChange pitch;
};

enum class AccidentalScope { Pitch, Octave, None };

// A tune-wide notation policy. Consume directives in source order, before
// splitting voices or expanding repeats; queries use the original note location.
class AccidentalPolicy {
public:
    bool                          consume(const Token& token, Diagnostics& diagnostics);
    [[nodiscard]] AccidentalScope at(Location at) const { return scope.at(at.written); }

private:
    WrittenSetting<AccidentalScope> scope{AccidentalScope::Pitch};
};

// Collect the last explicit programbase before rendering. ABC 2.1 uses
// General MIDI instrument numbers for chordprog in the absence of an override.
class ProgramNumbers {
public:
    bool              consume(const Token& token, Location reference, Diagnostics& diagnostics);
    [[nodiscard]] i32 read(std::string_view& args, Diagnostics& diagnostics, Location at) const;
    [[nodiscard]] i32 read_chord(
        std::string_view& args, Diagnostics& diagnostics, Location at
    ) const;
    [[nodiscard]] i32 midi(i32 written) const { return std::max(0, written - base.value_or(0)); }

private:
    std::optional<i32> base;
};

// The tune's percussion routes persist across MIDI passes. Read validates a
// written route; applying it replaces only the route for its source pitch.
class DrumMap {
public:
    static std::optional<DrumRoute> read(
        std::string_view args, Location at, Diagnostics& diagnostics
    );
    void apply(DrumRoute route) { routes[route.source] = route.destination; }
    void apply(const DrumMap& changes) {
        for (size_t i = 0; i < routes.size(); ++i)
            if (changes.routes[i]) routes[i] = changes.routes[i];
    }
    [[nodiscard]] std::optional<i32> destination(i32 source) const { return routes[source]; }
    [[nodiscard]] i32 pitch(i32 source) const { return routes[source].value_or(source); }

private:
    std::array<std::optional<u8>, kMidiMax + 1> routes{};
};

i32 midi_integer(std::string_view& args, i32 low, i32 high, Diagnostics& diagnostics, Location at);

struct ChordAttackSettings {
    static constexpr i32 kDefaultGap  = 10;
    i32                  gap          = kDefaultGap;
    i32                  random_limit = 0;
};

// These MIDI settings survive from the end of one generated track to the next.
// Written K:/V: pitch options and one-shot decorations are separate.
struct MidiTrackSettings {
    i32                 shift = 0;
    ChordAttackSettings attack;

    void apply(const MidiTrackCommand& command);
};

std::optional<MidiTrackCommand> midi_track_command(const Token& token, Diagnostics& diagnostics);

Length ratio(
    std::string_view text, Diagnostics& diagnostics, Location at, std::string_view context
);
Meter meter(std::string_view text, Diagnostics& diagnostics, Location at);
f64   unit_length(std::string_view text, Diagnostics& diagnostics, Location at);
// A clef or pitch option alone updates the voice without setting a new signature.
// An unrecognized signature returns no value; the caller decides whether it is fatal.
std::optional<KeyField> key(
    std::string_view text, const Key& previous, Diagnostics& diagnostics, Location at
);
i32 tempo(const TempoNotation& written, f64 unit, Diagnostics& diagnostics, Location at);
[[nodiscard]] VoiceSpec voice_spec(std::string_view text, Diagnostics& diagnostics, Location at);
std::string_view        key_name(i32 fifths, bool minor);

// Floor a fractional playback tick, allowing for arithmetic noise just below
// an integer boundary. A result outside the nonnegative int32 domain is fatal.
i32 tick(f64 time, Diagnostics& diagnostics, Location at);

}   // namespace symusic::abc::detail

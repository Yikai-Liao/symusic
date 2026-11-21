#include "settings.h"

#include <bit>
#include <cmath>
#include <limits>

namespace symusic::abc::detail {
namespace {

using namespace std::literals;

constexpr std::string_view kSharpOrder = "FCGDAEB";
constexpr std::string_view kFlatOrder  = "BEADGCF";
constexpr std::array       kMajorNames{
    "Cb", "Gb", "Db", "Ab", "Eb", "Bb", "F", "C", "G", "D", "A", "E", "B", "F#", "C#"
};
constexpr std::array kMinorNames{
    "Abm", "Ebm", "Bbm", "Fm", "Cm", "Gm", "Dm", "Am", "Em", "Bm", "F#m", "C#m", "G#m", "D#m", "A#m"
};

std::string ascii_lower(std::string_view value) {
    std::string result(value);
    for (auto& c : result) {
        if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
    }
    return result;
}

struct Mode {
    std::string_view prefix;
    i32              shift;
    i32              minor;
};
constexpr std::array kModes{
    Mode{"maj", 0, 0},
    Mode{"ion", 0, 0},
    Mode{"min", -3, 1},
    Mode{"aeo", -3, 1},
    Mode{"dor", -2, 0},
    Mode{"phr", -4, 0},
    Mode{"lyd", 1, 0},
    Mode{"mix", -1, 0},
    Mode{"loc", -5, 0},
    Mode{"m", -3, 1},
};

const Mode* mode_of(std::string_view name) {
    const auto found = std::ranges::find(kModes, ascii_lower(name.substr(0, 3)), &Mode::prefix);
    return found == kModes.end() ? nullptr : &*found;
}

void set_signature(Key& key, i32 fifths, i32 minor = 0) {
    key.fifths = fifths;
    key.minor  = minor;
    key.alterations.fill({});
    const auto order = fifths < 0 ? kFlatOrder : kSharpOrder;
    for (const auto c : order.substr(0, static_cast<size_t>(std::abs(fifths)))) {
        key.alterations[letter_index(c)] = Alteration{fifths < 0 ? -1 : 1};
    }
}

std::optional<i32> tonic(std::string_view& name) {
    if (name.empty() || !note_letter(name.front())) return {};
    i32 fifths = kTonicFifths[letter_index(upper(name.front()))];
    name.remove_prefix(1);
    if (name.starts_with('#') || name.starts_with('b')) {
        fifths += name.front() == '#' ? 7 : -7;
        name.remove_prefix(1);
    }
    return fifths;
}

const Mode* take_mode(std::string_view& options) {
    auto        remaining = options;
    const auto* mode      = mode_of(word(remaining));
    if (mode) options = remaining;
    return mode;
}

i32 supported_signature(i32 fifths) {
    if (fifths > 7) return fifths - 12;
    if (fifths < -7) return fifths + 12;
    return fifths;
}

bool parse_signature(
    std::string_view& options, Key& result, Diagnostics& diagnostics, Location at
) {
    auto       name    = word(options);
    const auto special = ascii_lower(name);
    if (name.empty() || special == "none") {
        set_signature(result, 0);
        return true;
    }
    if (special == "hp") {
        set_signature(result, 2);
        // HP omits the printed signature but uses the same pipe scale as Hp.
        if (name == "HP") result.fifths = 0;
        return true;
    }

    // abc2midi can recover the tonic of an uppercase key with an unknown suffix.
    const bool can_recover = !lower(name.front());
    const auto fifths      = tonic(name);
    if (!fifths) return false;
    const auto* mode = name.empty() ? take_mode(options) : mode_of(name);
    if (!mode && !name.empty()) {
        if (!can_recover) return false;
        diagnostics.warn(
            at, "unknown-key-mode", fmt::format("ignored key mode '{}'; used the major key", name)
        );
    }
    constexpr Mode kMajor{"", 0, 0};
    const auto&    effective = mode ? *mode : kMajor;
    set_signature(result, supported_signature(*fifths + effective.shift), effective.minor);
    return true;
}

using WrittenAccidentals = std::array<std::optional<Alteration>, 7>;

bool custom_accidental(
    std::string_view word, WrittenAccidentals& overrides, Diagnostics& diagnostics, Location at
) {
    if (word.empty() || std::string_view("^_=").find(word.front()) == word.npos) return false;
    if (word.size() < 2 || !note_letter(word.back())) {
        diagnostics.warn(
            at, "invalid-accidental", fmt::format("key accidental '{}' needs a note A to G", word)
        );
        return true;
    }
    if (const auto value = accidental(word.substr(0, word.size() - 1), at, diagnostics))
        overrides[letter_index(upper(word.back()))] = *value;
    return true;
}

i32 shift_value(
    std::string_view text, Diagnostics& diagnostics, Location at, std::string_view option, i32 limit
) {
    const auto shift = integer(text);
    if (!shift || *shift < -limit || *shift > limit) {
        diagnostics.error(
            at,
            "invalid-transposition",
            fmt::format("{} must be between {} and {}", option, -limit, limit)
        );
    }
    return *shift;
}

std::optional<i32> clef_octave(std::string_view value) {
    // ABC 2.1 section 4.6: the suffix sounds an octave away without changing the written note.
    const auto shift = value.ends_with("+8") ? 1 : value.ends_with("-8") ? -1 : 0;
    if (shift) value.remove_suffix(2);
    if (!value.empty() && value.back() >= '1' && value.back() <= '5') value.remove_suffix(1);
    constexpr std::array kNames{"treble"sv, "alto"sv, "tenor"sv, "bass"sv, "perc"sv, "none"sv};
    if (std::ranges::find(kNames, ascii_lower(value)) == kNames.end()) return {};
    return shift;
}

bool read_pitch_option(
    PitchChange&     pitch,
    std::string_view name,
    std::string_view value,
    Diagnostics&     diagnostics,
    Location         at
) {
    const auto lower = ascii_lower(name);
    if (lower == "clef") {
        if (const auto shift = clef_octave(value)) pitch.clef_shift = shift;
    } else if (lower == "octave") {
        pitch.written_octave = shift_value(value, diagnostics, at, name, 16);
    } else if (lower == "transpose" || lower == "t") {
        pitch.transpose = shift_value(value, diagnostics, at, name, kMidiMax);
    } else if (const auto shift = clef_octave(name)) {
        if (value.empty()) pitch.clef_shift = shift;
    } else {
        // These engraving options also allow a K: field to omit its signature.
        return lower == "middle" || lower == "m" || lower == "stafflines";
    }
    return true;
}

std::string_view quoted_option(std::string_view& options) {
    options.remove_prefix(1);
    size_t end = 0;
    while (end < options.size() && options[end] != '"') {
        end += options[end] == '\\' && end + 1 < options.size() ? 2 : 1;
    }
    const auto value = options.substr(0, end);
    options.remove_prefix(std::min(end + 1, options.size()));
    return value;
}

std::pair<std::string_view, std::string_view> option(std::string_view& options) {
    options = trim(options);
    if (options.find_first_of("=^_") == 0) return {word(options), {}};
    const auto name = options.substr(0, options.find_first_of("= \t\r\f\v"));
    const auto tail = options.substr(name.size());
    options         = trim(tail);
    if (!options.starts_with('=')
        || (!tail.starts_with('=') && (clef_octave(name) || ascii_lower(name) == "exp"))) {
        // A natural after a bare clef name or exp belongs to the key signature.
        return {name, {}};
    }
    options = trim(options.substr(1));
    return {name, options.starts_with('"') ? quoted_option(options) : word(options)};
}

void read_key_options(
    KeyField& key, std::string_view options, Diagnostics& diagnostics, Location at
) {
    // exp clears the implied scale. Written accidentals on either side of it still apply.
    WrittenAccidentals overrides{};
    while (!trim(options).empty()) {
        const auto [name, value] = option(options);
        if (ascii_lower(name) == "exp") {
            key.state.alterations.fill({});
            key.sets_signature = true;
        } else if (!custom_accidental(name, overrides, diagnostics, at)) {
            read_pitch_option(key.pitch, name, value, diagnostics, at);
        }
    }
    for (size_t i = 0; i < overrides.size(); ++i) {
        if (const auto& alteration = overrides[i]) {
            key.state.alterations[i] = *alteration;
            key.sets_signature       = true;
        }
    }
}

i32 named_tempo(std::string_view label) {
    // Conventional text tempi use abc2midi's quarter-note metronome rates.
    struct NamedTempo {
        std::string_view name;
        i32              bpm;
    };
    static constexpr NamedTempo kNames[]{
        {"larghissimo", 40},
        {"adagissimo", 44},
        {"lentissimo", 48},
        {"largo", 56},
        {"adagio", 59},
        {"lento", 62},
        {"larghetto", 66},
        {"adagietto", 76},
        {"andante", 88},
        {"andantino", 96},
        {"moderato", 104},
        {"allegretto", 112},
        {"allegro", 120},
        {"vivace", 168},
        {"vivo", 180},
        {"presto", 192},
        {"allegrissimo", 208},
        {"vivacissimo", 220},
        {"prestissimo", 240}
    };
    const auto found = std::ranges::find(kNames, ascii_lower(label), &NamedTempo::name);
    return found == std::end(kNames) ? 0 : found->bpm;
}

}   // namespace

void Key::apply(const PitchChange& change) {
    if (change.transpose) transpose = change.transpose;
    if (change.written_octave) written_octave = *change.written_octave;
    if (change.clef_shift) clef_shift = *change.clef_shift;
}

bool AccidentalPolicy::consume(const Token& token, Diagnostics& diagnostics) {
    const Instruction command(token);
    auto              value = trim(command.args);
    if (command.midi == "barlines")
        value = "pitch";
    else if (command.midi == "nobarlines")
        value = "not";
    else if (command.name != "propagate-accidentals")
        return false;

    if (value == "pitch")
        scope.record(token.at.written, AccidentalScope::Pitch);
    else if (value == "octave")
        scope.record(token.at.written, AccidentalScope::Octave);
    else if (value == "not")
        scope.record(token.at.written, AccidentalScope::None);
    else
        diagnostics.warn(
            token.at,
            "invalid-accidental-scope",
            "expected 'pitch', 'octave' or 'not' for propagate-accidentals"
        );
    return true;
}

std::optional<DrumRoute> DrumMap::read(
    std::string_view args, Location at, Diagnostics& diagnostics
) {
    const auto source = drum_spelling(word(args), at, diagnostics);
    if (!source) return {};
    const auto alteration = source->accidental.value_or(Alteration{});
    // A fractional accidental can have an int-sized numerator even on a middle-octave note.
    const i64 note = 60 + i64{12} * source->octave + kNaturalPitch[letter_index(source->letter)]
                     + alteration.numerator;
    if (note < 0 || note > kMidiMax || alteration.denominator != 0)
        diagnostics.error(
            at, "invalid-drummap", "drummap source must be a MIDI pitch without a microtone"
        );
    const auto destination = midi_integer(args, 1, kMidiMax, diagnostics, at);
    if (destination < 35 || destination > 81)
        diagnostics.warn(
            at, "drummap-range", "destination is outside General MIDI percussion (35–81)"
        );
    // Both operands have been validated as MIDI data bytes.
    return DrumRoute{static_cast<u8>(note), static_cast<u8>(destination)};
}

bool ProgramNumbers::consume(const Token& token, Location reference, Diagnostics& diagnostics) {
    Instruction command(token);
    if (command.midi != "programbase") return false;
    if (token.at < reference) {
        diagnostics.warn(
            token.at,
            "program-base-placement",
            "ignored programbase before X:; place it inside the tune"
        );
    } else {
        base = midi_integer(command.args, 0, std::numeric_limits<i32>::max(), diagnostics, token.at)
               != 0;
    }
    return true;
}

i32 ProgramNumbers::read(std::string_view& args, Diagnostics& diagnostics, Location at) const {
    return midi(midi_integer(args, 0, kMidiMax + base.value_or(0), diagnostics, at));
}

i32 ProgramNumbers::read_chord(
    std::string_view& args, Diagnostics& diagnostics, Location at
) const {
    if (base) return read(args, diagnostics, at);
    return midi_integer(args, 1, kMidiMax + 1, diagnostics, at) - 1;
}

i32 midi_integer(std::string_view& args, i32 low, i32 high, Diagnostics& diagnostics, Location at) {
    const auto number = integer(word(args));
    if (number && *number >= low && *number <= high) return *number;
    diagnostics.error(
        at, "invalid-midi-value", fmt::format("expected a MIDI value between {} and {}", low, high)
    );
}

i32 MidiTransposition::apply(i32 shift) const {
    return relative ? std::clamp(shift + amount, -127, 127) : amount;
}

void MidiTrackSettings::apply(const MidiTrackCommand& command) {
    if (const auto* change = std::get_if<MidiTransposition>(&command)) {
        shift = change->apply(shift);
    } else {
        const auto& spacing                                 = std::get<ChordAttackChange>(command);
        (spacing.random ? attack.random_limit : attack.gap) = spacing.ticks;
    }
}

std::optional<MidiTrackCommand> midi_track_command(const Token& token, Diagnostics& diagnostics) {
    Instruction command(token);
    if (command.midi == "chordattack" || command.midi == "randomchordattack")
        return ChordAttackChange{
            midi_integer(command.args, 0, kTpq, diagnostics, token.at),
            command.midi == "randomchordattack"
        };
    if (command.midi != "transpose" && command.midi != "rtranspose") return {};
    return MidiTransposition{
        shift_value(word(command.args), diagnostics, token.at, command.midi, kMidiMax),
        command.midi == "rtranspose"
    };
}

bool Meter::default_accent(f64 position) const {
    if (group_starts.none()) {
        const auto step = numerator % 3 == 0 ? 3 : numerator % 2 == 0 ? 2 : numerator;
        return std::abs(std::remainder(position, step * beat())) < kTickTolerance;
    }
    // ABC 2.1 section 3.1.6: accent each written group, including when bar lines are omitted.
    const auto offset = std::fmod(position, measure());
    const auto pulse  = static_cast<i32>(std::round(offset / beat()));
    if (std::abs(offset - pulse * beat()) >= kTickTolerance) return false;
    return pulse == 0 || pulse == numerator || group_starts.test(pulse);
}

Length ratio(
    std::string_view text, Diagnostics& diagnostics, Location at, std::string_view context
) {
    const auto slash     = text.find('/');
    const auto numerator = integer(trim(text.substr(0, slash)));
    const auto denominator
        = slash == text.npos ? std::optional(1) : integer(trim(text.substr(slash + 1)));
    if (!numerator || *numerator < 0 || !denominator || *denominator <= 0) {
        diagnostics.error(
            at,
            "invalid-fraction",
            fmt::format("{} needs a nonnegative fraction, got '{}'", context, text)
        );
    }
    return {*numerator, *denominator};
}

f64 unit_length(std::string_view text, Diagnostics& diagnostics, Location at) {
    const auto value = ratio(text, diagnostics, at, "unit note length");
    if (value.numerator == 0)
        diagnostics.error(at, "invalid-unit-length", "L: needs a positive unit note length");
    return kWhole * value.fraction();
}

Meter meter(std::string_view text, Diagnostics& diagnostics, Location at) {
    if (text == "C") return {};
    if (text == "C|") return {2, 2};
    if (ascii_lower(text) == "none") return {4, 4, true};
    const auto written = meter_numbers(text);
    if (!written) diagnostics.error(at, "invalid-meter", "expected a meter such as 4/4, C or none");
    const auto denominator = written->denominator;
    if (denominator < 1 || denominator > 128
        || !std::has_single_bit(static_cast<u32>(denominator))) {
        diagnostics.error(
            at, "invalid-meter", "meter denominator must be a power of two between 1 and 128"
        );
    }
    Meter result{.numerator = 0, .denominator = denominator};
    for (const auto beats : written->groups) {
        if (beats < 1 || beats > 255 - result.numerator)
            diagnostics.error(at, "invalid-meter", "meter numerator must be between 1 and 255");
        if (result.numerator) result.group_starts.set(result.numerator);
        result.numerator += beats;
    }
    return result;
}

std::optional<KeyField> key(
    std::string_view text, const Key& previous, Diagnostics& diagnostics, Location at
) {
    KeyField result{previous, false, {}};
    auto     options         = trim(text);
    const auto [name, value] = option(options);
    // In first position, bare none is the key signature, not a clef change.
    if (ascii_lower(name) == "none"
        || !read_pitch_option(result.pitch, name, value, diagnostics, at)) {
        options = text;
        if (!parse_signature(options, result.state, diagnostics, at)) return {};
        result.sets_signature = true;
    }
    read_key_options(result, options, diagnostics, at);
    return result;
}

i32 tempo(const TempoNotation& written, f64 unit, Diagnostics& diagnostics, Location at) {
    if (!written.bpm) {
        if (const auto bpm = named_tempo(written.label)) return 60'000'000 / bpm;
        diagnostics.warn(
            at, "text-tempo", "tempo text has no recognized metronome value; kept the current tempo"
        );
        return 0;
    }
    const auto beat = written.beat * (written.unit_relative ? unit : kWhole);
    const auto usec = 60'000'000.0 * kTpq / (beat * *written.bpm);
    if (!(usec >= 1 && usec <= std::numeric_limits<i32>::max()))
        diagnostics.error(at, "tempo-range", "tempo is outside the score's range");
    return static_cast<i32>(usec);
}

VoiceSpec voice_spec(std::string_view text, Diagnostics& diagnostics, Location at) {
    VoiceSpec result{};
    result.id = word(text);
    if (result.id.empty()) diagnostics.error(at, "invalid-voice", "V: needs a voice identifier");
    while (!trim(text).empty()) {
        const auto [name, value] = option(text);
        if (name == "name" || name == "nm")
            result.name = value;
        else
            read_pitch_option(result.pitch, name, value, diagnostics, at);
    }
    return result;
}

std::string_view key_name(i32 fifths, bool minor) {
    // The tables run from seven flats to seven sharps.
    return (minor ? kMinorNames : kMajorNames).at(static_cast<size_t>(i64{fifths} + 7));
}

i32 tick(f64 time, Diagnostics& diagnostics, Location at) {
    const auto quantized = std::floor(time + kTickTolerance);
    if (!(quantized >= 0 && quantized <= std::numeric_limits<i32>::max())) {
        diagnostics.error(at, "time-range", "playback exceeds the score's tick range");
    }
    return static_cast<i32>(quantized);
}

}   // namespace symusic::abc::detail

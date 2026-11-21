#pragma once

#include <map>

#include "accompaniment.h"
#include "expression.h"
#include "midi_output.h"
#include "navigation.h"
#include "pitch_tuning.h"
#include "stress.h"
#include "symusic/score.h"

namespace symusic::abc::detail {

// The free-meter playback pulse and an additive meter's total both fit in u8.
// The score keeps only the numerator and denominator, as a MIDI file does.
inline TimeSignature<Tick> score_meter(const Meter& meter, i32 time) {
    return {time, static_cast<u8>(meter.numerator), static_cast<u8>(meter.denominator)};
}

// Opening values and tune-wide policies compiled before navigation. tempo is
// in microseconds per quarter note; unit is ticks per ABC unit note.
struct TuneSettings {
    Meter              meter{.free = true};
    Key                key;
    PitchTuning        tuning;
    AccidentalPolicy   accidentals;
    HarpCuts           harp_cuts;
    WrittenNoteLengths note_lengths;
    WrittenDynamics    dynamics;
    TuneStress         stress;
    ProgramNumbers     programs;
    i32                tempo              = 500000;
    f64                unit               = kWhole / 8.0;
    bool               separate_conductor = false;
    std::string        title;
};

// Sparse syllables keyed by the zero-based verse within this passage. Retain
// the full count so an empty verse takes its turn when the part repeats.
struct WrittenSyllables {
    size_t                              verse_count = 0;
    vec<std::pair<size_t, std::string>> by_verse;
};
using WrittenLyrics = vec<WrittenSyllables>;

template<class T>
using Located = std::pair<T, Location>;

// Rendered lane and backing tracks, plus metadata to merge across voices.
// Metadata times are ticks; the assembler applies any conductor-dependent
// tempo offset and resolves conflicting fields using their source locations.
struct Performance {
    vec<PlaybackPass>                 passes;
    vec<Located<Tempo<Tick>>>         tempos;
    vec<Located<TimeSignature<Tick>>> meters;
    vec<Located<KeySignature<Tick>>>  keys;
};

// The effect of a lane's performed MIDI commands on any incoming track settings.
class TrackSettingsChange {
public:
    void                            record(const MidiTrackCommand& command);
    [[nodiscard]] MidiTrackSettings closing(MidiTrackSettings opening) const;

private:
    i32                shift   = 0;
    i32                lowest  = -kMidiMax;
    i32                highest = kMidiMax;
    std::optional<i32> gap;
    std::optional<i32> random_limit;
};

struct LanePlan {
    Location            declaration;
    std::optional<i32>  declared_channel;
    BackingTracks       backing;
    i32                 chord_channels = 0;
    TrackSettingsChange settings_change;
    DrumMap             drum_changes;

    MidiTrackChannels channels;
    MidiTrackSettings note_settings;
    DrumMap           opening_drums;
    BackingOpening    backing_opening;
};

// The plan owns the written voice. It keeps written track declarations even
// when navigation skips them, but counts only performed MIDI requests.
struct VoicePlan {
    vec<Token>    source;
    PlayOrder     order;
    vec<LanePlan> lanes;
    bool          uses_conductor   = false;
    bool          expands_releases = false;
};

[[nodiscard]] VoicePlan plan_voice(
    vec<Token>&& written_voice, PlayOrder&& order, Location declaration, Diagnostics& diagnostics
);

// Assign channels and opening settings in MIDI track order, starting with the
// file's percussion map. Channel exhaustion is diagnosed at the performed request.
void assign_midi_tracks(std::span<VoicePlan> voices, bool separate_tracks, DrumMap file_drums);

// Consume a voice plan while rendering its navigation steps.
// Share chord_attacks across voices in one tune to keep random chord spread in
// track order. Diagnostics throws on fatal failures. Times use the 480-tick grid.
Performance perform(
    const TuneSettings&  settings,
    VoicePlan&&          plan,
    const WrittenLyrics& lyrics,
    std::string          name,
    ChordAttackSequence& chord_attacks,
    Diagnostics&         diagnostics
);

}   // namespace symusic::abc::detail

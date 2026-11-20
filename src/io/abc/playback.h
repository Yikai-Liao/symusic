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
    long double        unit               = whole / 8;
    ExpressionDialect  expression         = ExpressionDialect::legacy;
    bool               separate_conductor = false;
    std::string        title;
};

struct LyricVerse {
    Location    at;
    std::string text;
};
using LegacyLyrics = std::map<size_t, vec<LyricVerse>>;

// Sparse syllables keyed by the zero-based verse within this passage. Retain
// the full count so an empty verse takes its turn when the part repeats.
struct WrittenSyllables {
    size_t                              verse_count = 0;
    vec<std::pair<size_t, std::string>> by_verse;
};
using WrittenLyrics = vec<WrittenSyllables>;
using VoiceLyrics   = std::variant<LegacyLyrics, WrittenLyrics>;

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
    vec<TextMeta<Tick>>               markers;
};

struct LanePlan {
    Location              declaration;
    std::optional<int>    declared_channel;
    BackingTracks         backing;
    int                   chord_channels = 0;
    MidiTrackChannels     channels;
    vec<MidiTrackCommand> midi_commands;
    MidiTrackSettings     note_settings;
    BackingOpening        backing_opening;
};

// The plan borrows the written voice. It keeps written track declarations even
// when navigation skips them, but counts only performed MIDI requests.
struct VoicePlan {
    std::span<const Token> source;
    PlayOrder              order;
    vec<LanePlan>          lanes;
    bool                   uses_conductor   = false;
    bool                   expands_releases = false;
};

VoicePlan plan_voice(
    const vec<Token>& written_voice, PlayOrder order, Location declaration, Diagnostics& diagnostics
);

// Assign numbered channels and inherited MIDI settings in track-declaration order.
// The renderer reports exhausted channel requests at the executing command.
void assign_midi_tracks(std::span<VoicePlan> voices, bool separate_tracks);

// Replay only the drum routing of the primary voice, in its performed order.
// The conductor emits no attacks but leaves its percussion map for voice one.
void prime_drum_map(const PlayOrder& first_voice, DrumMap& drum_map, Diagnostics& diagnostics);

// Render a voice using its planned navigation steps.
// Share chord_attacks across voices in one tune to keep random chord spread in
// track order. Diagnostics throws on fatal failures. Times use the 480-tick grid.
Performance perform(
    const TuneSettings&  settings,
    const VoicePlan&     plan,
    const VoiceLyrics&   lyrics,
    std::string          name,
    ChordAttackSequence& chord_attacks,
    Diagnostics&         diagnostics
);

}   // namespace symusic::abc::detail

#pragma once

namespace symusic {
namespace docstring {

constexpr const char* kSynthDoc = R"pbdoc(
High-level Prestosynth wrapper that renders Scores with SF2/SF3 SoundFonts.

The synthesizer keeps the SoundFont in memory, so repeated calls to render()
avoid disk I/O. Construct one instance per SoundFont and reuse it across
multiple scores for best performance.
)pbdoc";

constexpr const char* kSynthInitPathDoc = R"pbdoc(
Create a synthesizer from a filesystem path to a SF2/SF3 SoundFont.

Parameters
----------
sf_path : pathlib.Path
    Path to the SoundFont archive on disk.
sample_rate : int
    Output sample rate in Hz (e.g. 44100).
quality : int
    Prestosynth quality preset, 0 = fast, higher = better interpolation.
)pbdoc";

constexpr const char* kSynthInitStringDoc = R"pbdoc(
Create a synthesizer from a string path to a SF2/SF3 SoundFont.

Parameters mirror the pathlib constructor overload.
)pbdoc";

constexpr const char* kSynthRenderDoc = R"pbdoc(
Render a Score into a float32 waveform.

Parameters
----------
score : Score
    Score in ticks, quarters, or seconds depending on the overload.
stereo : bool, default True
    When True, emit two channels; otherwise return a single channel.

Returns
-------
numpy.ndarray[float32]
    Array shaped (channels, samples) laid out in Fortran order so it can be
    passed back into Prestosynth without copies.
)pbdoc";

constexpr const char* kDumpWavDoc = R"pbdoc(
Write a NumPy buffer to a WAV file using Prestosynth's writer.

Parameters
----------
path : str
    Destination path on disk.
data : numpy.ndarray[float32]
    Fortran-contiguous array shaped (channels, samples).
sample_rate : int
    Sample rate stored in the WAV header.
use_int16 : bool, default True
    If True, quantize to int16 PCM; otherwise keep float32 samples.
)pbdoc";

constexpr const char* kScoreDoc = R"pbdoc(
Score represents a collection of tracks, metadata, and events tied to a ticks-per-quarter resolution.
Use it to inspect, transform, or serialize musical data within the Symusic ecosystem.
)pbdoc";

constexpr const char* kScoreMidiFileCtorDoc = R"pbdoc(
Load a MIDI file from disk into a Score. The path accepts strings or pathlib.Path objects.
)pbdoc";

constexpr const char* kScoreFromFileDoc = R"pbdoc(
Read a Score from disk by auto-detecting a MIDI or ABC file, or override the detection via the format argument.
)pbdoc";

constexpr const char* kScoreFromMidiDoc = R"pbdoc(
Parse raw MIDI bytes into a Score. Enable sanitize_data to reject malformed payloads.
)pbdoc";

constexpr const char* kScoreFromAbcDoc = R"pbdoc(
Parse an ABC notation string into a Score. Requires the SYMUSIC_ABC2MIDI tool in the environment.
)pbdoc";

constexpr const char* kScoreDumpMidiDoc = R"pbdoc(
Write the Score to a MIDI file using the current ticks-per-quarter resolution.
)pbdoc";

constexpr const char* kScoreDumpsMidiDoc = R"pbdoc(
Serialize the Score into MIDI bytes for memory-based workflows.
)pbdoc";

constexpr const char* kScoreDumpAbcDoc = R"pbdoc(
Dump the Score to an ABC file through midi2abc. The SYMUSIC_MIDI2ABC backend must be configured.
)pbdoc";

constexpr const char* kScoreDumpsAbcDoc = R"pbdoc(
Return the Score as an ABC notation string via temporary conversion through midi2abc.
)pbdoc";

constexpr const char* kScoreToDoc = R"pbdoc(
Convert the Score to another time unit (Tick, Quarter, or Second). The optional min_dur argument snaps
durations when changing resolution.
)pbdoc";

constexpr const char* kScoreResampleDoc = R"pbdoc(
Resample the Score to a new ticks-per-quarter resolution. Use min_dur to control rounding when reducing resolution.
)pbdoc";

constexpr const char* kScorePianorollDoc = R"pbdoc(
Export a Tick score as a NumPy pianoroll array. Modes control aggregation while pitch_range and encode_velocity adjust the layout.
)pbdoc";

constexpr const char* kTrackDoc = R"pbdoc(
Track gathers all per-channel events (notes, controls, pedals, pitch bends, and metadata) plus the MIDI program/is_drum flags so you can inspect or mutate a single instrument lane.
)pbdoc";

constexpr const char* kNoteDoc = R"pbdoc(
Note describes a pitched event with duration and velocity anchored at a specific tick, quarter or second time unit.
)pbdoc";

constexpr const char* kKeySignatureDoc = R"pbdoc(
KeySignature fixes the tonal center at its time stamp by exposing the key offset and tonality.
)pbdoc";

constexpr const char* kTimeSignatureDoc = R"pbdoc(
TimeSignature records meter changes via numerator and denominator linked to a precise time.
)pbdoc";

constexpr const char* kControlChangeDoc = R"pbdoc(
ControlChange represents a controller number/value pair emitted at a time stamp for modulation of synth parameters.
)pbdoc";

constexpr const char* kPedalDoc = R"pbdoc(
Pedal captures sustain or soft pedal events by pairing a start time with a duration.
)pbdoc";

constexpr const char* kTempoDoc = R"pbdoc(
Tempo stores the tempo either as milliseconds per quarter note or as QPM, along with conveniences for shifting between them.
)pbdoc";

constexpr const char* kPitchBendDoc = R"pbdoc(
PitchBend carries the continuous pitch modulation value attached to a time stamp.
)pbdoc";

constexpr const char* kTextMetaDoc = R"pbdoc(
TextMeta attaches arbitrary text annotations (markers, lyrics, comments) to the score timeline.
)pbdoc";

}  // namespace docstring
}  // namespace symusic

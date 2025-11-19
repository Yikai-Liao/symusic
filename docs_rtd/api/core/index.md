(core-low-level-api)=
# Low-Level symusic.core API

Only the canonical tick-based bindings are expanded below so the page stays readable. Quarter- and second-based variants are called out in each entry when they differ only by their time axis. Run `python docs_rtd/scripts/sync_core_docs.py` after editing the docstrings to refresh this content.

## Time Units

### symusic.core.Tick

Discrete tick-based time unit that mirrors the raw MIDI resolution.

### symusic.core.Quarter

Musical quarter-note time unit derived from the score's ticks-per-quarter setting.

### symusic.core.Second

Real-time second based time unit for wall-clock aligned workflows.

## Scores and Tracks

### symusic.core.ScoreTick

`ScoreTick(tpq: int = 480)`

Container for a full MIDI score consisting of named tracks, tempo/key/time metadata, and helper
methods for serialization and transformations. This variant stores timestamps as discrete MIDI ticks so round-trips remain lossless.

Parameters:
- **tpq** (*int*): Ticks-per-quarter resolution that defines how many integer steps make a quarter note (usually ``480`` or ``960``).

**Time semantics**
- Time values are signed 32-bit tick offsets derived from the stored ticks-per-quarter (``tpq``).
- Optimized for lossless MIDI editing or workflows that must match the original resolution.

Notes:
- Score objects expose Pythonic semantics such as slicing, repr/eq, and pickling.
- ``tpq`` travels with the tempo map so conversions remain lossless.

Examples:
```python
>>> from symusic import ScoreTick
>>> score = ScoreTick(960)
>>> score.dump_midi("example.mid")
```

**Other variants:** `symusic.core.ScoreQuarter`, `symusic.core.ScoreSecond` mirror this API on quarter-note or second timelines. Convert between them via :py:meth:`symusic.core.ScoreTick.to`.

### symusic.core.TrackTick

`TrackTick(name: str = "", program: int = 0, is_drum: bool = False)`

Container for per-channel events (notes, controllers, pedals, pitch bends, metadata) tied to a MIDI
program. Tracks emulate Python lists: copy, compare, repr, and pickling propagate through the shared
pointer. This variant stores timestamps as discrete MIDI ticks so round-trips remain lossless.

**Parameters**
name:
    Human-friendly identifier used when printing or exporting.
program:
    MIDI program number (0-127). Defaults to acoustic grand piano.
is_drum:
    Flag drum/percussion channels. Useful when writing to SMF channel 10.

**Time semantics**
- Time values are signed 32-bit tick offsets derived from the stored ticks-per-quarter (``tpq``).
- Optimized for lossless MIDI editing or workflows that must match the original resolution.
- Durations and controller timestamps share the same tick units.

**Examples**
```python
>>> from symusic import TrackTick, NoteTick
>>> track = TrackTick(name="Piano")
>>> track.notes.append(NoteTick(0, 120, 60, 90))
```

**Other variants:** `symusic.core.TrackQuarter`, `symusic.core.TrackSecond` expose identical per-track operations on alternate time bases.

## Event Primitives

### symusic.core.NoteTick

`NoteTick(time: int, duration: int, pitch: int, velocity: int = 0)`

Pitched event anchored to a tick timestamp with editable duration and velocity.

**Time semantics**
- Time values are signed 32-bit tick offsets derived from the stored ticks-per-quarter (``tpq``).
- Optimized for lossless MIDI editing or workflows that must match the original resolution.
- Durations use the same tick units.

**Other variants:** `symusic.core.NoteQuarter`, `symusic.core.NoteSecond` share the same API on alternate time units.

### symusic.core.ControlChangeTick

`ControlChangeTick(time: int, number: int, value: int)`

Standard MIDI controller event carrying a controller number/value pair on the tick timeline.

**Time semantics**
- Time values are signed 32-bit tick offsets derived from the stored ticks-per-quarter (``tpq``).
- Optimized for lossless MIDI editing or workflows that must match the original resolution.

**Other variants:** `symusic.core.ControlChangeQuarter`, `symusic.core.ControlChangeSecond` share the same API on alternate time units.

### symusic.core.TempoTick

`TempoTick(time: int, mspq: int)`

Tempo change stored as microseconds-per-quarter note at a tick timestamp. The ``qpm`` property mirrors beats-per-minute and mutating it updates ``mspq`` accordingly.

**Time semantics**
- Time values are signed 32-bit tick offsets derived from the stored ticks-per-quarter (``tpq``).
- Optimized for lossless MIDI editing or workflows that must match the original resolution.

**Other variants:** `symusic.core.TempoQuarter`, `symusic.core.TempoSecond` share the same API on alternate time units.

### symusic.core.PedalTick

`PedalTick(time: int, duration: int)`

Sustain or soft pedal gesture represented as a timed range on the tick timeline.

**Time semantics**
- Time values are signed 32-bit tick offsets derived from the stored ticks-per-quarter (``tpq``).
- Optimized for lossless MIDI editing or workflows that must match the original resolution.
- Durations use the same tick units.

**Other variants:** `symusic.core.PedalQuarter`, `symusic.core.PedalSecond` share the same API on alternate time units.

### symusic.core.PitchBendTick

`PitchBendTick(time: int, value: int)`

Pitch-bend wheel value applied on the tick timeline. The raw 14-bit value matches the MIDI encoding.

**Time semantics**
- Time values are signed 32-bit tick offsets derived from the stored ticks-per-quarter (``tpq``).
- Optimized for lossless MIDI editing or workflows that must match the original resolution.

**Other variants:** `symusic.core.PitchBendQuarter`, `symusic.core.PitchBendSecond` share the same API on alternate time units.

### symusic.core.KeySignatureTick

`KeySignatureTick(time: int, key: int, tonality: int)`

Key signature change scheduled on the tick timeline. ``key`` is the number of sharps/flats relative to C and ``tonality`` is ``0`` for major, ``1`` for minor.

**Time semantics**
- Time values are signed 32-bit tick offsets derived from the stored ticks-per-quarter (``tpq``).
- Optimized for lossless MIDI editing or workflows that must match the original resolution.

**Other variants:** `symusic.core.KeySignatureQuarter`, `symusic.core.KeySignatureSecond` share the same API on alternate time units.

### symusic.core.TimeSignatureTick

`TimeSignatureTick(time: int, numerator: int, denominator: int)`

Meter change storing beats-per-bar and subdivision (e.g., ``3/4``) on the tick timeline. The denominator must be a power of two because it represents note lengths.

**Time semantics**
- Time values are signed 32-bit tick offsets derived from the stored ticks-per-quarter (``tpq``).
- Optimized for lossless MIDI editing or workflows that must match the original resolution.

**Other variants:** `symusic.core.TimeSignatureQuarter`, `symusic.core.TimeSignatureSecond` share the same API on alternate time units.

### symusic.core.TextMetaTick

`TextMetaTick(time: int, text: str)`

Free-form metadata event used for lyrics, markers, or comments on the tick timeline.

**Time semantics**
- Time values are signed 32-bit tick offsets derived from the stored ticks-per-quarter (``tpq``).
- Optimized for lossless MIDI editing or workflows that must match the original resolution.

**Other variants:** `symusic.core.TextMetaQuarter`, `symusic.core.TextMetaSecond` share the same API on alternate time units.

## Utilities

### symusic.core.Synthesizer

`Synthesizer(sf_path: str | Path, sample_rate: int = 44100, quality: int = 0)`

Prestosynth-powered renderer that turns scores into audio buffers using SF2/SF3 soundfonts. Reuse
a single synthesizer per soundfont to avoid repeated disk I/O.

**Parameters**
sf_path:
    Filesystem path to an SF2/SF3 soundfont (either ``str`` or ``pathlib.Path``).
sample_rate:
    Output sample rate in Hz.
quality:
    Prestosynth render quality hint (``0`` = default).

**Examples**
>>> from symusic import Score, Synthesizer
>>> score = Score.from_file("example.mid")
>>> synth = Synthesizer("MuseScore_General.sf3", sample_rate=48000)
>>> buffer = synth.render(score, stereo=True)
>>> Synthesizer.dump_wav("example.wav", buffer, 48000, use_int16=True)


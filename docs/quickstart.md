# Quickstart

## Install

```bash
pip install symusic
```

Wheels are published for:

- CPython 3.10 and later on Linux (manylinux and musllinux, x86_64 and aarch64), macOS 13 or later (x86_64 and arm64) and Windows (x64, x86 and ARM64). CPython 3.10 and 3.11 get one wheel each; from 3.12 on, one stable-ABI (`abi3`) wheel serves every later version.
- PyPy 3.11 on manylinux x86_64 and macOS.

Free-threaded CPython builds (`3.13t`, `3.14t`) are not supported. Elsewhere pip builds from the source distribution, which needs a C++20 compiler; pip fetches CMake and Ninja if they are missing. {doc}`CONTRIBUTING` describes building from a clone.

## Load a score

```python
from symusic import Score

score = Score("example.mid")
abc = Score("example.abc")
```

The format comes from the extension: `.mid`, `.midi`, `.MID` and `.MIDI` load as MIDI, `.abc` as ABC, and any other extension raises `ValueError`. Pass `fmt="midi"` or `fmt="abc"` to override it. `Score.from_midi(data)` and `Score.from_abc(text)` load from bytes and strings.

A score loads in ticks unless you pass `ttype`, and `to` converts it afterwards:

```python
score_quarter = Score("example.mid", ttype="quarter")
score_second = score.to("second")
score_tick = score_second.to("tick", min_dur=1)
```

`to` returns a converted copy. Converting to ticks rounds times and durations; note and pedal durations shorter than `min_dur` are raised to it, so short notes do not round to zero. {doc}`concepts/time_units` covers the units and their rounding.

## Inspect the hierarchy

```python
print(score)
print(score.ticks_per_quarter, len(score.tracks))

track = score.tracks[0]
print(track.name, track.program, track.is_drum)
print(len(track.notes), track.start(), track.end())
```

A `Score` holds the global `tempos`, `time_signatures`, `key_signatures` and `markers` lists and a list of `Track` objects. A `Track` holds `notes`, `controls`, `pitch_bends`, `pedals` and `lyrics`. The event lists behave like Python lists and add `sort`, `filter`, `adjust_time`, `copy` and `numpy`. {doc}`concepts/score`, {doc}`concepts/track` and {doc}`concepts/events` describe each type.

## Transform

```python
shifted = score.shift_pitch(2)
score.shift_time(480, inplace=True)
```

Transformations return a new object by default; `inplace=True` modifies the receiver instead.

## Beats and downbeats

```python
beats = score_quarter.get_beats()
downbeats = score_quarter.get_downbeats()
```

Both return NumPy arrays in the score's time unit, derived from the time signatures. Compound meters such as 6/8 count one beat per three eighth notes, following pretty_midi.

## NumPy export

```python
from symusic import Note

arrays = score.tracks[0].notes.numpy()
print(arrays.keys())  # time, duration, pitch, velocity

notes = Note.from_numpy(**arrays, ttype=score.ttype)
assert notes == score.tracks[0].notes
```

`numpy()` returns one array per field, and `from_numpy` builds the list back. Every event type supports both.

## Piano rolls

```python
small = score.resample(tpq=6, min_dur=1)
score_roll = small.pianoroll(
    modes=["onset", "frame"], pitch_range=(0, 128), encode_velocity=True
)
track_roll = small.tracks[0].pianoroll(
    modes=["onset", "frame", "offset"], pitch_range=(24, 108)
)
print(score_roll.shape)  # (modes, tracks, pitches, time)
print(track_roll.shape)  # (modes, pitches, time)
```

The time axis has one column per tick, so resample to a coarse resolution first. `pitch_range` is half-open, and `encode_velocity=True` stores velocities instead of ones. {doc}`tutorials/pianoroll` has the details.

## Save

```python
score.dump_midi("out.mid")
score.dump_abc("out.abc")
```

`dump_midi` and `dump_abc` accept all three time units. For ABC, pass `warn=True` to report lossy conversions. See {doc}`concepts/abc` for the timing and diagnostic rules.

## Pickle

Scores, tracks, events and event lists support `pickle`. A pickle stores the binary representation, so it round-trips exactly, and it is how scores travel to worker processes:

```python
import pickle

restored = pickle.loads(pickle.dumps(score))
assert restored == score
```

{doc}`tutorials/data_processing` shows a multiprocessing pipeline.

## Synthesize

```python
from symusic import Synthesizer, dump_wav

synth = Synthesizer(sample_rate=44100)
audio = synth.render(score, stereo=True)  # float32, (channels, samples)
dump_wav("out.wav", audio, sample_rate=44100, use_int16=True)
```

Without `sf_path`, `Synthesizer` downloads the MuseScore General SoundFont (about 38 MB) on first use. {doc}`tutorials/synthesis` covers SoundFonts and rendering.

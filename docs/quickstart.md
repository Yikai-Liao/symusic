# Quickstart

This guide walks through the most common Python workflow: install Symusic, load a MIDI file, inspect
and transform notes, and export the results. Every example runs on top of the same C++20 core that
powers the library in production, so the snippets scale from a notebook to a training pipeline.

## Install Symusic

### Pre-built wheels

```bash
pip install symusic
```

We publish wheels for Linux, macOS, and Windows across both x86 and ARM (Windows ARM64 is pending).
CPython 3.8–3.12 and PyPy 3.9 builds are uploaded for each release. CPython 3.12 still triggers
harmless nanobind leak warnings; if they become distracting, use 3.11 until upstream fixes land.

### Build from source

```bash
pip install --no-binary symusic symusic  # from PyPI source distribution
```

or clone the repository if you need the latest commit:

```bash
git clone --recursive https://github.com/Yikai-Liao/symusic
cd symusic
pip install -e .
```

Symusic targets C++20. Minimum compiler versions: GCC 11, Clang 15, MSVC 2022. On Linux machines
without sudo you can install the toolchain via conda and wire it in with `CC=... CXX=...`.

## Load a score

```python
from symusic import Score

score = Score("example.mid")         # format auto-detected
score_abc = Score("example.abc", fmt="abc")
```

Passing a string or `Path` dispatches to `Score.from_file`. To work in a specific time unit, use the
`ttype` argument or the convenience constructor names:

```python
from symusic import Score, TimeUnit

score_tick = Score("example.mid", ttype="tick")
score_quarter = Score("example.mid", ttype=TimeUnit.quarter)
score_second = score_tick.to("second")          # convert after loading
```

- `tick` retains the raw MIDI ticks (int32).
- `quarter` normalizes time so that a quarter note equals `1.0`.
- `second` evaluates the tempo map and produces wall-clock seconds.

The `Score.to(new_unit, min_dur=None)` method converts the entire hierarchy—tracks, notes, pedals,
etc.—to a different unit. Use `min_dur` when converting to ticks to avoid rounding durations to zero.

## Save back to disk

```python
score = Score("example.mid")
score.shift_pitch_inplace(1)
score.dump_midi("example_shifted.mid")
score.dump_abc("example.txt")
```

`dump_midi` internally works in ticks, so a score in quarters/seconds will be converted before
writing. For ABC export, set `fmt="abc"` when loading so the parser sees the correct metadata.

## Pickling and multiprocessing

Every container in Symusic is pickleable. We serialize through `zpp_bits`, which makes pickling far
faster than writing out intermediate MIDI files.

```python
import multiprocessing as mp
import pickle
from symusic import Score

score = Score("example.mid")
with open("score.pkl", "wb") as fh:
    pickle.dump(score, fh)

with mp.Pool(4) as pool:
    doubled = pool.map(lambda s: s.shift_pitch(12), [score] * 4)
```

## Inspect the score hierarchy

```python
from symusic import Score

score = Score("example.mid")
print(score.ticks_per_quarter)
print(len(score.tracks))
first_track = score.tracks[0]
print(first_track.name, first_track.program, "drum" if first_track.is_drum else "melodic")
print(len(first_track.notes), len(first_track.controls))
```

`Score` owns lists of global events (`tempos`, `time_signatures`, `key_signatures`, `markers`) as well
as a list of `Track` objects. Each `Track` contains lists (`NoteList`, `ControlChangeList`,
`PitchBendList`, `PedalList`, `TextMetaList`) that expose list-like APIs plus helpers such as
`.start()`, `.end()`, `.empty()`, `.copy()`, `.sort()`, and `.filter()`.

## Structured NumPy (SoA) exports

Event lists can produce a *struct of arrays* view that plays nicely with NumPy or PyTorch:

```python
from typing import Dict
import numpy as np
from symusic import Score, Note

score = Score("example.mid")
notes: Dict[str, np.ndarray] = score.tracks[0].notes.numpy()
print(notes.keys())  # dict of time, duration, pitch, velocity arrays

reconstructed = Note.from_numpy(**notes, ttype=score.ttype)
assert reconstructed == score.tracks[0].notes
```

This pattern avoids Python loops when batching features for machine-learning pipelines.

## Piano roll conversion

```python
from symusic import Score

score = Score("example.mid").resample(tpq=6, min_dur=1)
roll = score.pianoroll(modes=["onset", "frame"], pitch_range=[0, 128], encode_velocity=True)
track_roll = score.tracks[0].pianoroll(modes=["onset", "frame", "offset"], pitch_range=[24, 108])
```

- Use `Score.resample(tpq, min_dur)` before converting to limit the time axis.
- `modes` chooses which binary/velocity channels to emit.
- `pitch_range` controls the pitch axis `[low, high)`.

Visualization example:

```python
from matplotlib import pyplot as plt
plt.imshow(track_roll[0] + track_roll[1], aspect="auto")
plt.show()
```

## Synthesis with SoundFonts

```python
from symusic import Score, Synthesizer, BuiltInSF3, dump_wav

score = Score("example.mid")
sf_path = BuiltInSF3.MuseScoreGeneral().path(download=True)
synth = Synthesizer(sf_path=sf_path, sample_rate=44100, quality=0)
wave = synth.render(score, stereo=True)  # returns np.ndarray[channel, samples]
dump_wav("example.wav", wave, sample_rate=44100, use_int16=True)
```

Symusic ships a lightweight SoundFont manager (`BuiltInSF2/SF3`) that can download curated fonts on
demand. Rendering internally converts the score to seconds, maps tracks to instruments via `program`
and `is_drum`, and streams the samples through Prestosynth. Use the returned NumPy array directly in
post-processing pipelines or dump a WAV with `dump_wav`.
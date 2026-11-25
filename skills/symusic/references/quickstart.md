# Quickstart

## Install

```bash
pip install symusic
```

Wheels are published for:

- CPython 3.10 and later on Linux (manylinux and musllinux, x86_64 and aarch64), macOS 13 or later (x86_64 and arm64) and Windows (x64, x86 and ARM64). CPython 3.10 and 3.11 get one wheel each; from 3.12 on, one stable-ABI (`abi3`) wheel serves every later version.
- PyPy 3.11 on manylinux x86_64 and macOS.

Free-threaded CPython builds (`3.13t`, `3.14t`) are not supported. Elsewhere pip builds from the source distribution, which needs a C++20 compiler; pip fetches CMake and Ninja if they are missing.

## Tour

Load a MIDI or ABC file, in ticks by default or in another unit with `ttype`. [files.md](files.md) covers the formats and [time_units.md](time_units.md) the units:

```python
from symusic import Score

score = Score("song.mid")
tune = Score("tune.abc")
in_quarters = Score("song.mid", ttype="quarter")
in_seconds = score.to("second")
```

A score holds global tempos, signatures and markers and a list of tracks; each track holds its notes, controls, pitch bends, pedals and lyrics. [data_model.md](data_model.md) describes every type:

```python
print(score)
track = next(track for track in score.tracks if track.notes)
print(track.name, track.program, track.is_drum, len(track.notes))
note = track.notes[0]
print(note.time, note.duration, note.pitch, note.velocity)
```

Transformations return a new object unless you pass `inplace=True`:

```python
higher = score.shift_pitch(2)
score.shift_time(480, inplace=True)
```

Event lists convert to NumPy arrays, and tick scores to piano rolls; see [arrays.md](arrays.md):

```python
track = next(track for track in score.tracks if track.notes)
arrays = track.notes.numpy()  # time, duration, pitch, velocity
roll = score.resample(tpq=6, min_dur=1).pianoroll(modes=["frame"])
```

Write MIDI or ABC, or pickle the score:

```python
import pickle

score.dump_midi("out.mid")
score.dump_abc("out.abc")
restored = pickle.loads(pickle.dumps(score))
```

Render audio with a SoundFont; see [synthesis.md](synthesis.md):

<!-- docs-test: skip, downloads a SoundFont -->
```python
from symusic import BuiltInSF3, Synthesizer

soundfont = BuiltInSF3.musescore_general().download()
audio = Synthesizer(soundfont).render(score)
```

[datasets.md](datasets.md) puts these together into a dataset pipeline, and [building_scores.md](building_scores.md) creates scores from model output.

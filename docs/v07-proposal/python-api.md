# Python API Design

Every change in [RFC #136](https://github.com/Yikai-Liao/symusic/issues/136) removes binding leaks, resolves mutability conflicts, and aligns with Python typing (`mypy`, `pyright`, `ty`).

## 1. Unified Surface, Time Units & Containers

| Dimension | 0.6 (Removed) | 0.7 |
|---|---|---|
| Public surface | `symusic.core`, `symusic.factory`, `symusic.types`, `symusic.soundfont` | Top-level `symusic` only; `symusic._core` & `symusic._soundfont` are private (`python -m symusic.skill` kept) |
| Time units | Free-form `"tick"` / `"second"`, `TimeUnit.tick` | Non-instantiable markers `Tick`, `Quarter`, `Second` (`TimeUnit = Tick \| Quarter \| Second`); `Score[Second]()` raises `TypeError` |
| Containers | `NoteTickList`, `TempoTickList`, `TextMetaTickList`, … | Single generic `EventList[Note[Tick]]` + `TrackList[Tick]`; strict `MutableSequence` (never equates to plain `list` — use `not notes` or `list(notes) == expected`) |
| Events & audio | `TextMeta`, `Soundfont`, `BuiltInSF2`/`BuiltInSF3`, `dump_wav()` | `TextEvent`, `Synthesizer`, and `download_soundfont(name="musescore_general_sf3", *, cache_dir=None, force=False) -> Path` |

## 2. Strict CQS Mutability & Ownership

No `inplace=` flags anywhere. Operations split cleanly by whether they keep all elements or drop/convert them:

| Category | Return | Operations | Contract |
|---|---|---|---|
| **In-place mutators** | `None` | `sort()`, `shift_time()`, `shift_pitch()`, `shift_velocity()` | Keep all elements (rewrite order/values in place); `Score.sort()`/`Track.sort()` sort all event lists while preserving `Score.tracks` order |
| **Returning transforms** | New object | `filtered()`, `clip()`, `trim()`, `adjust_time()`, `copy()`, `to()`, `resample()` | Select/drop elements or convert representation; source untouched (`notes.filter(f)` → `notes.filtered(f)`; use `notes[:] = [n for n in notes if f(n)]` to keep aliases) |
| **Copy & references** | Deep / Shallow | `.copy()` (deep, contiguous), `copy.copy(x)` (shallow) | Copy constructors (`Score(other)`, `Note(other)`) removed. `.copy()` compacts via `pyvec::compact()` into a single contiguous chunk (Python omits a separate `contiguous()` API) |

### Uniform Two-Level Reference Model

| Level | Assignment / Insertion | Semantics |
|---|---|---|
| **Container (heterogeneous)** | `track.notes = [n1, n2]`, `score.tracks = [t1]`, passing a Python `list` to `Track(...)`/`Score(...)` | Creates a **new** `EventList`/`TrackList` and rebinds the attribute (detached from the Python `list` and prior `old = track.notes` aliases; use `track.notes.clear()` or `track.notes[:] = [...]` to mutate through aliases) |
| **Container (homogeneous)** | `track.notes = existing_event_list` (e.g. `EventList.from_numpy(...)`, `other.notes`) | **Shares the container reference with zero copy** (`track.notes += [n]` naturally preserves aliases; call `.copy()` for an independent, contiguous list) |
| **Element (homogeneous)** | `append`, `insert`, `setitem`, `track.notes = [n1, n2]`, `score.tracks = [t1]` | **References each `Note`/`Track`'s underlying `shared_ptr` directly without copying** — consistent with indexing (`track.notes[0]`) and slicing (`track.notes[:]`) |

## 3. CC64 Sustain Pedals (Fixes #5)

Control change 64 in `track.controls` is now the single source of truth for sustain:

```text
track.controls (CC64 >= 64 press / < 64 release)
  ├──▶ track.pedals()  ──▶ tuple[Pedal, ...]  (derived on demand, closed pairs)
  └──▶ to_midi() & FluidSynth                 (round-trips & renders natively)
```

- Order is preserved on equal timestamps, repeated presses merge, and unclosed presses are omitted.
- Watch out: `clip()` and `trim()` filter controls by timestamp, so sustain regions crossing a window edge can lose their press or release event.

## 4. Before / After Workflows

### Loading, Strict Parsing & Single-Step Resampling

**0.7 (After)**

```python
from symusic import Score, Second

score = Score.from_file("piece.mid")
dirty = Score.from_file("raw.mid", format="midi", strict=False)

sec: Score[Second] = score.to(Second)
resampled = sec.resample(480, min_duration=1)  # single-step quantization

if not resampled.is_empty():
    beats = resampled.beats(start=0)
    print(resampled.ticks_per_quarter, resampled.note_count())
    resampled.to_file("out.mid")
```

**0.6 (Before)**

```python
from symusic import Score, TimeUnit

score = Score("piece.mid")
dirty = Score.from_file("raw.mid", fmt="midi", sanitize_data=True)

sec = Score.from_other(score, TimeUnit.second)
resampled = sec.resample(tpq=480, min_dur=1)

if not resampled.empty():
    beats = resampled.get_beats(start_time=0)
    print(resampled.tpq, resampled.note_num())
    resampled.dump_midi("out.mid")
```

### Building a Score & Typing

**0.7 (After)**

```python
from symusic import (
    EventList, KeySignature, Note, Score, Second, Tempo, TextEvent, Tick, Track
)

# Small Tick lists: pass a plain list directly (no redundant EventList / ttype=Tick)
score: Score[Tick] = Score(
    ticks_per_quarter=960,
    tempos=[Tempo(0, 120.0)],
    key_signatures=[KeySignature(0, 0, minor=False)],
    markers=[TextEvent(0, "Intro")],
    tracks=[
        Track("Piano", notes=[
            Note(0, 480, 60),       # velocity defaults to 64 (MIDI 1.0 0x40)
            Note(480, 480, 64, 80),
        ])
    ],
)

# Bulk or non-Tick construction: specify Note and ttype once via from_numpy
sec_notes: EventList[Note[Second]] = EventList.from_numpy(
    Note,
    time=[0.0, 0.5],
    duration=[0.5, 0.5],
    pitch=[60, 64],
    velocity=[64, 80],
    ttype=Second,
)
```

**0.6 (Before)**

```python
from symusic import Score, Track, Note, Tempo, KeySignature, TextMeta
from symusic.core import NoteTickList

score = Score.from_tpq(960)
score.tempos.append(Tempo(0, mspq=500_000))
score.key_signatures.append(KeySignature(0, 0, 0))
score.markers.append(TextMeta(0, "Intro"))

notes = NoteTickList([Note(0, 480, 60, 64), Note(480, 480, 64, 80)])
track = Track.empty("tick")
track.notes = notes
score.tracks.append(track)
```

### NumPy SoA Round-Trip & `uint8` Safety

**0.7 (After)**

```python
import numpy as np
from symusic import EventList, Note, Tick

cols = track.notes.to_numpy()  # TypedDict; pitch & velocity are uint8
intervals = np.diff(cols["pitch"].astype(np.int16))  # avoid uint8 wrap-around

us_per_q = score.tempos.to_numpy()["microseconds_per_quarter"]
rebuilt: EventList[Note[Tick]] = EventList.from_numpy(Note, **cols, ttype=Tick)
```

**0.6 (Before)**

```python
import numpy as np
from symusic import Note

arr = track.notes.numpy()
intervals = np.diff(arr["pitch"])

mspq = score.tempos.numpy()["mspq"]
rebuilt = Note.from_numpy(arr["time"], arr["duration"], arr["pitch"], arr["velocity"])
```

### FluidSynth & `download_soundfont()`

**0.7 (After)**

```python
import numpy as np
from scipy.io import wavfile
from symusic import Score, Synthesizer, download_soundfont

score = Score.from_file("piece.mid")
# Idempotent cached + SHA-384 verified SoundFont preset (default: "musescore_general_sf3")
synth = Synthesizer(download_soundfont(), sample_rate=44100)

audio = synth.render(score, stereo=True)  # float32 (frames, channels); mono (frames, 1)
pcm16 = (np.clip(audio, -1.0, 1.0) * 32767).astype(np.int16)
wavfile.write("out.wav", 44100, pcm16)
```

**0.6 (Before)**

```python
import symusic
from symusic import BuiltInSF3, Score, Synthesizer

score = Score("piece.mid")
sf_path = BuiltInSF3.MuseScoreGeneral().path(download=True)
synth = Synthesizer(sf_path=sf_path, sample_rate=44100, quality=4)

audio = synth.render(score, True)  # (channels, samples)
symusic.dump_wav("out.wav", audio, 44100)
```

## 5. Validation, Exceptions & Data Model

| Area | 0.7 Contract |
|---|---|
| Exceptions | `FileNotFoundError` / `OSError` for I/O; `TypeError` for wrong class / `ttype` / text stream / non-numeric `to_numpy()`; `ValueError` for ranges, unit mismatch, or malformed MIDI/ABC |
| Strict MIDI & ABC | Rejects MIDI data bytes `> 127` by default (`strict=False` clamps); ABC tool warnings emit public `ABCConversionWarning` |
| Binary streams | Require explicit `format`, remain caller-closed, handle short writes, typed with structural protocols |
| Python data model | `__eq__` returns `NotImplemented` for unknown types; `Tempo.__match_args__ == ("time", "qpm")`; escaped round-trippable `repr()`; `pianoroll()` is `Tick`-only |
| Pickles | Versioned `SYMZ` v2 header for inter-process transport only; rejects incompatible 0.6 payloads |

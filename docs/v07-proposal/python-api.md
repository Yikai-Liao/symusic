# Python API Design

Every change in [RFC #136](https://github.com/Yikai-Liao/symusic/issues/136) removes binding leaks, resolves mutability conflicts, and aligns with Python typing (`mypy`, `pyright`, `ty`).

## 1. Unified Surface, Time Units & Containers

| Dimension | 0.6 (Removed) | 0.7 |
|---|---|---|
| Public surface | `symusic.core`, `symusic.factory`, `symusic.types`, `symusic.soundfont` | Top-level `symusic` only; `symusic._core` & `symusic._soundfont` are private (`python -m symusic.skill` kept) |
| Time units | Free-form `"tick"` / `"second"`, `TimeUnit.tick` | `ttype` accepts non-instantiable marker **class objects** (`type[Tick] \| type[Quarter] \| type[Second]`); `Score[Second]()` raises `TypeError` |
| Containers | `NoteTickList`, `TempoTickList`, `TextMetaTickList`, … | Single generic `EventList[Note[Tick]]` + `TrackList[Tick]`; strict `MutableSequence` (never equates to plain `list` — use `not notes` or `list(notes) == expected`) |
| Events & audio | `TextMeta`, `Soundfont`, `BuiltInSF2`/`BuiltInSF3`, `dump_wav()` | `TextEvent`, `Synthesizer`, and `download_soundfont(name="musescore_general_sf3", *, cache_dir=None, force=False) -> Path` |

## 2. Strict CQS Mutability, Copy & Reference Semantics

No `inplace=` flags anywhere. Operations are grouped explicitly by receiver mutation and return value:

| Category | Return | Operations | Contract |
|---|---|---|---|
| **In-place value/order mutators** | `None` | `sort()`, `shift_time()`, `shift_pitch()`, `shift_velocity()` | Mutate the receiver in place. `Score.sort()`/`Track.sort()` sort all contained event lists while keeping `Score.tracks` in order. `shift_*` shifts every reachable event (`Note`, `ControlChange`, `Tempo`, `TextEvent`, etc.) **once per call** with atomic rollback on overflow |
| **Returning transforms** | New object | `filtered()`, `clip()`, `trim()`, `adjust_time()`, `to()`, `resample()` | Leave the source unchanged and return a new independent object (`notes.filter(f)` → `notes.filtered(f)`; use `notes[:] = [n for n in notes if f(n)]` to mutate through aliases; `MutableSequence` operations like `del notes[i]` still mutate in place per Python stdlib) |

### Explicit Copy, Deepcopy & Reference Model

Copy constructors (`Score(other)`, `Note(other)`) are removed. Copy and assignment semantics follow two crisp tables:

| Copy Operation | Semantics |
|---|---|
| `x.copy()` | **Independent value snapshot** — produces an independent copy and intentionally detaches internal duplicate aliases (e.g. `[n, n]`) and cross-track shared lists |
| `copy.copy(x)` | **Shallow structural copy** — new outer container/object sharing element and track references |
| `copy.deepcopy(x)` | **Memo-preserving deep copy** — deep-copies the graph while preserving internal aliases inside the copied graph per Python stdlib `copy` |

| Input Kind | Assignment / Insertion | Semantics |
|---|---|---|
| **Plain Python `list` / iterable** | `track.notes = [n1, n2]`, `score.tracks = [t1]`, passing a `list` to `Track(...)`/`Score(...)` | Creates a **new** `EventList`/`TrackList` and rebinds the attribute (detached from the Python `list` and any prior `old = track.notes` alias; use `track.notes.clear()` or `track.notes[:] = [...]` to mutate through aliases) |
| **Existing `EventList` / `TrackList`** | `track.notes = existing_list` (e.g. `EventList.from_numpy(...)`, `other.notes`) | **Shares the container reference with zero copy** (`track.notes += [n]` naturally preserves aliases; call `.copy()` for an independent snapshot) |
| **Individual `Note` / `Track` objects** | `append`, `insert`, `setitem`, `track.notes = [n1, n2]`, `score.tracks = [t1]` | **References each object's underlying handle directly without copying** — consistent with indexing (`track.notes[0]`) and slicing (`track.notes[:]`) |

## 3. CC64 Sustain Pedals (Fixes #5)

Control change 64 in `track.controls` is now the single source of truth for sustain:

```text
track.controls (CC64 >= 64 press / < 64 release)
  ├──▶ track.pedals()  ──▶ tuple[Pedal, ...]  (derived on demand, closed pairs)
  └──▶ to_midi() & FluidSynth                 (round-trips & renders natively)
```

- Order is preserved on equal timestamps, repeated presses merge, and unclosed presses are omitted.
- `clip()` and `trim()` perform raw timestamp filtering on controls, so sustain regions crossing a window boundary can lose their press or release event (a future playable-excerpt helper can synthesize active CC64 / pitch-bend / tempo state at window start).

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

# to_numpy() returns independent copy snapshots (pitch & velocity are uint8)
cols = track.notes.to_numpy()
intervals = np.diff(cols["pitch"].astype(np.int16))  # avoid uint8 wrap-around

us_per_q = score.tempos.to_numpy()["microseconds_per_quarter"]
# from_numpy() accepts 1D ArrayLike / ndarray and validates ranges before narrowing
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

## 5. Validation, Exceptions, Concurrency & Data Model

| Area | 0.7 Contract |
|---|---|
| Exceptions | `FileNotFoundError` / `OSError` for I/O; `TypeError` for wrong class / `ttype` / text stream / non-numeric `to_numpy()`; `ValueError` for ranges, unit mismatch, or malformed MIDI/ABC |
| Numeric domains & MIDI wire | In-memory fields validate musical ranges (`0..127`, `ticks_per_quarter >= 1`); MIDI export additionally enforces SMF limits (`ticks_per_quarter` in `1..32767`; `Note(..., velocity=0)` encodes as MIDI NoteOff on export) |
| Strict MIDI & ABC | Rejects 7-bit MIDI channel data bytes `> 127` by default (`strict=False` clamps channel data bytes only); ABC converter warnings emit public `ABCConversionWarning` |
| Binary streams | Require explicit `format`, remain caller-closed, handle short writes, typed with structural protocols |
| Concurrency & reentrancy | Targets standard GIL CPython (free-threaded `3.13t`/`3.14t` deferred until per-container locking); `sort(key=...)` and Python callbacks enforce a reentrancy guard (`ValueError` on structural mutation during callback) |
| Python data model | `__eq__` returns `NotImplemented` for unknown types; `Tempo.__match_args__ == ("time", "qpm")`; escaped round-trippable `repr()`; `pianoroll()` is `Tick`-only |
| Pickles (`SYMZ v3`) | Private Python little-endian codec carrying a versioned **`SYMZ v3`** header with full `__setstate__` validation (rejecting NaN / out-of-range fields and incompatible 0.6 payloads); for inter-process transport only |

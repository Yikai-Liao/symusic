# Python API Design

Every change in [RFC #136](https://github.com/Yikai-Liao/symusic/issues/136) removes binding leaks, resolves mutability conflicts, and aligns with Python typing (`mypy`, `pyright`, `ty`).

## 1. Unified Surface, Time Units & Containers

| Dimension | 0.6 (Removed) | 0.7 |
|---|---|---|
| Public surface | `symusic.core`, `symusic.factory`, `symusic.types`, `symusic.soundfont` | Top-level `symusic` only; `symusic._core` & `symusic._soundfont` are private (`python -m symusic.skill` kept) |
| Time units | Free-form `"tick"` / `"second"`, `TimeUnit.tick` | `ttype` accepts non-instantiable marker **class objects** (`type[Tick] \| type[Quarter] \| type[Second]`); `Score[Second]()` raises `TypeError` |
| Containers | `NoteTickList`, `TempoTickList`, `TextMetaTickList`, … | Single generic `EventList[Note[Tick]]` + `TrackList[Tick]`; strict `MutableSequence` (never equates to plain `list` — use `not notes` or `list(notes) == expected`) |
| Events & audio | `TextMeta`, `Soundfont`, `BuiltInSF2`/`BuiltInSF3`, `dump_wav()` | `TextEvent`, `Synthesizer`, `write_wav()` (zero-dep WAV; use `soundfile` for multi-format), and `download_soundfont(name="musescore_general_sf3", *, cache_dir=None, force=False) -> Path` |

## 2. Strict CQS Mutability, Copy & Reference Semantics

No `inplace=` flags anywhere. Operations are grouped explicitly by receiver mutation and return value:

| Category | Return | Operations | Contract |
|---|---|---|---|
| **In-place value/order mutators** | `None` | `sort()`, `shift_time()`, `shift_pitch()`, `shift_velocity()` | Mutate the receiver in place. `sort()` is **stable**: events with equal keys keep their relative order, as `list.sort` does, so equal-time tempos and CC64 events keep their playback order. `Score.sort()`/`Track.sort()` sort all contained event lists while keeping `Score.tracks` in order. `shift_*` shifts every reachable event (`Note`, `ControlChange`, `Tempo`, `TextEvent`, etc.) **once per call** with atomic rollback on overflow |
| **Returning transforms** | New object | `filtered()`, `clip()`, `adjust_time()`, `to()`, `resample()` | Leave the source unchanged and return a new object. `filtered(f)` returns a new list holding the **same** elements that `f` accepts, like `[n for n in notes if f(n)]`, so `notes.filtered(f)[0] is notes[0]` when the first note passes. Use `notes[:] = notes.filtered(f)` to mutate through aliases; `MutableSequence` operations like `del notes[i]` still mutate in place per Python stdlib |

### Explicit Copy, Deepcopy & Reference Model

Copy constructors (`Score(other)`, `Note(other)`) are removed. Copies follow the Python standard library: a list copy is shallow, as `list.copy()` is, and an object copy is deep, as `copy.deepcopy` is. This shallow/deep and alias-preserving matrix is a **Python host-protocol contract** (matching `MutableSequence` and stdlib `copy`); `ValueStorage` bindings (C++, C ABI, Go, WASM) share CQS and deep domain-object cloning without shallow container sharing or alias-preserving memo graphs.

| Copy Operation | Semantics |
|---|---|
| `notes.copy()`, `notes[:]`, `copy.copy(notes)` (`EventList` / `TrackList`) | **Shallow** — a new list holding the same element objects |
| `score.copy()`, `track.copy()`, `note.copy()` | **Deep** — identical to `copy.deepcopy(x)` |
| `copy.copy(score)`, `copy.copy(track)` | **Shallow** — a new outer object sharing its lists and tracks |
| `copy.deepcopy(x)` | **Deep, alias-preserving** — copies the whole graph and keeps aliases inside it (`[n, n]` copies to `[m, m]`), per Python stdlib `copy` |

| Input Kind | Assignment / Insertion | Semantics |
|---|---|---|
| **Plain Python `list` / iterable** | `track.notes = [n1, n2]`, `score.tracks = [t1]`, passing a `list` to `Track(...)`/`Score(...)` | Creates a **new** `EventList`/`TrackList` and rebinds the attribute (detached from the Python `list` and any prior `old = track.notes` alias; use `track.notes.clear()` or `track.notes[:] = [...]` to mutate through aliases) |
| **Existing `EventList` / `TrackList`** | `track.notes = existing_list` (e.g. `EventList.from_numpy(...)`, `other.notes`) | **Shares the container reference with zero copy** (`track.notes += [n]` naturally preserves aliases; `.copy()` gives a new list of the same elements, and `copy.deepcopy()` copies the elements too) |
| **Individual `Note` / `Track` objects** | `append`, `insert`, `setitem`, `track.notes = [n1, n2]`, `score.tracks = [t1]` | **References each object's underlying handle directly without copying** — consistent with indexing (`track.notes[0]`) and slicing (`track.notes[:]`) |

### Clipping a Window

`clip(start, end, /, *, head="drop", tail="keep")` returns the part of a score or track inside `[start, end)`. It replaces 0.6 `clip(start, end, clip_end=False)` and `trim(start, end, min_overlap, start_mode, end_mode)`.

A note lying wholly inside the window is kept, and one wholly outside is dropped. A note can stick out of the window on either side: its **head** is the part before `start`, and its **tail** the part after `end`. `head` and `tail` decide what happens to a note that has one:

| Value | The note is |
|---|---|
| `"drop"` | removed |
| `"cut"` | shortened to start at `start` (`head`) or end at `end` (`tail`) |
| `"keep"` | kept unchanged |

```python
score.clip(0, 1920)                           # notes starting in the window, unchanged, as in 0.6
score.clip(0, 1920, tail="drop")              # only notes lying wholly inside: 0.6 clip_end=True
score.clip(0, 1920, tail="cut")               # notes starting in the window, cut at end
score.clip(0, 1920, head="cut", tail="cut")   # excerpt, cut at both ends
```

- The defaults keep the notes whose onset lies in the window, so cutting consecutive windows puts every note in exactly one of them. `tail="drop"` cannot be emulated by moving `end`, because notes with the same onset can end at different times.
- A note ending at `start` or starting at `end` touches the window without entering it and is dropped. 0.6 `trim` with `end_mode="retain"` kept a note starting exactly at `end`; 0.7 does not.
- A note with both a head and a tail follows both rules: either `"drop"` removes it, and each `"cut"` shortens its side.
- A zero-duration note has neither a head nor a tail; it is kept when its time lies in `[start, end)`.
- Events without a duration (controls, pitch bends, lyrics, markers) are kept when their time lies in `[start, end)`. On a score, the tempo, time signature and key signature in effect at `start` are kept and moved to `start`.
- With `head="keep"`, a kept note can start before `start`, where the result has no tempo, time signature or key signature; converting it to `Second` uses the default tempo there.
- `start > end`, or a NaN bound, raises `ValueError`.
- Times are not shifted; call `shift_time(-start)` on the result to move it to 0.
- `min_overlap` is removed. To drop short fragments left by `"cut"`, filter the result, for example `track.notes = track.notes.filtered(lambda n: n.duration >= 10)`.

## 3. CC64 Sustain Pedals (Fixes #5)

Control change 64 in `track.controls` is now the single source of truth for sustain:

```text
track.controls (CC64 >= 64 press / < 64 release)
  ├──▶ track.pedals()  ──▶ tuple[Pedal, ...]  (derived on demand, closed pairs)
  └──▶ to_midi() & FluidSynth                 (round-trips & renders natively)
```

- Order is preserved on equal timestamps, repeated presses merge, and unclosed presses are omitted.
- `clip()` selects controls by timestamp, so sustain regions crossing a window boundary can lose their press or release event (a future playable-excerpt helper can synthesize active CC64 / pitch-bend / tempo state at window start).

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
    print(resampled.tpq, resampled.note_count())
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
    tpq=960,
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

# A non-Tick event names its unit; ttype is keyword-only and defaults to Tick
intro = Note(0.0, 0.5, 60, ttype=Second)

# Bulk construction: specify Note and ttype once via from_numpy
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

### NumPy SoA Round-Trip

**0.7 (After)**

```python
import numpy as np
from symusic import EventList, Note, Tick

# to_numpy() returns independent copy snapshots; pitch and velocity stay int8,
# so differences of 0..127 values (-127..127) never wrap
cols = track.notes.to_numpy()
intervals = np.diff(cols["pitch"])

mspq = score.tempos.to_numpy()["mspq"]
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
import soundfile as sf
from symusic import Score, Synthesizer, download_soundfont, write_wav

score = Score.from_file("piece.mid")
# Idempotent cached + SHA-384 verified SoundFont preset (default: "musescore_general_sf3")
synth = Synthesizer(download_soundfont(), sample_rate=44100)

audio = synth.render(score, stereo=True)  # float32 (frames, channels); mono (frames, 1)
# Recommended for multi-format audio output (WAV, FLAC, OGG, Opus, MP3):
sf.write("out.flac", audio, 44100)
# Or zero-extra-dependency WAV export for quick debugging:
write_wav("out.wav", audio, 44100)  # encoding="pcm16" (default) or "float32"
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
| Exceptions | Defined once in the [error contract](../development/conventions.md#errors). Summary: `ValueError` for invalid values, modes and ranges; `symusic.ParseError` (a `ValueError` subclass) for malformed MIDI/ABC; `OverflowError` for time arithmetic outside the unit's type; `IndexError` only for list indexes; an `OSError` subclass chosen by `errno` for file I/O; `TypeError` for the wrong kind of object, including an event whose `ttype` differs from the list's; `RuntimeError` for a failed external tool or a missing FluidSynth library |
| Resolution | `tpq` is read-only after construction. Assigning it would reinterpret every tick, while `resample(tpq)` keeps musical positions; only the second is offered (reading `ticks_per_quarter` is deprecated in 0.7 and removed in 0.8) |
| Numeric domains & MIDI wire | In-memory fields validate musical ranges (`0..127`, `tpq >= 1`); MIDI export additionally enforces SMF limits (`tpq` in `1..32767`; `Note(..., velocity=0)` encodes as MIDI NoteOff on export) |
| Strict MIDI & ABC | Rejects 7-bit MIDI channel data bytes `> 127` by default (`strict=False` clamps channel data bytes only); ABC converter warnings emit public `ABCConversionWarning` |
| Binary streams | Require explicit `format`, remain caller-closed, handle short writes, typed with structural protocols |
| Concurrency & reentrancy | Targets standard GIL CPython (free-threaded `3.13t`/`3.14t` deferred until per-container locking); `sort(key=...)` and Python callbacks enforce a reentrancy guard (`ValueError` on structural mutation during callback) |
| Python data model | Equality is by value and requires the same `ttype` (`Score[Tick] == Score[Quarter]` is always `False`); mutable types are unhashable (`__hash__ = None`); `__eq__` returns `NotImplemented` for unknown types; `Tempo.__match_args__ == ("time", "qpm")`; escaped round-trippable `repr()`; `pianoroll()` is `Tick`-only |
| Pickles (`SYMZ v3`) | Private C++20 little-endian codec compiled into `bindings/python/` carrying a versioned **`SYMZ v3`** header with full `__setstate__` validation (rejecting NaN / out-of-range fields and incompatible 0.6 payloads); for inter-process transport only. #134 writes `SYMZ v2` through `zpp_bits`; the private codec is a new layout and bumps the version to 3 |

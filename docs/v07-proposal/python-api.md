# Python API Design

Every change in [RFC #136](https://github.com/Yikai-Liao/symusic/issues/136) removes binding leaks, resolves mutability conflicts, and aligns with Python typing (`mypy`, `pyright`, `ty`).

## 1. Unified Surface, Time Units & Containers

| Dimension | 0.6 (Removed) | 0.7 |
|---|---|---|
| Public surface | `symusic.core`, `symusic.factory`, `symusic.types`, `symusic.soundfont` | Top-level `symusic` only; `symusic._core` & `symusic._soundfont` are private (`python -m symusic.skill` kept) |
| Time units | Free-form `"tick"` / `"second"`, `TimeUnit.tick` | `ttype` accepts non-instantiable marker **class objects** (`type[Tick] \| type[Quarter] \| type[Second]`); `Score[Second]()` raises `TypeError` |
| Containers | `NoteTickList`, `TempoTickList`, `TextMetaTickList`, … | Single generic `EventList[Note[Tick]]` + `TrackList[Tick]`, built as `EventList(Note, items=(), *, ttype=Tick)` and `TrackList(items=(), *, ttype=Tick)`; `ttype` is never guessed from the items, and an item of another unit raises `TypeError`. Both are `MutableSequence`s. Their equality also compares the event type and unit, so by symusic's choice a list never equals a plain `list`: write `list(notes) == expected`, and `not notes` for emptiness |
| Events & audio | `TextMeta`, `Soundfont`, `BuiltInSF2`/`BuiltInSF3`, `dump_wav()` | `TextEvent`, `Synthesizer`, `write_wav()` (zero-dep WAV; use `soundfile` for multi-format), and `download_soundfont(name="musescore_general_sf3", *, cache_dir=None, force=False) -> Path` |
| Tracks | Tracks hold times in their unit | Unchanged. A `Track` has no `tpq` or tempo map: its times are coordinates in its unit, and converting a track to another unit goes through a `Score` |

## 2. Strict CQS Mutability, Copy & Reference Semantics

No `inplace=` flags anywhere. Operations are grouped explicitly by receiver mutation and return value:

| Category | Return | Operations | Contract |
|---|---|---|---|
| **In-place value/order mutators** | `None` | `sort()`, `shift_time()`, `shift_pitch()`, `shift_velocity()` | Mutate the receiver in place. `sort()` is **stable**: events with equal keys keep their relative order, as `list.sort` does, so equal-time tempos and CC64 events keep their playback order. `Score.sort()`/`Track.sort()` sort all contained event lists while keeping `Score.tracks` in order. `shift_time()` shifts every reachable event (`Note`, `ControlChange`, `Tempo`, `TextEvent`, etc.) once per call, while `shift_pitch()` and `shift_velocity()` shift every reachable `Note` once per call, each with atomic rollback when any result leaves its range |
| **Returning transforms** | New object | `filtered()`, `clip()`, `adjust_time()`, `to()`, `resample()` | Leave the source unchanged and return a new object. `filtered(f)` returns a new list holding the **same** elements that `f` accepts, like `[n for n in notes if f(n)]`, so `notes.filtered(f)[0] is notes[0]` when the first note passes. Use `notes[:] = notes.filtered(f)` to mutate through aliases; `MutableSequence` operations like `del notes[i]` still mutate in place per Python stdlib |

### Copies and References

Copy constructors (`Score(other)`, `Note(other)`) are removed. A list copy is shallow, as `list.copy()` is. The `copy()` method of a score, track or event is deep and equals `copy.deepcopy`: the standard library defines no such method, and symusic makes it deep because a score copy that shares its tracks is rarely what a caller wants. `copy.copy` and `copy.deepcopy` keep their standard meaning. This matrix is the Python spelling of [copies and references](./semantics.md#copies-and-references); `ValueStorage` bindings (C++, C ABI, Go, WASM) copy by value and have no shallow container sharing or memo graphs.

| Copy Operation | Semantics |
|---|---|
| `notes.copy()`, `notes[:]`, `copy.copy(notes)` (`EventList` / `TrackList`) | **Shallow** — a new list holding the same element objects |
| `score.copy()`, `track.copy()`, `note.copy()` | **Deep** — identical to `copy.deepcopy(x)` |
| `copy.copy(score)`, `copy.copy(track)` | **Shallow** — a new outer object sharing its lists and tracks |
| `copy.deepcopy(x)` | **Deep, alias-preserving** — copies the whole graph and keeps aliases inside it (`[n, n]` copies to `[m, m]`). It uses the caller's memo, so `copy.deepcopy([score, score.tracks[0]])` gives a list whose second item is the copied score's first track |
| `pickle.dumps(x)` | **Value snapshot** — aliases inside the object are not kept, and the bytes load only in the same symusic version |

| Input Kind | Assignment / Insertion | Semantics |
|---|---|---|
| **Plain Python `list` / iterable** | `track.notes = [n1, n2]`, `score.tracks = [t1]`, passing a `list` to `Track(...)`/`Score(...)` | Creates a **new** `EventList`/`TrackList` and rebinds the attribute (detached from the Python `list` and any prior `old = track.notes` alias; use `track.notes.clear()` or `track.notes[:] = [...]` to mutate through aliases) |
| **Existing `EventList` / `TrackList`** | `track.notes = existing_list` (e.g. `EventList.from_numpy(...)`, `other.notes`) | **Shares the container reference with zero copy** (`track.notes += [n]` naturally preserves aliases; `.copy()` gives a new list of the same elements, and `copy.deepcopy()` copies the elements too) |
| **Individual `Note` / `Track` objects** | `append`, `insert`, `setitem`, `track.notes = [n1, n2]`, `score.tracks = [t1]` | **References each object's underlying handle directly without copying** — consistent with indexing (`track.notes[0]`) and slicing (`track.notes[:]`) |

### Clipping a Window

`clip(start, end, *, head="drop", tail="keep")` returns the part of a score or track inside `[start, end)`. It replaces 0.6 `clip(start, end, clip_end=False)` and `trim(start, end, min_overlap, start_mode, end_mode)`. `head` and `tail` take `"drop"`, `"cut"` or `"keep"` and decide what happens to a note sticking out before `start` or after `end`; the full rules, including the tempo and signatures kept at the window start, are in [clip](./semantics.md#clip).

```python
score.clip(0, 1920)  # notes starting in the window, unchanged, as in 0.6
score.clip(
    0, 1920, tail="drop"
)  # notes lying wholly inside [0, 1920) (replaces clip_end=True)
score.clip(0, 1920, tail="cut")  # notes starting in the window, cut at end
score.clip(0, 1920, head="cut", tail="cut")  # excerpt, cut at both ends
excerpt = score.clip(960, 2880)
excerpt.shift_time(-960)  # clip keeps times; shift to move the window to 0
```

## 3. CC64 Sustain Pedals (Fixes #5)

Control change 64 in `track.controls` is now the single source of truth for sustain:

```text
track.controls (CC64 >= 64 press / < 64 release)
  ├──▶ track.pedals()  ──▶ tuple[Pedal, ...]  (derived on demand, closed pairs)
  └──▶ to_midi() & FluidSynth                 (round-trips & renders natively)
```

- Order is preserved on equal timestamps, repeated presses merge, and unclosed presses are omitted.
- `clip()` selects controls by timestamp, so sustain regions crossing a window boundary can lose their press or release event. A later `excerpt()` helper will carry the CC64, pitch-bend and program state in effect at the window start.

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
    EventList,
    KeySignature,
    Note,
    Score,
    Second,
    Tempo,
    TextEvent,
    Tick,
    Track,
)

# Small Tick lists: pass a plain list directly (no redundant EventList / ttype=Tick)
score: Score[Tick] = Score(
    tpq=960,
    tempos=[Tempo(0, 120.0)],
    key_signatures=[KeySignature(0, 0, minor=False)],
    markers=[TextEvent(0, "Intro")],
    tracks=[
        Track(
            "Piano",
            notes=[
                Note(0, 480, 60),  # velocity defaults to 64 (MIDI 1.0 0x40)
                Note(480, 480, 64, 80),
            ],
        )
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
from symusic import Score, Synthesizer, download_soundfont, write_wav

rate = 48000
score = Score.from_file("piece.mid")
# Cached, SHA-384-verified SoundFont preset (default: "musescore_general_sf3")
synth = Synthesizer(download_soundfont(), sample_rate=rate)

audio = synth.render(
    score
)  # float32 (sample_frames, 2); channels=1 gives (sample_frames, 1)
write_wav(
    "out.wav", audio, rate
)  # zero-dependency; encoding="pcm16" (default) or "float32"

# Soundtrack for a 30-second video: exactly 30 s, whatever the score's length
soundtrack = synth.render(score, sample_frames=30 * rate)
```

Other formats go through `soundfile`, which symusic does not depend on:

```python
import soundfile as sf

sf.write(
    "out.flac", audio, rate
)  # also OGG, Opus, MP3; accepts (frames, channels) float32
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
| Exceptions | Defined once in the [error contract](../development/conventions.md#errors). Summary: `ValueError` for invalid values, modes and ranges; `symusic.ParseError` (a `ValueError` subclass) for malformed MIDI/ABC; `OverflowError` for time or WAV header arithmetic outside the target type; `IndexError` only for list indexes; an `OSError` subclass chosen by `errno` for file I/O; `TypeError` for the wrong kind of object, including an event whose `ttype` differs from the list's; `symusic.CancelledError` (a `RuntimeError` subclass) for cooperative cancellation; `RuntimeError` for a failed external tool or a missing FluidSynth library |
| Resolution | `tpq` is read-only after construction. Assigning it would reinterpret every tick, while `resample(tpq)` keeps musical positions; only the second is offered (reading `ticks_per_quarter` is deprecated in 0.7 and removed in 0.8) |
| Numeric domains & MIDI wire | [Numeric input](./semantics.md#numeric-input): `Tick` takes integers only (`1.0` and `True` raise `TypeError`), float units reject NaN and infinity, and every field validates its musical range (`0..127`, `tpq >= 1`). MIDI export additionally enforces SMF limits (`tpq` in `1..32767`) and skips velocity-0 notes ([MIDI export](./semantics.md#midi-export)) |
| Strict MIDI & ABC | Rejects 7-bit MIDI channel data bytes `> 127` by default (`strict=False` clamps channel data bytes only); ABC converter warnings emit public `ABCConversionWarning` |
| Binary streams | Require explicit `format`, remain caller-closed, handle short writes, typed with structural protocols |
| Concurrency & reentrancy | Targets standard GIL CPython (free-threaded `3.13t`/`3.14t` deferred until per-container locking). The binding holds the GIL whenever it touches shared storage; `render()` copies the score's events under the GIL and releases it while synthesizing, so other threads run and may even edit the score. `sort(key=...)` and Python callbacks enforce a reentrancy guard (`ValueError` on structural mutation during callback). An exception from a callback leaves the receiver unchanged; whatever the callback itself changed elsewhere stays changed |
| Python data model | Equality is by value and requires the same `ttype` (`Score[Tick] == Score[Quarter]` is always `False`); mutable types are unhashable (`__hash__ = None`); `__eq__` returns `NotImplemented` for unknown types; `Tempo.__match_args__ == ("time", "qpm")`; escaped round-trippable `repr()`; `pianoroll()` is `Tick`-only. Public classes are final (subclassing raises `TypeError`) and their instances have no `__dict__`; `Score` and `Track` support weak references. `Score[Tick]` exists for annotations only: `isinstance(x, Score[Tick])` raises `TypeError`, so test `isinstance(x, Score) and x.ttype is Tick` |
| Arrays | `to_numpy()` returns a dict of fresh 1-D columns named after the fields: `time` and `duration` are `int32` for `Tick` and `float32` otherwise; `pitch`, `velocity` and `fifths` are `int8`; `number`, `value` (controls), `numerator` and `denominator` are `uint8`; `value` (pitch bends) and `mspq` are `int32`; `minor` is `bool`. `TextEvent` lists have no array form (`TypeError`). `from_numpy()` needs every column, 1-D and of equal length, follows the [numeric input](./semantics.md#numeric-input) rules and copies. `pianoroll()` returns a new C-contiguous `uint8` array ([pianoroll](./semantics.md#pianoroll)); `render()` returns a new C-contiguous `float32` array of shape `(sample_frames, channels)` ([rendering](./semantics.md#rendering)) |
| Pickles (`SYMZ v3`) | Private C++20 little-endian codec compiled into `bindings/python/` carrying a versioned **`SYMZ v3`** header with full `__setstate__` validation (rejecting NaN / out-of-range fields and incompatible 0.6 payloads); a value snapshot for inter-process transport, loadable only by the same symusic version. #134 writes `SYMZ v2` through `zpp_bits`; the private codec is a new layout and bumps the version to 3 |

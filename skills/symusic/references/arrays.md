# NumPy arrays and piano rolls

symusic turns event lists into one NumPy array per field, and tick scores and tracks into dense piano-roll tensors. Both run in C++ and avoid creating a Python object per event.

## Event lists as arrays

`to_numpy()` on an event list returns a dict of arrays, one per field. `from_numpy` on the event list class builds a list back:

```python
from symusic import NoteList, Score

score = Score("song.mid")
arrays = score.tracks[0].notes.to_numpy()
print({name: a.dtype for name, a in arrays.items()})

notes = NoteList.from_numpy(**arrays, ttype=score.ttype)
assert notes == score.tracks[0].notes
```

| Class | Keys |
|---|---|
| `Note` | `time`, `duration`, `pitch`, `velocity` |
| `Pedal` | `time`, `duration` |
| `ControlChange` | `time`, `number`, `value` |
| `PitchBend` | `time`, `value` |
| `Tempo` | `time`, `microseconds_per_quarter` |
| `TimeSignature` | `time`, `numerator`, `denominator` |
| `KeySignature` | `time`, `key`, `minor` |

- `time` and `duration` are `int32` in ticks and `float32` in quarters and seconds; `pitch` and `velocity` are `int8`.
- The arrays are copies; changing them does not change the score.
- `from_numpy` takes the same keys as keyword arguments plus `ttype`. The arrays must be one-dimensional CPU arrays of equal length.
- `TextMeta` lists (lyrics and markers) have no `to_numpy`.

Arrays make statistics over a whole score short:

```python
import numpy as np
from symusic import Score, Second

score = Score("song.mid", ttype=Second)
pitched = [t for t in score.tracks if not t.is_drum]
pitches = np.concatenate([t.notes.to_numpy()["pitch"] for t in pitched])
histogram = np.bincount(pitches, minlength=128)
print("most used pitch:", histogram.argmax())
print("notes per second:", score.note_count() / score.end())
```

`get_beats()` and `get_downbeats()` return arrays in the score's unit, derived from the time signatures, up to the end of the score. As in pretty_midi, compound meters, whose numerator is 6, 9, 12 or another multiple of 3 above 3, count one beat per three notes of the denominator: 6/8 has two dotted-quarter beats and 6/4 two dotted-half beats, while 3/8 counts every eighth note. Onsets map to beats with `np.searchsorted`:

```python
import numpy as np
from symusic import Quarter, Score

score = Score("song.mid", ttype=Quarter)
beats = score.get_beats()
onsets = score.tracks[0].notes.to_numpy()["time"]
beat_index = np.searchsorted(beats, onsets, side="right") - 1
```

## Piano rolls

`Score.pianoroll` and `Track.pianoroll` exist only on tick scores and tracks, and the time axis has one column per tick, so resample to a coarse grid first. `resample(6)` gives six columns per quarter note, which holds sixteenth notes and eighth-note triplets; `min_duration=1` keeps notes from rounding to zero length.

```python
from symusic import Score

grid = Score("song.mid").resample(6, min_duration=1)
roll = grid.pianoroll(
    modes=["onset", "frame"], pitch_range=(21, 109), encode_velocity=True
)
print(roll.shape)  # (modes, tracks, pitches, time)

track_roll = grid.tracks[0].pianoroll(modes=["frame"])
print(track_roll.shape)  # (modes, pitches, time)
```

- **Modes.** `"frame"` marks every column from the start to the end of a note, `"onset"` marks the start column and `"offset"` marks the end column. The first axis follows the order of `modes`; the default is `["frame", "onset"]`. Other names raise `ValueError`.
- **Pitches.** `pitch_range=(low, high)` is half-open, both bounds within 0–128, and row `i` is pitch `low + i`. Notes outside the range are skipped. An empty or reversed range raises `ValueError`.
- **Time.** The time axis has `end() + 1` columns, where `end()` is the end of the score or track; for a score that includes global events such as a late tempo change.
- **Values.** The array is `uint8`. A cell holds 1, or the note velocity with `encode_velocity=True`. Where notes overlap on the same pitch, the later note in the list overwrites the earlier one.
- **Errors.** A note with a negative time or duration raises `ValueError`. Encoding a negative velocity also raises `ValueError`. A note with zero duration marks `onset` and `offset` but no `frame` column.

symusic has no conversion from a piano roll back to notes.

# Time units

Every score, track, event and event list in symusic has one time unit, reported by its `ttype` attribute. All time values in it, such as `time`, `duration`, `start()` and the arguments of `shift_time` or `clip`, are in that unit.

| Unit | Python type | One unit is | Conversion uses |
|---|---|---|---|
| `Tick` | `int` (32-bit) | one MIDI tick; `ticks_per_quarter` ticks make a quarter note | |
| `Quarter` | `float` (32-bit) | one quarter note | `ticks_per_quarter` |
| `Second` | `float` (32-bit) | one second | the tempo map |

Use ticks to edit MIDI without rounding, quarters for rhythm and beat positions, and seconds to align with audio.

## Choosing and converting

```python
from symusic import Quarter, Score, Second, Tick

score = Score("song.mid", ttype=Quarter)  # load in quarters
seconds = score.to(Second)  # a new score
ticks = seconds.to(Tick, min_duration=1)  # keep every note at least 1 tick long
print(score.ttype, seconds.ttype, ticks.ttype)
```

- `ttype` and `to` accept the marker classes `Tick`, `Quarter`, and `Second`. The default is `Tick`. Anything else raises `ValueError`.
- `to` always returns a new score, even for the current unit. Only `Score` has `to`: to convert a track, convert the score that holds it.
- An event list accepts only events of its own unit. Appending a quarter `Note` to a tick track raises `ValueError`, so create events with the matching `ttype`, for example `Note(0.5, 1.0, 60, 100, ttype=Quarter)`.
- `ticks_per_quarter` is kept in every unit, because converting back to ticks and writing MIDI need it.

## Conversion rules

- **Tick and quarter.** A quarter value is `tick / ticks_per_quarter`. Converting to ticks multiplies and rounds to the nearest tick, halves away from zero.
- **Seconds.** Conversion to and from seconds follows the tempo map and keeps time zero fixed. A tempo event before zero applies from its own time until the next change. Before the first tempo event, and in a score without tempos, the tempo is 120 quarter notes per minute (500000 microseconds per quarter). Quarters and seconds convert directly into each other, not through ticks.
- **Durations.** In tick and quarter conversions the start and the duration of a note convert separately, so a converted end can differ by one tick from the converted original end. In conversions to seconds the duration is the converted end minus the converted start, so a tempo change inside a note changes its length in seconds.
- **`min_duration`.** After conversion, notes shorter than `min_duration` (in the target unit) are extended to it. The default `None` and negative values set no bound. Pass it when converting to ticks, where rounding can shorten a note to 0.
- **Overflow.** Tick arithmetic is checked. Shifting, computing an event's `end`, trimming and rescaling raise `OverflowError` when the result does not fit in `int32`, instead of wrapping around. Float units do not have this limit. Converting a quarter or second score to ticks, resampling, and creating a `Tempo` from a `qpm` are checked the same way: a value outside the `int32` range raises `OverflowError`, and NaN or a non-positive or non-finite `qpm` raises `ValueError`.
- **Precision.** Quarter and second values are 32-bit floats with about seven significant digits; at 1000 seconds the step between values is about 0.06 ms.

## Resampling

`score.resample(ticks_per_quarter, min_duration=None)` returns a tick score with `ticks_per_quarter == ticks_per_quarter`. It first converts the score to ticks at its current resolution, then scales every time and duration to the new resolution and rounds. `min_duration` is an integer number of ticks at the new resolution. A low resolution (for example `ticks_per_quarter=6`, a sixteenth-note triplet grid) is the usual step before building piano rolls; see [arrays.md](arrays.md).

## What differs between units

The three units share one API; only these points differ:

- Time values and time arguments are `int` in tick and `float` in quarter and second.
- `Score.pianoroll` and `Track.pianoroll` require a score or track in ticks. Call `resample` first.
- `resample` returns a tick score from any unit.
- `get_beats()` and `get_downbeats()` return NumPy arrays in the score's unit.

## Type annotations

Each class is generic over its unit marker. Annotations name the marker as the type parameter.

```python
from symusic import Score, Second


def first_onset(score: Score[Second]) -> float:
    return score.tracks[0].notes[0].time


print(first_onset(Score("song.mid", ttype=Second)))
```

`isinstance(x, symusic.Score)` is true for a score in any unit; pass `ttype=Second` to create a score in seconds.

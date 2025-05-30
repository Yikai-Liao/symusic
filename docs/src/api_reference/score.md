# Score API Reference

The `Score` class is the top-level container for musical data in Symusic. It holds tracks and global events like tempo, time signatures, and key signatures.

[Concepts: Score](./../concepts/score.md)

## Type Definition

`Score` is a generic class parameterized by time unit:

-   `ScoreTick` (`Score<Tick>` in C++): Time as integer ticks.
-   `ScoreQuarter` (`Score<Quarter>` in C++): Time as float quarter notes.
-   `ScoreSecond` (`Score<Second>` in C++): Time as float seconds.

## Attributes

| Attribute           | Type              | Description                                     |
| :------------------ | :---------------- | :---------------------------------------------- |
| `ticks_per_quarter` | `int`             | Ticks per quarter note resolution (TPQ / PPQ) |
| `tpq`               | `int`             | Alias for `ticks_per_quarter`                   |
| `tracks`            | `TrackList`       | List-like container for `Track` objects.      |
| `time_signatures`   | `TimeSignatureList`| List-like container for `TimeSignature` events. |
| `key_signatures`    | `KeySignatureList`| List-like container for `KeySignature` events.  |
| `tempos`            | `TempoList`       | List-like container for `Tempo` events.         |
| `markers`           | `TextMetaList`    | List-like container for `TextMeta` marker events.|

## Properties (Read-Only Methods)

| Method      | Return Type       | Description                                            |
| :---------- | :---------------- | :----------------------------------------------------- |
| `ttype`     | `TimeUnit` object | Returns the time unit object (`Tick`, `Quarter`, `Second`). |
| `start()`   | `int` or `float`  | Start time of the earliest event in the score.         |
| `end()`     | `int` or `float`  | End time of the latest event in the score.             |
| `note_num()`  | `int`             | Total number of notes across all tracks.             |
| `track_num()` | `int`             | Number of tracks in the `tracks` list.               |
| `empty()`   | `bool`            | `True` if the score contains no tracks or events.      |

## Constructors

```python
from symusic import Score, TimeUnit
from pathlib import Path

# Create an empty score with specified TPQ and time unit
s = Score(tpq: int = 960, ttype: str | TimeUnit = "tick")

# Load score from a file path
# Format (fmt) is usually inferred from extension, can be 'midi' or 'abc'
s = Score(path: str | Path, ttype: str | TimeUnit = "tick", fmt: str | None = None)

# Copy constructor (creates a deep copy)
s_copy = Score(other: Score, ttype: str | TimeUnit | None = None)
# If ttype is provided, performs deep copy AND time unit conversion.
# If ttype is None, keeps the original time unit.
```

## Factory Methods (Alternative Constructors)

```python
# Create empty score (equivalent to constructor)
s = Score.from_tpq(tpq: int = 960, ttype: str | TimeUnit = "tick")

# Load from file (equivalent to constructor)
s = Score.from_file(path: str | Path, ttype: str | TimeUnit = "tick", fmt: str | None = None)

# Load from MIDI data in memory
s = Score.from_midi(data: bytes, ttype: str | TimeUnit = "tick")

# Load from ABC notation string in memory
s = Score.from_abc(data: str, ttype: str | TimeUnit = "tick")

# Create from another score (deep copy + optional conversion)
s_copy = Score.from_other(other: Score, ttype: str | TimeUnit | None = None)
```

## I/O Methods

```python
# Save score to a MIDI file
# Automatically converts to Tick time unit if necessary.
score.dump_midi(path: str | Path) -> None

# Get MIDI file content as bytes
midi_bytes: bytes = score.dumps_midi()

# Save score to an ABC notation file
# Note: ABC support might be limited compared to MIDI.
score.dump_abc(path: str | Path, warn: bool = True) -> None

# Get ABC notation content as string
abc_string: str = score.dumps_abc(warn: bool = True)
```

## Conversion Methods

```python
# Create a copy (deep by default)
# use deep=False for shallow copy (rarely needed)
score_copy = score.copy(deep: bool = True)

# Convert score to a different time unit
# Returns a NEW Score object with the converted time unit.
# min_dur (only for to("tick")): Minimum duration in ticks for notes/pedals
# after conversion, prevents zero duration due to rounding.
score_new_ttype = score.to(ttype: str | TimeUnit, min_dur: int | float | None = None)

# Convert score to piano roll (numpy array)
# Requires score to be in Tick time unit (use resample first).
# modes: list of strings, e.g. ["onset", "frame", "offset"]
# pitch_range: tuple (min_pitch, max_pitch_exclusive), e.g. (0, 128)
# encode_velocity: bool (True for velocity values, False for binary 0/1)
piano_roll_array = score_tick.pianoroll(
    modes: list[str] = ["onset", "frame"],
    pitch_range: tuple[int, int] = (0, 128),
    encode_velocity: bool = False
) -> np.ndarray # Shape: (modes, tracks, pitch_bins, time_steps)
```

## Modification Methods

These methods modify the score. Use `inplace=True` to modify the object directly, otherwise a new modified Score object is returned.

```python
# Resample score to a specific TPQ (returns a ScoreTick)
# Useful before creating piano roll to control time resolution.
# min_dur: Minimum tick duration for notes/pedals after resampling.
score_resampled = score.resample(tpq: int, min_dur: int | None = None) -> ScoreTick

# Clip score events to a time range [start, end)
score_clipped = score.clip(
    start: int | float,
    end: int | float,
    clip_end: bool = False, # If True, also clip notes ending after 'end'
    inplace: bool = False
)

# Adjust event timings based on mapping original times to new times
# Performs linear interpolation for times between specified points.
score_adjusted = score.adjust_time(
    original_times: list[int | float],
    new_times: list[int | float],
    inplace: bool = False
)

# Sort all event lists within the score (tracks are also sorted)
score_sorted = score.sort(reverse: bool = False, inplace: bool = False)

# Shift time of all events by an offset
score_shifted = score.shift_time(offset: int | float, inplace: bool = False)

# Shift pitch of all notes by a semitone offset
score_transposed = score.shift_pitch(offset: int, inplace: bool = False)

# Shift velocity of all notes by an offset
score_dynamics_shifted = score.shift_velocity(offset: int, inplace: bool = False)
```

## Synthesis Methods (Convenience)

```python
# Synthesize score to audio data (shortcut for using Synthesizer class)
# See Synthesizer API for details on sf_path, sample_rate, quality
audio_data = score.to_audio(
    sf_path: str | Path | None = None, # Defaults to MuseScoreGeneral
    sample_rate: int = 44100,
    quality: int = 4 # Note: Default quality here might differ from Synthesizer constructor
) -> AudioData
```

## Serialization (Pickling)

`Score` objects support Python's `pickle` protocol for efficient serialization, which is useful for saving/loading objects quickly and for use with `multiprocessing`.

```python
import pickle

# Save score object to a pickle file
with open("score.pkl", "wb") as f:
    pickle.dump(score, f)

# Load score object from a pickle file
with open("score.pkl", "rb") as f:
    loaded_score = pickle.load(f)

# Internal methods (rarely called directly)
# state: bytes = score.__getstate__()
# score.__setstate__(state)
```

## Comparison Methods

```python
# Check if two Score objects are equal (deep comparison)
is_equal: bool = (score1 == score2)

# Check if two Score objects are not equal
is_not_equal: bool = (score1 != score2)
```
Equality comparison checks if all attributes, tracks, and events are identical between the two scores.

## String Representation

```python
# Get a string summary of the score using repr()
repr_string: str = repr(score)
# Or simply print the score (which calls repr())
print(score)
```
The string representation (obtained via `repr()` or `print()`) provides a concise summary of the score's properties (TPQ, time unit, number of tracks, event counts). There is no separate `.summary()` method.

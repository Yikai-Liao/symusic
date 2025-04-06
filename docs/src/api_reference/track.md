# Track API Reference

A `Track` object represents a single instrument track within a `Score`, containing its specific musical events.

[Concepts: Track](./../concepts/track.md)

## Type Definition

`Track` is a generic class parameterized by time unit:

-   `TrackTick` (`Track<Tick>` in C++)
-   `TrackQuarter` (`Track<Quarter>` in C++)
-   `TrackSecond` (`Track<Second>` in C++)

## Attributes

| Attribute     | Type              | Description                                      |
| :------------ | :---------------- | :----------------------------------------------- |
| `name`        | `str`             | Name of the track (e.g., "Piano", "Drums").     |
| `program`     | `int`             | MIDI program number (instrument ID, 0-127).      |
| `is_drum`     | `bool`            | `True` if the track is a percussion/drum track.  |
| `notes`       | `NoteList`        | List-like container for `Note` events.         |
| `controls`    | `ControlChangeList`| List-like container for `ControlChange` events.|
| `pitch_bends` | `PitchBendList`   | List-like container for `PitchBend` events.    |
| `pedals`      | `PedalList`       | List-like container for `Pedal` events.        |
| `lyrics`      | `TextMetaList`    | List-like container for lyric `TextMeta` events. |

## Properties (Read-Only Methods)

| Method      | Return Type       | Description                                         |
| :---------- | :---------------- | :-------------------------------------------------- |
| `ttype`     | `TimeUnit` object | Returns the time unit object (`Tick`, `Quarter`, `Second`). |
| `start()`   | `int` or `float`  | Start time of the earliest event in the track.      |
| `end()`     | `int` or `float`  | End time of the latest event in the track.          |
| `note_num()`  | `int`             | Number of notes in the `notes` list.              |
| `empty()`   | `bool`            | `True` if the track contains no events.           |

## Constructors

```python
from symusic import Track, TimeUnit

# Create an empty track
t = Track(name: str = "", program: int = 0, is_drum: bool = False, ttype: str | TimeUnit = "tick")

# Copy constructor (creates a deep copy)
t_copy = Track(other: Track)
```

## Modification Methods

Modify the track's events. Use `inplace=True` to modify directly, otherwise a new `Track` is returned.

```python
# Clip track events to a time range [start, end)
t_clipped = track.clip(
    start: int | float, 
    end: int | float, 
    clip_end: bool = False, # If True, also clip notes ending after 'end'
    inplace: bool = False
)

# Sort all event lists within the track
t_sorted = track.sort(reverse: bool = False, inplace: bool = False)

# Adjust event timings based on mapping original times to new times
t_adjusted = track.adjust_time(
    original_times: list[int | float], 
    new_times: list[int | float], 
    inplace: bool = False
)

# Shift time of all events by an offset
t_shifted = track.shift_time(offset: int | float, inplace: bool = False)

# Shift pitch of all notes by a semitone offset
t_transposed = track.shift_pitch(offset: int, inplace: bool = False)

# Shift velocity of all notes by an offset
t_dynamics_shifted = track.shift_velocity(offset: int, inplace: bool = False)
```

## Conversion Methods

```python
# Create a copy (deep by default)
track_copy = track.copy(deep: bool = True)

# Convert track to a different time unit
# Returns a NEW Track object with the converted time unit.
track_new_ttype = track.to(ttype: str | TimeUnit, min_dur: int | float | None = None)

# Convert track to piano roll (numpy array)
# Requires track to be in Tick time unit.
pr_array = track_tick.pianoroll(
    modes: list[str] = ["onset", "frame"], 
    pitch_range: tuple[int, int] = (0, 128), 
    encode_velocity: bool = False
) -> np.ndarray # Shape: (modes, pitch_bins, time_steps)
```

## Event List Methods

The attributes `notes`, `controls`, `pitch_bends`, `pedals`, and `lyrics` are specialized list objects with additional methods beyond standard Python list methods:

```python
# Example using track.notes (NoteList), other lists have similar methods
notes_list = track.notes

# Basic properties
start_time = notes_list.start()
end_time = notes_list.end()
is_empty = notes_list.empty()

# Copying
notes_copy = notes_list.copy(deep=True)

# Sorting (inplace by default)
# Default key sorts by time, duration, pitch, velocity
notes_list.sort(key=None, reverse=False, inplace=True)
# Sort by velocity (non-inplace)
notes_sorted_by_vel = notes_list.sort(key=lambda note: note.velocity, inplace=False)

# Check if sorted
is_sorted_by_default = notes_list.is_sorted(key=None, reverse=False)
is_sorted_by_pitch = notes_list.is_sorted(key=lambda note: note.pitch, reverse=False)

# Filtering (inplace by default)
# Keep only notes with pitch >= 60
notes_list.filter(lambda note: note.pitch >= 60, inplace=True)

# Time Adjustment (non-inplace by default)
adjusted_notes = notes_list.adjust_time([0, 480], [0, 500], inplace=False)

# Time Shifting (non-inplace by default)
shifted_notes = notes_list.shift_time(960, inplace=False)

# NumPy Conversion (for NoteList, ControlChangeList, etc.)
# Returns a dictionary mapping attribute names to numpy arrays
numpy_dict = notes_list.numpy() 
# e.g., numpy_dict['time'], numpy_dict['duration'], ...

# Create list from NumPy arrays (Class Method)
from symusic import Note # Use the specific event factory
new_notes_list = Note.from_numpy(
    time=np_time_array, 
    duration=np_duration_array, 
    pitch=np_pitch_array, 
    velocity=np_velocity_array, 
    ttype=track.ttype # Important: Specify the time unit!
)
```

## Serialization (Pickling)

`Track` objects support Python's `pickle` protocol.

```python
import pickle

# Save track object to a pickle file
with open("track.pkl", "wb") as f:
    pickle.dump(track, f)

# Load track object from a pickle file
with open("track.pkl", "rb") as f:
    loaded_track = pickle.load(f)

# Internal methods (rarely called directly)
# state: bytes = track.__getstate__()
# track.__setstate__(state)
```

## Comparison Methods

```python
# Check if two Track objects are equal (deep comparison)
is_equal: bool = (track1 == track2)

# Check if two Track objects are not equal
is_not_equal: bool = (track1 != track2)
```
Equality comparison checks if metadata and all event lists are identical.

## String Representation

```python
# Get a string summary of the track using repr()
repr_string: str = repr(track)
# Or simply print the track (which calls repr())
print(track)
```
The string representation (obtained via `repr()` or `print()`) provides a summary including name, program, drum status, and event counts. There is no separate `.summary()` method. 
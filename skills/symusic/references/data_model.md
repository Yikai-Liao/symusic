# Data model

A `Score` holds global events and a list of `Track` objects; each `Track` holds the events of one part. Every object has a time unit, described in [time_units.md](time_units.md). How these objects map to MIDI and ABC files is in [files.md](files.md).

## Score

| Attribute | Content |
|---|---|
| `ticks_per_quarter` | MIDI resolution, kept in every time unit |
| `tempos` | `Tempo` events |
| `time_signatures` | `TimeSignature` events |
| `key_signatures` | `KeySignature` events |
| `markers` | `TextMeta` events, such as rehearsal marks |
| `tracks` | `Track` objects |

The global lists apply to every track. Create a score with:

| Call | Result |
|---|---|
| `Score(path)` | Load a MIDI or ABC file |
| `Score.from_midi(data)`, `Score.from_abc(text)` | Load from bytes or a string |
| `Score(ticks_per_quarter=960)` | An empty score with 960 ticks per quarter |

All constructors take the `ttype` keyword, which defaults to `Tick`. Use `other.to(Second)` to copy and convert a score.

## Track

| Attribute | Content |
|---|---|
| `name` | Track name |
| `program` | General MIDI program, 0–127 |
| `is_drum` | Whether the track plays on the drum channel |
| `notes` | `Note` events |
| `controls` | `ControlChange` events |
| `pitch_bends` | `PitchBend` events |
| `lyrics` | `TextMeta` events |

```python
from symusic import Note, Quarter, Score, Track

score = Score(ticks_per_quarter=480, ttype=Quarter)
bass = Track("bass", program=33, ttype=Quarter)
bass.notes.append(Note(0.0, 1.0, 40, 90, ttype=Quarter))
score.tracks.append(bass)
print(score)
```

`Track(name, program, is_drum, notes, controls, pitch_bends, *, ttype=Tick)` copies the lists it receives. A track must have the unit of the score it joins; appending a quarter track to a tick score raises `ValueError`.

## Events

| Class | Fields | Lives in | Default sort key |
|---|---|---|---|
| `Note` | `time`, `duration`, `pitch`, `velocity` | `Track.notes` | `(time, duration, pitch, velocity)` |
| `Pedal` | `time`, `duration` | `Track.pedals()` | `(time, duration)` |
| `ControlChange` | `time`, `number`, `value` | `Track.controls` | `(time, number, value)` |
| `PitchBend` | `time`, `value` | `Track.pitch_bends` | `time` |
| `TextMeta` | `time`, `text` | `Track.lyrics`, `Score.markers` | `time` |
| `Tempo` | `time`, `qpm`, `microseconds_per_quarter` | `Score.tempos` | `time` |
| `TimeSignature` | `time`, `numerator`, `denominator` | `Score.time_signatures` | `time` |
| `KeySignature` | `time`, `key`, `minor` | `Score.key_signatures` | `time` |

- Every constructor takes the fields in table order, then `ttype`: `Note(0, 480, 60, 100)`, `ControlChange(0, 64, 127, ttype=Quarter)`.
- `Note` and `Pedal` also have `end` (`time + duration`). `Note.empty()` is true when the duration or the velocity is 0 or less.
- Pitch, velocity, and control numbers and values range from 0 to 127. Duration is non-negative. Invalid field values raise `ValueError`.
- `Tempo` stores microseconds per quarter note (`microseconds_per_quarter`, an integer); `qpm` (quarter notes per minute) is computed from it and can be set. Create one with `Tempo(0, qpm=100)` or `Tempo(0, microseconds_per_quarter=600000)`.
- `KeySignature.key` counts sharps (positive) or flats (negative); `minor` is `False` for major and `True` for minor. Keys range from −7 to 7.
- Scores, tracks, events and lists compare by value and are unhashable.

## Event lists

Every list attribute is a typed list, such as `NoteList[Tick]`. Event and track lists implement `collections.abc.MutableSequence`, including slicing, reverse iteration, and in-place extension with `+=`. They also provide:

| Method | Effect |
|---|---|
| `sort(key=None, reverse=False)` | Sort by `key`, or by the default sort key |
| `is_sorted(key=None, reverse=False)` | Check the order |
| `filter(function)` | Keep the events for which `function` returns true |
| `adjust_time(original_times, new_times)` | Warp times; see [Transformations](#transformations) |
| `to_numpy()`, `NoteList.from_numpy(...)` | Convert to and from one NumPy array per field; see [arrays.md](arrays.md) |

```python
from symusic import Score

score = Score("song.mid")
notes = score.tracks[0].notes
notes.filter(lambda n: n.velocity >= 40)  # in place
notes.sort(key=lambda n: (n.pitch, n.time))
print(notes.is_sorted(key=lambda n: (n.pitch, n.time)))
```

## References and copies

An event or track read from a list is a reference into the score: `score.tracks[0].notes[0].pitch = 72` changes the score. A reference stays valid when the list is appended to, sorted or shrunk, and an event or track removed with `pop` or `remove` stays usable on its own.

Copies differ in depth:

| Operation | Result |
|---|---|
| `score.copy()`, `track.copy()`, `copy.deepcopy(x)` | Deep: new tracks, lists and events |
| `copy.copy(score)` | A new score that shares the `tracks` list and every event |
| `copy.copy(notes)`, `notes[a:b]` | A new list that shares the events |
| `notes.copy()`, `copy.deepcopy(notes)` | A new list with new events |
| `track.notes = [...]` | A new list with copies of the events |
| `track.notes = other.notes` | Both attributes hold the same list |
| `score.tracks = [...]` | A new list that shares the tracks |

## Transformations

`sort`, `filter`, `shift_time`, `shift_pitch`, `shift_velocity`, and `adjust_time` mutate and return `None`. `clip`, `trim`, `to`, and `resample` return independent objects.

| Method | On | Effect |
|---|---|---|
| `shift_time(offset)` | Score, Track, events | Add `offset` to every time; times may become negative |
| `shift_pitch(offset)` | Score, Track, `Note` | Add `offset` to every pitch |
| `shift_velocity(offset)` | Score, Track, `Note` | Add `offset` to every velocity |
| `clip(start, end, clip_end=False)` | Score, Track | Keep the events in `[start, end)` |
| `trim(start, end, min_overlap=0, start_mode="remove", end_mode="remove")` | Score, Track | Keep notes based on how they overlap the window |
| `adjust_time(original_times, new_times)` | Score, Track, lists | Warp times piecewise linearly |
| `sort(reverse=False)` | Score, Track | Sort every event list by its default key |

- `shift_pitch` and `shift_velocity` raise `ValueError` when a result leaves 0–127.
- `clip` keeps events by their start time. With `clip_end=True`, notes must also end at or before `end`. Times are not shifted, so the clipped score still starts near `start`; follow with `shift_time(-start)` to move it to 0. The tempo, time signature and key signature in effect at `start` are kept and moved to `start`.
- `trim` keeps notes that overlap the window by at least `min_overlap`. At each boundary, `"remove"` drops crossing events, `"truncate"` cuts them at the boundary, and `"retain"` keeps them whole. Unknown modes, reversed or NaN bounds, and negative or NaN `min_overlap` raise `ValueError`. Control changes, pitch bends and lyrics are clipped by time.
- `adjust_time` maps each time through straight segments between the anchor pairs `(original_times[i], new_times[i])`, with at least two anchors in each list. The lists must be the same length. Original times must increase strictly, and new times must be nondecreasing. Events that start before the first anchor or end after the last are dropped. In ticks the mapped times are truncated toward zero.
- `Score.sort` and `Track.sort` sort the event lists of each track and the global lists; they do not reorder `tracks`.
- `start()` and `end()` on a score or track return the earliest event time and the latest event end, or 0 when there are no events. `Score.note_count()` counts notes across its tracks. `empty()` is true for a track without events and for a score without tracks or global events.

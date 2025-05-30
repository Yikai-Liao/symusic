# Events API Reference

Events represent individual musical occurrences or annotations within a `Score` or `Track`. All event classes are parameterized by time unit (`Tick`, `Quarter`, or `Second`).

[Concepts: Events](./../concepts/events.md)

## Common Event Patterns & Methods

Most event classes share these characteristics and methods:

-   **Time Unit Parameterization**: Classes exist for each time unit (e.g., `NoteTick`, `NoteQuarter`, `NoteSecond`). The `ttype` property returns the time unit.
-   **`time` Attribute**: The time at which the event occurs.
-   **Comparison (`__eq__`, `__ne__`)**: Events are comparable (e.g., `event1 == event2`). Equality checks if all attributes are identical. Events of different types or time units are never equal. Comparison (`<`, `>`, etc.) primarily uses time, with tie-breaking rules specific to the event type.
-   **Copying**: Support `copy.copy()` (shallow, usually sufficient as events are often simple data structures) and `copy.deepcopy()`.
-   **Serialization (Pickling)**: Support `pickle.dump()` and `pickle.load()` via `__getstate__` and `__setstate__`.
-   **`shift_time(offset)`**: Returns a *new* event shifted by `offset`.
-   **`shift_time_inplace(offset)`**: Modifies the event's time inplace.
-   **`to(ttype, ...)`**: Returns a *new* event converted to the target `ttype`. Event types with duration (`Note`, `Pedal`) accept an optional `min_dur` argument for the `to("tick", min_dur=...)` conversion.
-   **NumPy Conversion (for lists)**: Event lists like `NoteList` often have `.numpy()` and `EventFactory.from_numpy()` methods (See [Track API](./track.md#event-list-methods)).

---

## Note

Represents a musical note.

**Attributes:**

-   `time` (`int`|`float`): Start time.
-   `duration` (`int`|`float`): Duration.
-   `pitch` (`int`): MIDI pitch (0-127).
-   `velocity` (`int`): MIDI velocity (1-127). A velocity of 0 might indicate a note-off in some contexts, but Symusic typically uses explicit durations.

**Specific Methods:**

-   `end()`: Returns `time + duration`.
-   `empty()`: Returns `True` if `duration <= 0` or `velocity <= 0`.
-   `shift_pitch(offset)`: Returns a new note with shifted pitch.
-   `shift_pitch_inplace(offset)`: Shifts pitch inplace.
-   `shift_velocity(offset)`: Returns a new note with shifted velocity.
-   `shift_velocity_inplace(offset)`: Shifts velocity inplace.

**Factory:** `symusic.Note(time, duration, pitch, velocity, ttype="tick")`

---

## ControlChange

Represents a MIDI Control Change (CC) message.

**Attributes:**

-   `time` (`int`|`float`): Event time.
-   `number` (`int`): CC number (0-127).
-   `value` (`int`): CC value (0-127).

**Factory:** `symusic.ControlChange(time, number, value, ttype="tick")`

---

## PitchBend

Represents a MIDI Pitch Bend message.

**Attributes:**

-   `time` (`int`|`float`): Event time.
-   `value` (`int`): Pitch bend value (-8192 to 8191, where 0 is center).

**Factory:** `symusic.PitchBend(time, value, ttype="tick")`

---

## Pedal

Represents a sustain pedal event with duration.

**Attributes:**

-   `time` (`int`|`float`): Pedal press (onset) time.
-   `duration` (`int`|`float`): Duration the pedal is held down.

**Specific Methods:**

-   `end()`: Returns `time + duration` (pedal release time).

**Factory:** `symusic.Pedal(time, duration, ttype="tick")`

---

## TimeSignature

Represents a time signature (meter) change. Global event stored in `Score.time_signatures`.

**Attributes:**

-   `time` (`int`|`float`): Time the time signature takes effect.
-   `numerator` (`int`): Numerator (e.g., 4 in 4/4).
-   `denominator` (`int`): Denominator (e.g., 4 in 4/4).

**Factory:** `symusic.TimeSignature(time, numerator, denominator, ttype="tick")`

---

## KeySignature

Represents a key signature change. Global event stored in `Score.key_signatures`.

**Attributes:**

-   `time` (`int`|`float`): Time the key signature takes effect.
-   `key` (`int`): Number of sharps (positive) or flats (negative). Corresponds to MIDI key signature meta-message `sf` value.
-   `tonality` (`int`): 0 for major, 1 for minor. Corresponds to MIDI `mi` value.

**Specific Methods:**
- `degree()`: Calculates a numerical representation useful for harmonic analysis (implementation-defined).

**Factory:** `symusic.KeySignature(time, key, tonality, ttype="tick")`

---

## Tempo

Represents a tempo change. Global event stored in `Score.tempos`.

**Attributes:**

-   `time` (`int`|`float`): Time the tempo takes effect.
-   `qpm` (`float`): Tempo in Quarter Notes Per Minute (BPM).

**Internal Attribute (Not directly exposed in Python):**

-   `mspq` (`int`): Microseconds per quarter note (often used internally for precision). `qpm` is derived from/converts to this.

**Factory:** `symusic.Tempo(time, qpm, ttype="tick")`

---

## TextMeta

Represents a text event associated with a specific time.

**Attributes:**

-   `time` (`int`|`float`): Time of the text event.
-   `text` (`str`): The text content.

**Usage:**

-   Stored in `Track.lyrics` for lyrics.
-   Stored in `Score.markers` for global markers.

**Factory:** `symusic.TextMeta(time, text, ttype="tick")`

---

## Event Lists (e.g., `NoteList`, `TempoList`)

Events are stored in specialized list-like containers within `Score` and `Track` objects. See the [Track API Reference](./track.md#event-list-methods) for methods common to these lists (`append`, `sort`, `filter`, `numpy`, `from_numpy`, etc.).

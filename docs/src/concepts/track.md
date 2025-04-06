# Track Concept

The `Track` class represents a single, independent sequence of musical events within a `Score`. It typically corresponds to a single instrument or voice in a musical piece, analogous to a track in a DAW or a staff in traditional notation.

## Purpose and Role

Within the Symusic data hierarchy, a `Track` serves to group events associated with a specific musical part. This separation allows for:

-   **Instrument Assignment**: Each track has metadata like `program` number (MIDI instrument) and an `is_drum` flag, determining how its notes should be interpreted or synthesized.
-   **Independent Processing**: Operations like transposition, quantization, or applying specific MIDI effects (like channel volume or pan via Control Changes) are often performed at the track level.
-   **Structural Organization**: Provides a clear way to manage complex multi-instrumental pieces.

[See Track API Reference](../api_reference/track.md) for detailed usage.

## Key Components

A `Track` primarily contains time-ordered lists of various `Event` types specific to that track:

-   **Notes (`notes`)**: The core musical pitches with durations and velocities.
-   **Control Changes (`controls`)**: MIDI CC messages for parameters like volume, pan, modulation, sustain, etc.
-   **Pitch Bends (`pitch_bends`)**: Smooth pitch deviations.
-   **Pedals (`pedals`)**: Specialized representation for sustain pedal on/off durations.
-   **Lyrics (`lyrics`)**: Text syllables or words synchronized with the track's events.

Each of these is stored in a dedicated list object (e.g., `NoteList`, `ControlChangeList`) that offers efficient storage and manipulation.

## Metadata

Beyond the event lists, a `Track` holds identifying metadata:

-   **`name`**: A string identifier (e.g., "Piano Left Hand").
-   **`program`**: The MIDI program number (0-127) suggesting the instrument sound.
-   **`is_drum`**: A boolean flag indicating if the track uses MIDI percussion mapping (often channel 10).

## Time Unit Context

A `Track` itself doesn't define its time unit; it operates within the time unit (`Tick`, `Quarter`, or `Second`) of the `Score` it belongs to. All `time` and `duration` attributes of events within the track are interpreted according to the parent `Score`'s `ttype`. Converting the `Score`'s time unit automatically converts all contained tracks and events.

## Relationship to Score

A `Score` acts as the container for one or more `Track` objects. While tracks hold the specific performance data (notes, CCs, etc.), the `Score` provides the global context (tempo, time signatures, key signatures) needed to fully interpret the track data. For example, calculating the absolute start time in seconds of a note within a track requires accessing the tempo map stored in the `Score`.

## Working with Tracks (Conceptual Flow)

1.  **Access**: Obtain `Track` objects from a `Score`'s `tracks` list (e.g., `score.tracks[0]`).
2.  **Inspect**: Examine the track's metadata (`name`, `program`, `is_drum`) and its event lists (`notes`, `controls`, etc.).
3.  **Manipulate**: Modify the track directly (e.g., `track.shift_pitch(12)`) or modify its event lists (e.g., `track.notes.append(...)`, `track.controls.sort()`).
4.  **Analyze**: Calculate track-specific features (e.g., note density within the track, pitch range used).
5.  **Structure**: Add, remove, or reorder tracks within the `Score.tracks` list. 
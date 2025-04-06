# Events Concept

Events are the fundamental, time-stamped units of information in Symusic. They represent specific musical occurrences or annotations within a `Track` or globally within a `Score`.

## Purpose and Role

Events capture the discrete actions and descriptive markers that make up a piece of symbolic music. They form the lowest level of the data hierarchy, contained within lists inside `Track` and `Score` objects.

[See Events API Reference](../api_reference/events.md) for detailed usage of each event type.

## Common Characteristics

All event types share key features:

1.  **`time` Attribute**: Indicates when the event occurs, relative to the start of the score, measured in the parent `Score`'s time unit (`Tick`, `Quarter`, or `Second`).
2.  **Time Unit Parameterization**: Event classes are templates based on the time unit (e.g., `NoteTick`, `TempoQuarter`, `ControlChangeSecond`).
3.  **Specific Attributes**: Each type has attributes holding its relevant data (e.g., a `Note` has `pitch`, `duration`, `velocity`; a `Tempo` has `qpm`).
4.  **Containment**: Events live inside specialized lists within `Track` or `Score` objects (e.g., `Track.notes`, `Score.tempos`).

## Overview of Event Types

Here's a conceptual overview of the main event types:

-   **`Note`**: The most common event, representing a pitched sound with timing, duration, and intensity.
-   **`ControlChange` (CC)**: Generic MIDI control messages for expressive details (volume, pan, modulation, pedal status, etc.).
-   **`Tempo`**: Global event defining the playback speed (Quarter Notes Per Minute) from its `time` onwards.
-   **`TimeSignature`**: Global event defining the musical meter (e.g., 4/4) from its `time` onwards.
-   **`KeySignature`**: Global event defining the musical key (sharps/flats and major/minor) from its `time` onwards.
-   **`PitchBend`**: Track-specific event for smooth pitch variations.
-   **`Pedal`**: Track-specific, simplified representation of sustain pedal usage (time and duration).
-   **`TextMeta`**: Textual information linked to a specific time. Used for lyrics within tracks (`Track.lyrics`) and markers globally (`Score.markers`).

## Global vs. Track Events

Understanding the scope of an event is crucial:

-   **Track Events** (inside `Track.notes`, `Track.controls`, etc.): Define the performance of a specific instrument/part. Examples: `Note`, `ControlChange`, `PitchBend`, `Pedal`, Lyrics (`TextMeta`).
-   **Global Events** (inside `Score.tempos`, `Score.time_signatures`, etc.): Define the overall context for all tracks. Examples: `Tempo`, `TimeSignature`, `KeySignature`, Markers (`TextMeta`).

## Working with Events (Conceptual Flow)

1.  **Access**: Get event lists from `Score` or `Track` objects (e.g., `track.notes`, `score.tempos`).
2.  **Iterate/Inspect**: Loop through lists to examine individual event attributes.
3.  **Modify**: Change attributes of existing events (e.g., `note.velocity = 100`).
4.  **Add/Remove**: Append new events or remove existing ones from their lists (e.g., `track.notes.append(...)`, `score.tempos.pop(0)`).
5.  **Sort**: Use the `sort()` method on event lists to maintain chronological order after modifications.

Remember that an event's `time` and `duration` (if applicable) are always interpreted based on the `ttype` of the parent `Score`. 
# Score Concept

The `Score` class is the central container in Symusic, representing a complete musical piece, analogous to a full score in music notation. It serves as the primary object for loading, manipulating, and saving musical data.

## Purpose and Role

The `Score` object aggregates all the necessary information to define a piece of music:

-   **Structure**: Holds multiple `Track` objects, typically representing different instruments or voices.
-   **Global Context**: Contains events that apply to the entire piece, such as tempo changes, time signatures, and key signatures.
-   **Metadata**: Stores information like markers and the fundamental time resolution (`ticks_per_quarter`).

It acts as the root of the musical data hierarchy and provides methods for operations that affect the entire piece.

[See Score API Reference](../api_reference/score.md) for detailed usage.

## Key Components

-   **Tracks (`tracks`)**: A list (`TrackList`) containing `Track` objects. This is where most note-level data resides.
-   **Tempo Changes (`tempos`)**: A list (`TempoList`) defining the speed of the music over time.
-   **Time Signatures (`time_signatures`)**: A list (`TimeSignatureList`) defining the meter (e.g., 4/4, 3/4) over time.
-   **Key Signatures (`key_signatures`)**: A list (`KeySignatureList`) defining the key (e.g., C major, A minor) over time.
-   **Markers (`markers`)**: A list (`TextMetaList`) for textual annotations at specific time points.
-   **Ticks Per Quarter (`ticks_per_quarter` / `tpq`)**: An integer defining the time resolution in ticks for tick-based time units. Crucial for interpreting MIDI timing.

## Time Units Concept

A `Score` object (and its contained tracks/events) operates within a specific time unit system: `Tick`, `Quarter`, or `Second`. This is a core concept explained in detail [here](./time_units.md).

The `Score`'s time unit determines how the `time` and `duration` attributes of all its events are interpreted. You can convert a score between time units using the `to()` method (e.g., `score.to("second")`).

## Conceptual Model

Think of a `Score` as the root of a tree:

```
Score (e.g., ScoreTick)
│
├── ticks_per_quarter: int
│
├── Tracks (TrackList<TrackTick>)
│   ├── Track 1 (TrackTick)
│   │   ├── notes: NoteList<NoteTick>
│   │   ├── controls: ControlChangeList<ControlChangeTick>
│   │   └── ... (pitch_bends, pedals, lyrics)
│   └── Track 2 (TrackTick)
│       └── ...
│
├── Tempos (TempoList<TempoTick>)
│   ├── Tempo Event 1
│   └── ...
│
├── Time Signatures (TimeSignatureList<TimeSignatureTick>)
│   └── ...
│
├── Key Signatures (KeySignatureList<KeySignatureTick>)
│   └── ...
│
└── Markers (TextMetaList<TextMetaTick>)
    └── ...
```

## Working with Scores (Conceptual Flow)

1.  **Load/Create**: Instantiate a `Score` by loading a file (`Score("file.mid")`) or creating an empty one (`Score(tpq=480)`).
2.  **Inspect**: Examine tracks, global events, and metadata.
3.  **Manipulate**: Apply transformations (e.g., `shift_pitch`, `clip`, `sort`) or modify event lists directly.
4.  **Convert (Optional)**: Change time units using `to()` if needed for analysis or synthesis.
5.  **Save/Use**: Save back to a file (`dump_midi`) or use for further processing (e.g., synthesis, feature extraction).

This flow highlights the `Score` as the central object managing the musical data throughout its lifecycle. 
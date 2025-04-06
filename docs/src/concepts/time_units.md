# Time Units Concept

Time representation is a fundamental concept in symbolic music processing. Symusic offers flexibility by supporting multiple **time units**, allowing users to work with timings in the system that best suits their specific task, whether it's preserving MIDI fidelity, analyzing musical structure, or synchronizing with audio.

## Purpose and Role

Different applications benefit from different ways of measuring musical time:

-   **MIDI Editing/Preservation**: Requires the exact tick-level timing defined in the MIDI file.
-   **Musical/Rhythmic Analysis**: Benefits from time measured relative to musical durations (like quarter notes), abstracting away tempo variations.
-   **Audio Synthesis/Synchronization**: Needs time measured in absolute seconds to align with real-world time and audio samples.

Symusic's time unit system directly addresses these varying requirements.

[See Time Units API Reference](../api_reference/time_units.md) for detailed usage.

## The Three Time Units Explained

1.  **`Tick`**: Integer values (`int`). Represents the finest time resolution specified in the MIDI file header (`Score.ticks_per_quarter`). This is the **raw timing** from the source file, essential for lossless representation.
2.  **`Quarter`**: Floating-point values (`float`). Represents time relative to the duration of a **quarter note**, where `1.0` equals one quarter note. This unit is useful for analyzing musical structure (e.g., a half note has duration `2.0`) independent of tempo.
3.  **`Second`**: Floating-point values (`float`). Represents **absolute time** in seconds from the beginning of the piece. Calculating this requires considering the `Score`'s tempo map, as the duration of a tick or quarter note in seconds changes with tempo.

## How Time Units are Applied (`TType`)

Symusic uses a generic programming approach (C++ templates / Python `TypeVar`) where core data structures like `Score`, `Track`, and event types (`Note`, `Tempo`, etc.) are parameterized by the time unit (`TType`).

This means you interact with specific versions like:
- `ScoreTick`, `TrackTick`, `NoteTick`
- `ScoreQuarter`, `TrackQuarter`, `NoteQuarter`
- `ScoreSecond`, `TrackSecond`, `NoteSecond`

The `ttype` property of these objects indicates their current time unit.

## Conversion Between Units

The `to()` method is the standard way to convert Symusic objects between time units:

```python
score_tick = Score("file.mid")
# Convert Score and all contained events to Second time unit
score_second = score_tick.to("second") 
# Convert back to Tick
score_tick_again = score_second.to("tick") 
```

**Conversion Logic Overview:**

-   **Tick <-> Quarter**: Relatively simple scaling based on `ticks_per_quarter`.
-   **Tick <-> Second**: Requires processing the `Score`'s tempo map to relate ticks to seconds at different points in time. This is more computationally intensive.
-   **Quarter <-> Second**: Usually involves an intermediate conversion through `Tick`.

**Important Considerations:**

-   Conversions involving `Second` depend on the `Tempo` events in the `Score`.
-   Converting from float (`Quarter`, `Second`) to int (`Tick`) involves rounding, which can introduce minor precision differences.
-   The `min_dur` parameter in `to("tick", min_dur=...)` is useful to prevent notes/pedals from collapsing to zero duration due to rounding.

## Choosing and Using Time Units

-   **Default**: Loading a MIDI file typically defaults to `Tick`.
-   **Analysis**: Use `Quarter` for rhythm/meter analysis, `Second` for duration/tempo analysis.
-   **Synthesis**: Convert to `Second` for accurate audio rendering.
-   **Saving MIDI**: Convert back to `Tick` before calling `dump_midi` for lossless saving (though `dump_midi` can often handle the conversion automatically).

Understanding the active time unit is crucial for interpreting `time` and `duration` values and the effect of time-based operations like `shift_time`. 
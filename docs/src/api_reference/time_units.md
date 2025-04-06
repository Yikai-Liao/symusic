# Time Units

Symusic supports multiple ways to represent time within musical events. This flexibility allows users to choose the most appropriate representation for their specific task. Time units are managed through the `TimeUnit` factory and specific time unit classes.

## The `TimeUnit` Factory

The `TimeUnit` object acts as a factory for obtaining specific time unit instances and converting string representations to time unit objects.

```python
from symusic import TimeUnit

# Get specific time unit instances
tick_unit = TimeUnit.tick
quarter_unit = TimeUnit.quarter
second_unit = TimeUnit.second

# Convert from string
unit1 = TimeUnit("tick")      # Returns TimeUnit.tick
unit2 = TimeUnit("quarter")   # Returns TimeUnit.quarter
unit3 = TimeUnit("second")    # Returns TimeUnit.second
```

While you can use string representations like `"tick"` directly in most Symusic functions (e.g., `score.to("quarter")`), using the explicit `TimeUnit` instances (`TimeUnit.tick`, `TimeUnit.quarter`, `TimeUnit.second`) is slightly more performant as it avoids string parsing.

## Available Time Units

Symusic provides three distinct time unit representations:

### 1. `TimeUnit.tick`

- **Description**: Represents time using integer ticks, typically derived directly from the MIDI file's header (ticks per quarter note). This is the most precise representation for preserving the original timing information from a MIDI file.
- **Underlying Type**: `int`
- **Usage**: Ideal for operations that require exact MIDI timing, lossless MIDI saving, or when the original timing resolution is critical.
- **Associated Classes**: `ScoreTick`, `TrackTick`, `NoteTick`, etc.

### 2. `TimeUnit.quarter`

- **Description**: Represents time in terms of quarter notes, using floating-point numbers. A value of `1.0` corresponds to the duration of one quarter note. This representation is often intuitive for musical analysis as it relates directly to musical note values.
- **Underlying Type**: `float`
- **Usage**: Useful for musical analysis based on note durations, comparing relative timings, or tasks where tempo variations are less important than rhythmic structure.
- **Associated Classes**: `ScoreQuarter`, `TrackQuarter`, `NoteQuarter`, etc.

### 3. `TimeUnit.second`

- **Description**: Represents time in absolute seconds, using floating-point numbers. This calculation takes into account the score's tempo changes.
- **Underlying Type**: `float`
- **Usage**: Essential for synchronizing with audio, calculating real-world durations, synthesis, and applications where absolute timing is crucial.
- **Associated Classes**: `ScoreSecond`, `TrackSecond`, `NoteSecond`, etc.

## Time Unit Conversion

Most Symusic objects (like `Score`, `Track`, and individual `Events`) provide a `to()` method to convert between different time units:

```python
from symusic import Score, TimeUnit

# Load score (defaults to tick)
score_tick = Score("path/to/file.mid")

# Convert to quarter note time
score_quarter = score_tick.to(TimeUnit.quarter)
# Or using string:
# score_quarter = score_tick.to("quarter")

# Convert quarter to seconds
score_second = score_quarter.to(TimeUnit.second)

# Convert seconds back to ticks, ensuring minimum duration of 1 tick
score_tick_again = score_second.to(TimeUnit.tick, min_dur=1)
```

When converting *to* `TimeUnit.tick`, the optional `min_dur` parameter can prevent notes or pedals from having zero duration due to rounding during conversions.

## Choosing the Right Time Unit

- Use **tick** for preserving MIDI fidelity and precise timing operations.
- Use **quarter** for rhythm-based analysis and musically intuitive timing.
- Use **second** for audio synchronization, synthesis, and real-world time calculations.

Many operations in Symusic are optimized for specific time units (e.g., synthesis works best with seconds, piano roll requires ticks after resampling). The library handles necessary conversions internally where possible, but understanding the different units helps in choosing the most efficient workflow. 
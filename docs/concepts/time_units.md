# Time Units

Symusic supports three interchangeable ways to represent musical time: `tick`, `quarter`, and
`second`. Choosing the right unit keeps your workflow intuitive—ticks preserve original MIDI timing,
quarters align with musical structure, and seconds integrate with audio.

## The three units

1. **Tick** (`int32`)
   - Raw timing from the MIDI header.
   - Highest precision; best for lossless round-trips and editing at sub-beat granularity.
2. **Quarter** (`float32`)
   - Normalized so that one quarter note equals `1.0`.
   - Ignores tempo fluctuations, making rhythmic analysis easier.
3. **Second** (`float32`)
   - Absolute wall-clock time.
   - Required for synthesis or aligning with audio waveforms.

## Generic `TType`

Score, Track, and every event class are templated by the time unit (`ScoreTick`, `TrackQuarter`,
`NoteSecond`, …). Python factory objects dispatch to the appropriate specialization automatically.

```python
from symusic import Score, TimeUnit

score_tick = Score("piece.mid", ttype="tick")
score_second = score_tick.to(TimeUnit.second)
```

## Conversion rules

- `tick ↔ quarter`: constant scaling using `ticks_per_quarter`.
- `tick ↔ second`: requires integrating the tempo map; Symusic handles this automatically when you
  call `.to("second")` or `.render()`.
- `quarter ↔ second`: typically routed through ticks internally.

When converting from floats back to ticks, pass `min_dur` to prevent rounding very short events down
to zero.

## Choosing an appropriate unit

| Goal | Recommended unit |
| --- | --- |
| Edit MIDI with maximum fidelity | `tick` |
| Analyze rhythm, quantize, histogram beats | `quarter` |
| Synthesize audio, align with wave files | `second` |

Regardless of the unit you pick, `score.ttype` tells you the active domain, and downstream APIs honor
it automatically. Read more in the :doc:`../api/_autosummary/symusic.TimeUnit` entry.

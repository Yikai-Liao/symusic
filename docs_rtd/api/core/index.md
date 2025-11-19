(core-low-level-api)=
# Low-Level symusic.core API

This page is generated directly from the docstrings that live next to the nanobind bindings under `py_src/bindings`. Run `python docs_rtd/scripts/sync_core_docs.py` after editing those docstrings to refresh the content.

## Time Units

### symusic.core.Tick

Discrete tick-based time unit that mirrors the raw MIDI resolution.

### symusic.core.Quarter

Musical quarter-note time unit derived from the score's ticks-per-quarter setting.

### symusic.core.Second

Real-time second based time unit for wall-clock aligned workflows.

## Scores and Tracks

### symusic.core.ScoreTick

Score represents a collection of tracks, metadata, and events tied to a ticks-per-quarter resolution. Use it to inspect, transform, or serialize musical data within Symusic.

### symusic.core.ScoreQuarter

Score represents a collection of tracks, metadata, and events tied to a ticks-per-quarter resolution. Use it to inspect, transform, or serialize musical data within Symusic.

### symusic.core.ScoreSecond

Score represents a collection of tracks, metadata, and events tied to a ticks-per-quarter resolution. Use it to inspect, transform, or serialize musical data within Symusic.

### symusic.core.TrackTick

Track gathers all per-channel events (notes, controllers, pedals, pitch bends, metadata) alongside the MIDI program/is_drum flags so you can mutate a single instrument lane.

### symusic.core.TrackQuarter

Track gathers all per-channel events (notes, controllers, pedals, pitch bends, metadata) alongside the MIDI program/is_drum flags so you can mutate a single instrument lane.

### symusic.core.TrackSecond

Track gathers all per-channel events (notes, controllers, pedals, pitch bends, metadata) alongside the MIDI program/is_drum flags so you can mutate a single instrument lane.

## Event Primitives

### symusic.core.NoteTick

Note represents a pitched event with duration and velocity anchored to a time unit.

### symusic.core.ControlChangeTick

ControlChange carries a MIDI controller number/value pair emitted at a timestamp.

### symusic.core.TempoTick

Tempo stores milliseconds-per-quarter (or derived QPM) at a given timestamp.

### symusic.core.PedalTick

Pedal captures sustain/soft pedal gestures by pairing a start time with a duration.

### symusic.core.PitchBendTick

PitchBend holds the wheel value used for continuous pitch modulation.

### symusic.core.KeySignatureTick

KeySignature fixes the tonal center at its timestamp via key offset and tonality.

### symusic.core.TimeSignatureTick

TimeSignature records meter changes with numerator/denominator tied to time.

### symusic.core.TextMetaTick

TextMeta attaches arbitrary lyrics/markers/comments along the score timeline.

## Utilities

### symusic.core.Synthesizer

High-level Prestosynth wrapper that renders Scores with SF2/SF3 SoundFonts. Reuse a single synthesizer per SoundFont to avoid disk I/O.


# Score

`Score` is the root container in Symusic. It mirrors the full musical piece, holding global context
(tempo, meter, key), metadata (markers, ticks-per-quarter), and an ordered list of `Track` objects.
Think of it as the DAW session or orchestral score that everything else plugs into.

## At a glance

- Aggregates every track plus global events.
- Stores `ticks_per_quarter`, which defines how `tick` time maps to musical durations.
- Provides transformations that affect the entire piece: clip/trim, sort, time/pitch/velocity shift,
  resample, convert between time units, export piano rolls, dump MIDI/ABC, synthesize, etc.

## Anatomy of a Score

```text
Score (e.g., ScoreTick)
│
├── ticks_per_quarter: int
├── tracks: TrackList[Track<T>]
├── tempos: TempoList
├── time_signatures: TimeSignatureList
├── key_signatures: KeySignatureList
└── markers: TextMetaList
```

- **Tracks** – Per-instrument containers; see {doc}`track`.
- **Tempos / Time signatures / Key signatures** – Global events affecting every track.
- **Markers** – Free-form text meta events.

Each list is backed by C++ `pyvec` objects, so slicing, copying, sorting, and filtering stay fast even
for large MIDI files.

## Time units

Every `Score` exists in a single time domain: `tick`, `quarter`, or `second`. The chosen unit governs
how event `time` and `duration` values are interpreted. Use `score.ttype` to inspect the current unit
and `score.to("second")` (or `.to(TimeUnit.quarter)`) to convert the entire hierarchy. See
{doc}`time_units` for deeper discussion.

## Typical workflow

1. **Load or create** – `Score("file.mid")` or `Score(tpq=960)`.
2. **Inspect** – `score.summary()`, `len(score.tracks)`, `score.tempos[0].qpm`, etc.
3. **Manipulate** – call helper methods (`shift_pitch_inplace`, `clip`, `resample`) or edit the lists
   directly.
4. **Convert** – change to a different time unit if downstream tooling needs quarters/seconds.
5. **Export** – `dump_midi`, `dump_abc`, `pianoroll`, `Synthesizer.render`, etc.

Refer to the :doc:`../api/generated/symusic.Score` entry for the full factory/API surface.

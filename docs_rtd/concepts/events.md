# Events

Events are the fundamental pieces of time-stamped information in Symusic. They describe what happens
(e.g., a note starts, tempo changes, lyrics appear) and live inside the lists owned by `Score` and
`Track` objects.

## Shared traits

- **`time`** – When the event occurs relative to the start of the score, expressed in the score's
  current time unit.
- **Time-aware types** – Each event has tick/quarter/second specializations (e.g., `NoteTick`,
  `TempoQuarter`). Python factories dispatch automatically based on `ttype`.
- **Containers** – Events live inside typed lists (`NoteList`, `TempoList`, `ControlChangeList`, …)
  that support slicing, iteration, sorting, filtering, SoA exports, and copying.

## Global vs. track-level events

| Scope | Examples | Lives in |
| --- | --- | --- |
| Global | `Tempo`, `TimeSignature`, `KeySignature`, score-level `TextMeta` markers | `Score.tempos`, `Score.time_signatures`, `Score.key_signatures`, `Score.markers` |
| Track | `Note`, `ControlChange`, `PitchBend`, `Pedal`, lyric `TextMeta` | `Track.notes`, `Track.controls`, `Track.pitch_bends`, `Track.pedals`, `Track.lyrics` |

Understanding scope matters when deciding whether an edit should affect every instrument (tempo map)
or a single part (transpose just the bass line).

## Working with events

1. **Access** a list: `notes = score.tracks[0].notes`, `tempi = score.tempos`.
2. **Inspect** attributes directly (`note.pitch`, `tempo.qpm`).
3. **Mutate** in place or replace items entirely. Lists remain Pythonic but operate on top of C++
   storage.
4. **Sort/filter**: `notes.sort(key=lambda n: (n.time, n.pitch))`, `notes.filter(lambda n: n.velocity > 60)`.
5. **Vectorize**: `notes.numpy()` returns a mapping of NumPy arrays suitable for ML pipelines.

Find type-specific APIs under :doc:`../api/generated/symusic.Note`,
:doc:`../api/generated/symusic.ControlChange`, and the related entries.

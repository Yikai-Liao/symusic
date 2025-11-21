# Track

A `Track` represents a single musical part inside a `Score`. It groups instrument-specific events and
metadata so you can process each part independently (transpose only the strings, quantize drums, etc.).

## What lives in a Track?

- **Notes** – `NoteList` containing note-on/off events with pitch, duration, velocity.
- **Control changes** – MIDI CC data (volume, pan, modulation, sustain pedal, …).
- **Pitch bends** – High-resolution pitch deviations.
- **Pedals** – Convenience representation of sustain pedal regions.
- **Lyrics/Text** – `TextMetaList` for per-track lyrics or annotations.

Each list exposes list-like methods plus helpers such as `.start()`, `.end()`, `.empty()`, `.sort()`,
`.filter()`, `.adjust_time()`, `.numpy()`, etc.

## Track metadata

- `name`: human-readable label, often populated from MIDI track names.
- `program`: MIDI program number (0–127) guiding synthesizer instrument selection.
- `is_drum`: marks percussion tracks (GM channel 10 semantics) so renderers choose drum kits.

## Time unit inheritance

Tracks do not own their own time units; they inherit the `Score`'s `ttype`. Converting a `Score`
between ticks/quarters/seconds updates every track and event automatically.

## Workflow tips

1. Access tracks via `score.tracks[i]` or iterate with `for track in score.tracks`.
2. Use `track.copy(deep=True)` when you need to branch edits without touching the original object.
3. Call `track.sort()` after manual edits to keep events in chronological order.
4. Use specialized helpers: `track.shift_pitch(offset)`, `track.shift_time(offset)`,
   `track.adjust_time(original_times, new_times)`, etc.

For signatures and constructor arguments, see :doc:`../api/_autosummary/symusic.Track`.

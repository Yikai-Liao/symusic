# MusicXML I/O Design

This page explains how Symusic integrates MusicXML through the `Score` API, why the current
implementation is more elaborate than the MIDI and ABC paths, and where the algorithmic and
constant-factor costs come from.

It is an explanation document, not an API reference. The public surface is intentionally small:
`Score(path)`, `Score.from_file(...)`, `Score.from_musicxml(...)`, `score.dump_musicxml(...)`, and
`score.dumps_musicxml()`. The complexity lives in the C++ core, inside
`src/io/musicxml.cpp`.

## Design goals

The MusicXML backend is built around four constraints:

- Preserve the `Score` contract rather than the full MusicXML document model.
- Keep the implementation in the pure C++ core layer, with no nanobind or Python coupling.
- Prefer limited, targeted `mx` changes over a large invasive fork when the existing API cannot
  carry required `symusic` semantics such as per-note velocity, onset lyrics, and exact tempo text.
- Preserve semantic musical data first: note timing, duration, pitch, velocity, track metadata,
  tempos, time signatures, key signatures, and lyric text/timing.

This immediately implies that the backend is not a generic engraving-preserving roundtrip engine.
MusicXML can represent much more notation state than `Score` can store. The Symusic backend
therefore performs a controlled reduction on import and a controlled reconstruction on export.

## What "semantic Score interchange" means

MusicXML is notation-oriented. `Score` is performance- and analysis-oriented.

The current backend maps between them with the following contract:

- One MusicXML part becomes one `Track`.
- Multiple staves and voices inside one part are flattened into that `Track`.
- Tied fragments are merged into one `Note` whenever they form a continuous note in time.
- Lyric text is reduced to track-level timed text events; exact note attachment is not preserved by
  the current `symusic` data model.
- Engraving-specific data such as layout, page geometry, many directions, ornaments, slurs, and
  other notation details are outside the v1 contract.

The most important consequence is that Symusic does not treat MusicXML as a lossless syntax tree.
It treats MusicXML as a source or sink for `Score` semantics.

## Why the implementation is non-trivial

`mx` already parses and writes MusicXML, but its native abstraction is still closer to a notation
document than to Symusic's `Score`. A direct "parse with mx, dump with mx" bridge is not sufficient
for the current contract because Symusic needs to:

- flatten part/staff/voice structure into `Track` objects,
- merge tied note fragments into stable `Note` events,
- deduplicate duplicated global events that often appear across staves or parts,
- rebuild a measure layout from `Score` data before exporting,
- split long notes back across measure boundaries with ties,
- reject default export cases that would require heuristic voice reconstruction, and
- carry exact note velocity, onset lyrics, and decimal tempo text through a small vendored `mx`
  fork instead of post-processing the generated DOM.

That is why the backend has both a model-building phase and a small amount of `mx` compatibility
work, even though the final export path no longer uses a Symusic-side DOM patch pass.

## End-to-end pipeline

The core path is:

1. Parse MusicXML bytes into `mx::core::Document`.
2. Convert timewise documents into partwise form.
3. Read `mx::api::ScoreData` with `mx::impl::ScoreReader`.
4. Import each part into a flattened `Track<Tick>`.
5. Canonicalize global events into a stable `Score<Tick>`.
6. Convert to `Quarter` or `Second` only after the tick-domain import is complete.
7. For export, convert any non-tick score back to `Tick`.
8. Canonicalize globals again, build measure layouts, split notes into exportable segments, and
   populate `mx::api::ScoreData`.
9. Ask `mx::impl::ScoreWriter` to build a partwise MusicXML DOM.

The implementation is organized around that split:

- import path: `parse_musicxml_document`, `import_musicxml_tick_score`,
  `import_musicxml_track_tick`
- normalization path: `canonicalize_*`, `build_measure_layouts`
- export path: `split_track_into_segments`, `export_score_to_musicxml_data`

## Import algorithm

### 1. Parse and normalize the document shape

`parse_musicxml_document(...)` builds an `mx` document from raw bytes and immediately converts
timewise MusicXML to partwise form. Symusic keeps only one downstream import shape: partwise
documents with a usable `ScoreData` snapshot and a `ScorePartwise` DOM.

This stage also sanitizes the ticks-per-quarter value. MusicXML files occasionally omit or damage
that field, so Symusic falls back to a stable default TPQ instead of letting the rest of the import
pipeline operate on zero or invalid timing scales.

### 2. Import one part at a time

`import_musicxml_track_tick(...)` is the core reducer from notation-oriented MusicXML to a flattened
`Track<Tick>`.

For each measure, Symusic:

- updates the active time-signature stream,
- collects key signatures and tempo directions from both measure-level and staff-level structures,
- traverses note/back-up/forward events with `mx::impl::MeasureCursor`,
- converts note pitch spelling to MIDI pitch,
- reads note dynamics into velocity,
- collects lyrics from the note's lyric elements,
- merges tied note fragments into a single `Note` when the fragments are continuous.

The traversal relies on `traverse_measure_notes(...)`, which walks the MusicXML measure in document
order while maintaining a cursor with tick time, voice index, staff index, and divisions.

### 3. Merge ties during import

MusicXML commonly encodes one sustained note as several tied fragments. `Score` should not expose
that as several unrelated notes. Symusic therefore keeps an `active_ties` map keyed by:

- staff index
- voice number
- pitch

When a tie stop lands exactly at the expected continuation time, Symusic extends the active note
instead of emitting a new note. If a tie chain ends, the merged note is finally written to the
track.

This is one of the most important reductions in the backend: it preserves the `Score` notion of a
note as one continuous event rather than one notation fragment.

### 4. Sort the flattened track

After the measure traversal finishes, imported notes and lyrics are sorted by semantic keys rather
than preserved in raw traversal order. This is deliberate. MusicXML document order contains layout-
driven details that should not leak into the `Score` contract.

## Global-event canonicalization

MusicXML files often duplicate the same tempo, key, or meter information across staves or parts.
That notation is legal and common, but `Score` stores global events only once. Symusic therefore
canonicalizes global streams after import and before export.

The canonicalization rules are intentionally simple:

- sort events by time,
- if two events land at the same time, keep the later one,
- if two nearby events carry the same value, collapse them into one,
- use the active measure length to decide what counts as "nearby".

This is what `canonicalize_musicxml_global_events(...)` and the specialized helpers do.

The goal is not to infer editorial intent. The goal is to turn notation-local duplication into a
stable global stream that roundtrips well through `Score`.

Maintainer direction:

- this strategy is now considered too aggressive for time signatures
- the intended v1 contract is to deduplicate repeated time signatures only when their timestamps are
  exactly identical
- broader near-window collapsing remains under review for other global event categories

## Export algorithm

### 1. Convert to tick space and canonicalize again

The exporter always works from `Score<Tick>`, even if the public object is in quarter or second
time. This keeps all later steps integer-based and avoids repeated floating-point quantization
during segmentation and measure placement.

Before building measures, Symusic canonicalizes tempos, time signatures, and key signatures again.
This matters because the in-memory `Score` may have been created from another source such as MIDI,
and MusicXML export still needs a stable, non-duplicated global stream.

### 2. Build the measure layout

`build_measure_layouts(...)` reconstructs a linear list of measures from the canonicalized
time-signature stream and the maximum event time.

The measure builder supports:

- a leading implicit span before the first explicit time signature,
- shortened measures when a time-signature change lands inside the nominal measure,
- at least one measure even for otherwise empty scores.

This layout drives every later export decision: where notes are split, where key and tempo events
are placed, and where `implicit=yes` must be emitted.

Maintainer direction:

- this reconstruction behavior is currently accepted as an implementation detail, but it should stay
  prominently documented
- future revisions should compare this behavior against peer libraries such as music21 and
  MuseScore before treating it as settled policy

### 3. Reject default export cases that need heuristic voices

`Track` has no explicit voice structure, but partwise MusicXML does. Symusic reconstructs voices
only for the trivial case where all simultaneous notes form chords with exactly matching
`(start, end)` spans.

The exporter first groups notes with the same `(start, end)` interval into chord clusters. If those
clusters overlap in time, the track would need heuristic voice reconstruction, so default v1 export
raises an explicit error instead of inventing a voice layout.

The supported default case is intentionally narrow:

- it is deterministic,
- it preserves true chords,
- it does not try to infer human engraving choices.

When export succeeds, every emitted note goes through a single explicit voice stream.

Maintainer direction:

- this heuristic voice reconstruction is not intended to be the default long-term export behavior
- a future version may expose it as an opt-in export mode after deeper study of `mx`, music21, and
  MuseScore behavior
- until then, cases that require heuristic voice reconstruction should be treated as outside the
  default v1 export contract and should raise an explicit export error rather than silently
  synthesizing voices
- the future export option name and interface are still TBD, so code comments and user-facing error
  messages must say that the heuristic mode is not yet exposed and must be kept in sync when that
  option is added later

### 4. Split notes into measure-bounded segments

MusicXML note durations cannot cross measure boundaries as one raw event. Long notes must be split
into one segment per covered measure and tied together.

`split_track_into_segments(...)` performs this segmentation. For each source note it:

- finds the containing measure,
- cuts the note at the current measure end,
- records tie start and tie stop flags,
- uses a standard duration plus dots when one exists,
- otherwise leaves the MusicXML `<duration>` as the authoritative spelling and omits `<type>` /
  dot metadata instead of patching the written DOM afterwards.

This stage is where the exporter deliberately prefers note stability over notation cleverness.
Symusic never splits notes just to preserve a lyric timestamp on a tied continuation. Instead,
continuation fragments are conservatively merged onto the surviving note onset, so lyric text is
preserved even when the original fragmentation is not.

### 5. Build `ScoreData` for `mx::impl::ScoreWriter`

`export_score_to_musicxml_data(...)` converts the canonicalized global events, measure layout, track
metadata, and note segments into `mx::api::ScoreData`.

That includes:

- one `PartData` per `Track`,
- measure-level time signatures, keys, and tempo directions,
- one synthesized staff per track in v1,
- one exported voice stream per track in the default v1 mode,
- note-level dynamics and onset lyrics carried directly in `mx::api::NoteData`, and
- exact decimal tempo text carried directly in `mx::api::TempoData`.

Maintainer direction:

- raw MusicXML parts that do not reconcile with the part-list should not force whole-document import
  failure in v1; matched parts should be imported and unmatched extras ignored silently because the
  current `symusic` import surface has no warning-reporting channel
- empty track names should remain empty rather than being rewritten to synthesized fallback names by
  default

This function prepares enough structure for `ScoreWriter` to emit the final MusicXML directly. The
remaining Symusic-specific detail lives in the `mx` fork rather than in a second DOM traversal.

## Why the vendored `mx` fork still matters

The Symusic exporter no longer patches the written MusicXML tree after `ScoreWriter` runs.
However, the vendored `mx` fork still matters because upstream `mx::api` did not expose enough
note-level state for Symusic's contract.

The local fork now teaches `mx::api::NoteData` / `mx::api::TempoData` and the corresponding writer
code to carry:

- exact note velocity via note dynamics attributes,
- onset-attached lyrics for the selected exported note, and
- decimal tempo strings without integer rounding.

This keeps the final export path single-pass on the Symusic side: build `ScoreData`, ask
`ScoreWriter` for a partwise DOM, serialize.

## Time complexity

Let:

- `B` be the size of the XML input or output text,
- `P` be the number of parts,
- `M` be the number of exported or imported measures,
- `N` be the number of logical `Score` notes,
- `S` be the number of exported note segments after splitting at measure boundaries,
- `G` be the total number of global events considered for canonicalization,
- `L` be the number of lyric items,
- `V` be the number of simultaneously active reconstructed voices in one track.

### Import

The import path is dominated by:

- XML parsing and DOM construction: `O(B)`
- measure traversal: `O(M + N + L)`
- per-track note and lyric sorting: `O(N log N + L log L)`
- canonicalization of global events: `O(G log G)`

So the practical import bound is:

```text
O(B + M + N log N + L log L + G log G)
```

The tie map operations are logarithmic in the number of active tied notes, but that set is small in
real scores and does not change the dominant bound.

### Export

The export path is more expensive because it reconstructs notation structure:

- global-event canonicalization: `O(G log G)`
- measure-layout construction: `O(M)`
- chord clustering and overlap validation: `O(N log N)`
- note segmentation and segment sorting: `O(S log S)`
- final XML serialization: `O(B)`

So the practical export bound is:

```text
O(B + G log G + M + N log N + S log S)
```

## Constant-factor costs

The asymptotic bounds hide the real reason the MusicXML backend feels heavier than MIDI.

The main constant costs are:

- `mx` builds a full MusicXML DOM on parse and on write.
- Export allocates several temporary structures:
  - measure layouts
  - chord clusters
  - note segments
  - lyric maps
- The vendored `mx` writer still builds a notation-oriented DOM even when Symusic only needs
  semantic interchange.

In other words, the backend is not expensive because of one pathological algorithm. It is expensive
because MusicXML export is a multi-stage reconstruction pipeline with a DOM writer in the middle.

## Concurrency and free-thread considerations

The Symusic-side backend avoids obvious shared mutable state:

- each parse or dump call creates its own `mx` document, score reader, score writer, and temporary
  vectors,
- Symusic does not use `mx::api::DocumentManager`,
- all MusicXML logic stays in the C++ core layer.

This means the Symusic algorithm itself is reentrant. It does not prove that every dependency is
free-thread-safe under every runtime, but it does mean the backend is not serialized by a Symusic
global lock.

In practice, multi-thread scaling is more likely to hit memory bandwidth, allocator pressure, and
DOM construction overhead before it hits a Symusic-side lock.

## Non-goals and known limitations

The current backend intentionally does not try to do all of the following:

- preserve compressed `.mxl` containers,
- preserve full engraving or layout state,
- preserve arbitrary text directions or rich notation objects,
- preserve the original timing and fragmentation of lyrics that were attached to tied continuations,
- infer aesthetically optimal voice spelling or beaming,
- act as a generic MusicXML editor.

Those are not accidental gaps. They follow directly from the choice to keep MusicXML support as a
`Score` interchange layer rather than a notation-document subsystem.

## Practical reading guide for the code

If you want to trace the implementation in `src/io/musicxml.cpp`, this order mirrors the actual
design:

1. `parse_musicxml_document(...)`
2. `traverse_measure_notes(...)`
3. `import_musicxml_track_tick(...)`
4. `canonicalize_*` helpers
5. `build_measure_layouts(...)`
6. `group_track_chords(...)`
7. `split_track_into_segments(...)`
8. `export_score_to_musicxml_data(...)`
9. `dump_musicxml_score(...)`

Reading the file in that order is usually easier than reading it top to bottom and trying to infer
the whole contract from local helper functions.

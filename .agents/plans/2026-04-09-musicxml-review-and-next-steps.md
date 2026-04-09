# MusicXML Static Review And Next Steps

Date: 2026-04-09
Repository: `/home/lyk/code/symusic`
Branch: `feat/mx`
Reference HEAD: `95f38e92189f6e7e5a541e61f0ff8bdfa12ee202`
Baseline for review: `main...HEAD`
Review mode: static review only

Staleness note:

- this document reflects the repository state around the `Reference HEAD` above
- before using this plan later, review the current diff against the reference revision, especially
  for:
  - `src/io/musicxml.cpp`
  - `tests/test_musicxml.py`
  - `docs/concepts/musicxml_io.md`
  - `.agents/plans/2026-04-09-musicxml-review-and-next-steps.md`
- suggested command:
  `git diff 95f38e92189f6e7e5a541e61f0ff8bdfa12ee202..HEAD -- src/io/musicxml.cpp tests/test_musicxml.py docs/concepts/musicxml_io.md .agents/plans/2026-04-09-musicxml-review-and-next-steps.md`

## Scope

This note records the static review focused on the new MusicXML branch work, especially the
concerns raised during review:

- style drift from established `symusic` C++ code
- tests that encode implementation quirks instead of user-facing contract
- compatibility or fallback code added mainly to cover previous implementation mistakes
- unnecessary code volume and avoidable complexity

Primary files reviewed:

- `src/io/musicxml.cpp`
- `tests/test_musicxml.py`
- `include/symusic/io/musicxml.h`
- `py_src/bindings/score/score_bindings.h`
- `python/symusic/factory.py`
- existing style references from `src/io/midi.cpp`, `src/io/common.cpp`, and `include/symusic/*`

## Executive Summary

The current branch is not blocked by obvious memory-safety or basic functional correctness issues.
Those concerns were already checked in the previous pass.

The real review problems are different:

1. parts of the test suite weaken the intended MusicXML contract in order to keep the suite green
2. the export implementation uses a heavy two-phase bookkeeping and patch pipeline that is larger
   and more indirect than the surrounding `symusic` style
3. there is at least one concrete correctness risk in the patch bookkeeping for duplicate notes
4. the file contains dead scaffolding and repeated branches that look like unfinished AI-generated
   structure rather than deliberate library code

In short: the branch is promising as a feature spike, but it is not yet in a shape that should set
the long-term style for `symusic`.

## Decisions Confirmed By Maintainer

The following product and architecture decisions have now been explicitly confirmed:

- MIDI remains the first-class format in `symusic`; MusicXML is supported as semantic interchange,
  not as a notation-fidelity subsystem.
- Full MusicXML engraving or layout fidelity is not a project goal.
- `part -> Track` is an acceptable semantic reduction.
- empty track names should remain empty; the exporter should not synthesize `Track N` names by
  default.
- tied fragments should still collapse into one `Note`.
- v1 only promises lyric text and lyric timing preservation, not exact note attachment fidelity.
- control changes, pitch bends, pedals, and similar non-note musical streams are deferred and do not
  belong in the first usable MusicXML version.
- v1 should tolerate unmatched raw MusicXML parts by importing only the parts that reconcile
  successfully with the part-list, while ignoring unmatched extras silently because `symusic` does
  not currently provide a warning channel for this import path.
- time-signature deduplication should only collapse events with exactly identical timestamps; the
  broader near-window collapse is considered too aggressive for meter.
- channel/program export policy can follow MIDI-oriented pragmatism.
- targeted `mx` changes are acceptable when justified; avoiding a very large invasive fork is still
  desirable, but limited `mx` API evolution is no longer considered off-limits.
- exact per-note velocity remains required.
- heuristic voice reconstruction should not be the default v1 export behavior; future versions may
  offer an opt-in export mode informed by deeper `mx`, music21, and MuseScore study, but the
  default path should error on cases that would require such heuristic reconstruction.
- the exact export option surface for future heuristic voice reconstruction is not settled yet; code
  comments and error messages should explicitly say that the option name/interface is still TBD,
  that the current export path intentionally rejects those cases, and that the wording must be kept
  in sync when the future option is introduced later.

## Architectural Constraint: Lyric-Note Binding Is Lost On Import

This must be treated as a first-order product constraint, not as an implementation detail.

Facts from the current `symusic` model:

- `Track` stores lyrics as `pyvec<TextMeta<T>>`, not as note-owned lyric objects.
- the lyric payload does not contain a note id, note reference, voice binding, or staff binding.
- the current MusicXML import path lowers note lyrics into plain `(time, text)` track events.

Representative locations:

- `include/symusic/track.h:28`
- `src/io/musicxml.cpp:742-747`

Implication:

When MusicXML is imported into `symusic`, the original note-to-lyric attachment is already lost.
Any later attempt to attach lyrics back onto exported MusicXML notes is therefore heuristic
reconstruction, not faithful roundtrip restoration.

Contract consequence:

- lyric text and lyric timing can be part of the supported contract
- exact note attachment of lyrics cannot be a strong contract under the current data model
- tests and docs must stop presenting note-attached lyric reconstruction as guaranteed behavior

## Style Baseline From Existing `symusic` C++

The mature `symusic` C++ code is not ultra-minimal, but it is consistently concrete and direct.

Observed style signals from existing code:

- small local helpers are used when they isolate one narrow concern
- long workflows are usually supported by one or two concrete state structs, not many temporary
  translation layers
- naming is literal and domain-first
- container choice is pragmatic and simple
- control flow is direct and fail-fast
- comments are sparse and local
- the codebase tolerates long functions, but it does not usually build large ad hoc abstraction
  layers inside a single translation unit

Good reference files:

- `src/io/midi.cpp`
- `src/io/common.cpp`
- `include/symusic/track.h`

## Review Findings

### 1. Tests currently weaken the contract in ways that hide real regressions

Severity: Major

Key points:

- `tests/test_musicxml.py` marks `ly41h_TooManyParts.xml` as an expected import failure, even
  though the bundled upstream `mx` import log records that sample as a successful import after
  reporting the XML inconsistency.
- `MX_LOSSY_ROUNDTRIP_CASES` removes three real corpus cases from the strict contract check and
  leaves them with only a weak "dump and parse does not throw" assertion.
- `musicxml_contract_snapshot()` rewrites empty track names into `Track {index + 1}`, which matches
  the current exporter fallback and therefore masks a visible metadata change.
- the parallel conversion test only checks `note_num()`, which is too weak to support any strong
  thread-safety claim.

Representative locations:

- `tests/test_musicxml.py:17-29`
- `tests/test_musicxml.py:81-97`
- `tests/test_musicxml.py:176-201`
- `src/io/musicxml.cpp:1318`
- `3rdparty/mx/Resources/lysuite/FinaleMusicXMLImportLog.txt:77-79`

Assessment:

The test suite is not just incomplete. It currently contains a few places where the asserted
"supported contract" has been softened to match current implementation limitations.

### 2. Export patch bookkeeping has a correctness risk for duplicate notes

Severity: Major

Key points:

- `OutputPatchKey` does not include `velocity`.
- `patch_lookup` stores a queue of `OutputPatchInfo` under that reduced key.
- the note vector is sorted with a key that also omits `velocity`.
- the patch phase later pops from the queue and writes velocity back into the generated DOM.

Representative locations:

- `src/io/musicxml.cpp:126-153`
- `src/io/musicxml.cpp:1361-1377`
- `src/io/musicxml.cpp:1383-1388`
- `src/io/musicxml.cpp:1529-1559`

Assessment:

If two notes share the same measure, local time, duration, pitch, and voice but differ in velocity,
the patch phase becomes order-sensitive. That is a real semantic bug, not just a style complaint.

### 3. The implementation is too indirect compared with the rest of the codebase

Severity: Major

Key points:

- `src/io/musicxml.cpp` is about 1.7k lines and builds a large translation pipeline inside one
  file.
- the exporter moves the same note semantics through multiple local structs:
  `SegmentInfo`, `TrackMeasureNote`, `OutputPatchInfo`, and finally patched DOM nodes.
- the patch phase exists for real reasons, but the current implementation pays for it with a large
  amount of local bookkeeping and duplicated state.

Representative locations:

- `src/io/musicxml.cpp:89-225`
- `src/io/musicxml.cpp:1144-1226`
- `src/io/musicxml.cpp:1242-1429`
- `src/io/musicxml.cpp:1491-1579`

Assessment:

This is the main style-pollution risk. The file is not bad because it is long; it is bad because it
contains too many one-off intermediate representations and too much patch-oriented scaffolding.

### 4. There is visible dead scaffolding and repeated logic

Severity: Minor

Examples:

- unused alias: `shared_track`
- unused struct: `TraversedNote`
- unused field: `MeasureLayout::measure`
- unused helper: `clamp_velocity`
- unused field: `TrackMeasureNote::abs_time`
- duplicated tie-stop branch logic where both arms do the same work

Representative locations:

- `src/io/musicxml.cpp:86-87`
- `src/io/musicxml.cpp:105-108`
- `src/io/musicxml.cpp:114`
- `src/io/musicxml.cpp:156-161`
- `src/io/musicxml.cpp:339`
- `src/io/musicxml.cpp:861-870`

Assessment:

These are cleanup issues, but together they reinforce the impression that the file was grown by
incremental patching instead of being intentionally reduced before review.

### 5. Not all fallback behavior is justified strongly enough yet

Severity: Minor to Major depending on path

Examples:

- strict part reconciliation currently hard-fails the whole import when `parts.size() != core_parts.size()`
- unsupported tempo markings are silently ignored in v1
- several lossy roundtrip cases are acknowledged by comment, but the product boundary is not yet
  expressed precisely enough in tests and docs

Representative locations:

- `src/io/musicxml.cpp:810-817`
- `src/io/musicxml.cpp:967-970`
- `tests/test_musicxml.py:24-29`

Assessment:

Some of these choices may be acceptable product decisions, but they need to be stated as real
contract boundaries. Right now they partly read as implementation escape hatches.

### 6. Several current implementation choices already conflict with confirmed maintainer direction

Severity: Major

Examples:

- time-signature canonicalization currently collapses same-value events within a multi-measure
  window instead of only deduplicating exact same-time duplicates
- import currently hard-fails when reconciled `PartData` count differs from raw `<part>` count,
  instead of tolerating unmatched extras
- export currently synthesizes `Track N` names for empty track names
- export currently reconstructs voices greedily instead of treating that behavior as an opt-in
  future mode

Representative locations:

- `src/io/musicxml.cpp:485-496`
- `src/io/musicxml.cpp:967-970`
- `src/io/musicxml.cpp:1099-1142`
- `src/io/musicxml.cpp:1318`

Assessment:

These are no longer merely open design questions. They are confirmed mismatches between the current
branch behavior and maintainer-approved product direction, so they should be corrected before the
branch is treated as a stable baseline.

## Patch Inventory After Clarifying The Contract

Once the product contract is clarified, the patch story becomes much simpler.

### Patch That Can Be Removed By Contract Choice

`tempo` decimal-string patching should be removable if exported MusicXML is allowed to quantize to
integer BPM. That matches the current `mx::api::TempoData` shape and removes one whole patch path.

### Patch That Should Not Define The Contract

`lyric` reattachment should not be treated as a required semantic patch because the note binding is
already lost when importing into `symusic`. At most this can be best-effort behavior for
unambiguous cases.

### Patch That Is Probably Avoidable With Better Segmentation

The note-type cleanup patch for unsupported raw durations is likely avoidable if export
segmentation is redesigned so every emitted segment has a valid MusicXML type-and-dot spelling, or
is further split until it does.

This is an algorithmic cleanup problem, not an inherent product limitation.

### Last Patch That Really Remains On The Current `ScoreWriter` Route

If the branch still wants to preserve per-note velocity across MusicXML roundtrip while continuing
to rely on `mx::api::ScoreData -> mx::impl::ScoreWriter`, the last genuinely hard patch is note
velocity restoration.

Reason:

- `mx::api::NoteData` and `mx::api::VoiceData` do not expose a direct per-note velocity or dynamics
  field on the API path used by `ScoreWriter`
- `symusic` note velocity is part of the in-memory score contract
- without patching the generated `mx::core::Note`, exported notes fall back to default velocity on
  re-import

Representative locations:

- `src/io/musicxml.cpp:1529-1559`
- `3rdparty/mx/Sourcecode/include/mx/api/NoteData.h:76`
- `3rdparty/mx/Sourcecode/include/mx/api/VoiceData.h:15`

Therefore, if the goal is to eliminate the final remaining patch, there are only two clean options:

1. relax the contract and stop preserving exact per-note velocity through MusicXML
2. stop relying on `mx::impl::ScoreWriter` for final note emission and write the relevant
   `mx::core` note DOM directly

## Recommended Execution Plan

The next step should not be "keep adding support until more corpus files pass." The branch first
needs contract tightening and structural reduction.

### Phase 1. Lock the user-facing contract before refactoring

Goal:

- make the tests describe the intended MusicXML support boundary honestly

Actions:

- remove expectation-masking where the current test suite rewrites or weakens user-visible behavior
- re-evaluate `ly41h_TooManyParts.xml` and decide whether the product contract is tolerant import or
  hard failure; encode that decision explicitly
- replace the current lossy-case smoke tests with narrow assertions that describe exactly what is
  still allowed to differ
- stop normalizing empty track names inside the contract snapshot unless that fallback name is a
  deliberate public contract
- explicitly state that lyric text/time may survive import-export, but exact note attachment does
  not survive import into the current `symusic` model

Acceptance criteria:

- every allowlist entry has a written reason
- no test hides a user-visible metadata mutation by rewriting expected values
- lossy cases are checked with bounded, explicit assertions instead of "does not throw"
- the tests no longer claim exact lyric-note attachment fidelity unless the data model changes

### Phase 2. Fix the concrete patch bookkeeping bug

Goal:

- remove the order-sensitive duplicate-note patch behavior

Actions:

- redesign `OutputPatchKey` or the patch matching strategy so that duplicate same-time notes with
  different velocity remain distinguishable
- add a targeted regression test for duplicated notes sharing time, duration, pitch, and voice

Acceptance criteria:

- duplicate-note velocity patching is deterministic
- the regression test fails on the old logic and passes on the fixed logic

### Phase 3. Reduce exporter indirection and dead local state

Goal:

- bring the implementation closer to existing `symusic` style

Actions:

- delete dead aliases, fields, structs, and helpers
- collapse obviously redundant local structs where one representation can carry the data directly
- remove repeated branches that perform the same work
- narrow the patch phase to only the cases that remain real requirements after contract cleanup,
  with note velocity as the main expected survivor on the current `ScoreWriter` route

Acceptance criteria:

- `src/io/musicxml.cpp` has fewer one-off local types
- the exporter moves note data through fewer temporary representations
- no unused scaffolding remains in the file

### Phase 4. Split responsibilities only where it improves clarity

Goal:

- reduce the "single huge translation unit" cost without replacing it with abstraction noise

Actions:

- identify one or two stable responsibility boundaries, for example:
  - import reduction
  - global-event canonicalization
  - export segmentation and patching
- only extract code when the resulting boundary is concrete and domain-driven

Acceptance criteria:

- extracted helpers have direct musical meaning
- no generic adapter or compatibility layers are introduced

### Phase 5. Record a repository-local style skill for future AI work

Goal:

- prevent future feature work from repeating the same style drift

Actions:

- create a small `.agents` skill or guidance note that captures the actual `symusic` C++ style:
  concrete naming, direct control flow, sparse comments, pragmatic container use, and bias against
  large local abstraction layers
- include examples from `src/io/midi.cpp`, `src/io/common.cpp`, and `include/symusic/*`

Acceptance criteria:

- future AI-assisted C++ changes can be reviewed against an explicit local style baseline instead of
  a generic C++ skill

## Suggested Work Order

Recommended order for the next implementation pass:

1. tighten tests and remove expectation masking
2. update the docs to reflect the confirmed contract decisions above
3. align the implementation with confirmed maintainer direction on:
   - exact-time-only time-signature deduplication
   - tolerant matched-part import
   - empty track-name preservation
   - non-default heuristic voice reconstruction
4. fix the duplicate-note velocity patch bug
5. delete dead scaffolding and repeated logic
6. simplify the export bookkeeping pipeline
7. document the measure-layout reconstruction behavior prominently and compare it later with peer
   libraries such as music21 and MuseScore
8. only then consider expanding corpus support again

## Non-Goals For The Next Pass

The next pass should not:

- chase full engraving-preserving roundtrip
- add more fallback branches just to make more corpus files go green
- keep known lossy behavior hidden behind broad smoke tests
- pretend that note-attached lyric fidelity exists after import when the current data model does not
  preserve that relation
- perform a large multi-file refactor before the contract is clarified

## Deliverables Expected From The Next Pass

- a stricter and more honest `tests/test_musicxml.py`
- at least one targeted regression test for duplicate-note patching
- a reduced `src/io/musicxml.cpp` with dead scaffolding removed
- an explicit note, either in docs or test comments, about the current supported MusicXML contract
- optionally, a local `.agents` style skill for `symusic` C++ review and implementation work

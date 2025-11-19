# ABC Grammar Support Tracker

The top-level goal is to **implement a full Lexy-based abstract syntax tree**, cover every rule in the declarative grammar, and then map that tree into the existing IR during the semantic phase. Everything below is meant to capture the current status, plan, and activity log so that contributors can pick up where things left off.

## Development Plan

1. **Lexy AST Rebuild**  
   Define productions that cover header, voices, body, inline fields, decorations, lyrics, chord symbols, grace notes, and continuations. The AST must preserve precise source locations for error reporting.
2. **Semantic Mapping on Top of Lexy**  
   Walk the AST to build `miniabc::Document`, fully support inline `K:/M:/L:` overrides, lyrics melisma (`-`, `_`, `*`), chord symbols, grace notes, and inline `I:` directives. Every grammar feature needs round-trip samples and regression tests (C++/Python + abc2midi parity).
3. **Testing & Documentation**  
   Keep `symusic_test` green, add targeted fixtures for new syntax, and update this document whenever milestones shift.

## Current Status

- ✅ Lexy AST data structures and grammar skeletons live in `3rdparty/miniabc/src/miniabc.cpp` (covers fields, directives, lyrics, notes, rests, chords, tuplets, grace groups, broken rhythms, and continuations).
- ⏳ Semantic hookup is in progress: the old line-oriented parser and `collect_fields()` are being removed, and AST-driven voice/header builders are being integrated.
- ⏳ Lyrics alignment, inline overrides, and grace-note semantics still rely on the legacy logic; they will move to the new pipeline once the AST-driven builder lands.
- ⚠️ Tests are currently failing because the parser rewrite is mid-flight; do not rely on HEAD for production use until the Lexy pipeline is complete and `symusic_test` passes (including the abc2midi comparisons).

## Development Log

- Established Lexy AST structures plus parsing rules for all major ABC constructs, including decorations, chord symbols, grace groups, broken rhythms, and continuations.
- Removed the legacy line-based `parse_lines()` / `collect_fields()` scaffolding and started routing `parse_document()` through the new AST.
- Identified remaining semantic gaps: lyrics melisma alignment, inline field propagation (`K:/M:/L:/I:`), chord symbol persistence, grace-note timing, and abc2midi parity tests.
- Next concrete step: finish the AST-to-IR mapper, then resurrect the regression tests (C++ Catch2 + Python + external abc2midi comparison).
- Migrated the note-token production to the callback API so the Lexy build compiles on modern toolchains, and normalized the ABC tick resolution to `480` so `parse_document()`/`document_to_native()` agree with `abc2midi`.

## Feature Matrix (based on ABC standard 2.1)

| Module | Status | Notes |
| --- | --- | --- |
| Core header fields `X/T/M/L/Q/K` | ✅ | Default inference plus `C`/`C|` shortcuts |
| Informational fields (`R/O/B/S/Z`, etc.) | ⚠️ | Everything other than `K/V/L/M` is still stored as plain markers |
| Voice definition / switching `V:` | ✅ | Supported in headers and inline; missing names auto-filled |
| Inline `K:/M:/L:` overrides | ⚠️ | Parsing exists, but semantic application is pending |
| Directives `%%...` | ✅ | Preserved verbatim and emitted as markers |
| Notes / rests / ties | ✅ | Includes decorations, broken rhythm tokens, tuplets |
| Chords `[CEG]` | ✅ | Round-trip supported |
| Lyrics `w:` / `W:` | ⚠️ | Token ordering works, melisma symbols (`-`, `_`, `*`) are not interpreted yet |
| Multi-line UTF-8 lyrics | ⚠️ | Decoded, but melisma/held-syllable alignment is incomplete |
| Decorations `!trill!`, `~`, etc. | ⚠️ | Parsed but not all variants affect playback yet |
| Comments `%` | ✅ | Ignored |
| Chord symbols `"C"` | ⚠️ | Currently parsed but not stored in IR |
| Inline voice attributes `V:1 clef=bass` | ❌ | Not implemented |
| Inline directives `I:` | ❌ | Not implemented |
| Multiple lyric lines / melisma | ⚠️ | Needs `_` / `*` handling |
| Grace notes `{}` | ❌ | Still treated as decorations without timing |

Legend: ✅ Supported · ⚠️ Partially supported · ❌ Not supported.

## Immediate Priorities

1. Finish the Lexy-driven semantic mapper (inline `K/M/L/I`, lyrics melisma, chord symbols, grace notes).
2. Flesh out tests that compare against abcmidi output to lock down behavioral parity.
3. Update Python bindings once the C++ core stabilizes, then revisit documentation and samples.

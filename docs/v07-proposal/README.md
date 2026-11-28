# symusic 0.7 Design Proposal

> [!NOTE]
> **Temporary pre-release design proposal.** This directory is the canonical Markdown specification for the ecosystem milestone that `symusic 0.7` starts: the breaking Python API, a stabilized C++ core, a C ABI, stable `minimidi` and `anchorvec` releases, and the groundwork for JS and Go (`cgo`) bindings. Contributors and coding agents read it directly during implementation; the preview site below renders these files, and where the two differ, the Markdown wins. Once 0.7 ships, stable contracts will move into `docs/design/` and `conformance/` and this directory will be cleaned up.

- **Bilingual Interactive Preview Site (EN / 中文):** https://symusic-v07-lyk.edge.randallflare.com/
- **Public RFC & Discussion:** [Issue #136](https://github.com/Yikai-Liao/symusic/issues/136)
- **Active Implementation PR Stack:** [PRs #129–#134](https://github.com/Yikai-Liao/symusic/pulls)

## Reading Guide

| Document | Scope |
|---|---|
| [python-api.md](python-api.md) | Python 0.7 public surface, `Tick`/`Quarter`/`Second`, generic `EventList[Note[Tick]]`, CQS split, uniform two-level reference model, CC64 pedals, FluidSynth & `download_soundfont()`. |
| [semantics.md](semantics.md) | Behaviour every binding shares: time precision, numeric input, unit conversion, bounds, sort keys, clip, `adjust_time`, copies, MIDI export and import, pianoroll, rendering and WAV. |
| [migration.md](migration.md) | Full 73-entry 0.6 → 0.7 migration table (57 Python + 16 C++ Core), highlighting silent traps. |
| [rfc-136.md](rfc-136.md) | Five open downstream feedback questions tracked in Issue #136. |
| [cpp-core.md](cpp-core.md) | Layered multi-binding architecture, Native C++ 0.7 API alignment, 7 verified `07/pr6-api` defects, and the decision to stay on C++20. |
| [cpp-boundaries.md](cpp-boundaries.md) | Module boundaries across `minimidi` (Tier-1 SMF), NoteOn/NoteOff pairing, `anchorvec` (renamed from `pyvec`: monotonic 3-state, zero-cycle `TaggedPtr`, `make_pinned`, C++/Rust monorepo), `symusic::abc`, and `assets/soundfonts.json` / `Synthesizer`. |
| [cpp-engineering.md](cpp-engineering.md) | Unified 7-rule C++ style contract (`symusic` · `minimidi` · `anchorvec`), C++-first multi-binding repo layout, 4-tier universal CMake consumption, and `tessera-order` docs ownership matrix. |
| [cpp-roadmap.md](cpp-roadmap.md) | PR `#129–#134` stack summary, implemented vs remaining 0.7 scope, and 4-wave zero-conflict rollout plan. |
| [js-cli.md](js-cli.md) | 0.8+ direction for the Node and WASM packages and the command-line tool, aimed at soundtracks for code-generated video. |

## Compatibility Promise

0.7 is the last large breaking release before 1.0. What it promises depends on the surface:

| Surface | Promise |
|---|---|
| Python API | Code that runs on 0.7 without a `FutureWarning` keeps working through every 1.x release. The four [deprecated forms](migration.md) warn in 0.7 and are removed in 0.8. |
| C++ source API | Code that compiles against the 0.7 public headers keeps compiling through 1.x; `detail/` headers and namespaces are excluded. There is no C++ ABI promise: rebuild against each release. |
| C ABI | Experimental until a JS or Go consumer has used it ([C ABI contract](cpp-engineering.md)). |
| Pickles | Load only in the symusic version that wrote them; store scores as MIDI. |
| Behaviour | The rules in [semantics.md](semantics.md) are part of the API. Changing a result that contradicts them is a bug fix, not a break. |

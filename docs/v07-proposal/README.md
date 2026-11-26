# symusic 0.7 Design Proposal

> [!NOTE]
> **Temporary pre-release design proposal.** This directory holds the single-source-of-truth Markdown specification for `symusic 0.7` (Python API, C++ core, tri-repo boundaries, and multi-binding architecture) so contributors and coding agents can read it directly during implementation. Once 0.7 ships, stable contracts will move into `docs/design/` and `conformance/` and this directory will be cleaned up.

- **Bilingual Interactive Preview Site (EN / 中文):** https://symusic-v07-lyk.edge.randallflare.com/
- **Public RFC & Discussion:** [Issue #136](https://github.com/Yikai-Liao/symusic/issues/136)
- **Active Implementation PR Stack:** [PRs #129–#134](https://github.com/Yikai-Liao/symusic/pulls)

## Reading Guide

| Document | Scope |
|---|---|
| [python-api.md](python-api.md) | Python 0.7 public surface, `Tick`/`Quarter`/`Second`, generic `EventList[Note[Tick]]`, CQS split, uniform two-level reference model, CC64 pedals, FluidSynth & `download_soundfont()`. |
| [migration.md](migration.md) | Full 46-entry 0.6 → 0.7 migration table (34 Python + 12 C++ Core), highlighting silent traps. |
| [rfc-136.md](rfc-136.md) | Five open downstream feedback questions tracked in Issue #136. |
| [cpp-core.md](cpp-core.md) | Layered multi-binding architecture, Native C++ 0.7 API alignment, 7 verified `07/pr6-api` defects, and empirical C++17 downgrade feasibility (`zpp_bits` as sole C++20 blocker). |
| [cpp-boundaries.md](cpp-boundaries.md) | Module boundaries across `minimidi` (Tier-1 SMF), NoteOn/NoteOff pairing, `pinvec` (tentative rename from `pyvec`: monotonic 3-state, zero-cycle `TaggedPtr`, `make_pinned`, C++/Rust monorepo), `symusic::abc`, and `assets/soundfonts.json` / `Synthesizer`. |
| [cpp-engineering.md](cpp-engineering.md) | Unified 7-rule C++ style contract (`symusic` · `minimidi` · `pyvec`), C++-first multi-binding repo layout, 4-tier universal CMake consumption, and `tessera-order` docs ownership matrix. |
| [cpp-roadmap.md](cpp-roadmap.md) | PR `#129–#134` stack summary, implemented vs remaining 0.7 scope, and 4-wave zero-conflict rollout plan. |

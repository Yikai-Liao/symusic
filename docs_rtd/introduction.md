# Introduction

Symusic (Symbolic Music) is a high-performance toolkit for symbolic music data. It provides a C++20
core that parses and manipulates MIDI at the note level and exposes ergonomic Python factories via
nanobind. Typical operations—loading a multi-track score, shifting the pitch of entire sections,
exporting piano rolls, or rendering audio with SoundFonts—complete hundreds of times faster than the
pure-Python stacks traditionally used in MIR and deep-learning pipelines.

## What makes Symusic different?

- **Speed with fidelity** – MIDI is decoded into note-level structures directly in C++ and exposed to
  Python without copies, so workflows that used to require custom C++ extensions now run inside a
  notebook.
- **Time-unit flexibility** – Score/Track/Event objects are available in `tick`, `quarter`, or
  `second` space, letting you choose between raw MIDI timing, musically normalized beats, or
  wall-clock seconds depending on the task.
- **Cross-language friendly** – The core lives entirely in standard C++, so bindings for other
  languages (Julia, Lua, etc.) can be built on the same foundation.
- **End-to-end pipeline** – Beyond parsing, Symusic includes transformations (filtering, trimming,
  resampling), structured NumPy exports (SoA), piano-roll conversion, and synthesis via the
  Prestosynth engine.

## Architecture at a glance

| Layer | Purpose | Key technologies |
| --- | --- | --- |
| C++ engine | MIDI/ABC parsing, time-unit conversions, vectorized containers | C++20, fmt, minimidi, zpp_bits |
| Python bindings | Factories (`Score`, `Track`, `Note`, …) + helper utilities | nanobind, stubs in `python/symusic/core.pyi` |
| Synthesizer | SoundFont 2/3 rendering and WAV dumping | Prestosynth, NumPy |
| Documentation | This site (Sphinx + MyST) + legacy mdBook for historical reference | Read the Docs |

## Installation overview

```bash
pip install symusic
```

We publish wheels for Linux, macOS, and Windows on both x86 and ARM (Windows ARM64 is the only
unsupported combination at the moment). When you need to build from source, clone the repository with
submodules and install in editable mode:

```bash
git clone --recursive https://github.com/Yikai-Liao/symusic
cd symusic
pip install -e .
```

Symusic requires a C++20 compiler (GCC ≥ 11, Clang ≥ 15, MSVC 2022). If you lack system compiler
permissions on Linux, a `conda-forge::gcc` toolchain plus the `CC=/path/to/gcc` override works well.

```{note}
The legacy mdBook remains online at <https://yikai-liao.github.io/symusic/> until every page is
migrated. Treat it as historical reference; the codebase and this site contain the canonical
behavior.

For doc migration history and local build instructions, see {doc}`project_notes`.
```

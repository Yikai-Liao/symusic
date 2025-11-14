# symusic Overview
- Purpose: High-performance symbolic music toolkit focused on note-level MIDI decoding/encoding, with a C++20 core and Python bindings for fast Score/Track/Event operations plus synthesis helpers.
- Tech stack: CMake-based C++ library (fmt, minimidi, prestosynth, pyvec, zpp_bits, pdqsort) exposed to Python via nanobind/scikit-build; packaging managed by `pyproject.toml`; docs currently in mdBook (`docs/`), plus a tutorial notebook.
- Repo layout: `src/` + `include/` hold the core engine, `py_src/` contains nanobind bindings, `python/symusic/` exposes the public Python API, `docs/` has mdBook sources, `examples/` and `figure/` host illustrative material, `tests/` has pytest cases and Catch2-based C++ tests, and `3rdparty/` vendors dependencies.
- Notable assets: `tutorial.ipynb` (usage guide), `generate_stub.sh` for nanobind stub regeneration, `pre_commit.sh` wrapper, and GitHub Actions flows for wheels/tests/coverage.

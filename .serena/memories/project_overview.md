# symusic overview
- Purpose: Cross-platform symbolic MIDI toolkit that emphasizes ultra-fast note-level decoding/encoding in C++20, exposed to Python via nanobind-based bindings and hooked up to Prestosynth synthesis helpers.
- Tech stack: CMake 3.20+ core with fmt, minimidi, prestosynth, pyvec, zpp_bits, pdqsort; Python publishes through scikit-build/nanobind (with optional numpy utilities) and uses Sphinx/Read the Docs plus mdBook in `docs`/`docs_rtd`.
- Repo layout: `src`/`include` hold the engine, `py_src` contains nanobind glue, `python/symusic` is the public API, `3rdparty` vendors dependencies, `tests` mixes pytest and Catch2 suites, and `examples`/`docs`/`figure` capture tutorials and reference material.
- Build & packaging: configurable via CMake flags (`BUILD_SYMUSIC_PY`, `BUILD_SYMUSIC_TEST`, `ENABLE_COVERAGE`, `MEM_LEAK_WARNING`), Python artifacts described in `pyproject.toml`, wheels pushed to PyPI, and the README documents `pip install symusic`/`pip install ./symusic`.
- Runtime focus: Score/Track/Event APIs offer MIDI I/O, note/time transforms, numpy export, filtering, and synth integration while remaining fast enough for realtime applications.
- Notable assets: `tutorial.ipynb` for usage guidance, `generate_stub.sh` for nanobind stub regeneration, `pre_commit.sh`, and GitHub Actions flows for wheels, tests, and documentation.

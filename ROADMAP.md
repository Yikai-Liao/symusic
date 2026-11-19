# Roadmap

Planned work and accepted debt. Remove an item when the change that resolves it merges.

## Toolchain compatibility

Workarounds and unsupported configurations with current toolchains.

- **CMake 4.** `CMAKE_POLICY_VERSION_MINIMUM` in `CMakeLists.txt` and `pyproject.toml` exists because CMake 4 rejects projects declaring a minimum below 3.5. The direct submodules all declare 3.8 or later; the remaining offender is `gcem` (3.1) nested in `prestosynth`, whose upstream has since updated `gcem` and `libsamplerate`. Updating `prestosynth` should let the setting go.
- **Free-threaded Python.** Importing the extension on `3.13t`/`3.14t` re-enables the GIL with a `RuntimeWarning`. Supporting it requires declaring free-threading support in the module and auditing shared state.
- **nanobind `<cstdio>` workaround.** `py_src/CMakeLists.txt` force-includes `<cstdio>` because nanobind v3.1.0 omits it, which breaks limited-API builds with libc++. Remove the workaround once nanobind tags a release containing upstream commit `942e75d`.
- **Stable ABI for source builds on newer CPython.** Building a Linux wheel from source on CPython 3.14 can produce a wheel tagged `cp312-abi3` that imports `Py_REFCNT`, a symbol unavailable on 3.12. Packaged stable-ABI wheels are built on the 3.12 baseline. Fix the dependency or wheel-tag selection, then verify wheels built on newer CPython with strict `abi3audit` and a 3.12 import.
- **Cross-compiled release wheels.** Building release wheels by cross-compiling on Linux with [xclang](https://github.com/clice-io/xclang) is the planned packaging path. The [experimental wheel path](docs/development/ci.md#cross-compilation-experiment) is opt-in. Integrate verified targets into `release.yml`, retain native builders for interpreter/platform combinations the experiment does not cover, and extend that coverage before retiring those builders.
- **Deprecated CMake option names.** `cmake/BuildOptions.cmake` still maps the pre-0.6.1 names (`BUILD_SYMUSIC_PY`, `ENABLE_LTO`, ...) to the `SYMUSIC_` options with a deprecation warning. Remove the aliases in the next minor release.

## Static analysis

- **clice lint as a gate.** Re-run `clice lint` after the `zpp_bits` serializer fix and triage the remaining findings in `examples/`, `py_src/` and `tests/cpp/`; use-after-move findings in tests may be intentional. Fix or suppress them, then run `clice lint` in the `lint` lane.
- **ty.** `ty check` on the installed package reports about 290 diagnostics, mostly generated-stub overrides and NumPy method names shadowed by the containers. Decide whether ty replaces the mypy checks in `tests/typecheck`, fix the stubs, then add it to CI.

## Memory safety

Signed `Tick` (`i32`) arithmetic and narrowing conversions can overflow, which is undefined behavior in C++. The policy for invalid times (throw, skip or clamp) is not yet defined; decide it once and apply it at every site below.

- **Tick arithmetic.** `Note::end()`, `Pedal::end()` and `shift_time_inplace()` compute `time + duration` and `time += offset` directly (`include/symusic/event.h`, `src/event.cpp`). The results propagate through `ops::end()`, `trim_inplace()` and `adjust_time_inplace_inner()` (`include/symusic/ops.h`), `Track::end()`, `Score::end()`, MIDI parse/export duration handling (`src/io/midi.cpp`) and time conversion (`src/conversion.cpp`). Use `int64_t` intermediates and one checked write-back helper.
- **Float-to-integer narrowing.** `Tempo::qpm2mspq()`, `Quarter2Tick::time()`, `Second2Tick::get_time()` and `resample_inner()` cast rounded floats to `i32` without checking finiteness or both bounds. Route them through one checked helper.
- **Python tests under ASan.** CI runs the C++ tests with ASan and UBSan, but not pytest. A sanitized extension needs `LD_PRELOAD` of `libasan` and `libubsan` even at install time, because the build runs nanobind's stub generator, which imports the extension (without it the install fails with "ASan runtime does not come first"). Preloading for the whole `uv pip install` process exhausted resources and was killed. Untried: build the extension with `SYMUSIC_BUILD_PYTHON=ON` and `SYMUSIC_PYTHON_STUB` pointing at a stub from a normal build to skip generation, then run pytest with the preload and `ASAN_OPTIONS=detect_leaks=0`.

## Behavior defects

Found while checking the guide against the package. Each fix needs a test that fails without it.

- **Lyrics on a separate track.** MIDI import files a lyric under `meta.channel()` (`src/io/midi.cpp`), which is 15 for every meta event, so lyrics land in a track of their own instead of the track with the notes, and a written track with notes and lyrics reads back as two tracks. Decide where lyrics belong (the MIDI track's first note channel, or a per-MIDI-track lyric list) and keep the round trip stable.
- **`trim` modes.** `trim_inplace` (`include/symusic/ops.h`) compares `start_mode` and `end_mode` with `"remove"` and `"truncate"`; any other string keeps the events that cross that boundary without cutting them. Reject unknown modes with `ValueError`, or replace the strings with an enum.
- **`adjust_time` with repeated anchors.** Equal consecutive values in `original_times` pass `check_times`; the segment between them gets an infinite or NaN scale factor (`adjust_time_inplace_inner` in `include/symusic/ops.h`), and casting the scaled time to an integer is undefined behavior. Require strictly increasing original times.
- **`KeySignature.degree()` for flat keys.** `(key * 5) % 12` is negative for negative `key`, and the `u8` result wraps, so the repr shows `degree=251` for F major (`key=-1`). Use a non-negative modulo.
- **`Score.trim`.** `trim` is bound for tracks only; the C++ score overload has no binding.
- **`Note.ent_time`.** A misspelled duplicate of `end()` is bound in `py_src/bindings/events/event_bindings.h`. Deprecate and remove it.
- **`to()` in the stubs.** `Score.to`, `Track.to` and the event `to` return `object` in `core.pyi`; type them by the target unit.
- **ABC tune selection.** ABC import reads only the first tune of a file. Accept the `X:` number of the tune to read.

## Test coverage

- **Boundary inputs.** The memory-safety items above need regression tests for large ticks and offsets, extreme tempos and malformed MIDI durations.

## Architecture

Changes that reduce the number of places one change must touch.

- **One field list per event.** Each event's fields are written out separately in `include/symusic/event.h`, `include/symusic/repr.h`, `include/symusic/soa.h`, `py_src/bindings/events/event_bindings.h`, `py_src/utils/python_helpers.h` and `python/symusic/factory.py`, while pickle copies the raw struct (`src/io/zpp.cpp`). Adding or retyping a field means editing all of them consistently. Describe each event's members once and derive repr, the structure-of-arrays and NumPy fields, the binding properties and the factory signatures from that description, keeping old pickles loadable.
- **Factory dispatch.** `python/symusic/factory.py` (about 670 lines) has a hand-written factory class per type that repeats the same `ttype` dispatch. Generate it from the type list, or register the dispatch in the binding, while keeping the public constructors, `isinstance(x, Score)` across units and the typed stubs.
- **Example-only headers.** `include/argparse.h` (about 2,200 lines, excluded from `clice lint`) sits in the core's public include directory but only `examples/cpp/` uses it. Move it under `examples/` or replace it with a pinned release.
- **minimidi branch.** `.gitmodules` names branch `template-container` for `3rdparty/minimidi`, but the pinned commit is on `main`. Make the declared branch match the pin.

## Features

- **FluidSynth as an optional synthesizer.** `Synthesizer` (`include/symusic/synth.h`) wraps prestosynth only, and its C++ interface returns `psynth::AudioData`. Plan:
  1. Put a backend boundary in the core: render a `Score<Second>` into a symusic-owned audio buffer, so the public C++ API no longer exposes prestosynth types. prestosynth stays the default backend and is always built.
  2. Add a FluidSynth backend. FluidSynth is LGPL-2.1, so link it dynamically. Proposed: load `libfluidsynth` at run time from the system or a user-supplied path, so wheels and the default install do not change, and selecting the backend without the library raises an error that says how to install it. Rendering is offline: configure the sample rate, load the SoundFont, send program, note, control-change and pitch-bend events in time order and pull blocks with `fluid_synth_write_float`.
  3. Open questions: the minimum FluidSynth version; whether source builds may also link it at build time; reverb and chorus defaults; how the Python API selects a backend. Test both backends on the same score for length, channel count and non-silent output rather than sample equality.
- **MusicXML through mx.** symusic has no MusicXML support; `DataFormat::MusicXML` in `include/symusic/io/iodef.h` is declared but not implemented. [mx](https://github.com/webern/mx) (MIT, C++20, tagged releases, latest v0.7.1) reads and writes MusicXML and offers `mx::api`, a score model timed in ticks per quarter. Plan:
  1. Add it as a pinned release submodule behind a build option. mx compiles about 1,300 source files, so measure the build-time and wheel-size cost before enabling it in wheels by default.
  2. Import first, into `Score<Tick>`, with one track per part. Decide and test each mapping: tied notes merge into one note; grace notes; repeats and jumps (written order or unrolled); voices and staves inside a part; dynamics to velocity; tempo from `<sound tempo>` and metronome marks; transposing instruments (sounding pitch); unpitched percussion. Check whether mx reads compressed `.mxl`, or unzip it in symusic.
  3. Export after import is stable. MusicXML needs notation that symusic does not model (measures, voices, ties across bar lines, note types), so export requires quantization and notation inference; scope it separately.
- **ABC playback and engraving coverage.** Extend the differential corpus for explicit overlays, mid-tune meter and MIDI changes, uncommon MIDI directives, and coda/segno jumps. Compare lyric placement when a numbered ending skips the end of a logical line, when a line contains only rests, and when a tie spans several sung syllables. abc2midi can emit unassigned syllables at a single tick; retain a located warning instead of inventing an attack. Consider optional bar and key-aware spelling for print-oriented exports; keep playback timing and the documented diagnostics as regression requirements.

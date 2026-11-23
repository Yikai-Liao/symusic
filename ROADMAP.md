# Roadmap

Planned work and accepted debt. Remove an item when the change that resolves it merges.

## Toolchain compatibility

Workarounds and unsupported configurations with current toolchains.

- **Compiler floor.** `CONTRIBUTING.md` promises GCC 11+, Clang 15+ or MSVC 2022, and no CI job builds with GCC 11 or Clang 15. The wheels use GCC 14.2 (manylinux_2_28 and musllinux_1_2), AppleClang 17.0 with macOS deployment target 13.0, and MSVC 19.44 (x86) to 19.52 (arm64). Decided for 0.7: promise only what CI builds. Steps:
  1. A compile probe of the C++20 features the 0.7 code wants (such as `<format>`, ranges algorithms and views, `std::to_chars` for floating point), built by every wheel toolchain and against the macOS deployment target, decides the subset recorded in the [conventions](docs/development/conventions.md#standard-and-toolchain).
  2. State the oldest compiler each CI job pins as the floor in `CONTRIBUTING.md`, so the promise stays tested.

- **Free-threaded Python.** Importing the extension on `3.13t`/`3.14t` re-enables the GIL with a `RuntimeWarning`. Supporting it requires declaring free-threading support in the module and auditing shared state.
- **nanobind `<cstdio>` workaround.** `py_src/CMakeLists.txt` force-includes `<cstdio>` because nanobind v3.1.0 omits it, which breaks limited-API builds with libc++. Remove the workaround once nanobind tags a release containing upstream commit `942e75d`.
- **Cross-compiled release wheels.** Building release wheels by cross-compiling on Linux with [xclang](https://github.com/clice-io/xclang) is the planned packaging path. The [experimental wheel path](docs/development/ci.md#cross-compilation-experiment) is opt-in. Integrate verified targets into `release.yml`, retain native builders for interpreter/platform combinations the experiment does not cover, and extend that coverage before retiring those builders.
- **Sphinx 9.** `docs/requirements.txt` pins `sphinx==7.4.7` and `docutils<0.21`; Sphinx 9.2 supports Docutils 0.23. Check that myst-parser and Furo support the new versions, then raise the pins and keep the `-W` build passing.

## Static analysis

- **clice lint as a gate.** Re-run `clice lint` after the `zpp_bits` serializer fix and triage the remaining findings in `examples/`, `py_src/` and `tests/cpp/`; use-after-move findings in tests may be intentional. Fix or suppress them, then run `clice lint` in the `lint` lane, after a configure step that writes `compile_commands.json`. Then extend `.clang-tidy` with the checks that the [language rules](docs/development/conventions.md#language-rules) name, and encode the [names table](docs/development/conventions.md#names) in `readability-identifier-naming`. Tighten one check at a time: a check joins `WarningsAsErrors` once the tree has no findings for it, and code rewritten for 0.7 clears the checks of its area when it merges. `modernize-*` checks that suggest library features outside the [supported subset](docs/development/conventions.md#standard-and-toolchain) stay off. clice 1.0.0 contains clang-tidy from LLVM 23.1.5. The lint runs once, on the Linux compilation database, so branches for MSVC or macOS only are not checked.
- **ty.** `ty check` on the installed package reports about 290 diagnostics, mostly generated-stub overrides and NumPy method names shadowed by the containers. Decide whether ty replaces the mypy checks in `tests/typecheck`, fix the stubs, then add it to CI.

## Memory safety

- **Python tests under ASan.** CI runs the C++ tests with ASan and UBSan, but not pytest. A sanitized extension needs `LD_PRELOAD` of `libasan` and `libubsan` even at install time, because the build runs nanobind's stub generator, which imports the extension (without it the install fails with "ASan runtime does not come first"). Preloading for the whole `uv pip install` process exhausted resources and was killed. Untried: build the extension with `SYMUSIC_BUILD_PYTHON=ON` and `SYMUSIC_PYTHON_STUB` pointing at a stub from a normal build to skip generation, then run pytest with the preload and `ASAN_OPTIONS=detect_leaks=0`.

## Behavior defects

Found while checking the guide against the package. Each fix needs a test that fails without it.

- **abc2midi stress velocities.** With a `%%MIDI stressmodel` file, abc2midi reads per-note stress velocities that it never initializes: `store.c` allocates `stressvelocity` without clearing it, and `genmidi.c` reads it before any bar fills it. On Windows the result changes from run to run, so `tests/test_abc.py` skips its stress tests there. Fix it upstream, update the pinned release and remove the skip.
- **Pickle restore skips range checks.** `__setstate__` accepts event fields that constructors and setters reject, such as a NaN note time in a quarter or second track. Validate the decoded object before it replaces the receiver. `tests/test_py_bindings.py` builds its NaN track this way; once restore validates, drop that case, since no public path can create a NaN time.

## Test coverage


## Architecture

Changes that reduce the number of places one change must touch.

- **MIDI semantics in minimidi.** Move the note layer in `include/symusic/detail/midi_notes.h` into minimidi with its own recording-sink tests, tag a release and pin it. Then introduce a source concept for writing and move the corresponding note encoding there. Symusic owns the split by channel and program, pedals and construction of its score model.
- **FluidSynth SoundFont and handle reuse.** `Synthesizer::render` currently creates a fresh `fluid_synth_t`, reloads and decompresses the SoundFont inside `lifecycle_mutex`, and destroys it on every call, and each `Synthesizer` opens its own dynamic-library handle. Revisit if batch SF3 rendering becomes a bottleneck by caching the loaded `Backend` per library path and anchoring the FluidSynth sample cache across renders without reintroducing teardown races.

## Code cleanup

Existing code that breaks the [code conventions](docs/development/conventions.md). Each item is a `refactor` without behavior change unless it says otherwise; the [architecture](#architecture) items above remove the duplication that several of these macros exist for.


## Features

- **MusicXML.** After 0.7. symusic has no MusicXML support. The module is `symusic::musicxml` with its own header, namespace and CMake target (see the [namespace rules](docs/development/conventions.md#namespaces-and-headers)). It reads into `Score<Tick>`, one track per part, by rendering the notation into performance events and writing them through the `Score` builder from [MIDI semantics in minimidi](#architecture); the layers are described in the [architecture](docs/design/architecture.md#score-and-file-formats). In Python it adds `Score.from_musicxml`/`to_musicxml` and the `.musicxml` and `.mxl` extensions. Plan:
  1. Choose the parser. [mx](https://github.com/webern/mx) (MIT, C++20, tagged releases, latest v0.7.1, under active development: 0.7.0 replaced `DocumentManager`) parses into `mx::api`, a notation model timed in ticks per quarter. Built at v0.7.1 with GCC on Linux, its libraries are 535 translation units and about 30 CPU-minutes, and they add about 10 MB stripped even to a binary that only imports, more than the whole extension today (6.5 MB). mx does not read `.mxl`, and `mx::api` keeps only the first transposition of a part. A performance import needs a small part of the schema (part list and MIDI instruments, attributes, notes, backup and forward, directions and sound, barlines), which an importer on pugixml, a single translation unit, can read directly and keep in the base wheel. The rendering in step 2 is the same work with either parser, and the module boundary hides the parser, so the choice can be revised later. mx's test corpus serves as regression input in both cases, after checking the license of each file.
  2. Import. Decide and test each mapping: tied notes merge into one note; grace notes; repeats and jumps (written order or unrolled); voices and staves inside a part; dynamics to velocity; tempo from `<sound tempo>` and metronome marks; transposing instruments (sounding pitch), including changes inside a part; unpitched percussion. Read compressed `.mxl` with a zip reader.
  3. Export after import is stable. MusicXML needs notation that symusic does not model (measures, voices, ties across bar lines, note types), so export requires quantization and notation inference; scope it separately.
- **MIDI message layer.** After 0.7. `Score` is a performance model and drops what a MIDI file holds beyond it: SysEx, unknown meta events, the order and division of tracks, the channel of each message. A read-only view of the messages, in `symusic.midi`, serves inspecting a parse, reading that data, and tokenizers that work in file order. It is the parse layer of MIDI, not a second score model; the note layer from [MIDI semantics in minimidi](#architecture) stays internal. Plan:
  1. Types: `MidiFile` (format, division, tracks), `MidiTrack`, one immutable class per channel message (`NoteOn`, `NoteOff`, `ControlChange`, `ProgramChange`, `PitchBend`, `ChannelPressure`, `PolyPressure`), `SysEx` with its bytes, and one `Meta` with a `MetaType` enum and the raw bytes. Meta events stay undecoded, since decoding tempo and signatures is the note layer's job, and no name clashes with the `Score` events. Times are absolute ticks in file order. The C++ types come from minimidi.
  2. `MidiTrack.to_numpy()` returns columns (time, kind, channel, data1, data2), as the event lists do; one Python object per message is what makes mido slow on large data sets.
  3. `MidiFile.from_bytes(data)`, `file.to_bytes()`, `MidiFile.from_score(score)` and `file.to_score(*, ttype=Tick, ...)` connect it to bytes and to `Score`; `Score` itself does not refer to `MidiFile`. `to_score` takes the same options as `Score.from_midi`. `Score.from_midi` keeps its direct path, and tests check that `Score.from_midi(data, **options)` equals `MidiFile.from_bytes(data).to_score(**options)` and that `score.to_midi()` equals `MidiFile.from_score(score).to_bytes()` byte for byte, which requires a deterministic writer. `MidiFile` keeps the raw bytes, including data bytes above 127, and written back to bytes it round-trips, apart from running status.
  4. Editing messages waits for a use case.
- **ABC through abcmidi.** ABC import and export build `abc2midi` and `midi2abc` as separate executables from the upstream sources (wrapper translation units in `cmake/abcmidi/`), ship them in `symusic/_vendor/abcmidi/` and run them as subprocesses (`python/symusic/_abc.py`). They stay the only ABC implementation and stay out of process:
  - The ABC that users convert follows `abc2midi`'s behavior, not only the ABC 2.1 standard: `%%MIDI` directives such as `gchord`, `drum`, `drone` and `beat`, how grace notes take time, voice overlay and its recovery from malformed input. A native parser would differ from it in each of these places, and keeping two implementations in agreement is a permanent cost.
  - abcmidi is GPL-2.0-or-later and symusic is MIT. Under the FSF's reading of the GPL, linking it into the extension, statically or dynamically, makes the combination a derivative work. A separate GPL package could link it, but abcmidi is written as a program, with global state and `exit()` on errors, so in-process use needs a maintained fork.

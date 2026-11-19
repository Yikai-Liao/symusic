# Roadmap

Planned work and accepted debt. Remove an item when the change that resolves it merges.

## 0.7 API

0.7 changes the Python API where it is not Pythonic, fixes the [memory safety](#memory-safety) and [behavior defects](#behavior-defects) items below and replaces the synthesizer with [FluidSynth](#features) in the same release. The [code conventions](docs/development/conventions.md#python-api) state the target rules. For each item, decide the new form before implementing it; whether the 0.6 names keep a deprecation period is open. The release notes carry the old-to-new table.

- **Copy and in-place defaults.** `sort` and `filter` default to `inplace=True` (on scores, tracks and event lists), while `clip`, `shift_time`, `adjust_time` and the other transformations default to `inplace=False`. In C++, `Track` and `Score` `*_inplace` return `void` and the event `*_inplace` return a reference.
- **Copy semantics.** `Score.copy()` and `Track.copy()` default to a deep copy, while `copy.copy` shares the tracks and event lists.
- **Queries.** `Note.end` is a property and `Track.end()`/`Score.end()` are methods; likewise `start`, `empty` and `note_num`. `symusic.Track.empty(ttype)` creates an empty track, while `track.empty()` tests for one.
- **Aliases and debugging API.** `tpq` duplicates `ticks_per_quarter`, `Score.from_file` takes both `fmt` and `format`, `Note.start` duplicates `time`, and `__use_count` exposes `shared_ptr` reference counts on public classes.
- **Strings for closed sets.** Time units (`ttype="tick"`), pianoroll modes, trim modes and file formats are strings matched at run time. Use enums or `Literal` types, and reject unknown values.
- **File I/O names.** Reading goes through `Score(path)`, `Score.from_file`, `from_midi(bytes)` and `from_abc(str)`; writing through `dump_midi(path)`, `dumps_midi()`, `dump_abc` and `dumps_abc`. `Score(x)` accepts a path, a ticks-per-quarter integer or another score in the same positional argument.
- **Per-unit classes.** Users see `ScoreTick`, `ScoreQuarter` and `ScoreSecond`, factory objects that imitate classes through `__instancecheck__`, and generic placeholders that raise `TypeError` when instantiated. Decide the public class model together with [factory dispatch](#architecture).
- **Pedals.** MIDI read keeps every control change 64 in `controls` and also derives `pedals` from it (on at value 64 or above, off below, per channel; `src/io/midi.cpp`), while MIDI write ignores `pedals`. The same information has two representations, and editing `pedals` has no effect on the written file. Decide which one is the authority: derive pedals from `controls` on request, or store pedals and write them as control change 64.
- **SoundFonts.** `Soundfont.path()` downloads when the file is missing or fails verification, and every call reads the whole file into memory to hash it (`python/symusic/soundfont.py`). Built-in fonts are static methods with `PascalCase` names (`BuiltInSF3.MuseScoreGeneral()`), the hash field is `hash_`, and `pySmartDL` is a runtime dependency used only for this download. Separate locating, verifying and downloading; cache the verification; download to a temporary file in the target directory and rename it on success; reconsider the dependency.
- **Temporary files for ABC.** Each ABC call writes its input or output through a new directory under the system temporary directory (`ScopedTempDir` in `py_src/utils/process_runner.cpp`). `from_abc(str)` writes the text to one temporary directory and `from_abc_file` creates a second one; on Windows input and output files are copied through it as well. The tool paths reach the binding through the process environment variables `SYMUSIC_ABC2MIDI` and `SYMUSIC_MIDI2ABC`, which `symusic.factory` sets at import and child processes inherit. The `T:` line of exported ABC names the temporary file (see [ABC output](#behavior-defects)). Pass the tool paths as arguments, and keep one temporary directory per call.

## Toolchain compatibility

Workarounds and unsupported configurations with current toolchains.

- **CMake 4.** `CMAKE_POLICY_VERSION_MINIMUM` in `CMakeLists.txt` and `pyproject.toml` exists because CMake 4 rejects projects declaring a minimum below 3.5. The direct submodules all declare 3.8 or later; the remaining offender is `gcem` (3.1) nested in `prestosynth`, whose upstream has since updated `gcem` and `libsamplerate`. Updating `prestosynth` should let the setting go.
- **Free-threaded Python.** Importing the extension on `3.13t`/`3.14t` re-enables the GIL with a `RuntimeWarning`. Supporting it requires declaring free-threading support in the module and auditing shared state.
- **nanobind `<cstdio>` workaround.** `py_src/CMakeLists.txt` force-includes `<cstdio>` because nanobind v3.1.0 omits it, which breaks limited-API builds with libc++. Remove the workaround once nanobind tags a release containing upstream commit `942e75d`.
- **Cross-compiled release wheels.** Building release wheels by cross-compiling on Linux with [xclang](https://github.com/clice-io/xclang) is the planned packaging path. The [experimental wheel path](docs/development/ci.md#cross-compilation-experiment) is opt-in. Integrate verified targets into `release.yml`, retain native builders for interpreter/platform combinations the experiment does not cover, and extend that coverage before retiring those builders.
- **Deprecated CMake option names.** `cmake/BuildOptions.cmake` still maps the pre-0.6.1 names (`BUILD_SYMUSIC_PY`, `ENABLE_LTO`, ...) to the `SYMUSIC_` options with a deprecation warning. Remove the aliases in the next minor release.

## Static analysis

- **clice lint as a gate.** Re-run `clice lint` after the `zpp_bits` serializer fix and triage the remaining findings in `examples/`, `py_src/` and `tests/cpp/`; use-after-move findings in tests may be intentional. Fix or suppress them, then run `clice lint` in the `lint` lane.
- **ty.** `ty check` on the installed package reports about 290 diagnostics, mostly generated-stub overrides and NumPy method names shadowed by the containers. Decide whether ty replaces the mypy checks in `tests/typecheck`, fix the stubs, then add it to CI.

## Memory safety

- **Negative tick times in conversion and export.** `SecondTimeConverter` inserts a tempo at tick 0 in front of a tempo with a negative time, which leaves its boundary list unsorted, and `src/io/midi.cpp` passes `i32` times to `minimidi` as `uint32_t` without a sign check. Both come from reading the code; neither has a regression test.
- **Integer narrowing outside tick arithmetic.** Tick arithmetic is checked; other integers are not. MIDI write casts `ticks_per_quarter` (`i32`) to `u16` (`src/io/midi.cpp`), so a value above 65535 wraps and one above 32767 sets the SMPTE bit of the MIDI division field. `KeySignature.degree()` wraps through `u8` (see [behavior defects](#behavior-defects)). Audit every subtraction on an unsigned type and every narrowing cast in `src/`, `include/` and `py_src/`, and apply the [integer rule](docs/development/conventions.md#integers-and-time).
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
- **ABC output.** `dumps_abc` prints "calling midi2abc" to stdout, and the `T:` line of the result names the temporary MIDI file. Silence the message and write the score's title, or none.
- **SoundFont download error.** The `ConnectionError` raised by `Soundfont.download` formats the bound method `self.path` instead of the file path.
- **ABC tune selection.** ABC import reads only the first tune of a file. Accept the `X:` number of the tune to read.

## Test coverage

- **Boundary inputs.** Malformed MIDI durations and negative tick times have no regression tests.

## Architecture

Changes that reduce the number of places one change must touch.

- **One field list per event.** Each event's fields are written out separately in `include/symusic/event.h`, `include/symusic/repr.h`, `include/symusic/soa.h`, `py_src/bindings/events/event_bindings.h`, `py_src/utils/python_helpers.h` and `python/symusic/factory.py`, while pickle copies the raw struct (`src/io/zpp.cpp`). Adding or retyping a field means editing all of them consistently. Describe each event's members once and derive repr, the structure-of-arrays and NumPy fields, the binding properties and the factory signatures from that description, keeping old pickles loadable.
- **Factory dispatch.** `python/symusic/factory.py` (about 670 lines) has a hand-written factory class per type that repeats the same `ttype` dispatch. Generate it from the type list, or register the dispatch in the binding, while keeping the public constructors, `isinstance(x, Score)` across units and the typed stubs.
- **Example-only headers.** `include/argparse.h` (about 2,200 lines, excluded from `clice lint`) sits in the core's public include directory but only `examples/cpp/` uses it. Move it under `examples/` or replace it with a pinned release.
- **minimidi branch.** `.gitmodules` names branch `template-container` for `3rdparty/minimidi`, but the pinned commit is on `main`. Make the declared branch match the pin.

## Code cleanup

Existing code that breaks the [code conventions](docs/development/conventions.md). Each item is a `refactor` without behavior change unless it says otherwise; the [architecture](#architecture) items above remove the duplication that several of these macros exist for.

- **Macro code generation in the core.** `REPEAT_ON` from the vendored `3rdparty/MetaMacro.h` instantiates templates per unit and generates per-field code in `include/symusic/repr.h`, `include/symusic/soa.h`, `include/symusic/io/midi.h`, `src/event.cpp`, `src/track.cpp`, `src/score.cpp`, `src/repr.cpp` and `src/io/zpp.cpp`. `BASIC_TIMESTAMP_METHODS` and `COMPILER_DEFAULT_METHODS` write class bodies in `include/symusic/event.h`; `PIANOROLL_MODES` is an X-macro in `include/symusic/pianoroll.h`; `KEY` in `include/symusic/ops.h` is never undefined; `ERROR_PARSE_DUMPS` guards with `static_assert(true, ...)`, which never fires. Replace them with templates over a unit list and with the event field descriptions, then delete `MetaMacro.h`.
- **Header hygiene.** Headers carry both `#pragma once` and an `#ifndef` guard, some with wrong names (`LIBSYMUSIC_TRACK_HSCORE_H` in `score.h`, `SYNTH_H` in `synth.h`). `event.h` includes `"iostream"` with quotes; about 40 `typedef`s, reserved identifiers (`_Ty` in `mtype.h`, `__COUNT` in the macros), camelCase names (`sfPath`, `timeDim`, `getBanks`) and comments that contradict the signature ("inplace sort, and return self reference" on `void` functions in `track.h` and `score.h`) remain.
- **Namespaces.** Core internals use both `symusic::details` and `symusic::ops::details`; binding code is split across `pyutils` and `symusic`, with docstrings in five more `*_docstrings` namespaces.
- **Binding layer.** Every transformation repeats the copy-or-in-place lambda for each type (`py_src/bindings/score/score_bindings.h`, `track/track_bindings.h`, `events/event_bindings.h`). Signatures are assembled by hand with `fmt::format` into `nb::sig`. Bytes-to-span conversion and pickle state handling are written inline per class. The `RW_COPY`, `NDARR`, `TO_NUMPY` and `BIND_*` macros generate binding code. ABC file staging, format detection by extension (`get_format`) and the `from_file` dispatch are implemented in `score_bindings.h` instead of the core.

## Features

- **FluidSynth in place of prestosynth.** `Synthesizer` (`include/symusic/synth.h`) wraps prestosynth, and its C++ interface returns `psynth::AudioData`. The renderer passes only notes and bank selects (control change 0) to prestosynth (`src/synth.cpp`), so pitch bends, other control changes and pedals do not reach the audio. Replace it with FluidSynth:
  1. Put a backend boundary in the core: render a `Score<Second>` into a symusic-owned audio buffer, so the public C++ API no longer exposes prestosynth types.
  2. Implement the FluidSynth backend. Rendering is offline: configure the sample rate, load the SoundFont, send program, note, control-change and pitch-bend events in time order and pull blocks with `fluid_synth_write_float`.
  3. Remove prestosynth. That also removes the [CMake 4](#toolchain-compatibility) workaround, which exists for the `gcem` copy nested in prestosynth.
  4. Distribution: FluidSynth is LGPL-2.1, so the extension loads `libfluidsynth` dynamically at run time and the base wheel does not contain it. `pip install symusic[fluidsynth]` installs the rendering dependencies, including a package that provides `libfluidsynth` for every wheel platform; without it, creating a `Synthesizer` raises an error that names the extra. Open: which package provides the library (an existing wheel on PyPI or a companion wheel built by symusic, with its dependencies such as glib), whether a system or user-supplied library is also accepted, the minimum FluidSynth version, reverb and chorus defaults, and how the `quality` argument maps to FluidSynth settings. Test length, channel count and non-silent output, and that pitch bends and control changes change the result, rather than sample equality.
- **MusicXML through mx.** symusic has no MusicXML support; `DataFormat::MusicXML` in `include/symusic/io/iodef.h` is declared but not implemented. [mx](https://github.com/webern/mx) (MIT, C++20, tagged releases, latest v0.7.1) reads and writes MusicXML and offers `mx::api`, a score model timed in ticks per quarter. Plan:
  1. Add it as a pinned release submodule behind a build option. mx compiles about 1,300 source files, so measure the build-time and wheel-size cost before enabling it in wheels by default.
  2. Import first, into `Score<Tick>`, with one track per part. Decide and test each mapping: tied notes merge into one note; grace notes; repeats and jumps (written order or unrolled); voices and staves inside a part; dynamics to velocity; tempo from `<sound tempo>` and metronome marks; transposing instruments (sounding pitch); unpitched percussion. Check whether mx reads compressed `.mxl`, or unzip it in symusic.
  3. Export after import is stable. MusicXML needs notation that symusic does not model (measures, voices, ties across bar lines, note types), so export requires quantization and notation inference; scope it separately.
- **ABC through abcmidi.** ABC import and export build `abc2midi` and `midi2abc` as separate executables from the upstream sources (wrapper translation units in `cmake/abcmidi/`), ship them in the wheel and run them through temporary files (`py_src/utils/process_runner.cpp`). They stay the only ABC implementation and stay out of process:
  - The ABC that users convert follows `abc2midi`'s behavior, not only the ABC 2.1 standard: `%%MIDI` directives such as `gchord`, `drum`, `drone` and `beat`, how grace notes take time, voice overlay and its recovery from malformed input. A native parser would differ from it in each of these places, and keeping two implementations in agreement is a permanent cost.
  - abcmidi is GPL-2.0-or-later and symusic is MIT. Under the FSF's reading of the GPL, linking it into the extension, statically or dynamically, makes the combination a derivative work. A separate GPL package could link it, but abcmidi is written as a program, with global state and `exit()` on errors, so in-process use needs a maintained fork.

  Work on the current integration:
  1. `abc2midi` exits with status 1 after any reported error, including errors it recovers from, and `event_fatal_error` also exits with 1, so symusic rejects tunes that `abc2midi` converts: a stray `]` inside a chord or a `\` in the middle of a line still writes a MIDI file, while a missing key signature writes none. Confirm that a fatal exit never leaves a partial file, then accept conversions that produced one and report the messages as warnings.
  2. Capture the converters' messages, which `abc2midi` prints on stdout, instead of passing them to the terminal, and include them in the exception, which now names only the command and the exit code.

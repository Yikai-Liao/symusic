# Architecture

symusic is a C++20 library for note-level symbolic music with a nanobind Python binding. This page describes how the code is layered, how data is owned and which behavior must survive refactors. User-facing explanations live in the [concept guides](../concepts/index.md).

## Layers

| Layer | Location | Owns |
|---|---|---|
| Core library | `include/symusic/`, `src/` | Data model, transformations, time conversion, MIDI and pickle I/O, piano rolls, synthesis glue |
| Binding | `py_src/` | The `symusic.core` extension: one class per C++ type and time unit, docstrings, NumPy and pickle bridges |
| Python package | `python/symusic/` | Public constructors (`symusic.factory`), typing aliases (`symusic.types`), SoundFont downloads (`symusic.soundfont`) |
| Type stubs | `scripts/lift_generic_stubs.py`, `py_src/CMakeLists.txt`, `cmake/LiftStubs.cmake.in` | `symusic/core.pyi`, generated at install time |

Dependencies point downward only: the Python package calls the binding, the binding calls the core, and the core knows nothing about Python except the `pyvec` container. Algorithms belong in the core so that C++ users and every time unit share them; the binding adapts types and arguments, and the Python layer only dispatches.

## Data model

Every musical type is a template over a time unit `T` (the `TType` concept in `include/symusic/time_unit.h`):

| Unit | `T::unit` | Meaning |
|---|---|---|
| `Tick` | `i32` | Raw MIDI ticks; resolution is `Score::ticks_per_quarter` |
| `Quarter` | `f32` | Quarter notes |
| `Second` | `f32` | Wall-clock seconds, derived from the tempo map |

Events derive from `TimeStamp<T>` (`include/symusic/event.h`): `Note`, `Pedal` (with duration), `ControlChange`, `PitchBend`, `TimeSignature`, `KeySignature`, `Tempo` and `TextMeta`. A `Track<T>` holds notes, controls, pitch bends, pedals and lyrics; a `Score<T>` holds `ticks_per_quarter`, tracks, time signatures, key signatures, tempos and markers.

Each container exists in two representations:

- **Shared** (`Score`, `Track`): fields are `shared<pyvec<Event>>` and tracks are `shared<Track>`. This is what Python sees. Copying the outer object shares the event lists; `deepcopy` copies them.
- **Native** (`ScoreNative`, `TrackNative`): plain `vec` fields with no shared ownership. Parsers build native values for speed and convert them once with `to_shared`; pickle serializes `to_native(...)`.

`pyvec` (submodule `3rdparty/pyvec`) stores elements in chunks that are never reallocated and keeps order in a separate pointer array. Indexing from Python returns an aliasing `shared_ptr` that keeps the chunks alive, so a Python reference to an event stays valid after the list is appended to, sorted or has the element removed.

## Data flow

| Path | Implementation |
|---|---|
| MIDI read | `minimidi` parses bytes; `src/io/midi.cpp` builds `ScoreNative<Tick>` and converts to shared. Other units convert after parsing |
| MIDI write | Converts to ticks, then `minimidi` encodes (`src/io/midi.cpp`) |
| ABC read/write | The PEGTL grammar in `src/io/abc/syntax.cpp` builds located music tokens. Navigation expands parts and repeats; playback renders notes, tracks and global events directly to `Score<Tick>`. Export validates and projects original event endpoints onto the ABC tick grid, then writes ABC voices from the resulting timeline. The binding handles paths, read-time unit conversion, and Python warnings. |
| Pickle | `zpp_bits` over the native representation (`src/io/zpp.cpp`) |
| Time units | `convert<To>(score, min_dur)` and `resample(score, tpq, min_dur)` in `include/symusic/conversion.h`; the tempo map drives seconds |
| NumPy | Structure-of-arrays types in `include/symusic/soa.h`; `to_numpy`/`from_numpy` in the event bindings |
| Piano roll | `src/pianoroll.cpp`, exposed as `Track.pianoroll` and `Score.pianoroll` |
| Synthesis | `prestosynth` renders a `Score<Second>` (`src/synth.cpp`); `symusic.soundfont` downloads SoundFonts |

### ABC playback state

ABC has two orders. Located tokens retain **written order** across voices and line continuations. Assembly uses that order to resolve tuning and accidental propagation policy and to attach verses to a voice's music lines. Navigation builds **performed order** from those tokens and identifies each scheduled occurrence of a part. Returning to a passage does not reapply a later written policy to its pitches.

Playback keeps a clock and measure-accidental cache per overlay lane. Channel output owns track routing, note releases and pedal pairing; lyric alignment uses timed attack anchors without needing a note-on. The tune shares one percussion map between channel 10 and generated drums so both resolve the same destination at attack time. The core returns a score and located warnings without invoking a converter; abc2midi is only a test oracle.

## Python typing

The extension registers concrete classes per unit (`ScoreTick`, `NoteQuarterList`, ...) and generic placeholders (`Score`, `Note`, `PyVec`, ...) that only exist for type checkers and raise `TypeError` when instantiated (`py_src/bindings/core/module.cpp`). At install time nanobind writes `core.raw.pyi`; `scripts/lift_generic_stubs.py` rewrites it into `core.pyi` so that concrete classes appear as specializations of the generics. Cross builds consume that generated file from a native build of the same revision through `SYMUSIC_PYTHON_STUB`, because the host cannot import the target extension. The experimental wheel workflow regenerates it on the target and compares declaration syntax trees, ignoring demangler whitespace around C++ template delimiters. `tests/typecheck/` checks the result with mypy. `symusic.factory` gives users unit-agnostic constructors such as `Score(path, ttype="tick")` and makes `isinstance(x, Score)` match every unit.

## Contracts

Refactors keep these behaviors unless a change is explicitly about them:

- **Unit symmetry.** Every type and operation exists for `Tick`, `Quarter` and `Second` with the same semantics.
- **Copy and in-place pairs.** C++ transformations come as `foo()` returning a new value and `foo_inplace()` mutating. Python methods take `inplace=False`; with `inplace=True` they mutate and return `self`.
- **Reference stability.** Python references to events, tracks and lists stay valid as described under [data model](#data-model).
- **Round trips.** MIDI read/write and pickle preserve the shared data model; a pickled score loads in the same version. Pickle bytes depend only on event values: struct padding is written as zeros.
- **Explicit errors.** Invalid input raises (`TypeError` for the wrong kind of object, `ValueError` for invalid values) rather than coercing silently. Malformed MIDI raises instead of crashing; `tests/testcases/MIDIs_corrupted/` holds regressions.
- **Stable ABI.** On GIL-enabled CPython 3.12 and later the extension uses only the limited C API (nanobind `STABLE_ABI`), so one `abi3` wheel per platform serves later GIL-enabled CPython 3 versions. Free-threaded builds require a different ABI and are not supported yet. Do not call CPython APIs outside the limited API in the binding; `abi3audit` in the wheel build rejects them.
- **Thin Python layer.** Hot paths stay in C++; the Python package does not reimplement core logic.

Known violations of memory safety at extreme values are tracked in the [roadmap](../ROADMAP.md#memory-safety).

## Adding a feature

1. Implement it in the core for all time units, with a copy and in-place form if it transforms data.
2. Bind it in the matching `py_src/bindings/<area>/` file, with a docstring at the binding site.
3. Extend `symusic.factory` only if construction or dispatch changes.
4. Reinstall (`pip install .`) to regenerate the stubs, and add C++ and Python tests at the new boundary.
5. Update the concept guide or tutorial that describes the area.

## Build ownership

The root `CMakeLists.txt` declares the core target and its public include/link requirements. `cmake/BuildOptions.cmake` owns project switches and compiler settings; `cmake/Dependencies.cmake` adds pinned submodules without their status chatter. `py_src/`, `tests/cpp/` and `examples/cpp/` each own their targets and installation or execution rules. Release LTO applies to the C++ core; bindings and nanobind remain native objects. Compiler-cache setup belongs to the wheel workflow, not the library's build contract.

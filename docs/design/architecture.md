# Architecture

symusic is a C++20 library for note-level symbolic music with a nanobind Python binding. This page describes how the code is layered, how data is owned and which behavior must survive refactors. User-facing explanations live in the [guide](../guide/quickstart.md).

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

## Score and file formats

`Score` is a performance model. It records what sounds and when: notes with onset, duration, pitch and velocity, controls, pitch bends, tempos, and the time and key signatures and text that MIDI carries. It does not record how the music is written: measures, voices, ties, enharmonic spelling and note types. MIDI stores the same kind of information, so MIDI is the format the model is shaped after and the only file format with a lossless round trip.

A notation format relates to `Score` in two directions of different difficulty:

- **Import renders** the notation into a performance: tied notes merge, repeats unroll, dynamics become velocities. The notation itself is discarded.
- **Export transcribes** a performance into notation, which needs quantization and inference of voices, ties and note types, so its result is an approximation.

A notation format is connected in three layers:

1. Parsing the file into the format's own model belongs to the format library.
2. Rendering that model into performance events needs the musical knowledge of the notation. It belongs to the format library, or to a symusic module for that format when the library only parses, as for MusicXML, whose candidate parsers (mx, or pugixml under a symusic reader) stop at the notation.
3. Writing the events into a `Score` is symusic's adapter. It maps fields and contains no musical interpretation.

`Score` does not gain fields that only a notation format needs. If symusic ever needs notation, that is a separate model that renders to `Score`.

## Data flow

| Path | Implementation |
|---|---|
| MIDI read | `minimidi` parses bytes; `src/io/midi.cpp` builds `ScoreNative<Tick>` and converts to shared. Other units convert after parsing |
| MIDI write | Converts to ticks, then `minimidi` encodes (`src/io/midi.cpp`) |
| ABC read/write | The PEGTL grammar in `src/io/abc/syntax.cpp` builds located music tokens. Navigation expands parts and repeats; playback renders notes, tracks and global events directly to `Score<Tick>`. Export validates and projects original event endpoints onto the ABC tick grid, then writes ABC voices from the resulting timeline. The core reads ABC files and resolves their header includes. The binding handles read-time unit conversion and Python warnings. |
| Pickle | `zpp_bits` over the native representation (`src/io/zpp.cpp`) |
| Time units | `convert<To>(score, min_dur)` and `resample(score, tpq, min_dur)` in `include/symusic/conversion.h`; the tempo map drives seconds |
| NumPy | Structure-of-arrays types in `include/symusic/soa.h`; `to_numpy`/`from_numpy` in the event bindings |
| Piano roll | `src/pianoroll.cpp`, exposed as `Track.pianoroll` and `Score.pianoroll` |
| Synthesis | `prestosynth` renders a `Score<Second>` (`src/synth.cpp`); `symusic.soundfont` downloads SoundFonts |

### ABC playback state

The reader resolves shorthand, tuning, accidental propagation and custom chord names in **written order** across voices and line continuations. It attaches lyrics and symbol-line marks to written note groups. Navigation expands parts and repeats into **performed order**. Each scheduled occurrence of a part selects its verse.

On entering a part or skipping an ending, notes use the key, measure accidentals, unit length and meter at their written destination. Tuplets and broken rhythm keep their written proportions. Each overlay lane has its own clock and measure accidentals. Its accompaniment and repeat-bar downbeats follow the performed meter.

The reader allocates chord channels in MIDI track order before rendering. Each lane's chord, drum and drone parts follow its clock. Channel output routes events, pairs releases and pedals, and records lyrics at their performed times. Backing tracks follow their melody lane in the score. Percussion mappings pass from the conductor to subsequent tracks in track order.

The writer assigns timed lyrics to voice lanes. It can split a held note with a tie to give a syllable a slot at the right tick. Each `w:` field uses the slots on its music line.

### ABC scope

ABC 2.1 governs supported notation for every input version. Declaring 2.1 or later enables stricter syntax checks. Older and unversioned input also accepts deprecated spellings such as `+name+` decorations and a bare `!` line break. Once parsed, each construct has the same playback rule across versions. For `%%MIDI` extensions and playback conventions outside the standard, `abc2midi` is the reference.

Chris Walshaw's *The abc music standard 2.1* is available under [CC BY-NC-SA 3.0](https://creativecommons.org/licenses/by-nc-sa/3.0/). [Plain-text copy](../reference/abc-2.1.txt) · [Original standard](https://abcnotation.com/wiki/abc:standard:v2.1)

ABC import is limited to what symusic's MIDI reader retains from `abc2midi` output. It drops single-note retuning carried in SysEx, copyright text and silence after the last event. Additive meters retain their total in the time signature. Features that need new score fields or event types are outside this scope.

Compare shared playback behavior against the `abc2midi` revision pinned in `tests/testcases/abc_oracle.json`. Where the converter differs from ABC 2.1, as with chord lengths and implicit tuplet ratios, test the standard's result directly.

### ABC input diagnostics

The first line of a diagnostic is `source:line:column: ABC [code]: message`. Lines and UTF-8 byte columns start at 1 and refer to the physical input, including after a line continuation or inside an included header. When that line exists, the next line quotes its original text. Long lines show only the area around the location, with `...` marking omitted text. Control characters and invalid bytes are escaped. The position marks the start of the relevant token or construct, not a highlighted range. For example, if line 5 is `[Cz/0] D`, the diagnostic points to the slash:

```text
<string>:5:4: ABC [zero-length-divisor]: duration divides by zero
"[Cz/0] D"
```

Consumers can use the location and bracketed code without matching the English message. A fatal diagnostic stops the read (`std::invalid_argument` in C++, `ValueError` in Python). Recoverable diagnostics accompany the score in `Result::warnings`; the Python binding emits them as `UserWarning`.

## Python typing

The extension registers concrete classes per unit (`ScoreTick`, `NoteQuarterList`, ...) and generic placeholders (`Score`, `Note`, `PyVec`, ...) that only exist for type checkers and raise `TypeError` when instantiated (`py_src/bindings/core/module.cpp`). At install time nanobind writes `core.raw.pyi`; `scripts/lift_generic_stubs.py` rewrites it into `core.pyi` so that concrete classes appear as specializations of the generics. Cross builds consume that generated file from a native build of the same revision through `SYMUSIC_PYTHON_STUB`, because the host cannot import the target extension. The experimental wheel workflow regenerates it on the target and compares declaration syntax trees, ignoring demangler whitespace around C++ template delimiters. `tests/typecheck/` checks the result with mypy. `symusic.factory` gives users unit-agnostic constructors such as `Score(path, ttype="tick")` and makes `isinstance(x, Score)` match every unit.

## Contracts

Refactors keep these behaviors unless a change is explicitly about them:

- **Unit symmetry.** Every type and operation exists for `Tick`, `Quarter` and `Second` with the same semantics.
- **Copy and in-place pairs.** C++ transformations come as `foo()` returning a new value and `foo_inplace()` mutating. Python methods take `inplace=False`; with `inplace=True` they mutate and return `self`. 0.7 replaces the pairs with [one form per operation](../development/conventions.md#design-rules) ([roadmap](../ROADMAP.md#07-api)).
- **Reference stability.** Python references to events, tracks and lists stay valid as described under [data model](#data-model).
- **Round trips.** MIDI read/write and pickle preserve the shared data model. The round trip starts from a `Score`: reading a MIDI file and writing it again does not reproduce the file, because `Score` drops SysEx, unknown meta events, the file's track layout and the channel of each message ([MIDI message layer](../ROADMAP.md#features)). Pickle serves transfer between processes, such as `multiprocessing`, and is not a storage format: a pickle loads only in the symusic version that wrote it. Pickle bytes depend only on event values: struct padding is written as zeros.
- **Explicit errors.** Invalid input raises (`TypeError` for the wrong kind of object, `ValueError` for invalid values such as NaN, `OverflowError` for a tick result outside `int32`) rather than coercing silently. Tick arithmetic and float-to-tick conversion go through the checked helpers in `include/symusic/utils.h`; the [code conventions](../development/conventions.md#errors) map C++ exceptions to Python ones. Malformed MIDI raises instead of crashing; `tests/testcases/MIDIs_corrupted/` holds regressions.
- **Stable ABI.** On GIL-enabled CPython 3.12 and later the extension uses only the limited C API (nanobind `STABLE_ABI`), so one `abi3` wheel per platform serves later GIL-enabled CPython 3 versions. Free-threaded builds require a different ABI and are not supported yet. Do not call CPython APIs outside the limited API in the binding; `abi3audit` in the wheel build rejects them.
- **Thin Python layer.** Hot paths stay in C++; the Python package does not reimplement core logic.

Known violations of memory safety at extreme values are tracked in the [roadmap](../ROADMAP.md#memory-safety).

## Adding a feature

1. Implement it in the core for all time units, in the form the [code conventions](../development/conventions.md#design-rules) give for a modifying or producing operation.
2. Bind it in the matching `py_src/bindings/<area>/` file, with a docstring at the binding site.
3. Extend `symusic.factory` only if construction or dispatch changes.
4. Reinstall (`pip install .`) to regenerate the stubs, and add C++ and Python tests at the new boundary.
5. Update the guide page in `skills/symusic/references/` that owns the area, with a runnable example when the feature is public.

## Build ownership

The root `CMakeLists.txt` declares the core target and its public include/link requirements. `cmake/BuildOptions.cmake` owns project switches and compiler settings; `cmake/Dependencies.cmake` adds pinned submodules without their status chatter. `py_src/`, `tests/cpp/` and `examples/cpp/` each own their targets and installation or execution rules. Release LTO applies to the C++ core; bindings and nanobind remain native objects. Compiler-cache setup belongs to the wheel workflow, not the library's build contract.

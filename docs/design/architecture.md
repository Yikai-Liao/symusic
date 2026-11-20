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

Located ABC tokens retain **written order** across voices and line continuations. Tokenization resolves shorthand definitions. Assembly uses written positions to resolve tuning, accidental propagation policy and custom chord names, and to place ABC 2.1 lyric syllables and symbol-line marks on written notes. Symbol-line marks become the same tokens as inline marks. Navigation builds **performed order** from those tokens and identifies each scheduled occurrence of a part, which selects the verse.

When playback jumps to a part or skips an ending, it restores the written key, measure accidentals, unit length and meter at the destination. The reader assigns tuplet ratios and cross-part broken rhythm to the written notes before rendering. A grace group can borrow from a host in the next written part when playback continues across the label. The lane tracks performed meter for accompaniment and repeat-bar downbeats. A written meter change keeps the beats already counted in the current measure.

Playback keeps a clock and measure-accidental cache per overlay lane. Before rendering, the reader counts each lane's performed chord-channel requests and assigns MIDI numbers in track order. This lets a secondary lane sound before a later primary-lane request without reversing their allocation order. Channel output owns track routing, note releases and pedal pairing; it can record a timed lyric without a new note-on. Legacy lyrics align to performed attacks on the music line. Chord, drum, and drone parts run on the clock of their melody lane, each with its own gate. The final tracks are published in **MIDI track order**, with backing following its melody and overlay lanes placed by their first written split. The conductor primes the percussion map from the first voice's performed primary lane. Each later pass applies its own `drummap` changes at each attack and uses the inherited map for source pitches it has not yet assigned. Its final changes advance the map for subsequent tracks. The core returns a score and located warnings without invoking a converter.

The writer maps timed lyrics onto its planned voice lanes before rendering. It can insert a tied continuation inside a held note to preserve a lyric at that time. As the writer emits each music line, it counts the actual note and chord slots and emits the matching `w:` field.

### ABC scope

ABC 2.1 defines how written notation sounds for every input, including files without a version line or with an older version; the reader has no second, abc2midi-compatible reading of the music. The version only sets how strictly the syntax is parsed. Input that declares 2.1 or later is parsed strictly, while other input also accepts notation that 2.1 deprecates, such as `+name+` decorations and `!` as a line break. Accepting a deprecated spelling never changes what the construct plays. `%%MIDI` directives are abc2midi extensions, so abc2midi defines their behavior, and it also settles playback conventions the standard leaves open.

ABC import adds nothing to the data model. Its result is what symusic's MIDI reader would build from the file abc2midi writes for the tune: notes, programs, controls, pitch bends, tempos, time and key signatures, lyrics, markers and track names. What that path drops, the native reader drops too, such as single-note tuning (MIDI Tuning Standard SysEx), copyright text, silence after the last event and the grouping of an additive meter. A new ABC feature that needs a field or event type the MIDI path lacks is out of scope.

`abc2midi` is the test oracle for the conventions and extensions above. The shared compatibility fixtures cover behavior on which the two agree; native tests assert the standard directly where it differs, for example on chord lengths and implicit tuplet ratios. The upstream revision lives in `tests/testcases/abc_oracle.json`, which CI also reads when building the reference executable.

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

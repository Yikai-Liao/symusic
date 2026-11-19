# Code conventions

Rules for writing and changing code in the C++ core, the nanobind binding and the Python package. Formatters own layout (clice with `.clang-format` for C++, Ruff for Python) and the [architecture](../design/architecture.md) owns layering and contracts; this page covers what neither decides. Much of the existing code predates these rules. New and changed code follows them. When a change touches code that breaks a rule, do not extend that code's pattern; the [roadmap](../ROADMAP.md#code-cleanup) lists the known violations and the planned cleanup.

## Design rules

These apply in every layer and decide most of the questions below.

- **One authority per piece of knowledge.** The list of time units, each event's fields, the file-format dispatch, the mapping from C++ exceptions to Python exceptions and the valid range of each field are each defined in one place, and every consumer derives from it. Before writing a second copy of a rule, such as another `Tick, Quarter, Second` list or another set of extension checks, find the existing authority and use it.
- **The core owns complete operations.** An operation that Python and C++ users both need, including its validation, lives in the core and is callable as a single function. The binding converts arguments and results and adds nothing else; `symusic.factory` dispatches on the unit and adds nothing else.
- **Validate at the boundary, then trust the type.** Values from files, Python or the user are checked once where they enter, and arithmetic that can leave a type's range is checked where it happens (see [integers and time](#integers-and-time)). No API documents a precondition that the caller must keep for memory safety.
- **Language features before macros.** Templates, concepts, `if constexpr`, fold expressions and ordinary functions express repetition. A macro is acceptable only for what the language cannot do (see [macros](#macros)).
- **One form per operation.** An operation either modifies its object or builds a different one, never both by option. Modifying operations (`sort`, `filter`, `shift_time`, `shift_pitch`, `shift_velocity`, `adjust_time`) mutate in place and return nothing; producing operations (`clip`, `trim`, `to`, `resample`, `pianoroll`, `copy`) leave the object unchanged and return the result. To transform a copy, copy first. C++ and Python use the same names and the same split.
- **Write the interface comment first.** For a new public function or type, state what it hides, what the caller must know and how it fails. If that needs a description of the internal steps, change the design before writing the body.

## C++

### Names

| Entity | Style | Example |
|---|---|---|
| Types, concepts, enums | `PascalCase` | `TimeSignature`, `TimeEvent`, `PianorollMode` |
| Functions, variables, data members | `snake_case` | `shift_time`, `ticks_per_quarter` |
| Enumerators | `PascalCase` | `PianorollMode::Onset`, `DataFormat::MusicXML` |
| Constants | `k` + `PascalCase` | `kSortDoc` |
| Template parameters | `PascalCase`; `T` for the time unit | `template<TType T>`, `template<TimeEvent E>` |
| Macros | `SYMUSIC_UPPER_CASE` | `SYMUSIC_COLD` |
| Files | `snake_case.h` / `.cpp` | `time_conversion.h` |

Identifiers never start with an underscore followed by a capital letter or contain a double underscore (`_Ty`, `__COUNT`): the standard reserves them. Following [one form per operation](#design-rules), a modifying method is a plain verb returning `void` (`void shift_time(unit offset)`), and a producing method is `const` and returns the new value (`Score<T> clip(unit start, unit end) const`). There are no `_inplace` names. A second, copying form is added only when a measurement shows that fusing the copy with the transformation is clearly faster than copying first; it is named `foo_copy` and its comment cites the measurement. A predicate reads as a question (`empty`, `is_sorted`).

### Namespaces and headers

- Public API is in `symusic`; implementation details in `symusic::detail`, in headers under `include/symusic/detail/` or in `src/`. Binding code is in `symusic::py`. An optional component with its own third-party dependency, such as a file-format module, has a nested namespace (`symusic::musicxml`), its own header and its own CMake target, and exposes free functions rather than specializations of core templates, so that a build without it fails to compile instead of failing to link. Do not open other top-level namespaces, and do not put `using namespace` in a header.
- Dependencies point from the binding to the core, never back. The core (`include/`, `src/`) does not include `py_src/`, nanobind or pyvec, and the core target does not link them, so the build rejects a violation; a namespace alone does not prevent one. The core still uses pyvec today ([roadmap](../ROADMAP.md#architecture)).
- Headers use `#pragma once` only, without an `#ifndef` guard.
- Include what the file uses, in four groups separated by a blank line: the file's own header, the standard library, third-party headers, project headers. Standard and third-party headers use `<...>`, project headers use `"symusic/..."` (core) or a path relative to `py_src/` (binding). `.clang-format` does not sort includes, so keep each group sorted by hand.
- A header declares what its users need. Template definitions that every unit instantiates go in `src/` with an explicit instantiation for each unit, so that headers stay light and the build stays incremental.

### Types

Use the aliases in `include/symusic/mtype.h`, never the spelled-out standard types:

| Alias | Meaning |
|---|---|
| `i8` ... `i64`, `u8` ... `u64` | Fixed-width integers |
| `f32`, `f64` | `float`, `double` |
| `vec<T>` | `std::vector<T>` |
| `shared<T>` | `std::shared_ptr<T>` |
| `pyvec<T>` | Reference-stable container from `3rdparty/pyvec` |

Declare aliases with `using`, not `typedef`. Inside a template over a time unit, name the scalar `unit` (`using unit = typename T::unit;`), and constrain templates with the existing concepts (`TType`, `TimeEvent`) instead of SFINAE or unconstrained `typename`. A field's type states its range: a MIDI data byte is `u8`, a signed offset is a signed type, and a count or index is `size_t`.

### Integers and time

- Tick arithmetic and float-to-tick conversion go through the checked helpers in `include/symusic/utils.h` (`add_time`, `sub_time`, `round_to_i32`, `narrow_time`, ...). They raise `std::overflow_error` instead of wrapping, and the float overloads are no-ops so that one code path serves every unit.
- The same rule holds for every other integer. A result that can leave its type's range, including any subtraction on an unsigned type, is checked or proven in range by an invariant stated in a comment next to it. A narrowing conversion is a checked helper call or a `static_cast` whose comment states why the value fits.
- Compare values that may overflow in a wide type (`wide_time_t`), and narrow only the values written back.

### Errors

Throw standard exceptions; nanobind maps them to Python built-ins, and that mapping is the error contract:

| C++ | Python | Use for |
|---|---|---|
| `std::invalid_argument` | `ValueError` | An argument or file value outside its valid set, NaN |
| `std::overflow_error` | `OverflowError` | A result outside the target integer type |
| `std::out_of_range` | `IndexError` | An index outside a container |
| `std::runtime_error` | `RuntimeError` | Malformed input files, failed external tools, I/O failures |

Type mismatches are rejected by nanobind as `TypeError`; the core does not throw for them. A message names the parameter or field, the constraint and the received value: `"qpm must be positive and finite, got -1"`. The core never prints to stdout or stderr; it throws, or returns what the caller needs to report.

### Macros

A macro is acceptable only for compiler and platform conditionals, such as an attribute that MSVC spells differently. These macros carry the `SYMUSIC_` prefix and are defined in `include/symusic/utils.h`; add them there rather than next to their first use.

Code generation by macro (`REPEAT_ON` from `3rdparty/MetaMacro.h`, class-body macros such as `BASIC_TIMESTAMP_METHODS`) is being removed; do not add uses. Repeat over time units with a template or a fold over a type list, and generate per-field code from the event descriptions once [one field list per event](../ROADMAP.md#architecture) exists.

### Comments

Comments state invariants, units, ownership and the reason for a non-obvious choice, next to the code they constrain. They do not restate the signature ("inplace sort, and return self reference"), narrate history or keep commented-out code. A `TODO` names a roadmap item; otherwise record the work in the roadmap.

## Binding

- **One binding function per type**, a template over the time unit (`bind_score<T>(m)`). Registering all units goes through one helper that holds the unit list, not a macro or three hand-written calls.
- **Shared adapters, not repeated lambdas.** Bytes to `std::span` conversion, NumPy array ownership and pickle state are each one helper in `py_src/utils/`. A method definition names the core function and its arguments; a lambda body longer than a few lines is core logic in the wrong layer.
- **Generated signatures.** Let nanobind derive signatures from the C++ types. Use `nb::sig` only where the derived signature is wrong for a Python user, such as a per-unit class name, and test the result in `tests/typecheck/`.
- **Docstrings** live at the binding site, as `kNameDoc` constants in the binding file for the area (or a function template where the text names the unit), and follow the [documentation rules](../CONTRIBUTING.md#documentation).
- **No private debugging API.** Helpers such as reference counts belong in tests or behind a test-only build option, not in the public classes.

## Python API

These rules decide the Python surface of new APIs and the [0.7 API](../ROADMAP.md#07-api) changes.

- **One name per thing.** No aliases (`tpq` for `ticks_per_quarter`), no alternative spellings, no second keyword for the same argument (`fmt` and `format`).
- **Python protocols before custom methods.** Use `len`, iteration, `==`, `copy.copy`/`copy.deepcopy`, pickling and `repr` with their standard meaning; a `copy` method, where one exists, agrees with them.
- **Closed sets are typed.** A parameter that takes one of a fixed set of values (a time unit, a mode, a format) is an enum or a `Literal` in the stubs, and an unknown value raises `ValueError`.
- **No `inplace` parameter.** Methods follow [one form per operation](#design-rules): a modifying method returns `None`, as `list.sort` does, and a producing method returns a new object. Neither returns `self` for chaining.
- **Explicit side effects.** Reading an attribute or calling a query does not download, write files or print. Network and file access happen in functions whose name says so.
- **PEP 8 names.** Functions, methods and attributes are `snake_case`; classes are `PascalCase`; a trailing underscore appears only to avoid a keyword.
- **Stable keyword names.** A keyword argument has the same name as the C++ parameter and the guide's description of it, and a time-valued argument is in the unit of the object it is called on.

The Python package follows Ruff's rules from `pyproject.toml` and annotates every public function; `ruff format` owns its layout.

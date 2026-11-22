# Code conventions

Rules for writing and changing code in the C++ core, the nanobind binding and the Python package. Formatters own layout (clice with `.clang-format` for C++, Ruff for Python) and the [architecture](../design/architecture.md) owns layering and contracts; this page covers what neither decides. Much of the existing code predates these rules. New and changed code follows them. When a change touches code that breaks a rule, do not extend that code's pattern; the [roadmap](../ROADMAP.md#code-cleanup) lists the known violations and the planned cleanup.

## Design rules

These apply in every layer and decide most of the questions below.

- **One authority per piece of knowledge.** The list of time units, each event's fields, the file-format dispatch, the mapping from C++ exceptions to Python exceptions and the valid range of each field are each defined in one place, and every consumer derives from it. Before writing a second copy of a rule, such as another `Tick, Quarter, Second` list or another set of extension checks, find the existing authority and use it.
- **The core owns complete operations.** An operation that Python and C++ users both need, including its validation, lives in the core and is callable as a single function. The binding converts arguments and results, dispatches on the time unit, and adds nothing else; the Python package re-exports it and adds only what has no C++ counterpart, such as SoundFont downloads.
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
| Enumerators | `PascalCase` | `PianorollMode::Onset`, `DataFormat::ABC` |
| Constants | `k` + `PascalCase` | `kSortDoc` |
| Template parameters | `PascalCase`; `T` for the time unit | `template<TType T>`, `template<TimeEvent E>` |
| Macros | `SYMUSIC_UPPER_CASE` | `SYMUSIC_COLD` |
| Files | `snake_case.h` / `.cpp` | `time_conversion.h` |

Identifiers never start with an underscore followed by a capital letter or contain a double underscore (`_Ty`, `__COUNT`): the standard reserves them. Following [one form per operation](#design-rules), a modifying method is a plain verb returning `void` (`void shift_time(unit offset)`), and a producing method is `const` and returns the new value (`Score<T> clip(unit start, unit end) const`). There are no `_inplace` names. A second, copying form is added only when a measurement shows that fusing the copy with the transformation is clearly faster than copying first; it is named `foo_copy` and its comment cites the measurement. A predicate reads as a question (`empty`, `is_sorted`).

### Namespaces and headers

- Public API is in `symusic`; implementation details in `symusic::detail`, in headers under `include/symusic/detail/` or in `src/`. Binding code is in `symusic::py`. An optional component with its own third-party dependency, such as a file-format module, has a nested namespace (`symusic::musicxml`), its own header and its own CMake target, and exposes free functions rather than specializations of core templates, so that a build without it fails to compile instead of failing to link. Do not open other top-level namespaces, and do not put `using namespace` in a header.
- Dependencies point from the binding to the core, never back. The core (`include/`, `src/`) does not include `py_src/`, nanobind or pyvec, and the core target does not link them, so the build rejects a violation; a namespace alone does not prevent one.
- Headers use `#pragma once` only, without an `#ifndef` guard.
- Include what the file uses, in four groups separated by a blank line: the file's own header, the standard library, third-party headers, project headers. Standard and third-party headers use `<...>`, project headers use `"symusic/..."` (core) or a path relative to `py_src/` (binding). `.clang-format` does not sort includes, so keep each group sorted by hand.
- A header declares what its users need. A public operation is a non-template member of its class template, so that one explicit instantiation (`template struct Track<Tick>;`) covers it; helpers it calls are instantiated with it. Explicit instantiations are written out, one line per class and unit, in one file per area. Storage-dependent definitions live in `include/symusic/detail/*_impl.h`. Translation units in `src/` instantiate them for value storage, and those in `py_src/` instantiate them for binding storage.

### Types

Use the aliases in `include/symusic/mtype.h`, never the spelled-out standard types:

| Alias | Meaning |
|---|---|
| `i8` ... `i64`, `u8` ... `u64` | Fixed-width integers |
| `f32`, `f64` | `float`, `double` |
| `vec<T>` | `std::vector<T>` |
| `shared<T>` | `std::shared_ptr<T>` |
| `pyvec<T>` | Reference-stable container from `3rdparty/pyvec`, used by the binding |

Declare aliases with `using`, not `typedef`. Inside a template over a time unit, name the scalar `unit` (`using unit = typename T::unit;`), and constrain templates with the existing concepts (`TType`, `TimeEvent`) instead of SFINAE or unconstrained `typename`. A template parameter is a choice the caller makes at compile time; a value known only at run time, such as a file format, is a function argument or a separate named function. A flag that changes what a function does is a separate function or an options struct, not a `bool` parameter. Code that builds a new event list fills a `vec` and moves it into the container with `S::adopt`; it does not append to a `pyvec` element by element. A field's type states its range: a MIDI data byte is `u8`, a signed offset is a signed type, and a count or index is `size_t`.

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
| `std::runtime_error` | `RuntimeError` | Malformed input files, failed external tools |
| `std::filesystem::filesystem_error` | `OSError` subclass by error code (`FileNotFoundError`, `PermissionError`, ...) | A missing or unreadable file, a failed write |

nanobind maps the first four itself; `filesystem_error` derives from `std::runtime_error`, so one exception translator in `py_src/utils/` maps it by its error code, as `OSError` does for `errno`. Type mismatches are rejected by nanobind as `TypeError`; the core does not throw for them. A message names the parameter or field, the constraint and the received value: `"qpm must be positive and finite, got -1"`. The core never prints to stdout or stderr; it throws, or returns what the caller needs to report.

### Language rules

C++ offers several ways to write most of the following; symusic uses one. Each rule cites the [C++ Core Guidelines](https://isocpp.github.io/CppCoreGuidelines/CppCoreGuidelines) entry that gives its reasons, where one exists, and names the clang-tidy check that enforces it once the [roadmap](../ROADMAP.md#static-analysis) enables it in `clice lint`. The Google and LLVM guides are not the base, because both forbid exceptions, and exceptions carry symusic's [error contract](#errors).

- **Modifying operations give the strong guarantee.** An operation that mutates its object and can throw computes and checks every new value before it writes any, so a failure leaves the object unchanged: a `shift_time` that overflows at the 500th note has not moved the first 499. Python code that catches the exception keeps using the object.
- **Producing functions are `[[nodiscard]]`**, so that `score.clip(0, 960);`, written as if it mutated, is diagnosed (`modernize-use-nodiscard`).
- **`noexcept` only where throwing is impossible:** move operations, `swap` and destructors (C.66, E.12). A `noexcept` function that throws calls `std::terminate`, which ends the Python process instead of raising (`bugprone-exception-escape`, `performance-noexcept-move-constructor`).
- **Values own; views borrow.** Objects are values by default, and `shared<T>` appears only where Python can hold a reference into the object ([data model](../design/architecture.md#data-model)). There is no `new`, `delete` or `malloc` (R.11, R.20; `cppcoreguidelines-owning-memory`, `cppcoreguidelines-no-malloc`). A `std::span` or `std::string_view` is a parameter or a local, never a data member or a returned value that outlives the arguments (F.24, F.43; `bugprone-dangling-handle`).
- **Parameters** follow F.16 to F.20: cheap types by value, others by `const&`, a parameter the function moves from by `&&`, and results as return values rather than output parameters (`cppcoreguidelines-rvalue-reference-param-not-moved`).
- **Classes** follow the rule of zero (C.20). A constructor that takes one argument is `explicit` (C.46), and no type defines an implicit conversion operator (C.164) (`google-explicit-constructor`, `cppcoreguidelines-special-member-functions`).
- **Casts** are named casts (ES.49; `cppcoreguidelines-pro-type-cstyle-cast`), and narrowing follows [integers and time](#integers-and-time) (`cppcoreguidelines-narrowing-conversions`). `reinterpret_cast` appears only where file bytes are read as another type, with a comment on alignment and aliasing (`cppcoreguidelines-pro-type-reinterpret-cast`).
- **No mutable global state** (I.2; `cppcoreguidelines-avoid-non-const-global-variables`) and no global whose initialization runs code (I.22). Core functions are then safe to call concurrently on distinct objects, which free-threaded Python relies on.
- **Not used:** RTTI and `dynamic_cast`, virtual inheritance, `<iostream>` in the core, and C++20 modules.

Style choices that carry no risk, such as when to write `auto` or a range-`for` instead of an algorithm, are not rules.

### Standard and toolchain

The code is C++20. A feature may be used when every compiler of the supported floor provides it; for a standard library feature, Apple's libc++ must also provide it at the macOS deployment target (13.0, set in `pyproject.toml`), because that library ships with the operating system. The [roadmap](../ROADMAP.md#toolchain-compatibility) sets the floor and the list of features outside it from a compile probe. Until then the floor is the one in [CONTRIBUTING](../CONTRIBUTING.md#set-up), and the clang-tidy checks that suggest newer library features (`modernize-use-std-format`, `modernize-use-std-print`, `modernize-use-ranges`) stay off.

### Macros

A macro is acceptable only for compiler and platform conditionals, such as an attribute that MSVC spells differently. These macros carry the `SYMUSIC_` prefix and are defined in `include/symusic/utils.h`; add them there rather than next to their first use.

Write explicit instantiations out. Use templates and type lists to share implementations across time units. Repr, NumPy columns and pickle use the member pointers in `include/symusic/detail/event_fields.h`.

### Comments

Comments state invariants, units, ownership and the reason for a non-obvious choice, next to the code they constrain. They do not restate the signature ("inplace sort, and return self reference"), narrate history or keep commented-out code. A `TODO` names a roadmap item; otherwise record the work in the roadmap.

## Binding

- **One binding function per type**, with a shared helper to visit the C++ variant for the active unit.
- **Shared adapters, not repeated lambdas.** Bytes to `std::span` conversion, NumPy array ownership and pickle state are each one helper in `py_src/utils/`. A method definition names the core function and its arguments; a lambda body longer than a few lines is core logic in the wrong layer.
- **Generated signatures.** Let nanobind derive signatures from the C++ types. The derived signature is also the first line of the docstring, the only signature available at run time (`help()` shows it; `inspect.signature` does not work on nanobind functions), so it must read correctly for a Python user. Use `nb::sig` only where it does not, such as a method with an erased binding argument. Typing that no single runtime signature can state, such as a result type that depends on the time unit, belongs in the stub generation rather than in `nb::sig`: `scripts/core_stubgen.pattern`. Test the stubs in `tests/typecheck/`.
- **Docstrings** live at the binding site, as `kNameDoc` constants in the binding file for the area (or a function template where the text names the unit), and follow the [documentation rules](../CONTRIBUTING.md#documentation).
- **No private debugging API.** Helpers such as reference counts belong in tests or behind a test-only build option, not in the public classes.

## Python API

These rules define the Python surface of new APIs.

- **One name per thing.** No aliases (`ticks_per_quarter` for `ticks_per_quarter`), no alternative spellings, no second keyword for the same argument (`fmt` and `format`).
- **Python protocols before custom methods.** Use `len`, iteration, `==`, `copy.copy`/`copy.deepcopy`, pickling and `repr` with their standard meaning; a `copy` method, where one exists, agrees with them. A list type implements `collections.abc.MutableSequence`. A mutable type that compares by value is unhashable.
- **Closed sets are typed.** A parameter that takes one of a fixed set of values (a time unit, a mode, a format) is an enum or a `Literal` in the stubs, and an unknown value raises `ValueError`.
- **No `inplace` parameter.** Methods follow [one form per operation](#design-rules): a modifying method returns `None`, as `list.sort` does, and a producing method returns a new object. Neither returns `self` for chaining.
- **Explicit side effects.** Reading an attribute or calling a query does not download, write files or print. Network and file access happen in functions whose name says so.
- **PEP 8 names.** Functions, methods and attributes are `snake_case`; classes are `PascalCase`; a trailing underscore appears only to avoid a keyword.
- **Words, not abbreviations.** A name is spelled out (`ticks_per_quarter`, `min_duration`, `microseconds_per_quarter`) unless the abbreviation is the term that the main symbolic-music libraries use verbatim for the same thing, as `qpm` is. `ttype` predates the rule and stays, matching the C++ concept `TType`. C++ uses the same names.
- **Stable keyword names.** A keyword argument has the same name as the C++ parameter and the guide's description of it, and a time-valued argument is in the unit of the object it is called on.

The Python package follows Ruff's rules from `pyproject.toml` and annotates every public function; `ruff format` owns its layout.

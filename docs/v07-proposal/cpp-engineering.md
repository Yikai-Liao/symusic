# Style Contract, Repo Layout & Universal CMake

One C++20 engineering contract across `symusic`, `minimidi`, and `anchorvec`, paired with a `tessera-order`-inspired repository layout, universal CMake consumption, and a minimal C99 ABI contract.

## 1. Unified C++ Style & Engineering Contract

| Pillar | Enforceable Contract (`symusic` · `minimidi` · `anchorvec`) |
|---|---|
| Naming | `PascalCase` types, `snake_case` functions, variables and data members, `kPascalCase` constants (the [naming table](../development/conventions.md#names)); no `get_` prefix; uniform `is_*` predicates (`is_empty()`, `is_silent()`, `is_sorted()`) in C++ and Python, while later bindings follow their host's spelling (`isEmpty()` in JS, `IsEmpty()` in Go) (STL `.empty()` only on sequence containers like `std::vector` / `anchorvec`); English-only code, comments, and logs |
| Namespace & Headers | Public `include/symusic/` exposes declarations and C++20 standard-library headers only (`<iosfwd>` in headers, `<ostream>` in `.cpp`, zero `<fmt/...>` or `<minimidi/...>`); storage-generic `*_impl.h` headers live in repo-private `src/`; internal sub-library headers live under `<lib>/detail/`; no `using namespace` in headers or `namespace std` injection; strict IWYU |
| Ownership & Parameters | Pure `ValueStorage` (`std::vector`) by default; non-owning views (`span`, `string_view`) and explicit owning `const std::filesystem::path&` for filesystem paths; aggregate `*Options` structs instead of boolean flag chains |
| Error Taxonomy | The [error contract](../development/conventions.md#errors) owns the list: `std::invalid_argument`, domain `ParseError`, `std::overflow_error`, `std::out_of_range` (indexes only), `std::filesystem::filesystem_error`, `CancelledError` and `std::runtime_error`; `std::range_error`, `std::length_error`, `std::ios_base::failure` and bare `std::exception` are banned; zero exceptions across `extern "C"` |
| Qualifiers & Safety | `[[nodiscard]]` on pure/returning APIs, `const`-correctness, `noexcept` where non-throwing, strict aliasing/alignment/lifetime discipline (`std::uninitialized_copy` + `std::launder`, no union or `reinterpret_cast` type punning, checked narrowing) |
| Integer Aliases | Standalone `minimidi` and `anchorvec` use standard `std::uint*_t` / `std::int*_t`; `symusic` keeps `u8..f64` inside `namespace symusic` and drops `using size_t` pollution |
| Unified Tooling | Identical `.clang-format` and `.clang-tidy` across all 3 repos; `-std=c++20 -pedantic-errors -Wall -Wextra -Wpedantic`; mandatory ASan + UBSan CI lanes |

## 2. C++-First Multi-Binding Directory Layout

**symusic**

```text
include/symusic/            # Public C++20 declarations only (stdlib-only, zero third-party/vendor headers)
src/                        # Core C++20 TUs + repo-private *_impl.h + private MIDI wire TU + optional symusic::abc
bindings/
  python/                   # nanobind + anchorvec SharedStorage + private C++20 SYMZ v3 pickle codec
  js/                       # Node & Bun (N-API) and WASM, ValueStorage + TypedArray (deferred to 0.8+)
  c/                        # C99 ABI for Go/Rust/C consumers (zero exceptions across extern "C"; deferred to 0.8+)
  go/                       # cgo over bindings/c (deferred to 0.8+)
extras/
  abcmidi/                  # Shared GPLv2 external converter recipe
  fluidsynth/               # Shared LGPL dynamic runtime recipe
assets/soundfonts.json      # Declarative SoundFont manifest (IDs, URLs, SHA-384, licenses)
conformance/ & testdata/    # Cross-binding semantic contracts & shared MIDI/ABC fixtures
bench/ & tools/             # Repeatable microbenchmarks & agent-first verification CLI
docs/ & skills/             # Single-owner docs (docs/README.md) & bundled agent skill
CMakePresets.json & cmake/  # Standard presets + ConfigVersion.cmake
```

**minimidi**

```text
include/minimidi/           # Tier-1 streaming SMF wire headers (C++20; Tier-2 DOM deferred 0.8+)
include/minimidi/detail/    # Private VLQ, status-byte, and wire encoder helpers
tests/ & bench/             # Malformed VLQ/SMPTE/SysEx corpus & throughput benches
cmake/                      # minimidi::minimidi install export + ConfigVersion.cmake
docs/ & AGENTS.md           # Wire-format contract, unified C++20 rules, agent routing
```

**anchorvec (C++ / Rust Dual Monorepo)**

```text
CMakeLists.txt              # Root CMake entry for C++20 single-header library
Cargo.toml                  # Root Rust workspace
LICENSE                     # Dual-licensed MIT OR Apache-2.0
spec/                       # Shared invariants (3-state monotonicity, TaggedPtr, zero cycles)
cpp/
  include/anchorvec/anchorvec.hpp # Zero-dependency C++20 single header + make_pinned<T>
  tests/                    # Self-reinsert, cross-list cycle, is_flat, ASan/UBSan suite
rust/
  anchorvec/                   # Companion Rust crate with Miri CI verification
bench/                      # Shared tessera-order-style Bun PR benchmark & plotting harness
```

**Legacy Cleanup**

```text
- Remove MANIFEST.in (scikit-build-core uses pyproject.toml sdist rules)
- Remove redundant setuptools & scikit-build from build-system.requires
- Remove include/symusic/vendor/ (after moving MIDI wire parsing, formatting, and *_impl.h out of public headers)
- Remove dead unordered_dense.h and stray pre_commit.sh / build_docs_with_pagefind.sh
```

## 3. Universal CMake Consumption

### 4-Tier `PRIVATE` Dependency Resolution Pipeline (`fmt` & `minimidi`)

```text
1. Existing TARGET (reuse host fmt::fmt / minimidi::minimidi at build time)
   └─▶ 2. find_package() (system / vcpkg / Conan)
       └─▶ 3. 3rdparty/ (offline git submodule)
           └─▶ 4. FetchContent (online core-only fallback)
```

Because both `fmt` and `minimidi` link **`PRIVATE`** (`$<BUILD_INTERFACE:...>`), installed `symusic::symusic` exports zero third-party include directories or `find_dependency()` calls, avoiding ODR clashes with host libraries such as `libtorch` or `spdlog`.

### Key CMake Hygiene Rules

- Require **CMake 3.21+** (for `PROJECT_IS_TOP_LEVEL`, controlling default test/install options while allowing host projects to opt into install explicitly). At this floor the 4-tier resolution above is written by hand (`if(TARGET)`, `find_package`, `add_subdirectory`, `FetchContent`), because `FetchContent_Declare(... FIND_PACKAGE_ARGS)` needs CMake 3.24.
- Split **`SYMUSIC_ENABLE_SYNTH`** (compiles `Synthesizer` in the core; `OFF` drops `Threads` and `dlopen` for single-threaded WASM and embedded builds) from **`SYMUSIC_BUILD_FLUIDSYNTH`** (builds the bundled LGPL FluidSynth + `libsndfile` runtime in `extras/fluidsynth/`, which requires **CMake 3.25+** and checks the version explicitly).
- **Prerequisite for zero-vendor public headers:** Move MIDI wire parsing (`midi_impl.h`), string formatting (`repr.h` / `<fmt/...>`), and storage-generic `*_impl.h` templates into `src/` **before** marking `minimidi` and `fmt` `PRIVATE` and deleting `include/symusic/vendor/`.
- Export `VERSION` + `ConfigVersion.cmake`, ship `CMakePresets.json`, drop `LANGUAGES C` from core `project()`, and stop leaking LTO flags onto `INTERFACE`.
- Enforce **4 mandatory CI consumer tests**: `consumer_submodule`, `consumer_fetchcontent`, `consumer_find_package` (in a clean prefix with no system `fmt`), and `consumer_host_fmt`.

### Consumer Snippets

**find_package**

```cmake
find_package(symusic 0.7 REQUIRED)
target_link_libraries(my_app PRIVATE symusic::symusic)
```

**FetchContent (Online)**

```cmake
include(FetchContent)
FetchContent_Declare(
  symusic
  GIT_REPOSITORY https://github.com/Yikai-Liao/symusic.git
  GIT_TAG v0.7.0
)
FetchContent_MakeAvailable(symusic)
target_link_libraries(my_app PRIVATE symusic::symusic)
```

**git submodule (Offline)**

```cmake
# Host may already define fmt::fmt — symusic reuses existing TARGETs first
add_subdirectory(3rdparty/symusic)
target_link_libraries(my_app PRIVATE symusic::symusic)
```

## 4. Minimal C99 ABI Contract (0.8+ Go / C Consumers)

Node and Bun link the C++ `ValueStorage` core directly via N-API ([JS & CLI](./js-cli.md)), so `bindings/c/` is built alongside its first C-FFI consumer (`bindings/go` `cgo` in 0.8+) rather than speculatively in 0.7. Its boundary rules are fixed now so core changes stay compatible with it:

| Rule | Contract |
|---|---|
| Opaque handles | `typedef struct symusic_score symusic_score_t;` — zero C++ structs or STL types in C headers |
| Explicit ABI scalars | Integer fields and statuses use `<stdint.h>` fixed-width types (`int32_t`, `uint8_t`, ...), floating-point times and samples use IEEE-754 `float`/`double`, and host buffer lengths use `size_t`. No C `enum` appears in a signature or struct, because its size depends on the compiler |
| Buffers and strings | Explicit `(uint8_t** out_data, size_t* out_len)` outputs and read-only `(const uint8_t*, size_t)` inputs; an empty input may pass `NULL` with length 0. Strings and paths are UTF-8 with an explicit length: an embedded NUL is rejected rather than truncating, and Windows paths are converted from UTF-8 |
| Paired deallocation | Every allocated handle or byte buffer pairs with `symusic_*_free(...)`, so allocator boundaries never cross. Freeing `NULL` does nothing; a double free is undefined |
| Status and error | Every call returns an `int32_t` status: one stable code per row of the [error contract](../development/conventions.md#errors) (`INVALID_ARGUMENT`, `PARSE_ERROR`, `OVERFLOW`, `OUT_OF_RANGE`, `IO_ERROR`, `CANCELLED`, `RUNTIME_ERROR`, `BAD_ALLOC`), plus `UNKNOWN_ERROR`. Detail goes into an optional caller-allocated `symusic_error_t*` (`NULL` allowed; holds `code`, `int32_t sys_errno` for `IO_ERROR`, `message`, and optional byte offset) released with `symusic_error_clear(&err)`. There is no thread-local last error, because consecutive `cgo` calls may move across OS threads. When allocation fails, the status alone reports it and `message` stays `NULL` |
| Cancellation | Long-running calls such as `symusic_synthesizer_render` accept an optional `const uint8_t* cancel_flag` polled atomically at block boundaries, returning `CANCELLED` on request |
| Failure outputs | On failure every out pointer is set to `NULL` and every out length to 0 |
| Options | Every options struct starts with its own `size`, so a newer library reads an older caller's struct and uses defaults for the fields it lacks |
| Values, not borrows | Events and tracks cross as value snapshots or whole columns (`EventColumns` / `events_from_columns`). No pointer into a `std::vector` crosses the boundary, and a note list moves in one call, not one call per note |
| Exception boundary | Every `extern "C"` entry wraps its body in `try { ... } catch (...)` — zero C++ exceptions unwind across C |
| Versions | `symusic_abi_version()` gives the C ABI version and `symusic_has_feature(...)` reports optional parts such as synthesis; neither is the package version |

## 5. `tessera-order` Docs Ownership Matrix

Every architectural fact lives in **one** owning document (`docs/README.md`):

| Fact / Concern | Single Owning Document |
|---|---|
| Public API & cross-binding invariants | `conformance/semantics.md` |
| Module boundaries & storage design | `docs/design/architecture.md` |
| Benchmarks & failed-experiment evidence | `docs/design/evidence.md` |
| Build, CI, sanitizers & consumer tests | `docs/development/ci.md` & `RELEASING.md` |
| Planned debt & follow-up items | `ROADMAP.md` |
| Agent routing & review guardrails | `AGENTS.md` (`CLAUDE.md -> AGENTS.md`) & `.agentic-review.md` |

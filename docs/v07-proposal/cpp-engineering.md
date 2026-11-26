# Style Contract, Repo Layout & Universal CMake

One C++17 engineering contract across `symusic`, `minimidi`, and `pinvec`, paired with a `tessera-order`-inspired repository layout, universal CMake consumption, and a minimal C99 ABI contract.

## 1. Unified C++ Style & Engineering Contract

| Pillar | Enforceable Contract (`symusic` · `minimidi` · `pinvec`) |
|---|---|
| Naming | `PascalCase` types, `snake_case` functions/variables, `kPascalCase` constants, `member_` private fields; no `get_` prefix; STL `.empty()` on containers; English-only code, comments, and logs |
| Namespace & Headers | Internal headers live under `<lib>/detail/`; no `using namespace` in headers; no global `fmt::formatter<std::shared_ptr<T>>` or `namespace std` injection; strict IWYU |
| Ownership & Parameters | Pure `ValueStorage` (`std::vector`) by default; non-owning views (`span`, `string_view`) and explicit owning `const std::filesystem::path&` for filesystem paths; aggregate `*Options` structs instead of boolean flag chains |
| 5-Class Error Taxonomy | Ban `std::ios_base::failure`; use `std::invalid_argument`, `std::out_of_range`, `std::overflow_error`, domain `ParseError`, and `std::filesystem::filesystem_error`; zero exceptions across `extern "C"` |
| Qualifiers & Safety | `[[nodiscard]]` on pure/returning APIs, `const`-correctness, `noexcept` where non-throwing, strict aliasing/alignment/lifetime discipline (`std::uninitialized_copy` + `std::launder`, no union or `reinterpret_cast` type punning, checked narrowing) |
| Integer Aliases | Standalone `minimidi` and `pinvec` use standard `std::uint*_t` / `std::int*_t`; `symusic` keeps `u8..f64` inside `namespace symusic` and drops `using size_t` pollution |
| Unified Tooling | Identical `.clang-format` and `.clang-tidy` across all 3 repos; `-std=c++17 -pedantic-errors -Wall -Wextra -Wpedantic`; mandatory ASan + UBSan CI lanes |

## 2. C++-First Multi-Binding Directory Layout

**symusic**

```text
include/symusic/            # Public C++17 headers + detail/*_impl.h (no vendor/ ODR bombs)
src/                        # Core C++17 library + private MIDI wire TU + optional symusic::abc
bindings/
  c/                        # C99 ABI (zero exceptions across extern "C")
  python/                   # nanobind + pinvec SharedStorage + private SYMZ v3 pickle codec
  js/                       # pinvec / WASM / N-API (deferred to 0.8+)
  go/                       # cgo (deferred to 0.8+)
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
include/minimidi/           # Tier-1 streaming SMF wire headers (C++17; Tier-2 DOM deferred 0.8+)
include/minimidi/detail/    # Private VLQ, status-byte, and wire encoder helpers
tests/ & bench/             # Malformed VLQ/SMPTE/SysEx corpus & throughput benches
cmake/                      # minimidi::minimidi install export + ConfigVersion.cmake
docs/ & AGENTS.md           # Wire-format contract, unified C++17 rules, agent routing
```

**pinvec (C++ / Rust Dual Monorepo)**

```text
CMakeLists.txt              # Root CMake entry for C++17 single-header library
Cargo.toml                  # Root Rust workspace
LICENSE                     # Dual-licensed MIT OR Apache-2.0
spec/                       # Shared invariants (3-state monotonicity, TaggedPtr, zero cycles)
cpp/
  include/pinvec/pinvec.hpp # Zero-dependency C++17 single header + make_pinned<T>
  tests/                    # Self-reinsert, cross-list cycle, is_flat, ASan/UBSan suite
rust/
  pinvec/                   # Companion Rust crate with Miri CI verification
bench/                      # Shared tessera-order-style Bun PR benchmark & plotting harness
```

**Legacy Cleanup**

```text
- Remove MANIFEST.in (scikit-build-core uses pyproject.toml sdist rules)
- Remove redundant setuptools & scikit-build from build-system.requires
- Remove include/symusic/vendor/ (after moving MIDI wire parsing to private .cpp TUs)
- Remove dead unordered_dense.h and stray pre_commit.sh / build_docs_with_pagefind.sh
```

## 3. Universal CMake Consumption

### 4-Tier Dependency Resolution Pipeline

```text
1. Existing TARGET (reuse host fmt::fmt)
   └─▶ 2. find_package() (system / vcpkg / Conan)
       └─▶ 3. 3rdparty/ (offline git submodule)
           └─▶ 4. FetchContent (online core-only fallback)
```

### Key CMake Hygiene Rules

- Require **CMake 3.21+** (for `PROJECT_IS_TOP_LEVEL`, controlling default test/install options while allowing host projects to opt into install explicitly).
- **Prerequisite for `minimidi PRIVATE`:** Move MIDI wire parsing out of public `include/symusic/detail/midi_impl.h` into private `.cpp` translation units **before** marking `minimidi` `PRIVATE` and deleting `include/symusic/vendor/`.
- Export `VERSION` + `ConfigVersion.cmake`, ship `CMakePresets.json`, drop `LANGUAGES C` from core `project()`, and stop leaking LTO flags onto `INTERFACE`.
- Enforce **4 mandatory CI consumer tests**: `consumer_submodule`, `consumer_fetchcontent`, `consumer_find_package`, and `consumer_host_fmt`.

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

## 4. Minimal C99 ABI Contract (Wave 4 Prototype)

Before exposing `bindings/c/` for Go/Rust/JS FFI consumers, the C ABI locks down six invariants:

| Invariant | Contract |
|---|---|
| Opaque handles | `typedef struct symusic_score symusic_score_t;` — zero C++ structs or STL types in C headers |
| Buffer + length outputs | Explicit `(uint8_t** out_data, size_t* out_len)` outputs (and read-only `(const uint8_t*, size_t)` inputs) |
| Paired deallocation | Every allocated handle or byte buffer pairs with `symusic_*_free(...)` so allocator boundaries never cross |
| Stable status codes | Explicit `symusic_status_t` enum mapping the 5 C++ exception classes to stable integer codes |
| Error text lifetime | Thread-local or caller-owned error string (`symusic_last_error()` valid until next call on same thread) |
| Exception boundary | Every `extern "C"` entry wraps body in `try { ... } catch (...)` — zero C++ exceptions unwind across C |

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

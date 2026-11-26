# Style Contract, Repo Layout & Universal CMake

One C++ engineering contract across `symusic`, `minimidi`, and `pyvec`, paired with a `tessera-order`-inspired repository layout and zero-ODR CMake consumption.

## 1. Unified C++ Style & Engineering Contract

| Pillar | Enforceable Contract (`symusic` · `minimidi` · `pyvec`) |
|---|---|
| Naming | `PascalCase` types, `snake_case` functions/variables, `kPascalCase` constants, `member_` private fields; no `get_` prefix; STL `.empty()` on containers; English-only code, comments, and logs |
| Namespace & Headers | Internal headers live under `<lib>/detail/`; no `using namespace` in headers; no global `fmt::formatter<std::shared_ptr<T>>` or `namespace std` injection; strict IWYU |
| Ownership & Parameters | Pure `ValueStorage` (`std::vector`) by default; non-owning `span` / `string_view` / `std::filesystem::path` parameters; aggregate `*Options` structs instead of boolean flag chains |
| 5-Class Error Taxonomy | Ban `std::ios_base::failure`; use `std::invalid_argument`, `std::out_of_range`, `std::overflow_error`, domain `ParseError`, and `std::filesystem::filesystem_error`; zero exceptions across `extern "C"` |
| Qualifiers & Safety | `[[nodiscard]]` on pure/returning APIs, `const`-correctness, `noexcept` where non-throwing, zero UB (no `reinterpret_cast` or union type punning, checked narrowing) |
| Integer Aliases | Standalone `minimidi` and `pyvec` use standard `std::uint*_t` / `std::int*_t`; `symusic` keeps `u8..f64` inside `namespace symusic` and drops `using size_t` pollution |
| Unified Tooling | Identical `.clang-format` and `.clang-tidy` across all 3 repos; `-Wall -Wextra -Wpedantic`; mandatory ASan + UBSan CI lanes |

## 2. C++-First Multi-Binding Directory Layout

**symusic**

```text
include/symusic/            # Public C++ headers + detail/*_impl.h (no vendor/ ODR bombs)
src/                        # Core C++ library + optional symusic::abc adapter
bindings/
  c/                        # C99 ABI (zero exceptions across extern "C")
  python/                   # nanobind + pyvec SharedStorage + SYMZ v2 pickle codec
  js/                       # WASM / N-API
  go/                       # cgo
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
include/minimidi/           # Tier-1 streaming + Tier-2 lossless DOM headers (C++17)
include/minimidi/detail/    # Private VLQ, status-byte, and wire encoder helpers
tests/ & bench/             # Malformed VLQ/SMPTE/SysEx corpus & throughput benches
cmake/                      # minimidi::minimidi install export + ConfigVersion.cmake
docs/ & AGENTS.md           # Wire-format contract, unified C++ rules, agent routing
```

**pyvec**

```text
include/pyvec/pyvec.hpp     # Single zero-dependency header in namespace pyvec (C++17, std::stable_sort + sort_with)
tests/                      # Chunk stability, Arena{chunks, externals}, compact/erase_if, ASan/UBSan
bench/                      # C++ microbenchmarks + tessera-order Bun stats/report/plot toolchain (PR-scoped)
cmake/                      # pyvec::pyvec INTERFACE export + ConfigVersion.cmake
AGENTS.md & .clang-format   # Unified tri-repo style config & verification commands
```

**Legacy Cleanup**

```text
- Remove MANIFEST.in (scikit-build-core uses pyproject.toml sdist rules)
- Remove redundant setuptools & scikit-build from build-system.requires
- Remove include/symusic/vendor/ (fmt, minimidi public ODR collisions)
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

### Key CMake Hygiene Fixes

- Guard tests, examples, and install rules with `PROJECT_IS_TOP_LEVEL`.
- Export `VERSION` + `ConfigVersion.cmake` and ship `CMakePresets.json`.
- Drop `LANGUAGES C` from core `project()`, stop leaking LTO flags onto `INTERFACE`, and remove `include/symusic/vendor/` completely.
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

## 4. `tessera-order` Docs Ownership Matrix

Every architectural fact lives in **one** owning document (`docs/README.md`):

| Fact / Concern | Single Owning Document |
|---|---|
| Public API & cross-binding invariants | `conformance/semantics.md` |
| Module boundaries & storage design | `docs/design/architecture.md` |
| Benchmarks & failed-experiment evidence | `docs/design/evidence.md` |
| Build, CI, sanitizers & consumer tests | `docs/development/ci.md` & `RELEASING.md` |
| Planned debt & follow-up items | `ROADMAP.md` |
| Agent routing & review guardrails | `AGENTS.md` (`CLAUDE.md -> AGENTS.md`) & `.agentic-review.md` |

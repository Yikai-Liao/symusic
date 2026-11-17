# Roadmap

Planned work and accepted debt. Remove an item when the change that resolves it merges.

## Toolchain compatibility

Workarounds and unsupported configurations with current toolchains.

- **CMake 4.** `CMAKE_POLICY_VERSION_MINIMUM` in `CMakeLists.txt` and `pyproject.toml` exists because CMake 4 rejects projects declaring a minimum below 3.5. The direct submodules all declare 3.8 or later; the remaining offender is `gcem` (3.1) nested in `prestosynth`, whose upstream has since updated `gcem` and `libsamplerate`. Updating `prestosynth` should let the setting go.
- **Free-threaded Python.** Importing the extension on `3.13t`/`3.14t` re-enables the GIL with a `RuntimeWarning`. Supporting it requires declaring free-threading support in the module and auditing shared state.
- **Cross-compiled wheels.** Prioritize macOS 13+ (arm64 and x86_64) and Linux ARM64 with [xclang](https://github.com/clice-io/xclang). The optional [cross-validation workflow](docs/development/ci.md#cross-compilation-experiment) also tests Windows MSVC targets using Microsoft's SDK. Python wheels still need target Python headers/libraries, a host-side stub-generation plan, wheel repair and installed-package tests. Keep native packaging until these contracts are verified.

## Static analysis

- **clice lint as a gate.** Re-run `clice lint` after the `zpp_bits` serializer fix and triage the remaining findings in `examples/`, `py_src/` and `tests/cpp/`; use-after-move findings in tests may be intentional. Fix or suppress them, then run `clice lint` in the `lint` lane.
- **ty.** `ty check` on the installed package reports about 290 diagnostics, mostly generated-stub overrides and NumPy method names shadowed by the containers. Decide whether ty replaces the mypy checks in `tests/typecheck`, fix the stubs, then add it to CI.
- **Markdown code blocks.** Ruff can format Python blocks in Markdown; the `lint` lane does not check them yet.

## Merge checks

- **Require CI before merging.** Configure a GitHub branch protection rule or ruleset requiring `ci-ok`; the workflow reports the result but cannot enforce merge restrictions by itself.

## Memory safety

Signed `Tick` (`i32`) arithmetic and narrowing conversions can overflow, which is undefined behavior in C++. The policy for invalid times (throw, skip or clamp) is not yet defined; decide it once and apply it at every site below.

- **Pianoroll bounds.** `TrackPianoroll` and `ScorePianoroll` multiply dimensions without overflow checks, size the time axis from `track.end() + 1` / `score.end() + 1`, and write through unchecked offsets in `set()`, `get()` and `operator()` (`src/pianoroll.cpp`). Negative times or durations become huge `size_t` offsets.
- **Tick arithmetic.** `Note::end()`, `Pedal::end()` and `shift_time_inplace()` compute `time + duration` and `time += offset` directly (`include/symusic/event.h`, `src/event.cpp`). The results propagate through `ops::end()`, `trim_inplace()` and `adjust_time_inplace_inner()` (`include/symusic/ops.h`), `Track::end()`, `Score::end()`, MIDI parse/export duration handling (`src/io/midi.cpp`) and time conversion (`src/conversion.cpp`). Use `int64_t` intermediates and one checked write-back helper.
- **Float-to-integer narrowing.** `Tempo::qpm2mspq()`, `Quarter2Tick::time()`, `Second2Tick::get_time()` and `resample_inner()` cast rounded floats to `i32` without checking finiteness or both bounds. Route them through one checked helper.
- **Null container elements.** Equality and containment helpers in `py_src/utils/vector_bindings.h` dereference `shared_ptr` elements without null checks. Reject `None` at the binding boundary.
- **Padding in pickles.** Events are pickled with `zpp::bits::as_bytes`, which copies struct padding; the padding is uninitialized, so pickling the same score twice can give different bytes. Zero the padding or serialize members explicitly, keeping old pickles loadable.
- **Sanitizers in CI.** Run ASan/UBSan builds in CI rather than only locally.

## Test coverage

- **Python references into containers.** No test checks that a reference such as `note = notes[1]` stays valid after the element or its container is modified or removed.
- **Resample through Python.** Rounding on downsampling and upsampling is tested only in C++ (`tests/cpp/test_conversion.hpp`).
- **Boundary inputs.** The memory-safety items above need regression tests for negative times and durations, large ticks and offsets, extreme tempos, malformed MIDI durations and `None` elements.

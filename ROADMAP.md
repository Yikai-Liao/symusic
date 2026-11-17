# Roadmap

Planned work and accepted debt. Remove an item when the change that resolves it merges.

## Toolchain compatibility

CI has failed on `main` since the runner images moved to newer toolchains (observed November 2026). Each item below was diagnosed but not fixed.

- **Clang 21 / AppleClang 21.** `src/io/zpp.cpp` fails to compile: with C++26 structured-binding packs, the pinned `zpp_bits` tries to decompose `Note` and other events whose base class `TimeStamp` also has members. Upstream fixed this in `zpp_bits` v4.7.7 (commit `0f35eab`, "Fix inspection_guarded to exclude types with explicit serializers"). A local GCC 14 build and C++ tests pass with v4.7.7, fmt 12.2.1, Catch2 v3.16.1 and nanobench v4.6.0.
- **CMake 4.** `CMAKE_POLICY_VERSION_MINIMUM` in `CMakeLists.txt`, `pyproject.toml` and `tests.yml` exists because CMake 4 rejects projects declaring a minimum below 3.5. The direct submodules all declare 3.8 or later; the remaining offender is `gcem` (3.1) nested in `prestosynth`, whose upstream has since updated `gcem` and `libsamplerate`. Updating `prestosynth` should let the setting go. On Windows, PowerShell splits the unquoted `-DCMAKE_POLICY_VERSION_MINIMUM=3.5` at the dot; CMake then rejects `"3"`, and `check_ipo_supported` fails because CMP0069 is unset.
- **Python 3.9.** `ubuntu-latest` (Ubuntu 26.04) no longer provides Python 3.9, and 3.9 is past end of life. Decide whether to raise `requires-python` or to test 3.9 on an older image.
- **nanobind 3.** The pinned v2.12.0 is one major version behind (3.1.0). Upgrading affects stub generation and `scripts/lift_generic_stubs.py`.
- **abcmidi.** Upstream has newer `abc2midi` releases (5.14) that change conversion output; updating needs the ABC tests re-baselined.
- **Free-threaded Python.** Importing the extension on `3.13t`/`3.14t` re-enables the GIL with a `RuntimeWarning`. Supporting it requires declaring free-threading support in the module and auditing shared state.

## Memory safety

Signed `Tick` (`i32`) arithmetic and narrowing conversions can overflow, which is undefined behavior in C++. The policy for invalid times (throw, skip or clamp) is not yet defined; decide it once and apply it at every site below.

- **Pianoroll bounds.** `TrackPianoroll` and `ScorePianoroll` multiply dimensions without overflow checks, size the time axis from `track.end() + 1` / `score.end() + 1`, and write through unchecked offsets in `set()`, `get()` and `operator()` (`src/pianoroll.cpp`). Negative times or durations become huge `size_t` offsets.
- **Tick arithmetic.** `Note::end()`, `Pedal::end()` and `shift_time_inplace()` compute `time + duration` and `time += offset` directly (`include/symusic/event.h`, `src/event.cpp`). The results propagate through `ops::end()`, `trim_inplace()` and `adjust_time_inplace_inner()` (`include/symusic/ops.h`), `Track::end()`, `Score::end()`, MIDI parse/export duration handling (`src/io/midi.cpp`) and time conversion (`src/conversion.cpp`). Use `int64_t` intermediates and one checked write-back helper.
- **Float-to-integer narrowing.** `Tempo::qpm2mspq()`, `Quarter2Tick::time()`, `Second2Tick::get_time()` and `resample_inner()` cast rounded floats to `i32` without checking finiteness or both bounds. Route them through one checked helper.
- **Null container elements.** Equality and containment helpers in `py_src/utils/vector_bindings.h` dereference `shared_ptr` elements without null checks. Reject `None` at the binding boundary.
- **Sanitizers in CI.** Run ASan/UBSan builds in CI rather than only locally.

## Test coverage

- **Python references into containers.** No test checks that a reference such as `note = notes[1]` stays valid after the element or its container is modified or removed.
- **Resample through Python.** Rounding on downsampling and upsampling is tested only in C++ (`tests/cpp/test_conversion.hpp`).
- **Boundary inputs.** The memory-safety items above need regression tests for negative times and durations, large ticks and offsets, extreme tempos, malformed MIDI durations and `None` elements.

# Roadmap

Planned work and accepted debt. Remove an item when the change that resolves it merges.

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

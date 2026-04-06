 # symusic C++ Undefined Behavior Audit Report

Date: 2026-04-05

Scope:
- `include/symusic`
- `src`
- `py_src` (only the binding and utility code used by the active build)

Method:
- Static review of the main C++ paths
- Targeted repository-wide search for:
  - unchecked `Tick(i32)` arithmetic
  - signed-to-`size_t` index construction
  - float-to-int narrowing into `Tick` / `i32`
- Cross-checks with parallel subagents in the earlier audit pass
- Local sanitizer-backed confirmation on the original `pianoroll` ownership bug

Summary:
- The previously confirmed `pianoroll` ownership UB has already been fixed in the current branch by switching to a move-only `std::unique_ptr<pianoroll_t[]>` owner.
- The remaining arithmetic and bounds risks are not limited to `pianoroll`.
- `pianoroll` is still the highest-priority active sink because it turns unchecked time values into raw pointer arithmetic and bulk writes.
- The same family of unchecked `Tick(i32)` arithmetic also appears in `event`, `ops`, MIDI parse/export, and conversion code.
- This report still does not claim exhaustive coverage of all possible UB in the codebase.

## 1. Overall Conclusion

The original `pianoroll` ownership bug was real and is now fixed. However, the broader numeric-safety problem is not isolated to `pianoroll`.

There are two active patterns that now matter most:

1. unchecked signed arithmetic on `Tick::unit == i32`
2. unchecked use of signed musical time values to build array dimensions, pointer offsets, and durations

`pianoroll` remains the most urgent active path because it combines both patterns and writes directly into heap-backed buffers. But the arithmetic sources are broader:
- `Note::end()` / `Pedal::end()`
- `shift_time_inplace()`
- `ops::trim_*()` and `ops::adjust_time_*()`
- MIDI parse/export duration handling
- conversion paths that narrow floating-point values back into `Tick` / `i32`

## 2. Findings And Status

### 2.1 `pianoroll` ownership UB has been fixed

Severity: Resolved

Current locations:
- `include/symusic/pianoroll.h:38-51`
- `include/symusic/pianoroll.h:77-91`
- `src/pianoroll.cpp:7-11`
- `src/pianoroll.cpp:80-85`

Current state:
- `TrackPianoroll` and `ScorePianoroll` now store `std::unique_ptr<pianoroll_t[]>`.
- Copy construction and copy assignment are deleted.
- Move construction and move assignment are explicitly defaulted.
- The Python export path still uses `release()` and `nb::capsule(... delete[] ...)`, which matches nanobind's documented ndarray ownership model.

Validation already added:
- C++ ownership and move tests:
  - `tests/cpp/test_pianoroll.hpp`
- Python ndarray lifetime tests:
  - `tests/test_pianoroll.py`
- Local ASan/UBSan verification on the `pianoroll` path

Conclusion:
- The original `new[]` / `delete` mismatch is no longer present in the current branch.
- The previous raw-pointer copy/double-free hazard is no longer present in the current branch.

### 2.2 Active `pianoroll` bounds and allocation-size risks remain

Severity: High

Locations:
- `src/pianoroll.cpp:11`
- `src/pianoroll.cpp:20`
- `src/pianoroll.cpp:25-29`
- `src/pianoroll.cpp:54-65`
- `src/pianoroll.cpp:73`
- `src/pianoroll.cpp:85`
- `src/pianoroll.cpp:94-106`
- `src/pianoroll.cpp:133-141`
- `src/pianoroll.cpp:149`

Description:
- `TrackPianoroll` and `ScorePianoroll` still compute allocation sizes with unchecked `size_t` multiplications.
- `from_track()` still builds `timeDim` from `track.end() + 1`.
- `from_score()` still builds `timeDim` from `score.end() + 1`.
- `set()` still accepts `size_t start` / `size_t duration` and forwards them directly into pointer arithmetic plus `std::fill_n`.
- `operator()` and `get()` still perform unchecked offset computation.

Risk:
- If a signed `Tick` time or duration is negative, it can still be converted into a huge `size_t` and become an out-of-bounds write.
- If `track.end() + 1` or `score.end() + 1` overflows in the upstream signed domain, the resulting `timeDim` is already invalid before allocation.
- If the `size_t` dimension multiplications wrap, the code can allocate an undersized buffer and then write beyond it.

This is the main reason the active memory-safety risk is still concentrated in `pianoroll`.

Remediation:
- Add checked dimension multiplication for constructors and `clear()`.
- Stop using `track.end() + 1` / `score.end() + 1` directly inside `pianoroll`.
- Build validated write intervals locally from note fields using `int64_t` intermediates.
- Add explicit bounds checks before pointer arithmetic and `std::fill_n`.

### 2.3 Unchecked `Tick(i32)` arithmetic is not limited to `pianoroll`

Severity: High

Primary UB sources:
- `include/symusic/event.h:95`
- `include/symusic/event.h:121`
- `src/event.cpp:32-33`

Description:
- `Note::end()` and `Pedal::end()` directly compute `time + duration`.
- `shift_time_inplace()` directly computes `time += offset`.
- For `Tick`, these are signed `i32` operations. Signed overflow is UB in C++.

Propagation sites:
- `include/symusic/ops.h:147-161`
- `include/symusic/ops.h:248`
- `include/symusic/ops.h:281-294`
- `include/symusic/ops.h:398`
- `src/track.cpp:30-38`
- `src/score.cpp:193-202`
- `src/pianoroll.cpp:20`
- `src/pianoroll.cpp:28`
- `src/pianoroll.cpp:95`
- `src/pianoroll.cpp:106`
- `src/io/midi.cpp:95`
- `src/io/midi.cpp:286-288`
- `src/io/midi.cpp:464`
- `src/conversion.cpp:233`
- `src/conversion.cpp:240-241`

What is happening:
- `ops::end()` calls `event.end()` for duration-carrying events, so overflow can already happen there.
- `Track::end()` and `Score::end()` propagate the overflowed result further upward.
- `ops::trim_inplace()` uses `event.end() - start`, `event.end() <= end`, and `event.duration -= event.end() - end`.
- `ops::adjust_time_inplace_inner()` builds `get_end(event)` as `event.time + event.duration`, then writes back `event.duration = end - start`.
- MIDI parsing writes `note.duration = end_time - note.time` and pedal durations as `cur_time - last_pedal_on[channel]`.
- MIDI export writes `minimidi::NoteOff(note.end(), ...)`.
- Conversion code uses `event->end()` and then writes back `duration = end - start`.

Conclusion:
- The active arithmetic problem is broader than `pianoroll`.
- `pianoroll` is just the most obvious place where overflowed or negative values become raw-memory writes.

Remediation:
- Introduce checked helpers for:
  - `time + duration`
  - `time += offset`
  - `end - start`
- Use `int64_t` intermediates, then validate before writing back into `Tick(i32)`.
- Replace direct `event.end()` use in the critical paths above with checked equivalents.

### 2.4 Float-to-int narrowing into `Tick` / `i32` still needs hardening

Severity: Medium

Locations:
- `include/symusic/event.h:182-183`
- `src/conversion.cpp:130-131`
- `src/conversion.cpp:278-285`
- `src/conversion.cpp:385-392`

Description:
- `Tempo::qpm2mspq()` casts `std::round(60000000. / qpm)` directly into `i32`.
- `Quarter2Tick::time()` casts `std::round(t * tpq)` directly into `Tick::unit`.
- `Second2Tick::get_time()` casts `std::round(cur_factor * (t - pivot_from))` directly into `Tick::unit`.
- `resample_inner()` checks the upper bound, but does not check:
  - the lower bound
  - `std::isfinite(x)`

Risk:
- Out-of-range or non-finite float-to-int conversion is UB in C++.
- These cases are less common than the `pianoroll` and `Tick` overflow paths, but they are still real language-level UB sites.

Remediation:
- Route all float-to-`Tick` / float-to-`i32` writes through one checked helper.
- Require:
  - `std::isfinite(x)`
  - lower bound
  - upper bound

### 2.5 Python binding vector helpers still need null-safety hardening

Severity: Medium

Locations:
- `py_src/utils/vector_bindings.h:407`
- `py_src/utils/vector_bindings.h:421`
- `py_src/utils/vector_bindings.h:635`

Description:
- Equality and containment helpers dereference `shared_ptr` elements without null checks.
- If Python can inject `None` into those containers, these code paths can dereference null.

Status:
- Not part of the `Tick` arithmetic family.
- Still an active UB risk if null elements are constructible from Python.

Remediation:
- Reject null elements at the binding boundary.
- Add explicit null checks in append/extend/compare/contains paths where relevant.

## 3. Why `pianoroll` Is Still The First Fix Even Though The Problem Is Broader

`pianoroll` is not the only place where unsafe arithmetic exists. It is still the first fix because it turns those values into:
- heap allocation sizes
- raw pointer offsets
- `std::fill_n()` write lengths

That makes it the shortest path from a bad musical-time value to memory corruption.

In contrast:
- `Track::end()` and `Score::end()` mostly propagate bad arithmetic
- `ops::*` mostly reshape event data
- MIDI parse/export and conversion mostly create or transform values

Those are still important, but `pianoroll` is the most direct memory-writing sink.

## 4. Nanobind Ownership Note For `pianoroll`

The earlier ownership recommendation has been implemented in the correct shape:
- keep the storage heap-backed
- keep the C++ owner move-only
- preserve `release()`
- keep Python ownership based on `nb::capsule`

Current binding behavior:
- `py_src/bindings/track/track_bindings.h:308-317`
- `py_src/bindings/score/score_bindings.h:533-543`

Implication:
- The current `unique_ptr<T[]>` plus `release()` plus `nb::capsule(delete[])` design remains valid.
- The remaining work on `pianoroll` is now about arithmetic and bounds safety, not ownership shape.

Reference:
- https://nanobind.readthedocs.io/en/latest/ndarray.html

## 5. Recommended Fix Order

Recommended order:

1. `pianoroll` dimension, interval, and pointer-bound validation
2. checked `Tick` arithmetic helpers in `event` and `ops`
3. MIDI parse/export duration hardening
4. checked float-to-`Tick` / float-to-`i32` conversions
5. null-safety hardening in Python binding helpers

## 6. TODO

- [x] Replace the owning raw pointer in `TrackPianoroll` and `ScorePianoroll` with a move-only owner that still supports `release()`, using `std::unique_ptr<pianoroll_t[]>`
- [x] Delete copy construction and copy assignment for `TrackPianoroll` and `ScorePianoroll`
- [x] Default move construction and move assignment for `TrackPianoroll` and `ScorePianoroll`
- [x] Remove the old `new[]` / `delete` mismatch from the active `pianoroll` implementation
- [x] Keep the nanobind ownership-transfer model based on `nb::capsule`
- [x] Add ownership and ndarray lifetime regression tests for `pianoroll`
- [x] Run local ASan/UBSan verification for the ownership fix
- [ ] Add checked dimension multiplication in `TrackPianoroll` and `ScorePianoroll`
- [ ] Replace direct `track.end() + 1` / `score.end() + 1` use inside `pianoroll` with a checked local interval scan
- [ ] Validate negative time, negative duration, and computed end positions before every `pianoroll` write
- [ ] Add explicit bounds checks for `pianoroll::operator()`, `get()`, `set()`, and `clear()`
- [ ] Define and document the policy for invalid `Tick` values in `pianoroll`:
- [ ] throw
- [ ] skip invalid events
- [ ] or clamp explicitly
- [ ] Introduce checked helpers for `Tick` arithmetic in `event.h` / `event.cpp`
- [ ] Replace direct `event.end()`-based arithmetic in `ops.h` with checked/int64-based logic
- [ ] Harden MIDI parse/export duration handling in `src/io/midi.cpp`
- [ ] Add one shared checked helper for float-to-`Tick` / float-to-`i32` conversions
- [ ] Reject or explicitly handle null `shared_ptr` elements in Python container bindings
- [ ] Add tests covering:
- [ ] negative time and negative duration
- [ ] large `Tick` and large offset values
- [ ] `pianoroll` dimension overflow
- [ ] extreme tempo / resample / conversion inputs
- [ ] MIDI parse/export boundary cases
- [ ] Python-side invalid objects and `None`
- [ ] Add sanitizer builds to CI, not only local verification

## 7. Suggested Validation

- Use the project virtual environment
- Run `pip install .` after code changes
- Run Python tests only after reinstalling the binding
- Keep a sanitizer build for targeted UB verification:
  - `-fsanitize=address,undefined`
  - `-fno-omit-frame-pointer`

## 8. Suggested External Wording

If this needs to be shared externally, a defensible wording is:

"A focused UB audit of symusic's C++ implementation confirmed multiple high-confidence undefined behavior issues. The original `pianoroll` ownership bug has already been fixed in the current branch, but active unchecked arithmetic and bounds risks remain in `pianoroll`, `event`, `ops`, MIDI parse/export, and conversion code. The current conclusion is based on static review plus limited sanitizer confirmation and does not claim exhaustive coverage of all possible UB."

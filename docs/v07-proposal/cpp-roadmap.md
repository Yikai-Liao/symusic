# PR Stack & Rollout Plan

Implementation status across stacked PRs [#129–#134](https://github.com/Yikai-Liao/symusic/pulls) and the four-wave rollout to the C++20 multi-binding ecosystem that 0.7 starts.

## 1. Stacked PRs (`#129`–`#134`)

| PR | Branch | Scope | Key Changes | Status |
|---|---|---|---|---|
| [#129](https://github.com/Yikai-Liao/symusic/pull/129) | `07/pr1-core-safety` | Core Safety | Negative-time tempo anchors, checked narrowing, validated `trim()`, MIDI import fixes, versioned ZPP binary serialization (breaks 0.6 pickles) | In review |
| [#130](https://github.com/Yikai-Liao/symusic/pull/130) | `07/pr2-abc-python` | ABC Process | Isolates GPLv2 `abc2midi`/`midi2abc` into subprocesses via `_abc.py`, adds `ABCConversionWarning`, exact `X:` tune selection, FIFO guards | In review |
| [#131](https://github.com/Yikai-Liao/symusic/pull/131) | `07/pr3-core-refactor` | Core Refactor | `ValueStorage` policy on `Score<T, S>`/`Track<T, S>`, drops `pyvec` & `*Native`, `detail::event_fields<E>`, faster MIDI writing (`5129d98`), removes `include/argparse.h` | In review |
| [#132](https://github.com/Yikai-Liao/symusic/pull/132) | `07/pr4-fluidsynth` | FluidSynth | Replaces `prestosynth` with dynamically loaded FluidSynth (`dlopen` / `LoadLibraryExW`), `SynthSettings`, `AudioBuffer`, fresh state per render (256 tracks), SHA-384 verification | In review |
| [#133](https://github.com/Yikai-Liao/symusic/pull/133) | `07/pr5-bindings` | Bindings | One Python class per concept (`std::variant`), private `symusic._core`, nanobind stubgen, `MutableSequence`, `read_score_file`, 0.75x Python note iteration time | In review |
| [#134](https://github.com/Yikai-Liao/symusic/pull/134) | `07/pr6-api` | 0.7 API | Removes `*_inplace`, CC64-derived `Track::pedals() const`, `EventColumns<E>`, drops `MetaMacro.h`, `MidiReadOptions`/`InvalidDataBytes`, `symusic::symusic` export, ~40% faster `clip` / ~30% faster `trim` | In review |

## 2. Implemented (`07/pr6-api`) vs Remaining 0.7 Scope

| Area | Implemented in PRs `#129–#134` | Remaining 0.7 Scope (RFC `#136` & Ecosystem Plan) |
|---|---|---|
| Storage & Standard | `ValueStorage` / `SharedStorage`, `detail/*_impl.h`, `EventColumns<E>`, no `pyvec` / `*Native` / `MetaMacro.h` / `argparse.h` | `anchorvec` 3-state zero-cycle `TaggedPtr` + `is_flat()` + `make_pinned<T>`, two-level Python reference model, standard-library copy semantics, remove `zpp_bits` → private `SYMZ v3` codec |
| Naming & Containers | `Score::from_midi` / `to_midi`, `note_count()`, `track_count()`, `microseconds_per_quarter`, `minor: bool`, `MidiReadOptions` | `TextEvent`, `beats(start=0)`, `from_file()` / `to_file()`, `download_soundfont()`, unify MIDI timing units on `Score::tpq` and `Tempo::mspq` (`from_qpm`/`from_mspq`, `qpm_to_mspq`/`mspq_to_qpm`), uniform `is_*` predicates (`Score::is_empty()`, `Track::is_empty()`, `Note::is_silent()`), `clip` with `ClipOptions` (`head`/`tail`) replacing `trim`, single generic `EventList[Note[Tick]]` |
| Operations & Safety | No `*_inplace`, transactional `FieldUpdates`, CC64 `Track::pedals() const`, ~40% `clip` / ~30% `trim` speedups | Returning `adjust_time`, `resample()` as a member (the Python binding already resamples in one step), stable `sort` (a C++ tessera sort implementation follows), `shift_*` once-per-event atomic rollback, reentrancy guard on callbacks, `Note` `velocity=64` default, read-only `tpq`, the [error contract](../development/conventions.md#errors), `strict` read option, `KeySignature::fifths` and `tonic()` replacing `key` and `degree()`, `AudioChannels` + expanded `SynthSettings` (`gain`, FluidSynth 2.6 `interpolation`, `cpu_cores`, optional `library_path` with bundled-first search), the four Python [deprecated forms](../../ROADMAP.md#07-api), the [time events at a clip start](../../ROADMAP.md#behavior-defects) fix, fix all 7 verified header audit defects |
| Synthesis & Packaging | Runtime-loaded FluidSynth (`dlopen` / `LoadLibraryExW`), `SynthSettings`, `AudioBuffer`, ABC subprocess, `symusic::symusic` | Private MIDI wire `.cpp` → `minimidi PRIVATE` + remove `vendor/`, CMake 3.21+ 4-tier deps & 4 CI consumer tests, optional `symusic::abc`, `assets/soundfonts.json`, minimal C99 ABI prototype |

## 3. 4-Wave Zero-Conflict Rollout

1. **Wave 1 — Core PR Stack (`#129–#134`) + Unreleased Sub-Library Validation:** Land `#129–#134` on `main`, AND develop `anchorvec` (C++20 single-header + dual C++/Rust layout + Bun PR bench) & `minimidi` (C++20 Tier-1 streaming of the bytes as written, `EventView::is_note_off()`) against real unpublished `symusic` Python + native consumers **before** tagging `v0.1.0` / `v0.10.0`.
2. **Wave 2 — Finalized 0.7 API, Bug Fixes & Sub-Library Pin Bump:** Tag & adopt `anchorvec v0.1.0` + `minimidi v0.10.0`; ship all remaining finalized 0.7 Python + Native C++ API items (`EventList[Note[Tick]]`, two-level reference model, standard-library copy semantics, stable `sort`, read-only `tpq` replacing `ticks_per_quarter`, `strict` read option, the error contract, `TextEvent`, `beats`/`downbeats`, `from_file`/`to_file`, single-step `resample`, `download_soundfont()`, `Tempo::mspq` + `from_qpm`/`from_mspq` + `qpm_to_mspq`/`mspq_to_qpm` reverting `#134`'s `microseconds_per_quarter`, uniform `is_empty()`/`is_silent()` predicates, `clip` with `ClipOptions` (`head`/`tail`) replacing `trim`, callback reentrancy guard, `KeySignature::fifths` and `tonic()` replacing `key` and `degree()`, `AudioChannels` for `render` + expanded `SynthSettings` (`gain`, FluidSynth 2.6 `interpolation`, `cpu_cores`, optional `library_path` with bundled-first search), the four Python [deprecated forms](../../ROADMAP.md#07-api), the [time events at a clip start](../../ROADMAP.md#behavior-defects) fix, private C++20 `SYMZ v3` Python pickle codec in the current `py_src/` (it moves to `bindings/python/` in Wave 4), and all 7 verified defect fixes).
3. **Wave 3 — Core Packaging & Universal CMake:** Move MIDI wire implementation out of public `detail/midi_impl.h` into private `.cpp` translation units (enabling `minimidi PRIVATE` + deleting `include/symusic/vendor/`), remove `zpp_bits` from the core, bump CMake to `3.21+`, wire 4-tier dependency resolution, and enable all 4 CI consumer smoke tests.
4. **Wave 4 — Multi-Binding Layout & `tessera-order` Governance:** Move existing code into `bindings/python/` and `extras/` without behaviour changes, and add the new `assets/soundfonts.json`, `conformance/semantics.md`, `docs/README.md` and the minimal C99 ABI prototype (`bindings/c/`).

> [!NOTE]
> **Explicitly Deferred to 0.8+**
>
> `minimidi` Tier-2 lossless DOM freeze, full JS (N-API/WASM) and Go (`cgo`) bindings, browser in-memory synthesis, and free-threaded Python `3.13t`/`3.14t` wheels.

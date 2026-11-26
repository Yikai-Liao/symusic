# PR Stack & Rollout Plan

Implementation status across stacked PRs [#129–#134](https://github.com/Yikai-Liao/symusic/pulls) and the four-wave path to the clean multi-binding 0.7 ecosystem.

## 1. Stacked PRs (`#129`–`#134`)

| PR | Branch | Scope | Key Changes | Status |
|---|---|---|---|---|
| [#129](https://github.com/Yikai-Liao/symusic/pull/129) | `07/pr1-core-safety` | Core Safety | Negative-time tempo anchors, checked narrowing, validated `trim()`, MIDI import fixes, versioned `SYMZ` v2 ZPP serialization (breaks 0.6 pickles) | In review |
| [#130](https://github.com/Yikai-Liao/symusic/pull/130) | `07/pr2-abc-python` | ABC Process | Isolates GPLv2 `abc2midi`/`midi2abc` into subprocesses via `_abc.py`, adds `ABCConversionWarning`, exact `X:` tune selection, FIFO guards | In review |
| [#131](https://github.com/Yikai-Liao/symusic/pull/131) | `07/pr3-core-refactor` | Core Refactor | `ValueStorage` policy on `Score<T, S>`/`Track<T, S>`, drops `pyvec` & `*Native`, `detail::event_fields<E>`, single-pass MIDI merge (~44% faster), removes `include/argparse.h` | In review |
| [#132](https://github.com/Yikai-Liao/symusic/pull/132) | `07/pr4-fluidsynth` | FluidSynth | Replaces `prestosynth` with dynamically loaded FluidSynth (`dlopen` / `LoadLibraryExW`), `SynthSettings`, `AudioBuffer`, fresh state per render (256 tracks), SHA-384 verification | In review |
| [#133](https://github.com/Yikai-Liao/symusic/pull/133) | `07/pr5-bindings` | Bindings | One Python class per concept (`std::variant`), private `symusic._core`, nanobind stubgen, `MutableSequence`, `read_score_file`, 0.75x Python note iteration time | In review |
| [#134](https://github.com/Yikai-Liao/symusic/pull/134) | `07/pr6-api` | 0.7 API | Removes `*_inplace`, CC64-derived `Track::pedals() const`, `EventColumns<E>`, drops `MetaMacro.h`, `MidiReadOptions`/`InvalidDataBytes`, `symusic::symusic` export, ~40% faster `clip` / ~30% faster `trim` | In review |

## 2. Implemented (`07/pr6-api`) vs Remaining 0.7 Scope

| Area | Implemented in PRs `#129–#134` | Remaining 0.7 Scope (RFC `#136` & Ecosystem Plan) |
|---|---|---|
| Storage & Standard | `ValueStorage` / `SharedStorage`, `detail/*_impl.h`, `EventColumns<E>`, no `pyvec` / `*Native` / `MetaMacro.h` / `argparse.h` | `pyvec` `Arena{chunks, externals}` + `is_contiguous()`/`compact()` backing `.copy()`, uniform two-level Python reference model, replace `zpp_bits` with Python-only codec, drop `include/symusic/vendor/`, full C++17 target |
| Naming & Containers | `Score::from_midi` / `to_midi`, `note_count()`, `track_count()`, `microseconds_per_quarter`, `minor: bool`, `MidiReadOptions` | `TextEvent`, `beats(start=0)`, `from_file()` / `to_file()`, `download_soundfont()`, `Tempo::from_qpm`/`from_mspq`, `TrimMode` enum, single generic `EventList[Note[Tick]]` |
| Operations & Safety | No `*_inplace`, transactional `FieldUpdates`, CC64 `Track::pedals() const`, ~44% MIDI / ~40% `clip` / ~30% `trim` speedups | Returning `adjust_time`, single-step `resample()`, `shift_*` `Note*` pointer dedup, `Note` `u8` + `velocity=64`, fix 7 audit defects (`lowest()`, `HasDuration`, `fmt::formatter<shared_ptr>`, channel-15 lyrics) |
| Synthesis & Packaging | Runtime-loaded FluidSynth (`dlopen` / `LoadLibraryExW`), `SynthSettings`, `AudioBuffer`, ABC subprocess, `symusic::symusic` | Optional `symusic::abc` C++ adapter, `assets/soundfonts.json`, in-memory `fluid_sfloader_t`, 4-tier CMake resolution + `ConfigVersion.cmake` |

## 3. 4-Wave Zero-Conflict Rollout

1. **Wave 1 — Standalone Sub-Libraries:** Release `pyvec v0.1.0` (C++17, zero-dependency single-header `<pyvec/pyvec.hpp>` dropping bundled `timsort`, `Arena{chunks, externals}`, `is_contiguous()`/`compact()`, 6 bug fixes, plus PR-scoped `tessera-order` Bun benchmark/plot suite) and `minimidi v0.10.0` (C++17, Tier-1 streaming + Tier-2 DOM, wire-safety fixes).
2. **Wave 2 — Land Core Stack:** Merge PRs `#129–#134` into `symusic` `main` cleanly without mixing directory moves.
3. **Wave 3 — CMake & Dependency Decoupling:** Implement 4-tier CMake resolution, eliminate `include/symusic/vendor/` ODR bombs, move `zpp_bits` out of core (C++17), and enable `consumer_host_fmt` + all 4 CI consumer tests.
4. **Wave 4 — Pure `git mv` Layout & Governance:** Move into `bindings/{c,python,js,go}/`, `extras/{abcmidi,fluidsynth}/`, `assets/soundfonts.json`, and `conformance/`, and enable automated architecture and docs guards.

# Native C++ API & C++17 Feasibility

symusic 0.7 establishes a C++-first core (`symusic::symusic`, default `ValueStorage`) shared across Python, C99, JS/WASM, and Go bindings—with zero `pyvec`, zero `libcurl`/TLS, and zero GPL linkage.

## Layered Architecture

```text
bindings/python (nanobind + pyvec SharedStorage + SYMZ v2 pickle)
bindings/c (C99 ABI)   bindings/js (WASM/N-API)   bindings/go (cgo)   C++ (find_package)
        │                          │                       │                  │
        └──────────────────────────┴───────────┬───────────┴──────────────────┘
                                               ▼
                     symusic::symusic  (Pure C++ Core · ValueStorage)
              Score<T> · Track · detail/event_fields.h · EventColumns<E> · Pianoroll
          ┌─────────────────────┬─────────────────────────┬─────────────────────────┐
          ▼                     ▼                         ▼                         ▼
  minimidi (PRIVATE)    symusic::abc (MIT)       symusic::synth (LGPL)     assets/soundfonts.json
  Tier-1 SMF wire       extras/abcmidi GPLv2     dlopen / span<const u8>   per-binding stdlib fetch
```

## Native C++ API Alignment

Reconciling `origin/07/pr6-api` headers with finalized Python 0.7 ([RFC #136](./rfc-136.md)) while keeping idioms genuinely native C++:

| Area | Current (`07/pr6-api`) | Proposed Native C++ 0.7 |
|---|---|---|
| Score defaults & naming | `Score<T, S = ValueStorage>`, `TextMeta<T>`, `get_beats()` / `get_downbeats()` | `Score<T = Tick, S = ValueStorage>`, `TextEvent<T>`, `beats(start=0)` / `downbeats(start=0)` |
| `Note` fields & defaults | `i8` pitch/velocity, default `velocity = 0`, redundant `start()` | `u8` pitch/velocity, default `velocity = 64` (`0x40`), drop `start()` (keep `.time`, `.end()`) |
| `Tempo` construction | `Tempo(0, 120)` silently means `120 µs/quarter` (500,000 QPM trap!) | Explicit `Tempo::from_qpm(t, 120.0)` and `Tempo::from_mspq(t, 500000)` |
| `trim` & `adjust_time` | String `start_mode`/`end_mode`; mutating free `void ops::adjust_time` | `enum class TrimMode { Remove, Truncate, Retain }` + `TrimOptions`; returning `score.adjust_time(orig, target)` taking `span` |
| File I/O & members | `read_score()` free function only; free `convert` / `resample` / `pianoroll` | `Score::from_file(path, opts)` / `to_file(path)`, `to<To>()`, single-step `resample()`, `Tick`-only `pianoroll()` members |
| `Pianoroll` & `Synthesizer` | `operator()` returns `T*`, non-`const get()`; `Synthesizer` requires absolute `library_path` | `operator()` returns `T&` (+ `const`), `const get()`, `span` view; `Synthesizer` adds system-search & in-memory `span<const u8>` (`fluid_sfloader_t`) |
| Emptiness idiom | `Score::empty()`, `Track::empty()`, `Note::empty()` | Keep STL `.empty()` on containers (bound to `.is_empty()` in Python); rename `Note::empty()` → `is_silent()` |

> [!WARNING]
> **7 verified defects caught during the `07/pr6-api` header audit**
>
> - **`std::numeric_limits<f32>::min()` instead of `lowest()`** in `ops::end` / `Track::end` / `Score::end` — breaks negative floating-point timestamps.
> - **`HashDuration` typo** (`HasDuration`) in public concepts.
> - **Global `fmt::formatter<std::shared_ptr<T>>`** specialization in `repr.h` — ODR hazard for any host project using `fmt`.
> - **Unconstrained `auto` event constructors** silently convert `i64 > 2^53` through `f64`, losing precision.
> - **`read_score()` drops `MidiReadOptions`**, ignoring caller strict/clamp policy.
> - **Unused `#include "pdqsort.h"`** leaked by `ops.h`.
> - **`minimidi` `Message::channel()` on `0xFF` Meta events** masks low nibble (`0xF`), misassigning every lyric to channel 15.

## Empirical C++17 Downgrade Feasibility

> [!TIP]
> **Headline Verdict**
>
> `zpp_bits` (868 LOC, used **only** for Python pickle) is the **sole C++20-only dependency** in the entire stack. Replacing it with a ~150–200 LOC explicit little-endian `BinaryWriter`/`BinaryReader` in `bindings/python/` removes `serialize`/`deserialize` from C++ core, fixes padding/endianness/validation gaps, and lets all 3 repos target **C++17 with 0% runtime performance loss** (using a ~40-line `span<T>` with implicit `std::span` interop under C++20).

### Third-Party Dependency C++ Floors

| Dependency | Minimum Standard | Role |
|---|---|---|
| `utf8.h` | C++98 | UTF-8 validation |
| `fmt` | C++11 | String formatting |
| `pdqsort` | C++11 | Sorting |
| `Catch2` | C++14 | C++ unit tests |
| `nanobind` | C++17 | Python bindings |
| **`zpp_bits`** | **C++20 (HARD BLOCKER)** | **Python pickle only — replace in `bindings/python/`** |

### Per-Repo C++17 Migration Effort

| Repository | Current Floor | Migration Work | Runtime Cost |
|---|---|---|---|
| `pyvec` | Source is already C++17 (`CMakeLists.txt` mislabels `cxx_std_20`) | ~15 LOC CMake + hygiene fixes (`v0.1.0`) | 0% |
| `minimidi` | Minor C++20 `concepts` + `std::span` (README already says C++17) | ~35 LOC `ByteSpan` (implicit `std::span` under C++20) + C++17 traits (`v0.10.0`) | 0% |
| `symusic` | `zpp_bits` + minor `concepts` / `std::span` / designated initializers | ~150–200 LOC explicit LE binary codec in `bindings/python/` + ~40 LOC `span<T>` | 0% |

### Strategic Options

| Option | Assessment | Trade-off |
|---|---|---|
| **1. Full C++17 across all 3 repos** | **Recommended target** | Unlocks C++17 `FetchContent`, Go `cgo`, and Emscripten/WASM hosts; drops `zpp_bits`; 0% perf loss |
| **2. Hybrid (C++17 headers + C++20 `.cpp`)** | **Not recommended** | Pays ~85% of refactor cost while still breaking C++17 `FetchContent` and `cgo` builds |
| **3. Staged bridge** | **Pragmatic short-term** | Drop `minimidi` (`v0.10.0`) & `pyvec` (`v0.1.0`) to C++17 immediately; keep `symusic` C++20 through PRs `#129–#134`, then replace `zpp_bits` |

## C++ Before / After

**0.7 Native C++ Proposal**

```cpp
#include "symusic.h"
using namespace symusic;

auto score = Score<>::from_file(
    "piece.mid", {.invalid_data_bytes = InvalidDataBytes::Clamp}
);

auto tempo = Tempo<>::from_qpm(0, 120.0);           // no 500,000 QPM trap
auto trimmed = score.trim(0, 1920, {.start_mode = TrimMode::Truncate});
auto warped = trimmed.adjust_time(orig_span, new_span);
auto resampled = warped.to<Second>().resample(480);  // single-step

// System-search FluidSynth or in-memory SoundFont span<const u8>
auto synth = Synthesizer::create("gm.sf3", {.sample_rate = 44100});
AudioBuffer audio = synth.render(resampled, /*stereo=*/true);
```

**0.7 Current (PRs #129–#134)**

```cpp
#include "symusic.h"
using namespace symusic;

// Score<Tick> defaults to ValueStorage (pure std::vector, zero shared_ptr)
auto bytes = read_score_file("piece.mid");
auto score = Score<Tick>::from_midi(
    bytes, {.invalid_data_bytes = InvalidDataBytes::Clamp}
);

auto window = score.clip(0, 1920);
EventColumns<Note<Tick>> cols(window.tracks[0].notes);
auto pedals = window.tracks[0].pedals();  // derived from CC64
auto midi_out = window.to_midi();

// Runtime-loaded FluidSynth (dlopen / LoadLibraryExW)
Synthesizer synth(lib_path, "gm.sf3", SynthSettings{.sample_rate = 44100});
AudioBuffer audio = synth.render(window, /*stereo=*/true);
```

**0.6 (Legacy)**

```cpp
#include "symusic.h"
using namespace symusic;

auto bytes = read_file("piece.mid");
auto shared = Score<Tick>::parse<DataFormat::MIDI>(bytes);
ScoreNative<Tick> score = shared.to_native();

score.clip_inplace(0, 1920);
NoteArr<Tick> cols(score.tracks[0].notes);
auto midi_out = score.dumps<DataFormat::MIDI>();

Synthesizer synth("gm.sf3", 44100, 4);
psynth::AudioData audio = synth.render(shared, true);
```

# Native C++ API & C++17 Downgrade

symusic 0.7 establishes a C++-first **C++17** core (`symusic::symusic`, default `ValueStorage`) shared across Python, C99, JS/WASM, and Go bindings—with zero `pinvec`, zero `zpp_bits`, zero `libcurl`/TLS, and zero GPL linkage.

## Layered Architecture

```text
bindings/python (nanobind + pinvec SharedStorage + private SYMZ v3 pickle)
bindings/c (C99 ABI)   bindings/js (pinvec / WASM / N-API)   bindings/go (cgo)   C++ (find_package)
        │                              │                             │                   │
        └──────────────────────────────┴──────────────┬──────────────┴───────────────────┘
                                                      ▼
                       symusic::symusic  (Pure C++17 Core · ValueStorage)
              Score<T> · Track · detail/event_fields.h · EventColumns<E> · Pianoroll
          ┌─────────────────────┬─────────────────────────┬─────────────────────────┐
          ▼                     ▼                         ▼                         ▼
  minimidi (PRIVATE)    symusic::abc (MIT)       symusic::synth (LGPL)     assets/soundfonts.json
  Tier-1 SMF wire       extras/abcmidi GPLv2     dlopen / span<const u8>   per-binding stdlib fetch
```

## Native C++ API Alignment

Reconciling `origin/07/pr6-api` headers with finalized Python 0.7 ([RFC #136](./rfc-136.md)) while keeping idioms genuinely native C++17:

| Area | Current (`07/pr6-api`) | Finalized Native C++ 0.7 |
|---|---|---|
| Defaults & naming | `Score<T, S = ValueStorage>`, `Tempo<T>`, `TextMeta<T>`, `get_beats()` / `get_downbeats()` | `Score<T = Tick, S = ValueStorage>`, `Tempo<T = Tick>`, `TextEvent<T>`, `beats(start=0)` / `downbeats(start=0)` |
| `Note` domains & defaults | `i8` pitch/velocity, default `velocity = 0`, redundant `start()` | `u8` storage (`0..255`), validated musical domain (`0..127`), default `velocity = 64` (`0x40`), drop `start()` (keep `.time`, `.end()`); MIDI wire export additionally encodes `velocity=0` as NoteOff and caps `ticks_per_quarter` to `1..32767` |
| `Tempo` construction | `Tempo(0, 120)` silently means `120 µs/quarter` (500,000 QPM trap!) | Explicit `Tempo::from_qpm(t, 120.0)` and `Tempo::from_microseconds_per_quarter(t, 500000)` |
| `trim` & `adjust_time` | String `start_mode`/`end_mode`; mutating free `void ops::adjust_time` | `enum class TrimMode { Remove, Truncate, Retain }` + `TrimOptions`; returning `score.adjust_time(orig, target)` taking `span` |
| File I/O & members | `read_score()` free function only; free `convert` / `resample` / `pianoroll` | `Score::from_file(path, opts)` / `to_file(path)`, `to<To>()`, single-step `resample()`, `Tick`-only `pianoroll()` members |
| `Pianoroll` & `Synthesizer` | `operator()` returns `T*`, non-`const get()`; `Synthesizer` requires absolute `library_path` | `operator()` returns `T&` (+ `const`), `const get()`, `span` view; `Synthesizer` adds system-search & in-memory `span<const u8>` (`fluid_sfloader_t`) |
| Emptiness idiom | `Score::empty()`, `Track::empty()`, `Note::empty()` | Keep STL `.empty()` on containers (bound to `.is_empty()` in Python); rename `Note::empty()` → `is_silent()` |

> [!WARNING]
> **7 verified defects caught during the `07/pr6-api` header audit**
>
> - **`std::numeric_limits<f32>::min()` instead of `lowest()`** in `Track::end` / `ops::end` (`Score::end()` already uses `lowest()`) — breaks negative floating-point timestamps (add regression test for all-negative + empty track returning `0`).
> - **`HashDuration` typo** (`HasDuration`) in public concepts.
> - **Global `fmt::formatter<std::shared_ptr<T>>`** specialization in `repr.h` — ODR hazard for any host project using `fmt`.
> - **`checked_event_field` compares via `f64`**, losing exact integer bounds checking for `i64 > 2^53`.
> - **`read_score()` drops `MidiReadOptions`**, ignoring caller strict/clamp policy.
> - **Unused `#include "pdqsort.h"`** leaked by `ops.h`.
> - **`minimidi` `Message::channel()` on `0xFF` Meta events** masks the low nibble (`0xF`), misassigning lyrics to channel 15 on the no-notes fallback in `ScoreBuilder::finish_group`.

## Decision: Downgrade All 3 Repos to Strict C++17

> [!TIP]
> **Finalized Decision**
>
> All three repositories (`pinvec`, `minimidi`, and `symusic`) will target **strict C++17** (`-std=c++17 -pedantic-errors`). `zpp_bits` (868 LOC, used **only** for Python pickle) is the **sole C++20 third-party dependency**; removing `zpp_bits` and `serialize()`/`deserialize()` from C++ core (C++ interchange uses MIDI; Python owns a private little-endian **`SYMZ v3`** pickle codec in `bindings/python/`) unblocks C++17 `FetchContent`, Go `cgo`, and Emscripten/WASM hosts. Runtime performance parity with C++20 is a **benchmark-gated requirement** in Wave 3.

### Third-Party Dependency C++ Floors

| Dependency | Minimum Standard | Role |
|---|---|---|
| `utf8.h` | C++98 | UTF-8 validation |
| `fmt` | C++11 | String formatting |
| `pdqsort` | C++11 | Sorting |
| `Catch2` | C++14 | C++ unit tests |
| `nanobind` | C++17 | Python bindings |
| **`zpp_bits`** | **C++20 (REMOVED)** | **Python pickle only — replaced by private `SYMZ v3` LE codec in `bindings/python/`** |

### Per-Repo C++17 Lowering Scope

| Repository | C++20 Constructs to Lower | Gate |
|---|---|---|
| `pinvec` | Pure C++17 from day one (prototype already verified under `-std=c++17` + ASan/UBSan) | `-std=c++17 -pedantic-errors` + Bun PR bench |
| `minimidi` | Minor `concepts` + `std::span` → ~35-LOC `ByteSpan` (implicit `std::span` interop under C++20) + C++17 traits | `-std=c++17 -pedantic-errors` |
| `symusic` | Remove `zpp_bits` from core + lower internal C++20 language constructs in `origin/07/pr6-api` (`<=>` / defaulted `operator==`, `std::ranges`, `std::erase_if`, `std::in_range`, explicit template lambdas, abbreviated `auto` templates, `std::span` → ~40-LOC `span<T>`) | `-std=c++17 -pedantic-errors` + benchmark parity gate |

## C++ Before / After

**0.7 Native C++17 Proposal**

```cpp
#include "symusic.h"
using namespace symusic;

// Strict C++17: compiles cleanly under g++ -std=c++17 -pedantic-errors
MidiReadOptions read_opts;
read_opts.invalid_data_bytes = InvalidDataBytes::Clamp;
auto score = Score<>::from_file("piece.mid", read_opts);

auto tempo = Tempo<>::from_qpm(0, 120.0);  // no 500,000 QPM trap
TrimOptions<Tick> trim_opts;
trim_opts.start_mode = TrimMode::Truncate;
auto trimmed = score.trim(0, 1920, trim_opts);
auto warped = trimmed.adjust_time(orig_span, new_span);
auto resampled = warped.resample(480);     // direct resampling, no extra f32 pass

SynthSettings synth_opts;
synth_opts.sample_rate = 44100;
auto synth = Synthesizer::create("gm.sf3", synth_opts);
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
ScoreNative<Tick> score = to_native(shared);  // free function in 0.6

shared.clip_inplace(0, 1920);
NoteArr<Tick> cols(score.tracks[0].notes);
auto midi_out = score.dumps<DataFormat::MIDI>();

Synthesizer synth("gm.sf3", 44100, 4);
psynth::AudioData audio = synth.render(shared, true);
```

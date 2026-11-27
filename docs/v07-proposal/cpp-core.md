# Native C++ API & Language Standard

symusic 0.7 establishes a C++-first **C++20** core (`symusic::symusic`, default `ValueStorage`) shared across Python, C99, JS/WASM, and Go bindings—with zero `anchorvec`, zero `zpp_bits`, zero `libcurl`/TLS, and zero GPL linkage.

## Layered Architecture

```text
bindings/python (nanobind + anchorvec SharedStorage + private SYMZ v3 pickle)
bindings/c (C99 ABI)   bindings/js (anchorvec / WASM / N-API)   bindings/go (cgo)   C++ (find_package)
        │                              │                             │                   │
        └──────────────────────────────┴──────────────┬──────────────┴───────────────────┘
                                                      ▼
                       symusic::symusic  (Pure C++20 Core · ValueStorage)
              Score<T> · Track · detail/event_fields.h · EventColumns<E> · Pianoroll
          ┌─────────────────────┬─────────────────────────┬─────────────────────────┐
          ▼                     ▼                         ▼                         ▼
  minimidi (PRIVATE)    symusic::abc (MIT)       symusic::synth (LGPL)     assets/soundfonts.json
  Tier-1 SMF wire       extras/abcmidi GPLv2     dlopen / span<const u8>   per-binding stdlib fetch
```

## Native C++ API Alignment

Reconciling `origin/07/pr6-api` headers with finalized Python 0.7 ([RFC #136](./rfc-136.md)) while keeping idioms genuinely native C++20:

| Area | Current (`07/pr6-api`) | Finalized Native C++ 0.7 |
|---|---|---|
| Defaults & naming | `Score<T, S = ValueStorage>`, `Tempo<T>`, `TextMeta<T>`, `get_beats()` / `get_downbeats()` | `Score<T = Tick, S = ValueStorage>`, `Tempo<T = Tick>`, `TextEvent<T>`, `beats(start=0)` / `downbeats(start=0)` |
| `Note` domains & defaults | `i8` pitch/velocity; the constructor with arguments requires `velocity`, and default construction sets it to `0`; redundant `start()` | `i8` storage, validated musical domain (`0..127`), default `velocity = 64` (`0x40`), drop `start()` (keep `.time`, `.end()`); MIDI wire export additionally encodes `velocity=0` as NoteOff and caps `ticks_per_quarter` to `1..32767` |
| `Tempo` construction | `Tempo::from_qpm(t, 120.0)` exists, but brace initialization `Tempo<Tick>{0, 120}` silently means `120 µs/quarter` (500,000 QPM) | Keep `Tempo::from_qpm(t, 120.0)` and add `Tempo::from_microseconds_per_quarter(t, 500000)`, so every construction names its unit |
| `clip`/`trim` & `adjust_time` | `clip(start, end, bool clip_end)`; `trim(start, end, min_overlap, start_mode, end_mode)` with string modes; mutating free `void ops::adjust_time` | `clip(start, end, const ClipOptions& = {})`; `ClipOptions` holds `head` (default `ClipMode::Drop`) and `tail` (default `ClipMode::Keep`) of `enum class ClipMode { Keep, Drop, Cut }`; `trim` and `min_overlap` go; returning `score.adjust_time(orig, target)` taking `span` |
| File I/O & members | Free `read_score(path, format, decode_abc)` and `read_score_file(path)` returning bytes; free `convert` / `resample` / `pianoroll` | `Score::from_file(path, opts)` / `to_file(path)`, `to<To>()`, single-step `resample()`, `Tick`-only `pianoroll()` members |
| `Pianoroll` & `Synthesizer` | `operator()` returns `T*`, non-`const get()`; `Synthesizer` requires absolute `library_path` | `operator()` returns `T&` (+ `const`), `const get()`, `span` view; `Synthesizer` adds system-search & in-memory `span<const u8>` (`fluid_sfloader_t`) |
| Emptiness idiom | `Score::empty()`, `Track::empty()`, `Note::empty()` | Keep STL `.empty()` on containers (bound to `.is_empty()` in Python); rename `Note::empty()` → `is_silent()` |

> [!WARNING]
> **7 verified defects caught during the `07/pr6-api` header audit**
>
> - **`std::numeric_limits<f32>::min()` instead of `lowest()`** in `Track::end` / `ops::end` (`Score::end()` already uses `lowest()`) — breaks negative floating-point timestamps (add regression test for all-negative + empty track returning `0`).
> - **`HashDuration` typo** (`HasDuration`) in public concepts.
> - **Global `fmt::formatter<std::shared_ptr<T>>`** specialization in `repr.h` — ODR hazard for any host project using `fmt`.
> - **`checked_event_field` compares via `f64`.** Every current destination field's bounds are exact in `f64`, so no input is misjudged today; a future `i64` destination would lose exact bounds checking above `2^53`. Compare integers with `std::in_range` instead.
> - **`read_score()` cannot accept `MidiReadOptions`**: its MIDI branch always uses the defaults, so a file reader cannot opt out of strict parsing.
> - **Unused `#include "pdqsort.h"`** leaked by `ops.h`.
> - **`minimidi` `Message::channel()` on `0xFF` Meta events** masks the low nibble (`0xF`), misassigning lyrics to channel 15 on the no-notes fallback in `ScoreBuilder::finish_group`.

## Decision: Stay on C++20

> [!TIP]
> **Finalized Decision**
>
> `symusic`, `minimidi` and `anchorvec` stay on **C++20** (`cxx_std_20`). Lowering to C++17 would help only hosts that cannot compile C++20: Android NDK r23–r25, whose libc++ lacks `std::ranges::sort`, hosts pinned to `/std:c++17` such as ROS 2 and older JUCE projects, and old Xcode toolchains. The planned C ABI, JS/WASM and Go `cgo` bindings consume a compiled library and do not depend on its language standard. The cost would be rewriting about 178 constrained template parameters, 23 `requires` clauses and 125 `std::span` uses at #134.
>
> Lowering later stays possible without breaking callers as long as public signatures do not change. The one exception is vocabulary types in signatures: `std::span` must either stay or be replaced before 0.7 freezes the API, so that choice is settled now: public signatures keep `std::span`.

`zpp_bits` (6,058-line header) is still removed from the core, for a different reason: it serves only the Python pickle. C++ interchange uses MIDI, and Python owns a private little-endian **`SYMZ v3`** codec in its binding.

## C++ Before / After

**0.7 Native C++20 Proposal (planned API)**

```cpp
#include <span>
#include <symusic.h>
using namespace symusic;

int main() {
    auto score = Score<>::from_file("piece.mid", {.strict = false});

    auto tempo = Tempo<>::from_qpm(0, 120.0);  // names its unit
    auto window = score.clip(0, 1920, {.tail = ClipMode::Cut});  // cut notes at end

    const Tick::unit original[] = {0, 960, 1920};
    const Tick::unit target[] = {0, 1200, 1920};
    auto warped = window.adjust_time(std::span(original), std::span(target));
    auto resampled = warped.resample(480);  // one rounding step

    auto synth = Synthesizer::create("gm.sf3", {.sample_rate = 44100});
    AudioBuffer audio = synth.render(resampled, /*stereo=*/true);
}
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

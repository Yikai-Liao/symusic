# Tri-Repo Module Boundaries

Clear ownership boundaries across **`minimidi`**, **`pyvec`**, and **`symusic`** eliminate header leaks, duplicated policy, and license contagion across all language bindings.

## 1. `minimidi` — Standalone Two-Tier SMF Wire Library

**Conclusion:** `minimidi` owns the Standard MIDI File wire protocol only; `symusic` links `minimidi` **`PRIVATE`** and consumes Tier 1 directly with zero vendor header leaks.

- **Tier 1 (Zero-alloc streaming):** `read_smf(ByteSpan, Visitor&, ReadOptions)` emits stack-allocated `EventView` records and applies `DataBytePolicy::{Strict, Clamp, Raw}` on decoded fields without mutating read-only input buffers. `SmfWriter` / `TrackWriter` enforce every SMF wire limit in one authority.
- **Tier 2 (Lossless DOM):** `MidiFile` / `Track` / `Event` built on top of Tier 1 — ready for standalone C++ users and `symusic`'s future MIDI Message-Level API.
- **Wire-safety bugs fixed in `minimidi`:** buffer over-reads, `reinterpret_cast` type-punning UB, SMPTE union UB, EOT delta loss, `0xF7` continuation SysEx handling, `Message::channel()` on `0xFF` Meta events, and banning `std::ios_base::failure`.

## 2. NoteOn / NoteOff Pairing (`midi_notes.h`) — Keep in `symusic`

**Conclusion:** Keep note pairing in `symusic`; move **only** the stateless MIDI 1.0 spec predicate `EventView::is_note_off()` (`0x8n` or `0x9n` with `velocity == 0`) to `minimidi`.

| Concern | Owner | Why |
|---|---|---|
| Raw MIDI messages & `EventView::is_note_off()` | `minimidi` | MIDI 1.0 wire protocol defines only instant messages, never duration notes |
| FIFO/LIFO overlap pairing, hanging-note policy, channel/program/drum grouping, `i32` overflow, CC64 sustain tracking | `symusic` | Performance-modeling policy tightly coupled to `ScoreBuilder` |

## 3. `pyvec` — Reference-Stable Binding Vector (Python-Only)

**Conclusion:** Isolate `pyvec` strictly inside `bindings/python/` (`SharedStorage`). C++ core, C99 ABI, JS (WASM/N-API), and Go (`cgo`) use pure `ValueStorage` (`std::vector`).

- **Arena memory model (`shared<Arena>`):** `_resources` upgrades to `shared<Arena>` where `Arena { vec<vec<T>> chunks; vec<shared<T>> externals; }`. Bulk paths (MIDI parsing, `EventList.from_numpy(...)`) write into one contiguous `chunks` block (`externals` empty, 0% overhead), while inserting existing Python `Note` objects (`shared_ptr<T>`) appends to `externals` + `_ptrs.push_back(ptr.get())` with zero copy.
- **Alias-safe `shift_*` deduplication:** When the same `Note` appears multiple times (`track.notes = [n, n]`), transactional `shift_*` updates deduplicate by `Note*` so shared objects are never double-shifted.
- **`is_contiguous()` fast path & `compact()`:** `is_contiguous()` is an O(1) check (`chunks.size() <= 1 && externals.empty()` with sequential `_ptrs`), letting `to_midi`, `to_numpy`, and `clip` read a direct `const T*` span; `compact()` produces a fresh single-chunk contiguous `pyvec` with zero `externals`.
- **Why no `contiguous()` in Python's public API:** In-place compaction would silently sever live external `Note` references, while conditional-copy (`return self` if contiguous else copy) creates unpredictable aliasing on mutable containers and leaks `pyvec` internals (C++ `ValueStorage` is always contiguous). Instead, **`.copy()` calls `pyvec::compact()`** so `.copy()` guarantees a single contiguous chunk with explicit detachment, while bulk construction avoids fragmentation via `EventList.from_numpy(...)`.
- **Zero-dependency single header & 6 bug fixes:** Move to a single zero-dependency header `<pyvec/pyvec.hpp>` in `namespace pyvec` — **drop bundled `gfx/timsort`** completely (`pyvec::sort()` defaults to C++17 `std::stable_sort`, with an optional `sort_with(Sorter, ...)` hook for callers who benchmark and want a custom sorter like Timsort on nearly-sorted data). Replace unsafe `pbegin()` with `compact()` / `erase_if()`, and fix 6 verified bugs (`different_type` typo, `reserve` underflow, `swap`, `setitem`/`delitem`, bitwise `|`/`&`).
- **PR-level benchmark suite (`tessera-order` Bun toolchain):** As a foundational container, `pyvec` ships a `bench/` suite reusing `tessera-order`'s Bun harness (`statistics.ts`, `report/`, `plot/` chart generation) for local and PR-level regression checks (deferring `main`-branch continuous tracking for now), covering bulk `chunks` vs `std::vector` vs `vector<shared_ptr<T>>`, `Arena::externals` vs `compact()`, `is_contiguous()` vs fragmented iteration, slicing, `erase_if`, and `std::stable_sort` vs custom sorters on sorted/nearly-sorted/random inputs.

## 4. ABC & SoundFont / `Synthesizer` Across Bindings

**Conclusion:** Share security-critical ABC staging in one optional MIT C++ adapter; keep networking/TLS completely out of C++ core via one declarative SoundFont manifest.

### ABC (`symusic::abc`)

- Move the 328-line security-critical staging (`_stage_input_files`, `_isolate_tune`, `_complete_midi` EOT check, diagnostics, `parseabc.c` guards) out of Python-only `_abc.py` into **one optional MIT C++ adapter `symusic::abc`**.
- Invokes external GPLv2 `abc2midi` / `midi2abc` out-of-process (shared recipe in `extras/abcmidi/`), with an injectable runner callback for WASM/sandboxed hosts.
- C++, Python, Go, and Node share one authority with zero GPL linkage.

### SoundFont & `Synthesizer` (`assets/soundfonts.json`)

- **Zero `libcurl` / TLS in C++ core.** One declarative manifest **`assets/soundfonts.json`** (preset IDs, URLs, SHA-384 digests, licenses) drives a ~50-line native-stdlib fetcher per binding (`urllib`+`hashlib` in Python, `net/http`+`crypto/sha512` in Go, `fetch`+WebCrypto in JS).
- Gate FluidSynth behind `SYMUSIC_ENABLE_SYNTH` (shared LGPL build recipe in `extras/fluidsynth/`), decouple pure event scheduling via `schedule_synth_events()`, and add optional in-memory `span<const u8>` loading via `fluid_sfloader_t` for WASM and diskless hosts.

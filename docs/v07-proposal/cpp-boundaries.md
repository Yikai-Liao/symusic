# Tri-Repo Module Boundaries

Clear ownership boundaries across **`minimidi`**, **`anchorvec`**, and **`symusic`** eliminate header leaks, reference cycles, duplicated policy, and license entanglement across all language bindings.

## 1. `minimidi` — Standalone SMF Wire Library

**Conclusion:** Ship and freeze **Tier 1 (zero-alloc streaming)** for 0.7; defer **Tier 2 (lossless DOM)** freeze to **0.8+** until a real consumer exists. `symusic` links `minimidi` **`PRIVATE`** (once MIDI wire parsing moves out of public `detail/midi_impl.h` into private `.cpp` translation units) with zero vendor header leaks.

- **Tier 1 (0.7 freeze):** `read_smf(ByteSpan, Visitor&)` emits stack-allocated `EventView` records holding the bytes as written, data bytes above 127 included. It takes no options and never mutates its read-only input; symusic's `Score` builder applies `MidiReadOptions::strict`, so the future `MidiFile` sees the file unchanged. `SmfWriter` / `TrackWriter` enforce every SMF wire limit in one authority.
- **Explicit round-trip boundary:** Tier 1 preserves SMF wire messages faithfully; `symusic::Score` normalizes note-level musical performance.
- **Wire-safety bugs fixed in `minimidi`:** buffer over-reads, `reinterpret_cast` type-punning UB, SMPTE union UB, EOT delta loss, `0xF7` continuation SysEx handling, `Message::channel()` on `0xFF` Meta events, and banning `std::ios_base::failure`.

## 2. NoteOn / NoteOff Pairing (`midi_notes.h`) — Keep in `symusic`

**Conclusion:** Keep note pairing in `symusic`; move **only** the stateless MIDI 1.0 spec predicate `EventView::is_note_off()` (`0x8n` or `0x9n` with `velocity == 0`) to `minimidi`.

| Concern | Owner | Why |
|---|---|---|
| Raw MIDI messages & `EventView::is_note_off()` | `minimidi` | MIDI 1.0 wire protocol defines only instant messages, never duration notes |
| FIFO/LIFO overlap pairing, hanging-note policy, channel/program/drum grouping, `i32` overflow, CC64 sustain tracking | `symusic` | Performance-modeling policy tightly coupled to `ScoreBuilder` |

## 3. `anchorvec` (Renamed from `pyvec`) — Zero-Cycle Binding Vector (Python & JS/WASM)

**Conclusion:** Isolate `anchorvec` inside `bindings/python/` (and JS N-API/WASM bindings needing stable object handles). C++ core, C99 ABI, and Go (`cgo`) use pure `ValueStorage` (`std::vector`).

- **Monotonic 3-state architecture:** `Flat` → `PagedDirect` (`kCap = 256` bitshift indexing) → `PagedIndirect`, **never implicitly demoting**, so a handle taken in one mode is not invalidated by a later mode change. This removes one class of use-after-free; free-threaded Python still needs per-container locking for concurrent mutation.
- **Zero strong reference cycles:** Leaf 4KB-class `AlignedPage` blocks never hold external references. External `shared_ptr<T>` pushes live in a list-owned `_ext_owners` vector + freelist referenced via a **Tagged Pointer (`bit 0 = 1`)** (`max(alignof(T), 2)` slot stride supports `alignof(T) == 1`). Passes self-reinsert (`a.push_back_shared(a.share(0))`) and cross-list (`a <-> b`) zero-leak ASan/UBSan tests, and releases deleted external objects immediately.
- **O(1) `is_flat()` / `flat_data()`:** Returns non-null **only** in `Mode::Flat` — zero false positives after `reverse`, `sort`, `del`, or `[::2]`. `share(i)` is non-`const`, so concurrent reads through a `const AnchorVec` are safe while no thread mutates it.
- **Source-level anti-fragmentation & clean C++20 STL API:** Companion `anchorvec::make_pinned<T>` (working name: the final name follows the handle type it returns, as `make_shared` follows `shared_ptr`, and drops the `pin` vocabulary) thread-local 4KB page pool for Python `Note(...)` / JS `new Note()` construction. Single zero-dependency header `<anchorvec/anchorvec.hpp>` dropping Pythonisms (`append`/`getitem`/`pypos`/`slice`) and bundled `timsort` (`if (std::is_sorted) return; std::stable_sort(...)`; skip premature `sort_with` hook in `v0.1`).
- **Why no `contiguous()` in Python's public API:** In-place compaction would silently sever live external `Note` references, while conditional-copy creates unpredictable aliasing on mutable containers and leaks binding storage internals. Instead, deep copies (`score.copy()`, `copy.deepcopy`) build fresh storage, flat unless a copied list holds the same element twice, while bulk callers construct via `EventList.from_numpy(...)`.

### Prototype Benchmarks (`-O3`; the prototype also passes ASan/UBSan)

| Workload | AnchorVec V2 Result | Baseline Comparison |
|---|---|---|
| Bulk 200k `Note` (`adopt` + RAM) | **12.0 B/el**, `adopt` **0.02 µs** | Matches `std::vector`; **40% less RAM** vs old `pyvec` (20 B/el) |
| 1-element slice retention | **4.1 KB** retained | vs **2.4 MB** (**580× reduction**) |
| External 10k `shared_ptr` append + 200k scan / random / `PagedDirect` | Append **44.9 µs**, scan **241.6 µs**, random **1,664 µs**, `kCap=256` **262.2 µs** | **8.4× faster** than ExtTable hash (379.1 µs); **1.69× faster** scan (409.1 µs); **1.40× faster** random (2,336 µs); **1.25× faster** bitshift vs `kCap=339` div/mod (326.9 µs) |
| Sort 200k (`is_sorted` + `std::stable_sort`) | Random **11.11 ms**, sorted **0.225 ms** | **25% less time** on random (14.86 ms) and **15% less time** on sorted (0.265 ms) than `gfx::timsort` |

## 4. ABC & SoundFont / `Synthesizer` Across Bindings

**Conclusion:** Share security-critical ABC staging in one optional MIT C++ adapter; keep networking/TLS completely out of C++ core via one declarative SoundFont manifest.

> [!NOTE]
> **License boundary note**
>
> Runtime `dlopen` (FluidSynth) and subprocess execution (`abcmidi`) are **technical isolation boundaries** keeping the MIT C++ core free of GPL/LGPL linkage. Wheels or binary distributions that bundle FluidSynth/libsndfile or GPL-2.0-or-later `abcmidi` must still fulfill LGPL/GPL notice and source-offer obligations.

### ABC (`symusic::abc`)

- Move the 328-line security-critical staging (`_stage_input_files`, `_isolate_tune`, `_complete_midi` EOT check, diagnostics, `parseabc.c` guards) out of Python-only `_abc.py` into **one optional MIT C++ adapter `symusic::abc`**.
- Invokes external GPLv2 `abc2midi` / `midi2abc` out-of-process (shared recipe in `extras/abcmidi/`), with an injectable runner callback for WASM/sandboxed hosts.

### SoundFont & `Synthesizer` (`assets/soundfonts.json`)

- **Zero `libcurl` / TLS in C++ core.** One declarative manifest **`assets/soundfonts.json`** (preset IDs, URLs, SHA-384 digests, licenses) drives a ~50-line native-stdlib fetcher per binding (`urllib`+`hashlib` in Python, `net/http`+`crypto/sha512` in Go, `fetch`+WebCrypto in JS). `download_soundfont(force=True)` downloads to a temp file and verifies the SHA-384 digest before atomically replacing the cached file.
- Gate FluidSynth behind `SYMUSIC_ENABLE_SYNTH` (shared LGPL recipe in `extras/fluidsynth/`), decouple pure event scheduling via `schedule_synth_events()`, and add optional in-memory `span<const u8>` SoundFont loading via `fluid_sfloader_t` (**lifetime contract:** `Synthesizer` must copy/own the SoundFont bytes—or restrict to synchronous single-shot load—because FluidSynth reloads a fresh state on every `render()` call).

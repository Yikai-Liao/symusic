# Tri-Repo Module Boundaries

Clear ownership boundaries across **`minimidi`**, **`anchorvec`**, and **`symusic`** eliminate header leaks, reference cycles, duplicated policy, and license entanglement across all language bindings.

## 1. `minimidi` — Standalone SMF Wire Library

**Conclusion:** Ship and freeze **Tier 1 (zero-alloc streaming)** for 0.7; defer **Tier 2 (lossless DOM)** freeze to **0.8+** until a real consumer exists. `symusic` links both `minimidi` and `{fmt}` **`PRIVATE`** (once MIDI wire parsing and string formatting move into `.cpp` translation units and storage-generic `*_impl.h` headers move into repo-private `src/`), leaving zero third-party or vendor headers in the installed `include/symusic/` tree.

- **Tier 1 (0.7 freeze):** `read_smf(ByteSpan, Visitor&)` emits stack-allocated `EventView` records holding the bytes as written, data bytes above 127 included. It takes no options and never mutates its read-only input; symusic's `Score` builder applies `MidiReadOptions::strict`, so the future `MidiFile` sees the file unchanged. `SmfWriter` / `TrackWriter` enforce every SMF wire limit in one authority.
- **Explicit round-trip boundary:** Tier 1 preserves SMF wire messages faithfully; `symusic::Score` normalizes note-level musical performance.

The Tier 1 contract covers MIDI 1.0 Standard MIDI Files only; MIDI 2.0 UMP, if added, gets its own types.

| Area | Contract |
|---|---|
| Input | One complete, read-only byte span. "Streaming" means the visitor sees one event at a time; an incremental reader for partial input can come later. Reading allocates nothing on success; building an error may allocate. |
| Header | Formats 0, 1 and 2. The division is a sum type: ticks per quarter, or SMPTE frames per second with ticks per frame. Header bytes beyond the six defined ones are skipped. |
| Time | Each event carries its raw delta and its absolute tick as `u64`. A delta VLQ longer than four bytes is an error. |
| Visitor | Calls come in a fixed order: the header, then for each track its begin (track index), its events (event index and byte offset) and its end. A callback returns `Continue` or `Stop`; after `Stop` the rest of the file is not checked. An exception from a callback propagates unchanged. |
| Lifetime | An `EventView` is valid during its callback. Its payload span points into the input and stays valid while the input lives unchanged; it never points at a temporary decode buffer. |
| Errors | A structural error stops reading with an error carrying the byte offset, track index and event index: a chunk or length that runs past the input, a data byte where a status byte is needed with no status to reuse, or an over-long VLQ. Data bytes above 127 are delivered as written only inside correctly framed channel messages; Tier 1 does not recover broken files. |
| Classification | Only channel messages have a channel (`std::optional<u8>`); meta and SysEx events have none. |
| SMF details | The end-of-track delta counts toward the track's length, and bytes after end of track in its chunk are skipped. Unknown chunks are skipped by their declared length. `F0` and `F7` events both carry a VLQ length, and an `F7` escape or continuation packet is delivered as its bytes. Running status reuses the last channel status, also across meta and SysEx events: the specification says those cancel it, files in the wild rely on it anyway, and no compliant file reads differently. |
| Writer | `SmfWriter` and `TrackWriter` build the file in memory: a track begins and ends explicitly, and its chunk length is filled in at the end, so the sink never needs to seek. Every value is checked before it is written (data bytes `<= 127`, `tpq` in `1..32767`, `mspq <= 16777215`, deltas below `2^28`); a failed check throws and discards the partial output. Writing the finished buffer to a file handles short writes. |
| Fidelity | Three levels, each named separately. **Message semantics** is the `Score` round trip of [MIDI export](./semantics.md#midi-export). **Raw payload** is what Tier 1 reading gives: each message's bytes as written. **Byte replay** writes a file's held messages back unchanged, invalid data bytes included; it is Tier 2 (0.8+) and a path separate from the checked writer, which keeps the writer's promise unconditional. |

The symusic adapter, not minimidi, owns the musical policy: note pairing, track grouping, the `i32` time limit, SMPTE rejection and reading a format 2 file's sequences onto one timeline, as for format 1 ([MIDI import](./semantics.md#midi-import)).
- **Wire-safety bugs fixed in `minimidi`:** buffer over-reads, `reinterpret_cast` type-punning UB, SMPTE union UB, EOT delta loss, `0xF7` continuation SysEx handling, `Message::channel()` on `0xFF` Meta events, and banning `std::ios_base::failure`.

## 2. NoteOn / NoteOff Pairing (`midi_notes.h`) — Keep in `symusic`

**Conclusion:** Keep note pairing in `symusic`; move **only** the stateless MIDI 1.0 spec predicate `EventView::is_note_off()` (`0x8n` or `0x9n` with `velocity == 0`) to `minimidi`.

| Concern | Owner | Why |
|---|---|---|
| Raw MIDI messages & `EventView::is_note_off()` | `minimidi` | MIDI 1.0 wire protocol defines only instant messages, never duration notes |
| FIFO pairing, hanging-note policy, channel/program/drum grouping ([MIDI import](./semantics.md#midi-import)), `i32` overflow, CC64 sustain tracking | `symusic` | Performance-modeling policy tightly coupled to `ScoreBuilder` |

## 3. `anchorvec` (Renamed from `pyvec`) — Zero-Cycle Binding Vector for Python

**Conclusion:** Isolate `anchorvec` inside `bindings/python/`, the one binding that hands out references into lists. The C++ core, C99 ABI, Go (`cgo`) and both JS targets use plain `ValueStorage` (`std::vector`). A reference into a list needs a host wrapper per element, and neither JS host releases wrappers deterministically: `FinalizationRegistry` callbacks run late or never, and `napi_adjust_external_memory` only tells the GC how much native memory exists. Node and WASM therefore share one object model, with explicitly released score handles and events read as values ([JS and CLI](./js-cli.md)).

- **Monotonic 3-state architecture:** `Flat` → `PagedDirect` (`kCap = 256` bitshift indexing) → `PagedIndirect`, **never implicitly demoting**, so a handle taken in one mode is not invalidated by a later mode change. This removes one class of use-after-free; free-threaded Python still needs per-container locking for concurrent mutation.
- **Zero strong reference cycles:** Leaf 4KB-class `AlignedPage` blocks never hold external references. External `shared_ptr<T>` pushes live in a list-owned `_ext_owners` vector + freelist referenced via a **Tagged Pointer (`bit 0 = 1`)** (`max(alignof(T), 2)` slot stride supports `alignof(T) == 1`). Passes self-reinsert (`a.push_back_shared(a.share(0))`) and cross-list (`a <-> b`) zero-leak ASan/UBSan tests, and releases deleted external objects immediately.
- **O(1) `is_flat()` / `flat_data()`:** Returns non-null **only** in `Mode::Flat` — zero false positives after `reverse`, `sort`, `del`, or `[::2]`. `share(i)` is non-`const`, so concurrent reads through a `const AnchorVec` are safe while no thread mutates it.
- **Source-level anti-fragmentation & clean C++20 STL API:** Companion `anchorvec::make_pinned<T>` (working name: the final name follows the handle type it returns, as `make_shared` follows `shared_ptr`, and drops the `pin` vocabulary) thread-local 4KB page pool for Python `Note(...)` construction. Single zero-dependency header `<anchorvec/anchorvec.hpp>` dropping Pythonisms (`append`/`getitem`/`pypos`/`slice`) and bundled `timsort` (`if (std::is_sorted) return; std::stable_sort(...)`; skip premature `sort_with` hook in `v0.1`).
- **Why no `contiguous()` in Python's public API:** In-place compaction would silently sever live external `Note` references, while conditional-copy creates unpredictable aliasing on mutable containers and leaks binding storage internals. Instead, deep copies (`score.copy()`, `copy.deepcopy`) build fresh storage, flat unless the copied graph shares an element: within one list (`[n, n]`) or across lists, when two copied lists hold the same note. Bulk callers construct via `EventList.from_numpy(...)`.

### Contract for anchorvec v0.1

The three modes, the 256-slot pages, the tagged pointer and the page pool are implementation choices; they may change while the performance budget and these rules hold.

| Operation | Contract |
|---|---|
| Insert a value | Creates an independent element. |
| `push_back_shared(h)` | Inserts the element that `h` owns. One element may sit in several slots and lists; removing one slot leaves the others. |
| Copy constructor | Deep: copies every element, as `std::vector` does. Python's shallow copy uses a separately named share path. |
| `share(i)` | Returns an owning `std::shared_ptr<T>` to element `i`; out of range throws `std::out_of_range`. The first call may move the list out of flat mode; a throw leaves the list unchanged. |
| `[]`, `at`, iterators | Return `T&` borrows, valid until the list's next structural change. A handle stays valid until its last owner drops it. |
| `sort`, `reverse`, `insert`, `erase` | The list's members move element handles, so a handle keeps meaning the same note. Generic algorithms that swap through `T&`, such as `std::sort(v.begin(), v.end())`, exchange values instead and do not keep identity. |
| `flat_data()` | Non-null only in flat mode. The pointer is a borrow, valid until the list's next mutation. |
| Removal and destruction | A removed element lives while a handle owns it and is destroyed exactly once, when its last owner goes. The page pool never reuses a live slot. |
| Cycles | The library's own structures hold no strong cycle. An element type that owns handles to its own list can still form one. |
| Threads | Concurrent reads of a list that no thread mutates are safe, and a page may be freed on another thread. A `const` list does not make shared elements read-only: another alias can still change them. |
| Allocation failure | Mode upgrades and owner registration commit after their allocations succeed, so `std::bad_alloc` leaves the list and every owner unchanged. |

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

- **Zero `libcurl` / TLS in C++ core.** One declarative manifest **`assets/soundfonts.json`** (preset IDs, URLs, SHA-384 digests, licenses) drives a ~50-line native-stdlib fetcher per binding (`urllib`+`hashlib` in Python, `net/http`+`crypto/sha512` in Go, `fetch`+WebCrypto in Node/Bun). `download_soundfont(force=True)` downloads to a temp file and verifies the SHA-384 digest before atomically replacing the cached file.
- **Orthogonal synthesis build gates & WASM backend seam.** `SYMUSIC_ENABLE_SYNTH` controls whether `Synthesizer` is compiled into `symusic` (setting it `OFF` drops `Threads` and `dlopen` so single-threaded WASM and minimal embedded targets build cleanly), while `SYMUSIC_BUILD_FLUIDSYNTH` controls whether the bundled LGPL FluidSynth + `libsndfile` shared objects are built from `extras/fluidsynth/` (requiring CMake 3.25+ and the audio submodules). `Synthesizer::Backend` separates its FluidSynth C function table from the native `dlopen` loader (**explicit absolute `SynthSettings::library_path` → co-located/bundled binary next to module/executable via `dladdr`/`GetModuleHandleExW`, never CWD → system library fallback**), so a browser WASM synthesis package can supply exports from a separate LGPL `fluidsynth.wasm` instance (the WebAssembly counterpart of `dlopen`, keeping base `symusic.wasm` pure MIT). Pure event scheduling lives in `schedule_synth_events()`, and `Synthesizer(std::span<const u8>, ...)` copies and owns SoundFont bytes via `fluid_sfloader_t` because each `render()` starts from a fresh FluidSynth instance.

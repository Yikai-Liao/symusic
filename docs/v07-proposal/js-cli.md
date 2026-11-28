# JS Packages and CLI (0.8+ Direction)

> [!NOTE]
> This page is the direction for work after 0.7. Nothing here is implemented, and nothing here is covered by the [compatibility promise](./README.md#compatibility-promise); names may change when the packages are built. The rules that 0.7 must already get right for it are listed under [what the core provides](#what-the-core-provides).

The target user writes code that produces a video, for example with Remotion, often with an AI agent writing that code, and needs a soundtrack that matches the cuts. They build scores programmatically in TypeScript (with ordinary loops, helpers and constants) or load MIDI, choose SoundFont presets and render exactly as many audio frames as the video is long, on a build machine with no audio device and no Python. The npm package `symusic` (targeting **Node.js, Bun, and WebAssembly**) and the `symusic` command line serve that path first; MIDI editing comes with them because it uses the same objects.

## First Example

```ts
import { readFile, writeFile } from "node:fs/promises";
import { init } from "symusic";
import { downloadSoundfont } from "symusic/node";

const sm = await init();
const rate = 48000;

using score = new sm.Score({
  ttype: "second",
  tempos: [{ time: 0, qpm: 120 }],
  tracks: [{
    name: "Piano",
    program: 0,
    notes: [
      { time: 0, duration: 0.4, pitch: 60, velocity: 80 },
      { time: 1.5, duration: 0.3, pitch: 67, velocity: 90 },
    ],
  }],
});

using synth = new sm.Synthesizer(await readFile(await downloadSoundfont()), { sampleRate: rate });
const audio = await synth.render(score, { sampleFrames: 30 * rate }); // exactly 30 s
await writeFile("music.wav", audio.toWav());
```

## What the Core Provides

The packages add no musical behaviour of their own. 0.7 already fixes what they need:

- `Second` scores render directly, and every rule a binding reproduces is in [semantics.md](./semantics.md), backed by the shared conformance cases.
- [Rendering](./semantics.md#rendering) states the natural length, the exact length given by `sample_frames`, the block-boundary timing bound, the drum bank lock and GM preset fallback, cooperative `stop_token` cancellation, the error on negative times and the independence of renders.
- [Audio buffers](./semantics.md#audio-buffers-and-wav) state the sample layout and the WAV encodings, which need no dependency.
- `events_from_columns` and `EventColumns` validate and convert bulk columns in one C++ core authority.
- The `Synthesizer` copies and owns the SoundFont bytes it is given and separates its FluidSynth function table from `dlopen` ([SoundFont and Synthesizer](./cpp-boundaries.md)), so a host without a file system can pass bytes and WASM builds need neither `Threads` nor `dlopen`.
- Node and Bun link the `ValueStorage` C++ core directly via ABI-stable **Node-API (N-API v8+)**, avoiding an extra C-FFI hop.

## Packages and Loading

- One package, `symusic`, ships ESM and TypeScript declarations. Prebuilt native N-API addons per `(os, arch, libc)` (including `linux-x64-musl` and `linux-arm64-musl` for Alpine/Docker video workers) and the base MIT WASM build are its `optionalDependencies` / internal dependencies, not packages users choose. Both **Node.js and Bun** load the same N-API `.node` addon natively. There is no CommonJS entry, so one process never holds two incompatible copies of the classes.
- `await init({ backend })` loads a core backend and returns a runtime whose members are the API (`sm.Score`, `sm.Track`, `sm.Synthesizer`). `backend` is `"auto"` (the default), `"native"` or `"wasm"`. `"auto"` picks the native N-API build when one exists for the platform and WASM otherwise; a native build that exists for the platform but fails to load raises instead of falling back silently. `libfluidsynth` is loaded lazily when `new sm.Synthesizer(...)` is called, so MIDI editing and conversion never fail `init()` on a machine without FluidSynth.
- Two runtimes may coexist in one process, which lets conformance tests run every case on both `"native"` and `"wasm"`. An object belongs to the runtime that created it, and passing it to another raises `TypeError`.
- `sm.backend`, `sm.version` and `sm.capabilities` report what was loaded. The backend never changes the result of an operation; `capabilities` says which operations it offers.
- The core takes and returns bytes (`Uint8Array`), never paths, URLs or `fetch`. Server/CLI code reads files with `node:fs/promises`. `symusic/node` uses only `node:fs/promises`, `node:path`, `node:os`, standard `fetch` and WebCrypto (`crypto.subtle`) to provide `downloadSoundfont()`, which caches and SHA-384-verifies presets from [`assets/soundfonts.json`](./cpp-boundaries.md) identically in **Node and Bun**; in a browser, the caller fetches the bytes directly.

## Object Model

Node, Bun and WASM share one object model. All use `ValueStorage` (`std::vector`), which moves elements when a list grows or is sorted, and no JS host releases element wrappers deterministically ([anchorvec](./cpp-boundaries.md)). JS therefore reads events as values and reaches embedded tracks and lists through generation-checked views:

- **`Score`, detached `Track`, and `Synthesizer` are handles to native/WASM memory.** Release them with `using` (`Symbol.dispose`) or `free()`; `sm.dispose()` releases everything the runtime still holds. Detached `Track` handles come from `new sm.Track(...)`, `track.clone()`, `track.clip(...)` and `track.adjustTime(...)`. A `FinalizationRegistry` frees forgotten handles as a backstop, not as the contract (WASM linear memory never shrinks automatically). Any use of a released handle raises with code `DISPOSED`.
- **Embedded tracks and event lists are checked views.** `score.tracks.at(0).notes.push(note)` edits the score in place on every backend, and calling `free()` on a borrowed view throws `TypeError` (`TYPE`). A view records its owner and a monotonic container/slot generation token; any structural or slot-rebinding change on its owner (`push`, `pop`, `splice`, `set`, `sort`, `reverse`, `clear`, `fromColumns`, or reassigning the list) invalidates older views so they raise `DISPOSED` instead of silently pointing at a different track or stale vector. Assigning `score.tracks = score.tracks.filter(pred)` snapshots the selected tracks before replacing the list.
- **Events are frozen values.** `notes.at(0)` returns a frozen plain object `{ time, duration, pitch, velocity }`. Assigning to a property throws `TypeError` in strict ESM code, so accidental in-place mutation never passes silently. Edit through the list (`notes.set(0, { ...note, pitch: 64 })`, `push`, `splice`), and `notes.filter(pred)` returns a plain JS `Array` of frozen event objects, just like `Array.prototype.filter`.
- **Columns are copies.** `notes.toColumns()` returns typed arrays matching the stored types (`Int32Array` for `Tick` times, `Float32Array` for `Quarter` and `Second` times, `Int8Array` for `pitch`/`velocity`/`fifths`), copied out of native or WASM memory so they remain valid after edits or `free()`. `notes.fromColumns(columns)` validates every row in C++ (`events_from_columns`) before committing any.
- **Audio and pianorolls are ordinary JS memory.** `AudioBuffer` (`sampleRate`, `channels`, `sampleFrames`, interleaved `Float32Array`) and `pianoroll()` (`modes`, `pitchRange`, `shape`, row-major `Uint8Array`) own JS typed arrays directly and need no `free()`.

Mutators return `undefined` and transforms return new objects, as in Python. Copies, value equality and the alias rules follow [copies and references](./semantics.md#copies-and-references).

## Names and Numbers

Names follow JS conventions; the meaning stays the Python one.

| Python | JS |
|---|---|
| `Score.from_midi(data)`, `score.to_midi()` | `Score.fromMidi(bytes)`, `score.toMidi()` |
| `Score.from_file(path)` | `Score.fromMidi(await readFile(path))`; the core takes no paths |
| `shift_time`, `shift_pitch`, `adjust_time` | `shiftTime`, `shiftPitch`, `adjustTime` |
| `is_empty()`, `note_count()` | `isEmpty()`, `noteCount()` |
| `filtered(pred)` | `filter(pred)`, returning a JS `Array` (events) or cloned `Track[]` |
| `copy()` (deep) | `clone()` |
| `to_numpy()`, `from_numpy()` | `toColumns()`, `fromColumns()` |
| `pianoroll(...)` | `pianoroll(...)` returning `{ modes, pitchRange, shape, data: Uint8Array }` |
| `ttype=Second` | `ttype: "second"`, typed as `"tick" \| "quarter" \| "second"` |
| Keyword arguments | One options object in `lowerCamelCase`: `clip(start, end, { tail: "cut" })`, `render(score, { sampleFrames })` |
| `download_soundfont()` | `downloadSoundfont()` in `symusic/node` |

`ttype` keeps its name as the domain term across bindings. A string union fits TypeScript narrowing and rejects unknown strings at run time.

Numbers follow [numeric input](./semantics.md#numeric-input). A `Tick` time is a JS `number` that must be an integer within `i32`; a fraction, `NaN` or out-of-range value raises `TypeError` or `RangeError` without bitwise coercion. A `boolean` is not a number. A JS `number` is `f64`, so storing a `Quarter` or `Second` time rounds to `f32`, as in Python.

## Errors

Errors are built-in JS classes (or `DOMException` for `AbortError`) with a stable `code`; tests compare codes, never messages.

| Semantics | JS class | `code` |
|---|---|---|
| Wrong type | `TypeError` | `TYPE` |
| Value out of range (`ValueError`) | `RangeError` | `RANGE` |
| Value beyond the stored type (`OverflowError`) | `RangeError` | `OVERFLOW` |
| Malformed MIDI | `SyntaxError` | `PARSE` |
| File or network failure (`symusic/node` & CLI) | `Error`, with the system error as `cause` | `IO` |
| Missing capability (no synthesis in this backend, no ABC tool) | `Error` | `UNSUPPORTED` |
| Released handle or invalidated view | `Error` | `DISPOSED` |
| Cancelled render (`AbortSignal`) | `DOMException` (`name: "AbortError"`) | `ABORT_ERR` |

## Rendering

`await synth.render(score, options)` takes `channels`, `sampleFrames` and `tailSeconds` with the meaning of [rendering](./semantics.md#rendering), plus `signal` (`AbortSignal`).

- **Native backend (Node & Bun):** Snapshots `score` synchronously on the JS thread and runs C++ `Synthesizer::render` directly on a background OS thread via N-API `napi_create_async_work` — keeping the JS event loop responsive with zero JS Worker startup or SoundFont duplication.
- **WASM backend:** Runs single-threaded WASM inside a persistent Worker owned by `Synthesizer` (requiring no `SharedArrayBuffer` or `COOP`/`COEP` headers) so repeated renders reuse the compiled module and SoundFont bytes. Browser synthesis supplies FluidSynth exports from a separate optional LGPL `fluidsynth.wasm` module into `Synthesizer::Backend`, keeping the base WASM package pure MIT and small.
- **Cancellation:** Triggering `signal` sets the C++ `RenderOptions::stop_token`, stopping at the next 64-frame block boundary and rejecting with `AbortError` while leaving `score` and `synth` intact.
- `audio.toWav({ encoding })` returns WAV bytes as a `Uint8Array`.
- Node, Bun and WASM share the exact scheduling and GM preset fallback rules; PCM is compared within a tolerance across platforms and FluidSynth builds.
- Render once in a build step or worker and hand the audio file to the video tool; cache keys combine score MIDI/content digest, SoundFont SHA-384, render settings and symusic version.

## Video Time

A soundtrack for video keeps a `Second` score as its time truth. Its `Quarter` reading follows from the tempo map ([converting between units](./semantics.md#converting-between-units)): changing the tempo in a `Second` score keeps every cut in place and changes how it reads in quarters.

The core has no video-frame unit. A package helper converts video frames to audio sample frames for rational frame rate `p/q` (`30/1`, NTSC `30000/1001`) in exact integer arithmetic, rounded half away from zero:

```text
sample_frame(f) = round(f × q × sample_rate / p)
```

At `30000/1001` fps and 48 kHz, 900 video frames give `1,441,440` sample frames without per-frame float drift. Point and batch `convertTime` / `convertTimes` wrap the core's `convert_time` / `convert_times` without copying the score.

## Command Line

The `symusic` CLI ships in the npm package (`npx symusic` / `bunx symusic`) on top of the JS API, needing no Python; Python wheels (`pip install symusic`) remain library-only unless a thin wrapper is requested later.

The first release covers the MIDI and soundtrack workflow:

```sh
symusic info input.mid --json
symusic validate input.mid
symusic render input.mid --soundfont gm.sf3 --sample-rate 48000 --sample-frames 1440000 -o music.wav
symusic clip input.mid --start 0 --end 30 --unit second -o cut.mid
symusic soundfont download musescore_general_sf3
symusic soundfont presets gm.sf3 --json
```

`resample` and `transpose` follow as single-operation commands. ABC conversion requires bundled `abcmidi` binaries in the npm distribution and reports `UNSUPPORTED` until then.

| Contract | Rule |
|---|---|
| Streams | `-` means stdin or stdout, and then `--input-format` or `--output-format` is required. stdout carries only the result; progress, warnings and diagnostics go to stderr. |
| Formats | A file's format comes from its extension, and an explicit flag wins. A stream's format is never guessed. |
| Overwriting | An existing output file is an error unless `--force` is given. Output goes to a temporary file in the target directory, renamed on success. |
| In place | Every writing command requires `-o`. A later `--in-place` gets its own definition. |
| Time | Options that take times take `--unit` (`tick`, `quarter`, `second`), defaulting to the input score's `ttype` (`tick` for MIDI). Tick values use the input score's `tpq`; a quarter is a quarter note, not a beat of the time signature. |
| Strictness | `strict` keeps the library's meaning and default; `--no-strict` clamps channel data bytes above 127. `validate` reports and never repairs. |
| Validation | `validate` checks that the input decodes into a valid `Score` and satisfies render preconditions (`<= 256` note-bearing tracks, no negative event times, valid WAV/frame bounds, plus preset fallback diagnostics when `--soundfont` is passed). Separate raw-SMF vs `Score` wire reporting joins once `minimidi` Tier-2 `MidiFile` lands. |
| SoundFonts | `render` never downloads. It takes `--soundfont`; `soundfont download` is the explicit step. |
| Length | `--sample-frames` gives the exact length; without it, the natural length and `--tail-seconds` apply. |
| JSON output | `--json` output on `info`, `validate` and `soundfont presets` has `schemaVersion` and fixed fields; new fields may be added. With `--json`, errors on stderr are JSON objects with `code` and `message`. |

| Exit code | Meaning |
|---|---|
| 0 | Success |
| 1 | Unclassified failure, a bug to report |
| 2 | Usage: unknown option, missing `-o`, output exists without `--force` |
| 3 | Invalid input data: parse errors, out-of-range fields |
| 4 | I/O: missing input, permissions, disk full |
| 5 | Missing capability or resource: no SoundFont, no synthesis in this backend, no ABC tool |

## Acceptance for the First Release

One soundtrack case, run on Node and Bun without an audio device and without Python, and on WASM where synthesis is enabled:

- 30 s at 48 kHz built in `Second` coordinates, with several cuts, one tempo change, a note held across a cut and silence at the end.
- The output has exactly `1,440,000` sample frames, and every onset is within the block bound of [rendering](./semantics.md#rendering).
- `Second` and `Quarter` checks of the cuts use the same tempo map.
- Rendering twice gives the same output, and aborting an in-flight render via `AbortSignal` rejects with `AbortError` at the next block boundary without leaking state.
- Node, Bun and WASM give the same objects, view invalidation errors and results for the same input and edits.

## Open Questions

- **Whether a standalone Score JSON format is worth shipping.** In code-generated video workflows, AI agents already write executable TypeScript or Python against `Score` using loops, variables and helper functions, whereas whether models can reliably author and maintain flat JSON arrays of raw note events is unproven. Score JSON is therefore **not** committed for 0.8. If empirical testing later justifies a text score interchange format, it must be cross-language (`snake_case` derived from C++ `detail::event_fields` as a single validation authority, carrying top-level `tpq` and canonical integer `mspq` for lossless MIDI round-tripping) rather than a JS-only schema.
- **How the npm package distributes GPL-2.0-or-later `abcmidi` executables** across platforms (or as a separate optional package).
- **Stems, streaming PCM, real-time playback and framework-specific video adapters** come later and must not change `Score` or the rendering rules. Stems are not promised to sum bit-exactly to the full mix, since polyphony limits and effects are shared.

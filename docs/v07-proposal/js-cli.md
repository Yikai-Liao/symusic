# JS Packages and CLI (0.8+ Direction)

> [!NOTE]
> This page is the direction for work after 0.7. Nothing here is implemented, and nothing here is covered by the [compatibility promise](./README.md#compatibility-promise); names may change when the packages are built. The rules that 0.7 must already get right for it are listed under [what the core provides](#what-the-core-provides).

The target user writes code that produces a video, for example with Remotion, often with an AI agent writing that code, and needs a soundtrack that matches the cuts. They write notes in seconds, choose SoundFont presets and render exactly as many audio frames as the video is long, on a build machine with no audio device and no Python. The npm package `symusic` and the `symusic` command line serve that path first; MIDI editing comes with them because it uses the same objects.

## First Example

```ts
import { readFile, writeFile } from "node:fs/promises";
import { init } from "symusic";
import { downloadSoundfont } from "symusic/node";

const sm = await init();
const rate = 48000;

using score = sm.Score.fromObject({
  schemaVersion: 1,
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
- [Rendering](./semantics.md#rendering) states the natural length, the exact length given by `sample_frames`, the block-boundary timing bound, the error on negative times and the independence of renders.
- [Audio buffers](./semantics.md#audio-buffers-and-wav) state the sample layout and the WAV encodings, which need no dependency.
- The `Synthesizer` copies and owns the SoundFont bytes it is given ([SoundFont and Synthesizer](./cpp-boundaries.md)), so a host without a file system can pass bytes.
- The core uses `ValueStorage`, and the experimental [C ABI](./cpp-engineering.md) freezes only after a JS or Go consumer has used it.

## Packages and Loading

- One package, `symusic`, ships ESM and TypeScript declarations. Native builds per platform and the WASM build are its dependencies, not packages users choose. There is no CommonJS entry, so one process never holds two incompatible copies of the classes.
- `await init({ backend })` loads a backend and returns a runtime whose members are the API (`sm.Score`, `sm.Synthesizer`). `backend` is `"auto"` (the default), `"native"` or `"wasm"`. `"auto"` picks the native build when one exists for the platform and WASM otherwise; a native build that exists but fails to load raises instead of falling back. A named backend that is unavailable raises.
- Two runtimes may coexist in one process, which lets tests run the same case on both backends. An object belongs to the runtime that created it, and passing it to another raises `TypeError`.
- `sm.backend`, `sm.version` and `sm.capabilities` report what was loaded. The backend never changes the result of an operation; `capabilities` says which operations it offers, since browser synthesis comes later than the rest.
- The core takes and returns bytes (`Uint8Array`), never paths, URLs or `fetch`. Node code reads files with `node:fs`. `symusic/node` adds `downloadSoundfont()`, which caches and verifies a preset from [`assets/soundfonts.json`](./cpp-boundaries.md); in a browser, the caller fetches the bytes and the manifest gives the URL and SHA-384 digest.

## Object Model

Node and WASM share one object model. Both use `ValueStorage` (`std::vector`), which moves elements when a list grows or is sorted, and neither host can release a wrapper per element deterministically ([anchorvec](./cpp-boundaries.md)). So JS reads events as values and reaches tracks through checked views:

- **Scores and synthesizers are handles to native memory.** Release them with `using` (`Symbol.dispose`) or `free()`; `sm.dispose()` releases everything the runtime still holds. A `FinalizationRegistry` frees forgotten handles as a backstop, not as the contract. Any use of a released handle raises with code `DISPOSED`.
- **Tracks and event lists are views.** `score.tracks.at(0).notes.push(note)` edits the score on both backends. A view records the score and the position it was taken at; once the score's track list changes shape (insert, remove, sort), using an older view raises with code `DISPOSED`, so a view never silently moves to another track. `track.clone()` gives an owned, independent track.
- **Events are frozen values.** `notes.at(0)` returns a frozen plain object `{ time, duration, pitch, velocity }`. Assigning to it throws `TypeError` in strict code, which every ES module is, so the mistake does not pass silently. Write through the list: `notes.set(0, { ...note, pitch: 64 })`, `push`, `splice`.
- **Columns are copies.** `notes.toColumns()` returns typed arrays whose element types match the stored types (`Int32Array` for `Tick` times, `Float32Array` for `Quarter` and `Second` times), copied out of native or WASM memory, so they stay valid after the score changes or is released. `notes.fromColumns(columns)` validates every row before it commits any. A borrowing view into WASM memory, if one is ever added, will be a separate API with a stated scope.
- **Audio is ordinary JS memory.** An `AudioBuffer` holds `sampleRate`, `channels`, `sampleFrames` and an interleaved `Float32Array`, and needs no release.

Mutators return `undefined` and transforms return new objects, as in Python. Copies, value equality and the alias rules follow [copies and references](./semantics.md#copies-and-references).

## Names and Numbers

Names follow JS conventions; the meaning stays the Python one.

| Python | JS |
|---|---|
| `Score.from_midi(data)`, `score.to_midi()` | `Score.fromMidi(bytes)`, `score.toMidi()` |
| `Score.from_file(path)` | `Score.fromMidi(await readFile(path))`; the core takes no paths |
| `shift_time`, `shift_pitch`, `adjust_time` | `shiftTime`, `shiftPitch`, `adjustTime` |
| `is_empty()`, `note_count()` | `isEmpty()`, `noteCount()` |
| `filtered(pred)` | `filter(pred)`, which returns a new object as `Array.prototype.filter` does |
| `copy()` (deep) | `clone()` |
| `to_numpy()`, `from_numpy()` | `toColumns()`, `fromColumns()` |
| `ttype=Second` | `ttype: "second"`, typed as `"tick" \| "quarter" \| "second"` |
| Keyword arguments | One options object in `lowerCamelCase`: `clip(start, end, { tail: "cut" })`, `render(score, { sampleFrames })` |
| `download_soundfont()` | `downloadSoundfont()` in `symusic/node` |

`ttype` keeps its name, because it is the domain term in every binding. A string union suits JS, where the type checker narrows on it and the run time rejects any other string.

Numbers follow [numeric input](./semantics.md#numeric-input). A `Tick` time is a JS `number` that must be an integer within the `i32` range; a fraction, `NaN` or a value outside the range raises, and nothing is coerced through bitwise operators. A `boolean` is not a number. A JS `number` is an `f64`, so storing a `Quarter` or `Second` time as `f32` rounds it, as in Python.

## Errors

Errors are built-in JS classes with a stable `code`; tests compare codes, never messages.

| Semantics | JS class | `code` |
|---|---|---|
| Wrong type | `TypeError` | `TYPE` |
| Value out of range (`ValueError`) | `RangeError` | `RANGE` |
| Value beyond the stored type (`OverflowError`) | `RangeError` | `OVERFLOW` |
| Malformed MIDI or score JSON | `SyntaxError` | `PARSE` |
| File or network failure | `Error`, with the system error as `cause` | `IO` |
| Missing capability (no synthesis in this backend, no ABC tool) | `Error` | `UNSUPPORTED` |
| Released handle or stale view | `Error` | `DISPOSED` |

A score JSON error also carries `path`, such as `tracks[0].notes[1].duration`, so an agent can fix the field it names.

## Rendering

`await synth.render(score, options)` takes `channels`, `sampleFrames` and `tailSeconds` with the meaning of [rendering](./semantics.md#rendering), and `signal`, an `AbortSignal`.

- The render runs in a worker thread (a Node worker or a Web Worker), so the event loop keeps running; a `Promise` alone would not achieve that.
- At the call, the render takes a snapshot of the score. Later edits do not reach a running render, and a render never changes its input. Aborting rejects with an `AbortError` and leaves nothing changed.
- `audio.toWav({ encoding })` returns the WAV bytes as a `Uint8Array`.
- Node and WASM give the same objects and the same scheduling rules. Their PCM is not bit-exact: compilers, CPUs and FluidSynth builds differ, so tests compare audio with a tolerance.
- Render once, in a build step or a worker, and hand the file to the video tool. Rebuilding the synthesizer or rendering inside per-frame component code repeats the work on every frame. A cache key holds the score's content, the SoundFont digest, the render settings and the symusic version.

Browser synthesis needs FluidSynth compiled to WASM with SoundFont bytes as input; the native `dlopen` path does not carry over. It stays deferred, but the API above already works for it: bytes in, a worker, a snapshot.

## Video Time

A soundtrack for video keeps a `Second` score as its time truth. Its `Quarter` reading follows from the tempo map ([converting between units](./semantics.md#converting-between-units)): changing the tempo in a `Second` score keeps every cut in place and changes how it reads in quarters. Converting to `Quarter` does not align anything; it only shows where a cut falls in the music.

The core has no frame unit. A helper in the package converts video frames to sample frames for a frame rate `p/q`, where 30 fps is `30/1` and NTSC is `30000/1001`:

```text
sample_frame(f) = round(f × q × sample_rate / p)
```

Each frame is converted on its own, in exact integer arithmetic, rounded half away from zero; adding a floating-point step per frame would drift. For a Remotion composition, the `fps` and `durationInFrames` of `useVideoConfig()` give `sampleFrames` for `render`. At 30000/1001 fps and 48 kHz, 900 video frames give 1,441,440 sample frames.

Helpers that map arrays of times between `Second` and `Quarter` without copying the score, and grid reports that give the error of each cut, can come later on the same conversion rules.

## Score JSON

Score JSON is the encoding of the one `Score` model that `Score.fromObject(obj)`, `score.toObject()` and the CLI share. There is no second, simplified "AI score" with different meanings.

- The top level carries `schemaVersion` (1) and `ttype`, which applies to every time in the document. Field names are those of the JS objects (`isDrum`, `minor`, `fifths`).
- A tempo gives exactly one of `qpm` and `mspq`; giving both is an error, as it is for the Python `Tempo` constructor.
- Unknown fields are errors, so a misspelled field never passes silently. A missing optional field takes the default of the matching constructor.
- The package publishes a JSON Schema generated from the field descriptions, so the ranges in it are the ranges the core checks.
- Errors name the JSON path of the field and use the codes in [errors](#errors).

## Command Line

The `symusic` command ships in the npm package (`npx symusic`) and is written on the JS API, so it runs wherever the package runs and its results are the library's. It needs no Python.

The first release covers the soundtrack path:

```sh
symusic validate score.json
symusic render score.json --soundfont gm.sf3 --sample-rate 48000 --sample-frames 1440000 -o music.wav
symusic convert input.mid -o score.json
symusic info input.mid --json
symusic soundfont download musescore_general_sf3
```

`clip`, `resample` and `transpose` follow as single-operation commands. A preset query (`soundfont presets --json`) waits until the behaviour for a missing preset is settled ([rendering](./semantics.md#rendering)). ABC conversion needs the abcmidi tools in the npm package and reports `UNSUPPORTED` until then.

| Contract | Rule |
|---|---|
| Streams | `-` means stdin or stdout, and then `--input-format` or `--output-format` is required. stdout carries only the result; progress, warnings and diagnostics go to stderr. |
| Formats | A file's format comes from its extension, and an explicit flag wins. A stream's format is never guessed. |
| Overwriting | An existing output file is an error unless `--force` is given. Output goes to a temporary file in the target directory, renamed on success. |
| In place | Every writing command requires `-o`. A later `--in-place` gets its own definition. |
| Time | Options that take times also take `--unit`. Tick values use the input score's `tpq`; a quarter is a quarter note, not a beat of the time signature. |
| Strictness | `strict` keeps the library's meaning and default; `--no-strict` clamps data bytes above 127. `validate` reports and never repairs. |
| Validation | `validate` reports three levels separately: valid bytes (SMF or score JSON), a valid `Score`, and renderable by this backend. A file can pass the first and fail the others. |
| SoundFonts | `render` never downloads. It takes `--soundfont`; `soundfont download` is the explicit step. |
| Length | `--sample-frames` gives the exact length; without it, the natural length and `--tail-seconds` apply. |
| JSON output | `--json` output has `schemaVersion` and fixed fields; new fields may be added. The human-readable output is not a protocol. With `--json`, the error on stderr is a JSON object with `code`, `message` and `path`. |

| Exit code | Meaning |
|---|---|
| 0 | Success |
| 1 | Unclassified failure, a bug to report |
| 2 | Usage: unknown option, missing `-o`, output exists without `--force` |
| 3 | Invalid input data: parse errors, out-of-range fields |
| 4 | I/O: missing input, permissions, disk full |
| 5 | Missing capability or resource: no SoundFont, no synthesis in this backend, no ABC tool |

Pipelines of several operations, when they come, are a small versioned JSON list of steps run in order, not a shell DSL. Each step reads times in the unit and `tpq` of its own input, and unknown operations or fields are errors.

## Acceptance for the First Release

One soundtrack case, run on a machine without an audio device and without Python, on both backends where they support synthesis:

- 30 s at 48 kHz, with several cuts, one tempo change, a note held across a cut and silence at the end.
- The output has exactly 1,440,000 sample frames, and every onset is within the block bound of [rendering](./semantics.md#rendering).
- `Second` and `Quarter` checks of the cuts use the same tempo map.
- Rendering twice gives the same output: no state leaks from the first render.
- Node and WASM give the same objects, views and errors for the same input and edits.

## Open Questions

- What a render does when the SoundFont lacks a track's preset, and with it the preset query.
- Whether the JS bindings call the C++ core directly or through the C ABI.
- How the npm package ships abcmidi.
- Stems, streaming PCM, real-time playback and adapters for particular video tools come later and must not change the `Score` or the rendering rules. Stems are not promised to sum bit-exactly to the full mix, since polyphony limits and effects are shared.

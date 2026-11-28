# Shared Semantics

Rules that every symusic binding reproduces: the Python package in 0.7, and the C ABI, JS and Go bindings that follow. [cpp-core.md](./cpp-core.md) and [python-api.md](./python-api.md) give the spelling in each language; this page gives the behaviour, so a binding that disagrees with it has a bug. [migration.md](./migration.md) lists where 0.6 behaved differently. In Wave 4 this page becomes `conformance/semantics.md`, backed by shared test cases ([rollout](./cpp-roadmap.md)).

## Time units and precision

A score stores every time in one unit:

| Unit | Storage | Meaning |
|---|---|---|
| `Tick` | `i32` | MIDI ticks; `tpq` ticks per quarter note |
| `Quarter` | `f32` | Quarter notes |
| `Second` | `f32` | Seconds |

Storage stays 32-bit, because note lists dominate memory and a wider time field would double the size of a `Note`. A float time `t` has a spacing of `2^(floor(log2 t) − 23)` to the next representable value; one rounding is off by at most half of it:

| `Second` time | Spacing | At 48 kHz |
|---:|---:|---:|
| 60 s | 3.8 µs | 0.18 sample frames |
| 600 s | 61 µs | 2.9 sample frames |
| 3600 s | 244 µs | 11.7 sample frames |

A `Quarter` time above 8192 quarters (68 minutes at 120 qpm) has a spacing above 1/1024 quarter, coarser than one tick at `tpq = 960`. Work that needs more, such as sample-accurate placement in an hour-long `Second` score, keeps `Tick` times.

Computation is wider than storage, so no step adds error beyond the final rounding:

- Tempo-map arithmetic (conversion between units, beat positions) runs in `f64`.
- Rendering maps a `Tick` time to a sample frame in `f64` directly, without passing through an `f32` second.
- The MIDI reader accumulates absolute ticks in `u64`; a time beyond the `i32` range of `Tick` raises `OverflowError`.

## Numeric input

Each value is checked once where it enters an object: constructors, setters, `from_numpy`, readers, and operations that compute a new field value. Nothing is clamped or wrapped silently.

| Value | Accepted | Rejected |
|---|---|---|
| `Tick` time or duration | Integers, including any type with `__index__` such as NumPy integers | A float, even `1.0`, or a `bool`: `TypeError`. Outside `i32`: `OverflowError` |
| `Quarter` or `Second` time or duration | Integers and floats | NaN or infinity: `ValueError`. Outside the finite `f32` range: `OverflowError` |
| Duration | `>= 0` | Negative: `ValueError` |
| Musical integer (pitch, velocity, program, controller number and value, pitch bend, `fifths`, `numerator`, `denominator`, `mspq`, `tpq`) | Integers within the field's range ([C++ synopsis](./cpp-core.md)) | Outside the range, whatever its size: `ValueError`. A float: `TypeError` |

A `from_numpy` column follows the rule of its field, and every row is checked before any event is built. Arithmetic that leaves a type's range, such as `shift_time` past the `i32` limit, raises `OverflowError`. Writing a public C++ field directly is the one unchecked path; encoders check every value they write.

## Converting between units

`to(unit)` and `resample(tpq)` map times through the tempo map. Before the first tempo the tempo is 120 qpm (`mspq = 500000`); among tempos at the same time, the last in list order is in effect.

- **Each endpoint maps on its own.** A note's onset and end are mapped separately and its duration is the mapped end minus the mapped onset, so a note that crosses a tempo change gets the right length. A duration is never scaled by the tempo at its onset.
- **Rounding to `Tick`** takes the exact `f64` value and rounds half away from zero.
- **`min_duration`** applies last, in the target unit: a converted duration below it is raised to it. The default `0` sets no floor, a negative value raises `ValueError`, and a raised duration may overlap the next note.
- **`resample(tpq)`** converts from the source unit to `Tick` at the new resolution in one step, so a `Quarter` or `Second` score is rounded once. It returns a new score even when the resolution is unchanged, and the floor still applies.
- **A `Second` score keeps seconds.** Its tempo events matter only for conversion: editing them changes what the times mean in quarters, not where they are in seconds. In a `Tick` or `Quarter` score, editing a tempo moves the derived seconds instead.

## Bounds and emptiness

- `start()` is the earliest time of any event, and `end()` the latest end, where a note ends at `time + duration` and other events at `time`. A track scans its notes, controls, pitch bends and lyrics; a score adds its tempos, time signatures, key signatures and markers. Pedals are derived from CC64 controls and add nothing.
- Lists and tracks without events do not take part. A score or track without events has `start() == end() == 0`.
- `Track.is_empty()` is true when the track has no events; its name and program do not count.
- `Score.is_empty()` is true when no list of the score or of its tracks holds an event. Tracks without events do not make a score non-empty.
- `Note.is_silent()` is true when `duration == 0` or `velocity == 0`.

## Sort keys

`sort()` is stable: events with equal keys keep their relative order. The default keys are ascending tuples:

| Event | Key |
|---|---|
| `Note` | `(time, duration, pitch, velocity)` |
| `ControlChange` | `(time, number)` |
| Every other event | `time` |
| `Track` in a `TrackList` | `(is_drum, program, name, note count)` |

The controller value is not part of the key, so a CC64 release and press at the same tick stay in the order they are played. Descending order (`reverse=True`, `SortOrder::Descending`) reverses the comparison and keeps equal keys in their original order, as `list.sort(reverse=True)` does. `Score.sort()` and `Track.sort()` sort every contained event list and keep the track order. A sort computes every key first and then reorders once, so a key function that raises leaves the list unchanged.

## Shifting

`shift_time(offset)` moves every event once: every track list and, on a score, its tempos, time signatures, key signatures and markers. `shift_pitch` and `shift_velocity` change every note of every track, drum tracks included. A result outside its range raises (`OverflowError` for time, `ValueError` for pitch and velocity), and the object is then unchanged: no event has moved.

## Clip

`clip(start, end)` selects the part of a score or track inside the half-open window `[start, end)`. It is a selection in the same coordinates: times are not shifted, and `shift_time(-start)` on the result moves it to 0.

**Notes.** A note lying wholly inside the window is kept, and one wholly outside is dropped. A note can stick out on either side: its **head** is the part before `start`, its **tail** the part after `end`. The `head` and `tail` options decide what happens to a note that has one:

| Mode | The note is |
|---|---|
| `drop` | removed |
| `cut` | shortened to start at `start` (head) or end at `end` (tail) |
| `keep` | kept unchanged |

- The defaults, `head = drop` and `tail = keep`, keep the notes whose onset lies in the window, so cutting consecutive windows puts every note in exactly one of them. `tail = drop` keeps only notes lying wholly inside; it cannot be emulated by moving `end`, because notes with the same onset can end at different times.
- A note ending at `start` or starting at `end` touches the window without entering it and is dropped.
- A note with both a head and a tail follows both rules: either `drop` removes it, and each `cut` shortens its side.
- A zero-duration note has neither a head nor a tail; it is kept when its time lies in `[start, end)`.

**Other events.** Controls, pitch bends, lyrics and markers are kept when their time lies in `[start, end)`. Tempos, time signatures and key signatures describe state, so the window keeps the state in effect where it begins. Let `t0` be `start`, or the earliest onset of a kept note if that is earlier, which happens only with `head = keep`. Each of the three lists keeps its events inside `(t0, end)` and gains, at time `t0`, the event in effect there: the last event at or before `t0`, taking the last in list order among equal times. A list with no event at or before `t0` gains nothing, and the default tempo applies there.

**Edge cases.**

- `start == end` gives an empty window: the result holds no events, not even the state at `start`.
- `start > end`, or a NaN bound, raises `ValueError`.
- Every track is kept, including tracks left without events, so track indexes stay aligned with the source.
- The result shares nothing with the source.

**What clip does not restore.** Controls are selected by time, so a sustain pedal held across `start` loses its press, and the controller, pitch-bend and program state in effect at `start` is not carried into the window. A playable excerpt that carries that state and starts at 0 is a later helper (`excerpt()`, [roadmap](../../ROADMAP.md)).

## Warping time

`adjust_time(original, target)` maps times through the piecewise-linear function that takes each `original[i]` to `target[i]`. It returns a new object.

- Both anchor lists have the same length, at least 2, and are strictly increasing; their values are finite. Anything else raises `ValueError`. Anchors are in the unit of the object.
- An event outside `[original[0], original[-1]]` is dropped. A note is kept only when its onset and its end both lie inside.
- A note's onset and end are mapped on their own, and its duration is the mapped end minus the mapped onset. `Tick` results round half away from zero.
- Tempo values are unchanged; only event times move. A stretched segment of a `Tick` score therefore plays longer in seconds.
- The tempo, time signature and key signature in effect at `original[0]` are kept at `target[0]`, as [clip](#clip) keeps them at `t0`.

## Copies and references

- Events, tracks and scores compare by value, including their time unit.
- A copy of a score, track or event shares nothing with its source. A returning transform (`clip`, `adjust_time`, `to`, `resample`) builds a fresh value even when nothing changes; Python's `filtered` is the one operation that returns the source's elements.
- A serialized form (MIDI, a pickle) holds values: two places that refer to one note in memory become two notes.
- In a binding that hands out references into lists (Python), a reference keeps pointing at the same element through sorting, insertion and deletion of other elements, and an element removed from its list stays valid and keeps its values. A binding that cannot keep this promise hands out values, or views that fail once their element may have moved ([JS](./js-cli.md#object-model)); it never hands out a reference that silently moves to another element.

Python's copy protocols, `deepcopy` memo and identity rules are in [python-api.md](./python-api.md#copies-and-references).

## MIDI export

`to_midi()` writes a Standard MIDI File, format 1, at the score's `tpq`. A `Quarter` or `Second` score is first converted to `Tick` by the [rules above](#converting-between-units).

**Values.** Export raises `ValueError` for a `tpq` outside `1..32767`, a tempo with `mspq` above `16777215`, or an event at a negative time.

**Tracks and channels.** Each track becomes one MIDI track that starts with its program change at tick 0. Drum tracks use channel 9 (zero-based). Melodic tracks take the other fifteen channels (`0–8`, `10–15`) in track order, counting melodic tracks only, and wrap after fifteen. Names, lyrics and markers are written as UTF-8. Bank selects are ordinary controls (CC0, CC32) and are written as stored.

**Notes.**

- A velocity-0 note is skipped: SMF has no way to write it, because a NoteOn with velocity 0 means NoteOff.
- A zero-duration note writes its NoteOn and NoteOff at the same tick.
- At one tick, a track writes its non-note messages first, then NoteOffs, then NoteOns. A zero-duration note's NoteOn goes with the NoteOffs and its NoteOff with the NoteOns, so pairing on import restores it.
- The writer orders a copy of each list stably by time. The score need not be sorted, and events at equal times keep their list order.

**Round trip.** Reading the file back with `from_midi` gives the source score, converted to `Tick` and without its velocity-0 notes, once both are sorted, when these hold:

- Every track holds at least one event. A track without events writes no channel message and does not come back.
- No two notes of the same pitch in one track overlap with one inside the other. The reader pairs NoteOn and NoteOff first-in first-out, so a nested note comes back with a different duration.

Channel sharing does not break the round trip, because the reader groups notes per MIDI track. It matters to players that merge channels across tracks: with more than fifteen melodic tracks, some share a channel and its controller state, and drum tracks always share channel 9.

## MIDI import

- **Pairing.** Within each MIDI track, a NoteOff closes the earliest open NoteOn of the same channel and pitch, across program changes. A NoteOn with velocity 0 is a NoteOff. An unmatched NoteOff is ignored, and a NoteOn still open at the end of the track is dropped.
- **Tracks.** Each MIDI track splits into one symusic track per `(channel, program)`, with the program at 0 until a program change. Channel 9 gives a drum track. Controls and pitch bends follow their channel's current group, and a group with controls but no notes still becomes a track. Bank selects stay ordinary controls and do not split tracks.
- **Strictness.** With `strict` (the default), a channel data byte above 127 raises `ParseError`; with `strict = False` it is clamped to 127. Structural errors raise `ParseError` in both modes.
- **Unsupported input.** SMPTE time division raises `ParseError`.

## Pianoroll

`pianoroll()` exists on `Tick` tracks and scores and returns a new `u8` array owned by the caller.

- Axes are `(mode, pitch, time)` for a track and `(mode, track, pitch, time)` for a score. The mode axis follows the order of `modes`, by default `(frame, onset)`.
- `pitch_range` is half-open, `[low, high)` with `0 <= low < high <= 128`; notes outside it are skipped.
- The time axis has `end() + 1` cells. A note at a negative time raises `ValueError`.
- `onset` writes the cell at the note's time, `frame` fills `[time, end)`, and `offset` writes the cell at its end. A zero-duration note writes its onset and offset cells and no frame.
- A cell holds `1`, or the velocity with `encode_velocity`. Velocity-0 notes are skipped. Where notes overlap, the later note in list order wins.

## Rendering

`render(score)` synthesizes a score of any unit with the SoundFont and settings of its `Synthesizer`. A `Tick` or `Quarter` score is converted to sample frames in `f64` through its tempo map.

**What sounds.** Each track renders on its own synthesizer channel, so tracks never share controller state; a score with more than 256 tracks that hold notes raises `ValueError`. A track plays its program; a drum track plays the drum bank. Notes, controls (sustain, bank select, volume and the rest) and pitch bends are scheduled. Velocity-0 and zero-duration notes are skipped. An event at a negative time raises `ValueError`; shift the score first. What happens when the SoundFont lacks a track's preset is open.

**Timing.** The synthesizer processes audio in blocks whose size comes from FluidSynth (64 frames by default, 1.33 ms at 48 kHz). An event starts at the first block boundary at or after its exact frame, so it is late by less than one block, and a note sounds for at least one block. A later release may tighten this bound without changing the API. Audible onset also depends on the instrument's attack, which is part of the SoundFont, not of scheduling.

**Length.**

- By default the output has `ceil((end_seconds + tail_seconds) × sample_rate)` frames, where `end_seconds` is the score's `end()` in seconds and `tail_seconds` defaults to 1.0. A score without events gives `tail_seconds` of silence.
- With `sample_frames = N`, the output has exactly `N` frames, whatever the score's length: synthesis continues through releases and silence up to `N`, events at or after `N` do not sound, and the audio stops at `N` without a fade. `tail_seconds` is ignored, and an empty score gives `N` frames of silence.

**Independence.** Every render starts from fresh synthesizer state, so its output does not depend on earlier renders, and concurrent renders on one `Synthesizer` are safe while nothing mutates the score being rendered. Output is not promised bit-exact across platforms, FluidSynth versions or `cpu_cores`; tests compare with a tolerance.

**Windows.** Rendering a [clip](#clip) is not a slice of the full render: notes sounding into the window, controller and pedal state, and reverb tails from before it are missing. Slice a full render where the audio must match.

## Audio buffers and WAV

A rendered buffer holds `f32` samples, interleaved by frame (`L, R, L, R, ...` for stereo); its length in frames is its `sample_frames`. Mono is the average of the left and right channels. Samples are not normalized and may exceed `[-1, 1]`; `gain` sets the level.

WAV output (`to_wav`, `write_wav`) checks the buffer first: one or two channels, a whole number of frames, a positive sample rate and only finite samples. Any other buffer raises `ValueError`. The two encodings are:

| Encoding | WAV format | Sample |
|---|---|---|
| `pcm16` (default) | PCM, 16-bit little-endian | The sample clamped to `[-1, 1]`, times 32767, rounded to nearest |
| `float32` | IEEE float, 32-bit little-endian | The sample unchanged |

Output beyond the 4 GiB limit of a RIFF file raises `OverflowError`. `write_wav` reports file errors as an `OSError` subclass chosen by `errno`.

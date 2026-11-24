# Troubleshooting

Each entry starts with the error message or symptom and links to the page that explains the rule.

## Installing

- **`pip install symusic` compiles from source.** No wheel matches the interpreter or platform; [quickstart.md](quickstart.md#install) lists the published wheels. The source build needs a C++20 compiler.
- **`RuntimeWarning` about the GIL on import.** Free-threaded CPython builds (`3.13t`, `3.14t`) are not supported, and the extension re-enables the GIL. Use a regular CPython build.

## Loading files

- **`ValueError: Unknown file format`.** The extension is not one of `.mid`, `.midi`, `.MID`, `.MIDI` or `.abc`. Pass `format="midi"` or `format="abc"`; see [files.md](files.md#loading).
- **`FileNotFoundError` or `IsADirectoryError` from `Score(path)`.** The path does not exist or is a directory.
- **`TypeError: Score expects a path or binary file; pass ticks_per_quarter by keyword`.** `Score(...)` reads a path or binary file, or builds an empty score with `ticks_per_quarter=...`. Load bytes with `Score.from_midi(data)` and ABC text with `Score.from_abc(text)`.
- **`RuntimeError: MiniMidi: ...`.** The MIDI file is truncated or corrupt. In a dataset, catch the error and skip the file; see [datasets.md](datasets.md#parallel-loading).
- **`RuntimeError: Get control_value=168`, `Get pitch_bend=...` or similar.** The file contains a data byte above 127. Read the bytes with `Score.from_midi(data, invalid_data_bytes="clamp")`; see [files.md](files.md#reading-midi).
- **`RuntimeError: Division type is not ticks per quarter`.** The file uses SMPTE time division, which symusic does not read.
- **`RuntimeError: abc2midi ...` or `ABCConversionWarning` from an ABC file.** `abc2midi` reported diagnostics; fatal failures include the captured output in the exception message, and recovered imports emit `ABCConversionWarning`. See [files.md](files.md#abc).

## Building and editing scores

- **`ValueError: Time units must match`.** The object you add has a different time unit from the list. Create events with the list's `ttype`, or convert the source with `to`; see [time_units.md](time_units.md).
- **`ValueError: pitch must be in 0..127, got 128` when creating an event.** Pitch, velocity, and control numbers and values range from 0 to 127; see [data_model.md](data_model.md#events).
- **`TypeError: 'float' object cannot be interpreted as an integer` when creating an event.** Tick times and durations are integers. Round them, or create the event with `ttype=Quarter`; see [time_units.md](time_units.md).
- **`TypeError` on `Score(480)`.** Pass the resolution by keyword with `Score(ticks_per_quarter=480)`.
- **`ValueError: ttype must be Tick, Quarter or Second`.** `ttype` takes the markers `Tick`, `Quarter` and `Second` imported from `symusic`, not strings such as `"tick"`.
- **`ValueError: pitch must be in 0..127, got 128` from `shift_pitch` or `shift_velocity`.** The shift would move a value outside 0–127. The call changes nothing; see [data_model.md](data_model.md#transformations).
- **`ValueError: symusic::ops::adjust_time: ...`.** The anchor lists differ in length, have fewer than two entries, or violate the required ordering. Original times must increase strictly, and new times must be nondecreasing.
- **Notes have duration 0 after `to(Tick)`, `resample` or a MIDI round trip.** Durations shorter than half a tick round to 0. Pass `min_duration=1`; see [time_units.md](time_units.md#conversion-rules).
- **Editing a pedal snapshot.** Change control 64 in `controls` to change sustain. [MIDI writing](files.md#writing-midi) uses these controls.
- **A loaded track has lyrics but no notes.** Lyrics stay on their own track only when the MIDI track contains no notes; otherwise they attach to the note-bearing track; see [files.md](files.md#reading-midi).

## Arrays and piano rolls

- **`ValueError: Time units must match` from `pianoroll()`.** Piano rolls exist only for tick scores and tracks. Convert with `to(Tick)` or `resample`; see [arrays.md](arrays.md#piano-rolls).
- **`ValueError: Pitch range [...) is invalid` or `No such pianoroll mode called ...`.** See the rules in [arrays.md](arrays.md#piano-rolls).
- **Looping over notes in Python is slow.** Every element read from an event list creates a Python object. Use `to_numpy()` instead; see [arrays.md](arrays.md).

## Synthesis

- **FluidSynth load error from `Synthesizer`.** Check the reported library path under `symusic/_vendor/fluidsynth/`. Reinstall symusic if the bundled runtime is missing or damaged.
- **`FileNotFoundError` or `PermissionError` from `Synthesizer`.** Check that the SoundFont at the reported path exists and is readable when you render.
- **`ConnectionError` from `Soundfont.download()`.** The download failed or did not match its expected digest. See [SoundFonts](synthesis.md#soundfonts).
- **The rendered audio clips or sounds dry.** Samples are not normalized, so loud scores can clip. Sustain comes from control change 64 in `controls`; see [synthesis.md](synthesis.md).

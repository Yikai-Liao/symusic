# Troubleshooting

Each entry starts with the error message or symptom and links to the page that explains the rule.

## Installing

- **`pip install symusic` compiles from source.** No wheel matches the interpreter or platform; [quickstart.md](quickstart.md#install) lists the published wheels. The source build needs a C++20 compiler.
- **`RuntimeWarning` about the GIL on import.** Free-threaded CPython builds (`3.13t`, `3.14t`) are not supported, and the extension re-enables the GIL. Use a regular CPython build.

## Loading files

- **`ValueError: Unknown file format`.** The extension is not one of `.mid`, `.midi`, `.MID`, `.MIDI` or `.abc`. Pass `format="midi"` or `format="abc"`; see [files.md](files.md#loading).
- **`ValueError: ... is not a file`.** The path does not exist or is a directory.
- **`TypeError: Invalid type: <class 'bytes'>`.** `Score(...)` takes a path, a tick count or another score. Load bytes with `Score.from_midi(data)` and ABC text with `Score.from_abc(text)`.
- **`RuntimeError: MiniMidi: ...`.** The MIDI file is truncated or corrupt. In a dataset, catch the error and skip the file; see [datasets.md](datasets.md#parallel-loading).
- **`RuntimeError: Get control_value=168`, `Get pitch_bend=...` or similar.** The file contains a data byte above 127. Load it with `sanitize_data=True`; see [files.md](files.md#reading-midi).
- **`RuntimeError: Division type is not ticks per quarter`.** The file uses SMPTE time division, which symusic does not read.
- **`RuntimeError: abc2midi ...` or `ABCConversionWarning` from an ABC file.** `abc2midi` reported diagnostics; fatal failures include the captured output in the exception message, and recovered imports emit `ABCConversionWarning`. See [files.md](files.md#abc).

## Building and editing scores

- **`ValueError: Time units must match`.** The object you add has a different time unit from the list. Create events with the list's `ttype`, or convert the source with `to`; see [time_units.md](time_units.md).
- **`TypeError: __init__(): incompatible function arguments` when creating a `Note`.** A pitch or velocity outside −128..127 does not fit the 8-bit field, or a time has the wrong type for the unit; see [data_model.md](data_model.md#events).
- **`TypeError: ... unexpected keyword argument 'tpq'`.** Pass the resolution positionally, `Score(480)`, or call `Score.from_tpq(480)`.
- **`ValueError: ttype must be Tick, Quarter, Second or string`.** The unit name is misspelled; use `"tick"`, `"quarter"` or `"second"`.
- **`ValueError: Overflow while adding ...`.** `shift_pitch` or `shift_velocity` pushed a value outside −128..127; see [data_model.md](data_model.md#transformations).
- **`ValueError: symusic::ops::adjust_time: ...`.** The anchor lists differ in length, have fewer than two entries, contain a non-finite value, or violate the required ordering; see [data_model.md](data_model.md#transformations).
- **Notes have duration 0 after `to("tick")`, `resample` or a MIDI round trip.** Durations shorter than half a tick round to 0. Pass `min_dur=1`; see [time_units.md](time_units.md#conversion-rules).
- **Editing `pedals` does not change the written file.** MIDI stores the sustain pedal as control change 64 in `controls`; see [files.md](files.md#writing-midi).
- **A loaded track has lyrics but no notes.** Lyrics stay on their own track only when the MIDI track contains no notes; otherwise they attach to the note-bearing track; see [files.md](files.md#reading-midi).

## Arrays and piano rolls

- **`AttributeError: ... has no attribute 'pianoroll'`.** Piano rolls exist only for tick scores and tracks. Convert with `to("tick")` or `resample`; see [arrays.md](arrays.md#piano-rolls).
- **`ValueError: Pitch range [...) is invalid` or `No such pianoroll mode called ...`.** See the rules in [arrays.md](arrays.md#piano-rolls).
- **Looping over notes in Python is slow.** Every element read from an event list creates a Python object. Use `numpy()` instead; see [arrays.md](arrays.md).

## Synthesis

- **FluidSynth load error from `Synthesizer`.** Check the reported library path under `symusic/_vendor/fluidsynth/`. Reinstall symusic if the bundled runtime is missing or damaged.
- **`FileNotFoundError` or `PermissionError` from `Synthesizer`.** Check that the SoundFont at the reported path exists and is readable when you render.
- **`ConnectionError` from `Soundfont.download()`.** The download failed or did not match its expected digest. See [SoundFonts](synthesis.md#soundfonts).
- **The rendered audio clips or sounds dry.** Samples are not normalized, so loud scores can clip. Sustain comes from control change 64 in `controls`; the `pedals` list is not rendered. See [synthesis.md](synthesis.md).

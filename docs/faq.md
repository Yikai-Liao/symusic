# FAQ

## Installation

### `pip install symusic` compiles from source

No wheel matches your interpreter or platform; {doc}`quickstart` lists the published wheels. The source build needs a C++20 compiler, and {doc}`CONTRIBUTING` lists the supported ones.

### Importing symusic warns about the GIL

Free-threaded CPython builds (`3.13t`, `3.14t`) are not supported: the extension re-enables the GIL and Python emits a `RuntimeWarning` on import. Use a regular CPython build.

## Loading files

### `ValueError: Unknown file format`

`Score(path)` picks the format from the extension and only knows `.mid`, `.midi`, `.MID`, `.MIDI` and `.abc`. For other names, pass `fmt="midi"` or `fmt="abc"`.

### A MIDI file raises `RuntimeError: Get control_value=168` or similar

The file contains a data byte above 127, which MIDI does not allow. `Score(path, sanitize_data=True)` and `Score.from_midi(data, sanitize_data=True)` clamp such bytes to the 7-bit range before parsing. Other damage, such as an unknown status byte, still raises `RuntimeError`.

### An ABC tune raises `RuntimeError: Process exited with code 1`

ABC import runs the bundled `abc2midi`, which exits with status 1 whenever it reports an error, including errors it recovers from. Its messages are printed to the terminal with line and character positions; fix the reported lines. Accepting recovered conversions is planned in the {doc}`ROADMAP`.

## Saving files

### `dump_abc` produces irregular notation

`dump_abc` writes the score as MIDI and converts it with `midi2abc`, which infers note lengths and bar lines from the timing. A score that is not quantized to a regular grid gives tuplets, ties and odd lengths.

## Performance

### Looping over notes in Python is slow

Every element read from an event list in Python creates a Python object. `track.notes.numpy()` exports all fields in one call; see {doc}`quickstart`.

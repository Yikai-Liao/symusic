# ABC notation

Symusic reads ABC as performed music, not as an engraving. Use `Score.from_abc(text)` for a string or `Score(path)` for a file. The reader takes the first tune, expands parts and repeats, and produces notes, tracks, and global metadata at 480 ticks per quarter. You can request quarter-note or second times with `ttype`.

The reader follows `abc2midi` playback where practical. That includes a one-tick initial note offset, beat accents, grace-note timing, voice overlays, and MIDI accompaniment from chord symbols and supported directives. Backing instruments and instrument changes may appear on separate tracks. Lyrics are kept when they can be aligned to notes. Layout and references to external files do not affect playback. Some advanced MIDI directives and jumps such as coda and segno are not performed.

## Diagnostics

- **Recoverable input:** Python issues a `UserWarning` and returns the score, for example after ignoring a stray closing bracket or encountering a second tune. Warnings include the source, one-based line, UTF-8 byte column, and a code. Unsupported non-binary note-length divisors are rounded up to the next power of two, with a warning.
- **Invalid input:** A missing tune header, an unclosed music group, division by zero, or playback beyond the score's range raises `ValueError` with the same source format. Playback expansion and chord size are bounded. File access errors are reported separately.
- **Lossy export:** `dumps_abc(warn=True)` and `dump_abc(path, warn=True)` issue `UserWarning` for omissions, resampling, and voice splitting. With the default `warn=False`, these warnings are suppressed, but invalid scores still raise.

## Export timing and compatibility

The writer emits playable ABC rather than reconstructing the original notation. It preserves note pitches, velocities, timing on its 480-tick grid, programs, tempos, keys, meters and supported control changes. It uses ties, tuplets, and additional voices when needed. If an audible note starts at tick zero, all note onsets shift one tick to accommodate the ABC playback offset. Short audible notes that collapse during resampling are extended to one tick. Arbitrary timed lyrics, pitch bends, and markers are not exported.

The exporter splits dense chords across voices and refuses scores that need more than 15 voices, for compatibility with `abc2midi`. Other converters can shorten tied and drum notes, place some metadata and controllers at nearby MIDI events, and discard per-voice names. Native round trips and upstream playback are tested separately.

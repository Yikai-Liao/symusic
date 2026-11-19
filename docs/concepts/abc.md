# ABC notation

Symusic reads ABC as performed music, not as an engraving. Use `Score.from_abc(text)` for a string or `Score(path)` for a file. The reader takes the first tune, expands parts and repeats, and produces notes, tracks, and global metadata at 480 ticks per quarter. You can request quarter-note or second times with `ttype`.

The reader follows `abc2midi` playback where practical. That includes a one-tick initial note offset, beat accents, grace-note timing, voice overlays, and MIDI accompaniment from chord symbols and supported directives. Backing instruments and instrument changes may appear on separate tracks. Lyrics follow the music line they annotate in the same voice. A muted note still takes a syllable, and a chord takes one. When the part order replays a part, its written verses cycle by occurrence. Layout and references to external files do not affect playback. Some advanced MIDI directives and jumps such as coda and segno are not performed.

Fractional accidentals in notes and key signatures produce MIDI pitch bends. Bends are shared by notes on a track: if overlapping notes require different bends, the reader warns and retains the channel's bend changes. Per-pitch-class offsets from `%%temperament` also produce bends; they apply in written order across voices and repeats. The reader also supports `%%MIDI temperamentlinear`, `%%MIDI temperamentequal`, and `%%MIDI temperamentnormal`.

## Diagnostics

- **Recoverable input:** Python issues a `UserWarning` and returns the score, for example after ignoring a stray closing bracket or encountering a second tune. Warnings include the source, one-based line, UTF-8 byte column, and a code. Unsupported non-binary note-length divisors are rounded up to the next power of two, with a warning.
- **Invalid input:** Invalid UTF-8 or embedded NUL, a missing tune header, an unclosed music group, division by zero, or playback beyond the score's range raises `ValueError` with the same source format. Playback expansion and chord size are bounded. File access errors are reported separately.
- **Lossy export:** `dumps_abc(warn=True)` and `dump_abc(path, warn=True)` issue `UserWarning` for omissions, resampling, and voice splitting. With the default `warn=False`, these warnings are suppressed, but invalid scores still raise.

## Export timing and compatibility

The writer emits playable ABC rather than reconstructing the original notation. It preserves note pitches, velocities, timing on its 480-tick grid, programs, tempos, keys, meters, supported control changes, and explicit MIDI pitch bends. It uses ties, tuplets, and additional voices when needed. If an audible note starts at tick zero, all note onsets shift one tick to accommodate the ABC playback offset. Export validates original event times in all three time units before rounding to the ABC grid. Short audible notes that collapse during rounding are extended to one tick; pedal intervals that collapse are omitted. Arbitrary timed lyrics and markers are not exported.

The exporter splits dense chords across voices. It rejects scores that exceed the playback work limit or need more than 15 voices, the `abc2midi` channel limit. Other converters can shorten tied and drum notes, move some metadata and controllers to earlier MIDI events, and discard control-only tracks or per-voice names. abc2midi may also place a timed pitch bend early and shift subsequent notes. Use MIDI export when bend timing must agree across players. Long rests are split into bounded atoms to avoid length overflow in abc2midi. Very long holds and MIDI delta times can still exceed that converter’s limits; the exporter warns when it detects that risk. Native round trips and upstream playback are tested separately.

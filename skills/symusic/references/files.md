# Files: MIDI, ABC and pickle

symusic reads and writes Standard MIDI Files natively, converts ABC notation through the bundled `abc2midi` and `midi2abc` programs, and pickles any of its objects. The objects themselves are described in [data_model.md](data_model.md).

## Loading

```python
from symusic import Score

score = Score("song.mid")  # the same as Score.from_file("song.mid")
with open("song.mid", "rb") as f:
    same = Score.from_midi(f.read())
assert score == same
```

`Score(path)` and `Score.from_file(path)` pick the format from the extension: `.mid`, `.midi`, `.MID` and `.MIDI` are MIDI, `.abc` is ABC, and any other extension raises `ValueError`. Pass `fmt="midi"` or `fmt="abc"` (in any letter case) to override it. `Score.from_midi(data)` reads bytes and `Score.from_abc(text)` reads a string. Every loader takes `ttype`; see [time_units.md](time_units.md).

## Reading MIDI

- **Resolution.** `ticks_per_quarter` comes from the file header. Files with SMPTE time division raise `RuntimeError` ("Division type is not ticks per quarter").
- **Tracks.** symusic splits each MIDI track by channel and current program, so one MIDI track that plays two instruments, or switches program partway, becomes several `Track` objects. Each takes the name of its MIDI track, and `is_drum` is set for channel 10 (index 9).
- **Retained events.** Tracks with notes, control changes, pitch bends or lyrics are kept. A new note track takes all control changes and pitch bends buffered on its channel. Events still buffered at the end of the MIDI track form one track for each channel and the program in effect when they arrived. Lyrics do not take buffered events; a lyric shares a track with them only when both have the same channel and program. Note-ons that are never released are dropped.
- **Notes.** Note-on and note-off events pair first in, first out per channel and pitch; a note-on with velocity 0 counts as a note-off.
- **Pedals.** Sustain pedal regions are derived from control change 64 on each channel: a value of 64 or more presses the pedal, a lower value releases it. The control changes stay in `controls` as well.
- **Meta events.** Track names, lyrics and markers are read as text with bytes that are not valid UTF-8 removed; empty lyrics and markers are dropped. Markers go to `score.markers`. Lyrics go to the first channel/program track created by a note-on in the same MIDI track that still contains notes after pairing. If none contains notes, lyrics use the channel 16 track for the program in effect where each lyric appears. Tempo, time signature and key signature events from every MIDI track go to the global lists. All other meta events and SysEx messages are ignored.
- **Order.** Notes are sorted by time only, so `notes.is_sorted()` with the default key can be false after loading; call `score.sort()` when you need the full order. The global lists are sorted by time.
- **Invalid data.** A data byte above 127 raises `RuntimeError`, for example "Get control_value=168", and so does a pitch bend outside −8192..8191 ("Get pitch_bend=..."). `sanitize_data=True` clamps those bytes to 127 and accepts the pitch bend instead.

## Writing MIDI

`score.dump_midi(path)` writes a file and `score.dumps_midi()` returns bytes. A score in quarters or seconds is converted to ticks first, the same way as `to("tick")`; to keep short notes from rounding to zero length, convert yourself with `score.to("tick", min_dur=1)` and dump the result.

- The file is a multi-track (format 1) file with the score's `ticks_per_quarter`. A resolution outside 1–32767 raises `ValueError`. Each `Track` becomes one MIDI track with its name and a program change at time 0.
- The global events go into the first MIDI track, or into a track of their own when the score has no tracks.
- Drum tracks use channel 10. Every other track takes a channel from 1–9 and 11–16 by its position in `tracks`, drum tracks included, so channels repeat from the sixteenth track on.
- An event with a negative start time raises `ValueError` with the event kind and time. Pitch bends, tempos, time signatures and event deltas must fit their MIDI fields.
- `pedals` are not written; the sustain pedal reaches the file only through the control changes in `controls`, so editing `pedals` does not change the file.

A MIDI file read and written again keeps notes, control changes, pitch bends, lyrics, track names, programs and the global events. It loses what reading drops, and the track layout follows the rules above rather than the original file.

## ABC

```python
from symusic import Score

score = Score("tune.abc")
abc_text = score.dumps_abc()
score.dump_abc("out.abc")
```

- **Reading.** symusic runs `abc2midi` on the text and reads the MIDI file it writes, so the result follows `abc2midi`, including its `%%MIDI` directives, and the rules in [Reading MIDI](#reading-midi). The first tune is the default. Pass `tune=` with an `X:` number to select a tune. `Score.from_abc_file(path, tune=...)` reads ABC regardless of the extension. `abc2midi` starts notes one tick late, at tick 1 of 480 ticks per quarter.
- **Errors.** Import reports diagnostics from a usable conversion as `symusic.ABCConversionWarning`. Failed conversions raise `RuntimeError` with the converter's diagnostics. An unknown tune raises `ValueError`, and a missing converter raises `FileNotFoundError`.
- **Writing.** `dump_abc` and `dumps_abc` convert the score's MIDI bytes with `midi2abc`. The converter infers note lengths and bar lines from the timing, so unquantized scores can produce irregular notation. Export omits the tune title. Pass `warn=True` to receive `ABCConversionWarning` for converter diagnostics on success.

## Pickle

Scores, tracks, events and event lists support `pickle` and `copy.deepcopy`. A pickle stores symusic's binary representation, so it round-trips exactly and is the fast way to send scores to worker processes:

```python
import pickle
from symusic import Score

score = Score("song.mid")
restored = pickle.loads(pickle.dumps(score))
assert restored == score
```

Use the same symusic version in every worker. Pickle state with an unsupported format version or a malformed payload raises `ValueError`. [datasets.md](datasets.md) shows a multiprocessing pipeline.

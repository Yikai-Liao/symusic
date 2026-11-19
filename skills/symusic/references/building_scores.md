# Building scores

Model output, generated music and converted data all end the same way: create a score, fill its lists and write a file. The objects are described in [data_model.md](data_model.md).

## From events

Build in the unit that suits the source; quarters are natural for generated rhythms. `dump_midi` converts to ticks when it writes.

```python
from symusic import ControlChange, Note, Score, Tempo, TimeSignature, Track

q = "quarter"
score = Score(480, ttype=q)
score.tempos.append(Tempo(0.0, qpm=100, ttype=q))
score.time_signatures.append(TimeSignature(0.0, 3, 4, ttype=q))

melody = Track("melody", program=73, ttype=q)
for i, pitch in enumerate([72, 74, 76, 77, 79, 77]):
    melody.notes.append(Note(i * 0.5, 0.5, pitch, 90, ttype=q))
melody.controls.append(ControlChange(0.0, 64, 127, ttype=q))  # sustain on
melody.controls.append(ControlChange(3.0, 64, 0, ttype=q))  # sustain off

drums = Track("drums", is_drum=True, ttype=q)
for beat in range(6):
    drums.notes.append(
        Note(float(beat), 0.25, 36 if beat % 3 == 0 else 42, 100, ttype=q)
    )

score.tracks.extend([melody, drums])
score.dump_midi("generated.mid")
```

- Every event, track and score must use the same `ttype`; mixing units raises `TypeError`.
- `Score(480)` sets the resolution that the file gets. Choose one that represents every duration you generate: 480 holds sixteenth notes, triplets and quintuplets exactly.
- Add tempos and time signatures yourself; without them players assume 120 quarter notes per minute and 4/4.
- Set `is_drum=True` for percussion. Its notes choose instruments by pitch (36 is a bass drum, 42 a closed hi-hat), and the track is written to channel 10.
- Write the sustain pedal as control change 64 in `controls`. `pedals` is not written to MIDI.
- `dump_midi` orders events by time itself, so the lists need not be sorted. Call `score.sort()` if your own code relies on list order.

## From arrays

For model output already in arrays, `from_numpy` builds a whole list in one call:

```python
import numpy as np
from symusic import Note, Score, Track

onsets = np.arange(16) * 0.25  # quarters
pitches = np.array([60, 64, 67, 72] * 4)
notes = Note.from_numpy(
    time=onsets,
    duration=np.full(16, 0.25),
    pitch=pitches,
    velocity=np.full(16, 80),
    ttype="quarter",
)

score = Score(480, ttype="quarter")
track = Track("arpeggio", ttype="quarter")
track.notes = notes
score.tracks.append(track)
score.to("tick", min_dur=1).dump_midi("arpeggio.mid")
```

Converting to ticks with `min_dur=1` before writing keeps very short generated notes from rounding to zero length; see [time_units.md](time_units.md).

## From another score

To build a variant of an existing score, copy it and edit the copy. `score.copy()` is deep, so the original is not affected:

```python
from symusic import Score

original = Score("song.mid")
variant = original.copy()
variant.tracks = [t for t in variant.tracks if t.is_drum]
for tempo in variant.tempos:
    tempo.qpm *= 1.25
variant.dump_midi("drums_faster.mid")
```

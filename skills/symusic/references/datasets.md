# Processing datasets

This page shows the parts of a dataset pipeline: parallel loading, handling bad files, cleaning, segmenting and augmenting.

## Parallel loading

Run one worker per core and return only what you need. Scores pickle quickly, but returning features or arrays instead of whole scores keeps the transfer small.

```python
from concurrent.futures import ProcessPoolExecutor
from pathlib import Path

from symusic import Score


def features(path):
    try:
        score = Score(path, ttype="second")
    except (RuntimeError, ValueError) as error:
        return path.name, None, str(error)
    return path.name, {"notes": score.note_num(), "seconds": score.end()}, None


if __name__ == "__main__":
    paths = sorted(Path(".").glob("*.mid"))
    with ProcessPoolExecutor() as pool:
        for name, result, error in pool.map(features, paths, chunksize=16):
            print(name, result if error is None else f"skipped: {error}")
```

- The `if __name__ == "__main__":` guard is required on Windows and macOS, where worker processes start by importing the script.
- A corrupt or truncated MIDI file raises `RuntimeError`; a path with an unknown extension raises `ValueError`. Catch both in the worker so one bad file does not stop the run.
- Files that store data bytes above 127, common in some scraped collections, load with `sanitize_data=True`; see [files.md](files.md#reading-midi).
- To parse a dataset once and reuse it, store the pickled scores (see [files.md](files.md#pickle)) instead of the MIDI files.

## Cleaning

```python
from symusic import Score

score = Score("song.mid")
score.tracks = [t for t in score.tracks if not t.is_drum and t.note_num() > 0]
for track in score.tracks:
    track.notes.filter(lambda n: n.duration > 0 and n.velocity > 0)
score.sort()
```

- Assigning a filtered Python list to `score.tracks` keeps the global tempos, signatures and markers. The tracks are shared with the list you assign, not copied.
- `notes.filter` and `score.sort` work in place. `filter` calls the Python function once per note; for large lists, a mask over `notes.numpy()` passed to `Note.from_numpy` avoids that cost.
- `score.empty()` is false for a score that has tempo events but no notes; check `score.note_num() == 0` to skip files without notes.

## Segmenting

Cut a score into windows of a fixed number of bars with the downbeats and `clip`, then move each window to time 0:

```python
from symusic import Score

score = Score("song.mid", ttype="quarter")
downbeats = score.get_downbeats()
bars = 4
segments = []
for i in range(0, len(downbeats) - bars, bars):
    start, end = downbeats[i], downbeats[i + bars]
    segment = score.clip(start, end, clip_end=False)
    segment.shift_time(-start, inplace=True)
    segments.append(segment)
print(len(segments), segments[0])
```

`clip` keeps notes that start inside the window, so a note can extend past `end`; pass `clip_end=True` to drop those, or use `Score.trim` or `Track.trim` to cut them at the boundary. The tempo and signatures in effect at `start` are kept. See [data_model.md](data_model.md#transformations).

## Augmenting

```python
import random

from symusic import Score

score = Score("song.mid")
pitches = [n.pitch for t in score.tracks if not t.is_drum for n in t.notes]
low, high = min(pitches), max(pitches)
offset = random.randint(max(-6, -low), min(5, 127 - high))
transposed = score.copy()
for track in transposed.tracks:
    if not track.is_drum:
        track.shift_pitch(offset, inplace=True)
```

- Transpose pitched tracks only: drum tracks use pitch to choose the instrument.
- `shift_pitch` does not clamp; keep the offset within the range the notes allow, as above. A result outside −128..127 raises `ValueError`, and values above 127 are not valid MIDI.
- For tempo augmentation, scale the tempo events of a tick score, `tempo.qpm *= factor`, which changes the score's length in seconds without changing its notes.

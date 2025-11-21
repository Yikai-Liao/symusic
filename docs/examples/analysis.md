---
orphan: true
---

# Music Analysis Examples

This page contains code snippets demonstrating how to use Symusic for music analysis tasks.

## Calculate Pitch Histogram

```python
from symusic import Score
import numpy as np
import matplotlib.pyplot as plt

score = Score("path/to/file.mid")

pitches = []
for track in score.tracks:
    # Optional: Filter for non-drum tracks
    if not track.is_drum:
        for note in track.notes:
            pitches.append(note.pitch)

if pitches:
    plt.figure()
    plt.hist(pitches, bins=np.arange(129) - 0.5, density=True)
    plt.title("Pitch Distribution")
    plt.xlabel("MIDI Pitch")
    plt.ylabel("Probability")
    plt.xlim([-0.5, 127.5])
    plt.show()
else:
    print("No notes found.")
```

## Calculate Note Density

```python
from symusic import Score

score = Score("path/to/file.mid", ttype="second") # Load in seconds

duration = score.end()
num_notes = score.note_num()

if duration > 0:
    note_density = num_notes / duration
    print(f"Note Density: {note_density:.2f} notes per second")
else:
    print("Score has zero duration or no notes.")
```

## Get Average Tempo

```python
from symusic import Score

score = Score("path/to/file.mid")

if score.tempos:
    # Simple average (might not be musically accurate if tempos change significantly)
    avg_tempo = sum(t.qpm for t in score.tempos) / len(score.tempos)
    print(f"Average Tempo (unweighted): {avg_tempo:.1f} BPM")

    # Time-weighted average (more accurate)
    score_sec = score.to("second")
    total_duration = score_sec.end()
    weighted_tempo_sum = 0
    last_time = 0
    for i, tempo in enumerate(score_sec.tempos):
        current_time = tempo.time
        duration_at_prev_tempo = current_time - last_time
        if i > 0:
             weighted_tempo_sum += score_sec.tempos[i-1].qpm * duration_at_prev_tempo
        last_time = current_time
    # Add contribution of last tempo
    if score_sec.tempos:
        weighted_tempo_sum += score_sec.tempos[-1].qpm * (total_duration - last_time)

    if total_duration > 0:
        weighted_avg_tempo = weighted_tempo_sum / total_duration
        print(f"Average Tempo (time-weighted): {weighted_avg_tempo:.1f} BPM")
else:
    print("No tempo information found.")
```

*(More analysis examples will be added here.)*

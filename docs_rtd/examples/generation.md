---
orphan: true
---

# Music Generation Examples

While Symusic primarily focuses on processing *existing* symbolic music data, its data structures can be used to represent and manipulate *generated* music before saving it to MIDI or other formats.

## Creating a Score Programmatically

```python
from symusic import Score, Track, Note, Tempo, TimeSignature

# Create an empty score with standard ticks per quarter
score = Score(tpq=480)

# Add tempo and time signature
score.tempos.append(Tempo(time=0, qpm=120))
score.time_signatures.append(TimeSignature(time=0, numerator=4, denominator=4))

# Create a piano track
piano_track = score.tracks.append_track(name="Generated Piano", program=0)

# Generate a simple C major scale
pitches = [60, 62, 64, 65, 67, 69, 71, 72] # MIDI pitches for C4 to C5
current_time = 0
quarter_note_duration = 480 # Since tpq=480

for pitch in pitches:
    note = Note(
        time=current_time,
        duration=quarter_note_duration,
        pitch=pitch,
        velocity=70
    )
    piano_track.notes.append(note)
    current_time += quarter_note_duration

# Save the generated score
score.dump_midi("generated_scale.mid")

print("Generated C major scale saved to generated_scale.mid")
```

## Manipulating Generated Structures

You can use Symusic's modification methods on programmatically created scores.

```python
# Continuing from the previous example...

# Transpose the generated scale up by a perfect fifth (7 semitones)
transposed_score = score.shift_pitch(7)
transposed_score.dump_midi("generated_g_major_scale.mid")

# Create a version with double tempo
double_tempo_score = score.copy() # Create a copy first
for tempo in double_tempo_score.tempos:
    tempo.qpm *= 2.0
double_tempo_score.dump_midi("generated_scale_double_tempo.mid")

# Create a version in quarter note time, shift rhythmically
score_quarter = score.to("quarter")
# Shift every other note later by an eighth note
for i, note in enumerate(score_quarter.tracks[0].notes):
    if i % 2 == 1:
        note.shift_time_inplace(0.5) # 0.5 quarter notes = eighth note
score_quarter.dump_midi("generated_scale_syncopated.mid")
```

## Integrating with Generation Algorithms

Symusic can serve as the output stage for music generation algorithms (e.g., from neural networks, rule-based systems).

```python
# Assume 'generated_events' is a list produced by your algorithm
# Each element could be like: ("note", time, duration, pitch, velocity)
# or ("tempo", time, qpm), etc.

def create_score_from_events(generated_events, tpq=480):
    score = Score(tpq=tpq)
    # Assume single track for simplicity
    track = score.tracks.append_track(name="Generated Music")

    for event_data in generated_events:
        event_type = event_data[0]
        if event_type == "note":
            _, time, duration, pitch, velocity = event_data
            track.notes.append(Note(time, duration, pitch, velocity))
        elif event_type == "tempo":
            _, time, qpm = event_data
            score.tempos.append(Tempo(time, qpm))
        # ... handle other event types ...

    # It's good practice to sort events after adding them
    score.sort()
    return score

# Example generated events (simple melody)
generation_output = [
    ("tempo", 0, 100),
    ("note", 0, 480, 60, 80),
    ("note", 480, 480, 62, 80),
    ("note", 960, 960, 64, 80),
    ("tempo", 1920, 80),
    ("note", 1920, 480, 62, 70),
    ("note", 2400, 480, 60, 70),
]

generated_score = create_score_from_events(generation_output)
generated_score.dump_midi("output_from_algorithm.mid")
```

*(More generation-related examples will be added here.)*

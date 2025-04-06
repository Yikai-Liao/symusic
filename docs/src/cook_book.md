# Cook Book

This page contains concise code snippets for common tasks using Symusic. If you're looking for more detailed explanations, please refer to the Tutorials and API Reference sections.

## File Operations

### Loading a MIDI file

```python
from symusic import Score

# Load with default tick time unit
score = Score("path/to/file.mid")

# Load with quarter note time unit
score = Score("path/to/file.mid", ttype="quarter")

# Load with second time unit
score = Score("path/to/file.mid", ttype="second")
```

### Saving a MIDI file

```python
# Save to MIDI format
score.dump_midi("output.mid")

# Get MIDI bytes without saving to file
midi_bytes = score.dumps_midi()
```

### Loading and saving ABC notation

```python
# Load ABC notation
score = Score("path/to/file.abc", fmt="abc")

# Save to ABC notation
score.dump_abc("output.abc")
```

## Time Unit Conversion

```python
# Convert from ticks to quarter notes
score_quarter = score.to("quarter")

# Convert from any unit to seconds
score_second = score.to("second")

# Convert back to ticks with minimum duration
score_tick = score_second.to("tick", min_dur=1)
```

## Basic Manipulations

### Transposition

```python
# Transpose up by 2 semitones (non-inplace)
transposed_score = score.shift_pitch(2)

# Transpose down by 3 semitones (inplace)
score.shift_pitch_inplace(-3)
```

### Time Shifting

```python
# Shift all events forward by 480 ticks (non-inplace)
shifted_score = score.shift_time(480)

# Shift all events backward by 1 quarter note (inplace)
score_quarter.shift_time_inplace(-1.0)
```

### Velocity Adjustment

```python
# Increase velocity by 10 (non-inplace)
louder_score = score.shift_velocity(10)

# Decrease velocity by 20 (inplace)
score.shift_velocity_inplace(-20)
```

### Clipping

```python
# Extract a section from the score (non-inplace)
excerpt = score.clip(start=960, end=1920)

# Extract a section and also clip notes that extend beyond the end time
excerpt = score.clip(start=960, end=1920, clip_end=True)
```

## Track Operations

### Creating a new track

```python
# Create an empty score
score = Score(tpq=480)

# Add a piano track
piano_track = score.tracks.append_track(name="Piano", program=0, is_drum=False)

# Add a drum track
drum_track = score.tracks.append_track(name="Drums", program=0, is_drum=True)
```

### Adding notes to a track

```python
from symusic import Note

# Add individual notes
piano_track.notes.append(Note(time=0, duration=480, pitch=60, velocity=64))
piano_track.notes.append(Note(time=480, duration=480, pitch=64, velocity=64))
piano_track.notes.append(Note(time=960, duration=480, pitch=67, velocity=64))

# Add multiple notes using NumPy arrays
import numpy as np
times = np.array([0, 480, 960])
durations = np.array([480, 480, 480])
pitches = np.array([48, 55, 52])
velocities = np.array([64, 64, 64])
bass_notes = Note.from_numpy(times, durations, pitches, velocities, "tick")
for note in bass_notes:
    piano_track.notes.append(note)
```

### Working with specific tracks

```python
# Get the first track
first_track = score.tracks[0]

# Iterate through all tracks
for track in score.tracks:
    print(f"Track: {track.name}, Notes: {track.note_num()}")

# Extract a specific instrument
for track in score.tracks:
    if track.program == 0 and not track.is_drum:  # Piano
        piano_track = track
    elif track.is_drum:  # Drums
        drum_track = track
```

## Global Events

### Adding tempo changes

```python
from symusic import Tempo

# Set initial tempo (120 BPM)
score.tempos.append(Tempo(time=0, qpm=120.0))

# Add a tempo change
score.tempos.append(Tempo(time=1920, qpm=100.0))
```

### Adding time signatures

```python
from symusic import TimeSignature

# Set 4/4 time signature
score.time_signatures.append(TimeSignature(time=0, numerator=4, denominator=4))

# Change to 3/4 at measure 5 (assuming 4/4 with 1920 ticks per measure)
score.time_signatures.append(TimeSignature(time=1920*4, numerator=3, denominator=4))
```

### Adding key signatures

```python
from symusic import KeySignature

# Set C major key signature (no sharps/flats)
score.key_signatures.append(KeySignature(time=0, key=0, tonality=0))

# Change to G major (1 sharp) at measure 9
score.key_signatures.append(KeySignature(time=1920*8, key=1, tonality=0))
```

## Piano Roll Conversion

```python
# Resample to reduce size before creating piano roll
score_resampled = score.resample(tpq=6, min_dur=1)

# Create piano roll for the entire score
pianoroll = score_resampled.pianoroll(
    modes=["onset", "frame"],  # Only include onset and frame information
    pitch_range=[21, 109],     # Piano range
    encode_velocity=True       # Use velocity values instead of binary
)

# Create piano roll for a single track
track_pianoroll = score_resampled.tracks[0].pianoroll(
    modes=["frame"],           # Only include frame information
    pitch_range=[0, 128],      # Full MIDI range
    encode_velocity=False      # Binary encoding (0 or 1)
)
```

## Audio Synthesis

```python
from symusic import Score, Synthesizer, BuiltInSF3, dump_wav

# Load a score
score = Score("input.mid")

# Create a synthesizer using a built-in SoundFont
synthesizer = Synthesizer(
    sf_path=BuiltInSF3.MuseScoreGeneral().path(download=True),
    sample_rate=44100,
    quality=4
)

# Render the score to audio
audio_data = synthesizer.render(score, stereo=True)

# Save the audio to a WAV file
dump_wav("output.wav", audio_data)
```

## Multiprocessing

```python
import multiprocessing as mp
import pickle
from symusic import Score

def process_file(file_path):
    # Load MIDI file
    score = Score(file_path)

    # Process the score (example: transpose up by 2 semitones)
    processed_score = score.shift_pitch(2)

    # Return or save the result
    return processed_score

if __name__ == "__main__":
    # List of MIDI files to process
    file_paths = ["file1.mid", "file2.mid", "file3.mid"]

    # Process files in parallel
    with mp.Pool(processes=4) as pool:
        results = pool.map(process_file, file_paths)

    # Save the results
    for i, processed_score in enumerate(results):
        processed_score.dump_midi(f"processed_{i}.mid")
```

## Analysis

```python
# Get basic score information
print(f"Tracks: {score.track_num()}")
print(f"Notes: {score.note_num()}")
print(f"Duration: {score.end() - score.start()}")

# Analyze pitch distribution
pitches = []
for track in score.tracks:
    for note in track.notes:
        pitches.append(note.pitch)

import numpy as np
pitch_histogram = np.histogram(pitches, bins=128, range=(0, 127))

# Find average velocity
velocities = []
for track in score.tracks:
    for note in track.notes:
        velocities.append(note.velocity)

avg_velocity = np.mean(velocities)
print(f"Average velocity: {avg_velocity:.1f}")
```

## Working with Specific Events

### Control Changes

```python
from symusic import ControlChange

# Add volume control (controller 7)
track.controls.append(ControlChange(time=0, number=7, value=100))

# Add expression control (controller 11)
track.controls.append(ControlChange(time=0, number=11, value=127))

# Add sustain pedal events (controller 64)
track.controls.append(ControlChange(time=0, number=64, value=127))
track.controls.append(ControlChange(time=960, number=64, value=0))
```

### Pitch Bends

```python
from symusic import PitchBend

# Add pitch bend events
track.pitch_bends.append(PitchBend(time=0, value=0))       # Center
track.pitch_bends.append(PitchBend(time=240, value=4096))  # Bend up
track.pitch_bends.append(PitchBend(time=480, value=0))     # Center again
```

### Pedal Events

```python
from symusic import Pedal

# Add sustain pedal with duration
track.pedals.append(Pedal(time=0, duration=960))
```

### Lyrics

```python
from symusic import TextMeta

# Add lyrics to a vocal track
track.lyrics.append(TextMeta(time=0, text="Hel-"))
track.lyrics.append(TextMeta(time=240, text="lo"))
track.lyrics.append(TextMeta(time=480, text="World!"))
```

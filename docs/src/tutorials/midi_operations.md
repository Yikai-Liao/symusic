# MIDI File Operations Tutorial

This tutorial covers the basics of loading, inspecting, modifying, and saving MIDI files using Symusic.

## Loading MIDI Files

The primary way to load a MIDI file is using the `Score` constructor or the `Score.from_file` class method.

```python
from symusic import Score, TimeUnit

# Load a MIDI file (defaults to 'tick' time unit)
score_tick = Score("path/to/your/midi_file.mid")

# Check the time unit and ticks per quarter note
print(f"Time unit: {score_tick.ttype}")
print(f"Ticks per quarter: {score_tick.ticks_per_quarter}")

# Load specifying a different time unit
score_quarter = Score("path/to/your/midi_file.mid", ttype="quarter")
print(f"Time unit: {score_quarter.ttype}")

score_second = Score.from_file("path/to/your/midi_file.mid", ttype=TimeUnit.second)
print(f"Time unit: {score_second.ttype}")
```

## Inspecting Score Contents

Once loaded, you can explore the `Score` object's contents.

```python
# Basic information
print(f"Number of tracks: {score_tick.track_num()}")
print(f"Total notes: {score_tick.note_num()}")
print(f"Score duration (ticks): {score_tick.end() - score_tick.start()}")

# Access tracks
print("\nTracks:")
for i, track in enumerate(score_tick.tracks):
    print(f"  Track {i}: Name='{track.name}', Program={track.program}, IsDrum={track.is_drum}, Notes={track.note_num()}")

# Access global events (e.g., tempos)
print("\nTempo Changes:")
for tempo in score_tick.tempos:
    print(f"  Time={tempo.time}, QPM={tempo.qpm}")

# Access notes in the first track
first_track = score_tick.tracks[0]
print("\nFirst 5 notes of Track 0:")
for note in first_track.notes[:5]:
    print(f"  Time={note.time}, Dur={note.duration}, Pitch={note.pitch}, Vel={note.velocity}")
```

## Modifying MIDI Data

Symusic provides methods for various modifications. Most methods have an `inplace` option.

### Transposition

```python
# Transpose the entire score up by 2 semitones (creates a new Score)
transposed_score = score_tick.shift_pitch(2)

# Transpose only the first track down by 1 semitone (inplace)
score_tick.tracks[0].shift_pitch_inplace(-1)
```

### Time Adjustment

```python
# Shift all events later by 960 ticks (inplace)
score_tick.shift_time_inplace(960)

# Clip the score to keep only events between time 1920 and 3840
clipped_score = score_tick.clip(1920, 3840)
```

### Changing Tempo

```python
from symusic import Tempo

# Change the tempo at the beginning
if score_tick.tempos:
    score_tick.tempos[0].qpm = 100.0 # Set first tempo to 100 BPM
else:
    # Add a tempo event if none exist
    score_tick.tempos.append(Tempo(time=0, qpm=100.0))

# Add a gradual tempo change (ritardando)
# Convert to seconds for easier tempo manipulation if needed
# score_second = score_tick.to("second")
# ... manipulate tempo in seconds ...
# score_tick = score_second.to("tick")
```

### Removing Tracks

```python
# Remove the last track (inplace)
if score_tick.tracks:
    score_tick.tracks.pop(-1)

# Remove drum tracks (creates a new list)
non_drum_tracks = [track for track in score_tick.tracks if not track.is_drum]
score_tick.tracks = non_drum_tracks # Replace the track list
```

## Saving MIDI Files

Save the modified (or original) `Score` object back to a MIDI file.

```python
# Save the score to a new MIDI file
# Note: Saving always uses the 'tick' representation internally.
# If the score is not in 'tick', it will be converted automatically.
transposed_score.dump_midi("transposed_output.mid")

# Save the clipped score
clipped_score.dump_midi("clipped_output.mid")
```

Symusic automatically handles the conversion to the tick-based format required for MIDI files during the `dump_midi` process.

## Batch Processing MIDI Files

Symusic's speed makes it ideal for processing large collections of MIDI files, often combined with Python's `multiprocessing`.

```python
import multiprocessing as mp
from pathlib import Path
from symusic import Score

def process_midi(midi_path: Path, output_dir: Path):
    try:
        score = Score(midi_path)
        # Example: Keep only piano tracks (program 0)
        piano_tracks = [t for t in score.tracks if t.program == 0 and not t.is_drum]
        if not piano_tracks:
            return # Skip if no piano tracks

        score.tracks = piano_tracks
        score.dump_midi(output_dir / midi_path.name)
        print(f"Processed: {midi_path.name}")
    except Exception as e:
        print(f"Error processing {midi_path.name}: {e}")

if __name__ == "__main__":
    input_directory = Path("path/to/midi/collection")
    output_directory = Path("path/to/output/dir")
    output_directory.mkdir(exist_ok=True)

    midi_files = list(input_directory.glob("*.mid"))

    # Create tuples of arguments for starmap
    args = [(mf, output_directory) for mf in midi_files]

    # Use multiprocessing pool
    with mp.Pool(processes=mp.cpu_count()) as pool:
        pool.starmap(process_midi, args)

    print("Batch processing complete.")
```

This tutorial provides a starting point for working with MIDI files in Symusic. Explore the `Score` and `Track` API documentation for more advanced methods and options.

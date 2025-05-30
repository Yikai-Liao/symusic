# Piano Roll Conversion

Piano rolls are a popular representation of symbolic music data, especially in music generation and analysis tasks. Symusic provides efficient tools to convert between MIDI and piano roll representations.

## What is a Piano Roll?

A piano roll is a matrix representation of music where:
- The rows represent different pitch values
- The columns represent time steps
- The values in the matrix represent note presence, velocity, or other properties

Piano rolls are often used in machine learning models for music generation and analysis because they're easy to process using neural networks and other algorithms.

## Basic Piano Roll Conversion

Symusic can convert a Score or Track object to a piano roll representation:

```python
from symusic import Score
import matplotlib.pyplot as plt

# Load a MIDI file
score = Score("path/to/file.mid")

# Resample to reduce size (recommended before piano roll conversion)
# This makes the time grid more manageable
# tpq=6 means 6 ticks per quarter note (1/24 note resolution)
# min_dur=1 ensures notes are at least 1 time unit long
score_resampled = score.resample(tpq=6, min_dur=1)

# Convert the entire score to a piano roll
# This creates a 4D array: [modes, tracks, pitch, time]
pianoroll = score_resampled.pianoroll(
    modes=["onset", "frame"],  # Type of information to include
    pitch_range=[0, 128],      # Range of MIDI pitches (default: all 128)
    encode_velocity=True       # If True, use velocity values; if False, use binary (0/1)
)

# Visualize the piano roll (combined onset and frame for the first track)
plt.figure(figsize=(12, 6))
plt.imshow(pianoroll[0, 0] + pianoroll[1, 0], origin='lower', aspect='auto',
           extent=[0, pianoroll.shape[3], 0, 128])
plt.title('Piano Roll (Track 0)')
plt.xlabel('Time (in ticks)')
plt.ylabel('Pitch (MIDI note number)')
plt.colorbar(label='Velocity')
plt.show()
```

## Piano Roll Modes

Symusic supports different piano roll modes that capture different aspects of musical notes:

- **onset**: Only the beginning (attack) of each note is marked
- **frame**: The entire duration of each note is marked
- **offset**: Only the end (release) of each note is marked

You can specify one or more modes when creating a piano roll:

```python
# Create a piano roll with all three modes
pianoroll = score_resampled.pianoroll(
    modes=["onset", "frame", "offset"],
    pitch_range=[0, 128],
    encode_velocity=True
)

# Access each mode separately
onset_roll = pianoroll[0]   # Shape: [tracks, pitch, time]
frame_roll = pianoroll[1]   # Shape: [tracks, pitch, time]
offset_roll = pianoroll[2]  # Shape: [tracks, pitch, time]
```

## Single Track Piano Roll

You can also create a piano roll for a single track:

```python
from symusic import Score
import matplotlib.pyplot as plt

# Load a MIDI file
score = Score("path/to/file.mid")

# Resample for better processing
score_resampled = score.resample(tpq=6, min_dur=1)

# Select a track
track = score_resampled.tracks[0]  # First track

# Convert track to piano roll (creates a 3D array: [modes, pitch, time])
track_pianoroll = track.pianoroll(
    modes=["onset", "frame", "offset"],
    pitch_range=[0, 128],
    encode_velocity=True
)

# The shape is different from score.pianoroll() - no track dimension
print(f"Track piano roll shape: {track_pianoroll.shape}")  # [3, 128, time_steps]

# Visualize the track's piano roll
plt.figure(figsize=(12, 6))
plt.imshow(track_pianoroll[1], origin='lower', aspect='auto',  # Show only the frame mode
           extent=[0, track_pianoroll.shape[2], 0, 128])
plt.title(f'Piano Roll - {track.name}')
plt.xlabel('Time (in ticks)')
plt.ylabel('Pitch (MIDI note number)')
plt.colorbar(label='Velocity')
plt.show()
```

## Customizing Piano Roll Range

You can focus on a specific pitch range by specifying the `pitch_range` parameter:

```python
# Create a piano roll for just the piano range (21-108)
piano_range_roll = track.pianoroll(
    modes=["onset", "frame"],
    pitch_range=[21, 109],  # MIDI notes 21-108 (standard piano)
    encode_velocity=True
)

# Create a piano roll for just the bass range
bass_range_roll = track.pianoroll(
    modes=["frame"],
    pitch_range=[24, 60],  # Low range
    encode_velocity=False  # Binary (simpler visualization)
)
```

## Binary vs. Velocity Encoding

The `encode_velocity` parameter controls whether the piano roll contains velocity information:

```python
# Velocity-encoded piano roll (values are actual velocities: 0-127)
velocity_roll = track.pianoroll(
    modes=["frame"],
    encode_velocity=True
)

# Binary piano roll (values are just 0 or 1)
binary_roll = track.pianoroll(
    modes=["frame"],
    encode_velocity=False
)
```

Binary encoding is often simpler for visualization and certain types of processing, while velocity encoding preserves more musical information.

## Working with Piano Roll Data

Once you've created a piano roll, you can process it using NumPy operations:

```python
import numpy as np
from symusic import Score

# Load and convert to piano roll
score = Score("path/to/file.mid").resample(tpq=4, min_dur=1)
pianoroll = score.pianoroll(modes=["frame"], encode_velocity=True)

# If working with a score piano roll, access a specific track
track_roll = pianoroll[0, 0]  # Mode 0 (frame), track 0

# Basic statistics
active_notes = np.count_nonzero(track_roll)
total_cells = track_roll.size
activity = active_notes / total_cells
print(f"Note activity: {activity:.2%}")

# Find the highest and lowest pitches used
active_pitches = np.any(track_roll > 0, axis=1)
lowest_pitch = np.argmax(active_pitches)
highest_pitch = len(active_pitches) - np.argmax(active_pitches[::-1]) - 1
print(f"Pitch range: {lowest_pitch} to {highest_pitch}")

# Find the average velocity of active notes
avg_velocity = np.mean(track_roll[track_roll > 0])
print(f"Average velocity: {avg_velocity:.1f}")

# Create a "piano roll profile" (how often each pitch is used)
pitch_profile = np.sum(track_roll > 0, axis=1)
```

## Advanced Visualization

You can create more advanced visualizations of piano rolls:

```python
import matplotlib.pyplot as plt
import matplotlib.colors as colors
import numpy as np
from symusic import Score

# Load and prepare data
score = Score("path/to/file.mid").resample(tpq=6, min_dur=1)
track = score.tracks[0]
pianoroll = track.pianoroll(modes=["onset", "frame"], pitch_range=[21, 109], encode_velocity=True)

# Create a custom colormap for better visualization
cmap = plt.cm.viridis
norm = colors.Normalize(vmin=0, vmax=127)

# Create a figure with multiple subplots
fig, axs = plt.subplots(3, 1, figsize=(12, 10), sharex=True)

# Plot onset roll
onset = pianoroll[0]
im0 = axs[0].imshow(onset, origin='lower', aspect='auto', cmap=cmap, norm=norm)
axs[0].set_title('Note Onsets')
axs[0].set_ylabel('Pitch')
fig.colorbar(im0, ax=axs[0], label='Velocity')

# Plot frame roll
frame = pianoroll[1]
im1 = axs[1].imshow(frame, origin='lower', aspect='auto', cmap=cmap, norm=norm)
axs[1].set_title('Note Frames')
axs[1].set_ylabel('Pitch')
fig.colorbar(im1, ax=axs[1], label='Velocity')

# Plot combined view (onset + frame)
combined = np.maximum(onset * 0.7, frame)  # Emphasize onsets
im2 = axs[2].imshow(combined, origin='lower', aspect='auto', cmap=cmap, norm=norm)
axs[2].set_title('Combined View')
axs[2].set_ylabel('Pitch')
axs[2].set_xlabel('Time (ticks)')
fig.colorbar(im2, ax=axs[2], label='Velocity')

# Adjust layout and display
plt.tight_layout()
plt.show()
```

## Tips for Piano Roll Processing

### Resampling for Efficient Processing

Always resample your score before converting to piano roll to avoid generating excessively large matrices:

```python
# Bad: Direct conversion without resampling could create a huge matrix
# pianoroll = score.pianoroll(modes=["frame"])

# Good: Resample first to reduce the time resolution
score_resampled = score.resample(tpq=4, min_dur=1)  # 16th note resolution
pianoroll = score_resampled.pianoroll(modes=["frame"])
```

### Choosing the Right Resolution

The `tpq` (ticks per quarter note) parameter in `resample()` controls the time resolution:

- Lower values (e.g., 2-4) create smaller matrices but lose some timing precision
- Higher values (e.g., 8-12) preserve more detail but create larger matrices
- For most applications, values between 4-8 work well (1/16 to 1/32 note resolution)

### Memory Management

Piano rolls can be memory-intensive for long pieces or high resolutions. Consider:

1. Processing shorter segments if memory is an issue
2. Using lower `tpq` values for initial exploration
3. Limiting the pitch range to the most relevant notes
4. Using binary encoding (`encode_velocity=False`) to save memory

```python
# Memory-efficient piano roll for a long piece
efficient_roll = score.resample(tpq=4, min_dur=1).pianoroll(
    modes=["frame"],
    pitch_range=[36, 96],     # Focus on middle pitch range
    encode_velocity=False     # Binary encoding
)
```

## Converting Back to Notes

Currently, Symusic doesn't provide built-in functionality to convert piano rolls back to Score or Track objects. This is because the conversion is complex and can be ambiguous, especially when dealing with overlapping notes.

If you need to convert piano roll data back to MIDI, consider using external libraries like `pretty_midi` or implementing custom algorithms based on your specific use case.

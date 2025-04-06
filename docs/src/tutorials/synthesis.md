# Music Synthesis

Symusic provides a built-in synthesizer that can render MIDI files to audio using SoundFont (SF2/SF3) technology. The synthesizer is based on the [prestosynth](https://github.com/lzqlzzq/prestosynth) library and offers a simple yet powerful interface for audio rendering.

## Basic Synthesis

The simplest way to synthesize a score is to use the `Synthesizer` class:

```python
from symusic import Score, Synthesizer, BuiltInSF3, dump_wav

# Load a MIDI file
score = Score("path/to/file.mid")

# Create a synthesizer with default settings
synthesizer = Synthesizer(
    sf_path=BuiltInSF3.MuseScoreGeneral().path(download=True),
    sample_rate=44100,
    quality=4 # Default quality setting
)

# Render the score to audio (returns an AudioData object)
audio_data = synthesizer.render(score, stereo=True)

# Save the audio to a WAV file
dump_wav("output.wav", audio_data)
```

## Built-in SoundFonts

Symusic provides several built-in SoundFonts that you can use without having to find and download your own. These are accessed through the `BuiltInSF3` class:

```python
from symusic import BuiltInSF3

# Available built-in SoundFonts
sf_path = BuiltInSF3.MuseScoreGeneral().path(download=True)  # MuseScore General (default)
sf_path = BuiltInSF3.FluidR3Mono().path(download=True)       # FluidR3 Mono
sf_path = BuiltInSF3.FluidR3().path(download=True)           # FluidR3 GM
```

The `path()` method will return the path to the SoundFont file, downloading it if necessary when the `download` parameter is set to `True`.

## Synthesizer Parameters

The `Synthesizer` constructor takes several parameters that control the quality and characteristics of the audio output:

```python
synthesizer = Synthesizer(
    sf_path,            # Path to the SoundFont file (string or Path)
    sample_rate,        # Sample rate in Hz (default: 44100)
    quality             # Synthesis quality (0-6, default: 0 in factory, 4 often good)
)
```

## Working with Audio Data

The `render` method returns an `AudioData` object, which contains the synthesized audio data. You can manipulate this data directly or save it to a WAV file:

```python
# Render a score
audio_data = synthesizer.render(score, stereo=True)

# Get audio properties
sample_rate = audio_data.sample_rate
num_channels = audio_data.channels
num_frames = audio_data.frames

# Access the raw audio data as a NumPy array
import numpy as np
np_array = np.array(audio_data)  # Shape: [frames, channels]

# Manipulate the audio (example: amplify by 1.5)
np_array *= 1.5

# Save to a WAV file
dump_wav("output.wav", audio_data)
```

## Time Unit Considerations

The synthesizer works most efficiently with scores in `Second` time unit. When you pass a score in `Tick` or `Quarter` time unit, it will be automatically converted to `Second` internally:

```python
# Loading a score in any time unit works
score_tick = Score("file.mid", ttype="tick")
score_quarter = Score("file.mid", ttype="quarter")
score_second = Score("file.mid", ttype="second")

# All of these will produce the same audio
audio1 = synthesizer.render(score_tick, stereo=True)
audio2 = synthesizer.render(score_quarter, stereo=True)
audio3 = synthesizer.render(score_second, stereo=True)
```

## Custom Synthesis Workflow

For more control over the synthesis process, you might want to modify the score before rendering:

```python
from symusic import Score, Synthesizer, BuiltInSF3, dump_wav

# Load a score
score = Score("input.mid")

# Modify the score (example: transpose up by 2 semitones)
transposed_score = score.shift_pitch(2)

# Modify tempo (example: increase tempo by 20%)
for tempo in transposed_score.tempos:
    tempo.qpm *= 1.2

# Create a synthesizer
synthesizer = Synthesizer(
    sf_path=BuiltInSF3.MuseScoreGeneral().path(download=True),
    sample_rate=48000,
)

# Render to audio
audio_data = synthesizer.render(transposed_score, stereo=True)

# Save to WAV
dump_wav("output.wav", audio_data)
```

## Handling Multiple Scores

You can reuse the same synthesizer instance to render multiple scores:

```python
from symusic import Score, Synthesizer, BuiltInSF3, dump_wav

# Create a synthesizer once
synthesizer = Synthesizer(
    sf_path=BuiltInSF3.MuseScoreGeneral().path(download=True),
    sample_rate=44100,
)

# Process multiple files
for i in range(1, 4):
    # Load score
    score = Score(f"input{i}.mid")
    
    # Render to audio
    audio = synthesizer.render(score, stereo=True)
    
    # Save to WAV
    dump_wav(f"output{i}.wav", audio)
```

## Advanced: Audio Postprocessing

You can perform post-processing on the audio data using NumPy or other audio processing libraries:

```python
import numpy as np
from scipy import signal
from symusic import Score, Synthesizer, BuiltInSF3, dump_wav

# Load and render
score = Score("input.mid")
synthesizer = Synthesizer(
    sf_path=BuiltInSF3.MuseScoreGeneral().path(download=True),
    sample_rate=44100,
)
audio_data = synthesizer.render(score, stereo=True)

# Convert to NumPy array
audio_array = np.array(audio_data)  # Shape: [frames, channels]

# Example: Apply a low-pass filter (cutoff at 4000 Hz)
b, a = signal.butter(4, 4000 / (audio_data.sample_rate / 2), 'low')
filtered_audio = signal.filtfilt(b, a, audio_array, axis=0)

# Example: Normalize the audio
max_val = np.max(np.abs(filtered_audio))
if max_val > 0:
    normalized_audio = filtered_audio / max_val

# Convert back to AudioData (implementation depends on your use case)
# This is a simplified example - you'd need to create a new AudioData object
# with the processed samples

# Save the processed audio (assuming you have a way to convert back to AudioData)
dump_wav("processed_output.wav", audio_data)
```

## Performance Considerations

Synthesis can be CPU-intensive, especially with high-quality settings. Here are some tips for optimizing performance:

1. Use a lower `quality` setting for real-time or batch processing
2. Use a lower `sample_rate` if high fidelity isn't required
3. Consider rendering shorter segments of music if working with very long pieces
4. For batch processing of many files, consider using multiprocessing

## Troubleshooting

### Common Issues

1. **SoundFont not found**: Make sure the SoundFont path is correct and the file exists
   ```python
   # Check if SoundFont exists before using it
   import os
   sf_path = BuiltInSF3.MuseScoreGeneral().path(download=True)
   assert os.path.exists(sf_path), f"SoundFont not found at {sf_path}"
   ```

2. **Audio quality issues**: Try increasing the quality parameter
   ```python
   synthesizer = Synthesizer(sf_path, sample_rate=44100, quality=5)  # Higher quality
   ```

3. **Memory errors**: For very long or complex MIDI files, you might run into memory issues
   ```python
   # Process the score in smaller chunks (example implementation)
   def process_in_chunks(score, chunk_size=60):  # 60 seconds per chunk
       result_chunks = []
       total_duration = score.end()
       for start in range(0, int(total_duration), chunk_size):
           end = min(start + chunk_size, total_duration)
           chunk = score.clip(start, end)
           chunk_audio = synthesizer.render(chunk, stereo=True)
           result_chunks.append(np.array(chunk_audio))
       
       # Combine chunks (simplified example)
       return np.concatenate(result_chunks, axis=0)
   ```

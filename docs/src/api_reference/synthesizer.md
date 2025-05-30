# Synthesizer

The `Synthesizer` class provides functionality to render a `Score` object into audio data using SoundFont (SF2/SF3) files.

## Initialization

```python
from symusic import Synthesizer
from pathlib import Path

# Initialize with a SoundFont path
synthesizer = Synthesizer(
    sf_path: str | Path,      # Path to the .sf2 or .sf3 file
    sample_rate: int = 44100, # Desired sample rate in Hz
    quality: int = 0          # Synthesis quality (0-6, higher means better quality but slower)
)
```

- **sf_path**: The path to the SoundFont file. You can use the built-in SoundFonts provided by `symusic.BuiltInSF2` or `symusic.BuiltInSF3`, or provide a path to your own file.
- **sample_rate**: The sample rate for the output audio. Defaults to 44100 Hz.
- **quality**: Controls the trade-off between synthesis quality and performance. Higher values (up to 6) improve quality but increase computation time. The default is 0 (lowest quality).

## Methods

### `render(score, stereo=True)`

Synthesizes the given `Score` object into audio.

```python
from symusic import Score, AudioData

# Assume synthesizer and score are already initialized
audio_data: AudioData = synthesizer.render(
    score: Score,      # The Score object to synthesize
    stereo: bool = True # Whether to render in stereo (True) or mono (False)
)
```

- **score**: The `Score` object to be rendered. It can be a `ScoreTick`, `ScoreQuarter`, or `ScoreSecond` object. The synthesizer will internally convert it to `ScoreSecond` if necessary.
- **stereo**: If `True`, the output audio will have two channels. If `False`, it will be mono. Defaults to `True`.

**Returns**: An `AudioData` object containing the rendered audio samples and metadata.

## `AudioData` Object

The `render` method returns an `AudioData` object with the following properties:

- **`sample_rate`**: (int) The sample rate of the audio data.
- **`channels`**: (int) The number of audio channels (1 for mono, 2 for stereo).
- **`frames`**: (int) The number of audio frames (samples per channel).
- **`__array__()`**: Allows direct conversion to a NumPy array (`np.array(audio_data)`). The resulting array has the shape `(frames, channels)`.

## Utility Function: `dump_wav`

Symusic provides a helper function to easily save `AudioData` to a WAV file.

```python
from symusic import dump_wav

# Assume audio_data is an AudioData object obtained from render()
dump_wav(
    path: str | Path,    # Output file path
    data: AudioData,      # The AudioData object to save
    use_int16: bool = True # If True, save as 16-bit integer WAV (smaller file),
                          # otherwise save as 32-bit float WAV (higher precision).
)
```

## Example Usage

```python
from symusic import Score, Synthesizer, BuiltInSF3, dump_wav

# 1. Load a score
score = Score("path/to/your/midi.mid")

# 2. Initialize the synthesizer with a built-in SoundFont
sf_path = BuiltInSF3.MuseScoreGeneral().path(download=True)
synthesizer = Synthesizer(sf_path=sf_path, sample_rate=44100, quality=4)

# 3. Render the score to audio
audio_data = synthesizer.render(score, stereo=True)

# 4. Save the audio to a WAV file
dump_wav("output_audio.wav", audio_data)

# 5. (Optional) Access audio data as a NumPy array
import numpy as np
audio_array = np.array(audio_data)
print(f"Audio shape: {audio_array.shape}")
print(f"Sample rate: {audio_data.sample_rate}")
```

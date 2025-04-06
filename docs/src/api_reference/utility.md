# Utility Functions API

Symusic provides a few standalone utility functions for convenience.

## `dump_wav`

Saves an `AudioData` object (typically obtained from `Synthesizer.render()`) to a WAV file.

**Signature:**

```python
from symusic import dump_wav, AudioData
from pathlib import Path

dump_wav(
    path: str | Path,       # Path where the WAV file will be saved.
    data: AudioData,         # The AudioData object containing audio samples.
    use_int16: bool = True   # Format flag. If True, saves as 16-bit integer WAV.
                             # If False, saves as 32-bit float WAV.
) -> None:
```

**Parameters:**

-   `path`: The file path (as a string or `pathlib.Path`) for the output WAV file.
-   `data`: The `AudioData` object returned by the `Synthesizer.render()` method.
-   `use_int16`: Determines the output format. `True` (default) results in a smaller 16-bit integer WAV file, suitable for most playback purposes. `False` saves as a 32-bit floating-point WAV, preserving the full precision of the synthesizer's output but resulting in a larger file.

**Example:**

```python
from symusic import Score, Synthesizer, dump_wav, BuiltInSF3

score = Score("input.mid")
synth = Synthesizer(BuiltInSF3.MuseScoreGeneral().path())
audio = synth.render(score)

# Save as standard 16-bit WAV
dump_wav("output_int16.wav", audio)

# Save as 32-bit float WAV
dump_wav("output_float32.wav", audio, use_int16=False)
``` 
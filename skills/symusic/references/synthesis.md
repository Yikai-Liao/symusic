# Synthesis

`Synthesizer` renders a score to audio with an SF2 or SF3 SoundFont, using the bundled [prestosynth](https://github.com/lzqlzzq/prestosynth) engine; `dump_wav` writes the result to a WAV file.

<!-- docs-test: skip, downloads a SoundFont -->
```python
from symusic import Score, Synthesizer, dump_wav

score = Score("song.mid")
synth = Synthesizer(sample_rate=44100)  # downloads MuseScore General on first use
audio = synth.render(score, stereo=True)
print(audio.dtype, audio.shape)  # float32 (2, samples)
dump_wav("song.wav", audio, sample_rate=44100, use_int16=True)
```

## SoundFonts

`Synthesizer(sf_path=None, sample_rate=44100, quality=0)` loads the SoundFont at `sf_path`, a string or `pathlib.Path`. Without `sf_path` it uses MuseScore General (SF3, about 38 MB), which it downloads once into the user data directory and verifies by hash; a failed download raises `ConnectionError`.

`BuiltInSF3.MuseScoreGeneral()` and `BuiltInSF2.MuseScoreGeneral()` describe the two built-in fonts. `.path(download=True)` returns the local path and downloads the file if it is missing, so an offline machine can be prepared in advance:

<!-- docs-test: skip, downloads a SoundFont -->
```python
from symusic import BuiltInSF3, Synthesizer

sf_path = BuiltInSF3.MuseScoreGeneral().path(download=True)
synth = Synthesizer(sf_path, sample_rate=22050)
```

A synthesizer resamples each SoundFont sample the first time a note needs it and keeps the result, so reuse one synthesizer for many scores.

## What is rendered

- `render` accepts a score in any unit and converts it to seconds first.
- Each note plays the preset given by its track's `program` and bank. The bank comes from control change 0 (bank select) in `controls`, 0 by default; drum tracks always use bank 128, the General MIDI drum kit.
- Only note times, durations, pitches and velocities are rendered. Pedals, pitch bends and all other control changes, including volume (7), pan (10) and sustain (64), have no effect, so a sustained piano part sounds dry; extend the note durations yourself to hear the pedal.
- `quality` (0–4) selects how SoundFont samples are resampled to `sample_rate` when their rates differ: 0 is linear interpolation, 1 is zero-order hold, and 2, 3 and 4 are sinc interpolation from fastest to best. The sinc settings render several times slower than 0.

## Output

`render(score, stereo=True)` returns a `float32` NumPy array of shape `(channels, samples)`, with 2 channels in stereo and 1 in mono, at the synthesizer's sample rate. Samples are not normalized and can exceed ±1 when many notes sound together.

`dump_wav(path, data, sample_rate, use_int16=True)` writes such an array. `data` must be `float32` with shape `(channels, samples)`; `use_int16=True` writes 16-bit PCM and `False` writes 32-bit float samples. Scale the array first if its peak exceeds 1, because 16-bit output clips:

<!-- docs-test: skip, downloads a SoundFont -->
```python
import numpy as np
from symusic import Score, Synthesizer, dump_wav

synth = Synthesizer(sample_rate=44100)
audio = synth.render(Score("song.mid"))
peak = np.abs(audio).max()
if peak > 1:
    audio /= peak
dump_wav("normalized.wav", audio, sample_rate=44100)
```

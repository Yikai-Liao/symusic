# SoundFont synthesis

The symusic wheel includes the offline FluidSynth runtime in `symusic/_vendor/fluidsynth/`. Pass a local SF2 or SF3 SoundFont path to `Synthesizer` and a score in any time unit to `render`.

<!-- docs-test: skip, requires a local SoundFont -->
```python
from scipy.io.wavfile import write
from symusic import Score, Synthesizer

score = Score("song.mid")
audio = Synthesizer("piano.sf2", sample_rate=44100).render(score)
write("song.wav", 44100, audio)
```

## SoundFonts

`BuiltInSF2.musescore_general()` and `BuiltInSF3.musescore_general()` describe the curated MuseScore General SoundFonts. `path()` returns the local cache location. `verify()` checks the cached file against its SHA-384 digest. `download()` returns a verified local path, fetching the file when the cache has no valid copy.

<!-- docs-test: skip, downloads a SoundFont -->
```python
from symusic import BuiltInSF3, Synthesizer

font = BuiltInSF3.musescore_general()
synth = Synthesizer(font.download())
```

## What is rendered

`Synthesizer(soundfont, *, sample_rate=44100, polyphony=256, reverb=False, chorus=False, tail_seconds=1.0)` loads the SoundFont into a fresh FluidSynth instance on each render. The sample rate is in Hz and accepts 8000–96000. Polyphony limits simultaneous voices.

Each note-bearing track has its own channel, with a limit of 256. The track’s program defaults to bank 0 for melodic tracks or bank 128 for drum tracks. CC0 by itself selects a bank number. Once a track has sent CC32, its bank number is `CC0 * 128 + CC32`, with zero for a missing CC0. Control changes and pitch bends are applied in time order. CC124–127 release notes on the addressed track while preserving channel assignments. Use CC64 for sustain.

`render(score, *, stereo=True)` returns a C-contiguous `float32` NumPy array shaped `(frames, 2)` in stereo or `(frames, 1)` in mono. Mono is the average of the stereo channels. Samples use FluidSynth’s native gain. Each event is dispatched at the next internal block boundary, including its own frame when it falls on a boundary. The release tail follows the final event. Use `tail_seconds=0` to trim at the final boundary.

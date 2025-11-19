# Synthesis

Symusic can turn a `Score` into sound using SoundFont 2/3 data and the Prestosynth engine. This
section explains the moving pieces so you can audition scores, render datasets, or embed procedural
music into an application.

## Components

1. **SoundFont** – `.sf2`/`.sf3` file that defines instrument samples and playback rules. Use
   `BuiltInSF2/3` helpers to download curated fonts or provide your own path.
2. **Synthesizer** – Python/nanobind wrapper around Prestosynth. Constructed with `sf_path`,
   `sample_rate`, and an optional `quality` flag.
3. **Score** – Input data. Tracks contribute `program` and `is_drum` metadata so instruments map
   correctly.
4. **`render()`** – Converts the score to seconds (if necessary), schedules each event, and returns a
   NumPy array shaped `[channels, samples]`.
5. **`dump_wav()`** – Convenience helper to write the array to disk.

## Usage pattern

```python
from symusic import Score, Synthesizer, BuiltInSF3, dump_wav

score = Score("piece.mid")
sf_path = BuiltInSF3.MuseScoreGeneral().path(download=True)
synth = Synthesizer(sf_path=sf_path, sample_rate=48000, quality=1)
audio = synth.render(score, stereo=True)
dump_wav("piece.wav", audio, sample_rate=48000)
```

- Rendering always works in `second` units internally, so conversions are automatic.
- Percussion tracks (`is_drum=True`) trigger the GM drum kit region inside the SoundFont.
- `quality` toggles different interpolation / oversampling strategies inside Prestosynth.

## When to reach for synthesis

- Preview arrangements without leaving your notebook.
- Generate audio datasets aligned with symbolic annotations.
- Prototype differentiable or procedural music systems that need quick feedback loops.

See :doc:`../api/generated/symusic.Synthesizer` and :doc:`../api/generated/symusic.dump_wav` for all
arguments and return types.

---
name: symusic
description: Read, write, edit and analyze symbolic music with the symusic Python library (MIDI, ABC, NumPy arrays, piano rolls, SoundFont synthesis). Use when code imports symusic, or when a task loads or writes MIDI or ABC files, builds a dataset or model pipeline over symbolic music, or renders MIDI to audio.
---

# symusic

symusic is a C++ library with a Python binding for symbolic music. A `Score` holds global tempo, signature and marker events and a list of `Track` objects with notes, control changes, pitch bends, pedals and lyrics. Every object exists in three time units: ticks, quarter notes and seconds.

## Pages

Read the page for the task; each one is short and its examples run.

| Question | Page |
|---|---|
| How do I install it, and what does a typical session look like? | [quickstart.md](references/quickstart.md) |
| Which unit should I use, and how do conversions round? | [time_units.md](references/time_units.md) |
| What attributes and methods do scores, tracks and events have? When is something copied or shared? | [data_model.md](references/data_model.md) |
| What survives reading and writing MIDI, ABC or pickle? | [files.md](references/files.md) |
| How does ABC playback or export handle timing and invalid notation? | [abc.md](references/abc.md) |
| How do I get NumPy arrays, beats or piano rolls? | [arrays.md](references/arrays.md) |
| How do I process many files: parallel loading, cleaning, segmenting, augmenting? | [datasets.md](references/datasets.md) |
| How do I create a score from generated events or model output? | [building_scores.md](references/building_scores.md) |
| How do I render audio with a SoundFont? | [synthesis.md](references/synthesis.md) |
| What does this error mean? | [troubleshooting.md](references/troubleshooting.md) |

Exact signatures and docstrings are in the installed package's type stubs, `symusic/core.pyi`; `python -c "import symusic.core as c; print(c.__file__)"` shows its directory.

## Rules that prevent most mistakes

- Keep one time unit per score. Events, tracks and scores of different units cannot be mixed; create everything with the same `ttype` or convert with `to`.
- Pass `min_dur=1` when converting to ticks or resampling, so short notes do not get duration 0.
- Transformations return a copy unless you pass `inplace=True`; `sort` and `filter` on lists work in place by default.
- `score.copy()` is deep; `copy.copy(score)`, slices and `notes.copy()` share events with the original.
- Write the sustain pedal as control change 64 in `controls`; `pedals` is derived on read and not written.
- Do not transpose drum tracks; their pitch selects the instrument.
- Use `numpy()` and `from_numpy` for bulk work instead of looping over events in Python.

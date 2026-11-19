---
title: symusic
---

# symusic

symusic (symbolic music) loads MIDI and ABC files into note-level scores, transforms them and writes them back. It is meant for preprocessing large symbolic-music datasets, where pure-Python MIDI libraries are the bottleneck.

A score holds tracks of notes, control changes, pitch bends, pedals and lyrics, plus the global tempo, time-signature, key-signature and marker lists. symusic decodes the MIDI events into these note-level objects in C++, similar to [miditoolkit](https://github.com/YatingMusic/miditoolkit), rather than exposing the raw event stream as [mido](https://github.com/mido/mido) does. If you need event-level MIDI parsing in C++, use [minimidi](https://github.com/lzqlzzq/minimidi), the parser symusic is built on.

Every score, track and event exists in three time units: `tick` (integer MIDI ticks), `quarter` (floating-point quarter notes) and `second` (floating-point seconds from the tempo map). On top of loading and writing, symusic shifts, clips, trims, sorts, filters and resamples scores, computes beat grids, exports NumPy arrays and piano rolls, and renders audio from SF2 and SF3 SoundFonts. ABC import and export run the bundled `abc2midi` and `midi2abc` converters from [abcmidi](https://github.com/sshlien/abcmidi). MusicXML is not supported.

Start with the [quickstart](guide/quickstart.md).

## Agent skill

The guide pages on this site are also an agent skill. Install it from GitHub with `npx skills add Yikai-Liao/symusic`, adding `@<tag>` to pin the release that matches your installed symusic, or copy the version bundled in the installed package with `python -m symusic.skill install DIR`, where `DIR` is the agent's skills directory, such as `~/.claude/skills`. Method signatures and docstrings are in the package's type stubs, `symusic/core.pyi`.

## Benchmarks

![MIDI parsing benchmark](https://github.com/user-attachments/assets/5f663e4e-9562-436e-8f97-5b62e96d0314)

Parse time on GitHub Actions M1 runners. `mido` parses to event level in pure Python; `pretty_midi` and `miditoolkit` build note-level objects on top of `mido`; `midifile` is C++. Python libraries were timed with `timeit`, C++ libraries with `nanobench` and Julia libraries with `BenchmarkTools`. The scripts are in [symusic-benchmark](https://github.com/Yikai-Liao/symusic-benchmark).

```{toctree}
:hidden:
:caption: Guide

guide/quickstart
guide/time_units
guide/data_model
guide/files
guide/arrays
guide/datasets
guide/building_scores
guide/synthesis
guide/troubleshooting
```

```{toctree}
:hidden:
:caption: Development

design/architecture
development/conventions
development/ci
CONTRIBUTING
ROADMAP
```

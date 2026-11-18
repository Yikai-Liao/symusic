# Introduction

symusic (symbolic music) loads MIDI and ABC files into note-level scores, transforms them and writes them back. It is meant for preprocessing large symbolic-music datasets, where pure-Python MIDI libraries are the bottleneck.

A score holds tracks of notes, control changes, pitch bends, pedals and lyrics, plus the global tempo, time-signature, key-signature and marker lists. symusic decodes the MIDI events into these note-level objects in C++, similar to [miditoolkit](https://github.com/YatingMusic/miditoolkit), rather than exposing the raw event stream as [mido](https://github.com/mido/mido) does. If you need event-level MIDI parsing in C++, use [minimidi](https://github.com/lzqlzzq/minimidi), the parser symusic is built on.

Every score, track and event exists in three time units: `tick` (integer MIDI ticks), `quarter` (floating-point quarter notes) and `second` (floating-point seconds from the tempo map). {doc}`concepts/time_units` explains the conversions.

On top of loading and dumping, symusic provides:

- copy and in-place transformations: shifting time, pitch and velocity; clipping, trimming, sorting, filtering and resampling;
- beat and downbeat grids;
- NumPy export and import of event lists;
- piano rolls;
- audio rendering from SF2 and SF3 SoundFonts.

ABC import and export run in the C++ core without a converter executable. MusicXML is not supported.

## Benchmarks

![MIDI parsing benchmark](https://github.com/user-attachments/assets/5f663e4e-9562-436e-8f97-5b62e96d0314)

Parse time on GitHub Actions M1 runners. `mido` parses to event level in pure Python; `pretty_midi` and `miditoolkit` build note-level objects on top of `mido`; `midifile` is C++. Python libraries were timed with `timeit`, C++ libraries with `nanobench` and Julia libraries with `BenchmarkTools`. The scripts are in [symusic-benchmark](https://github.com/Yikai-Liao/symusic-benchmark).

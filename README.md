# symusic
[![Build and upload to PyPI](https://github.com/Yikai-Liao/symusic/actions/workflows/wheel.yml/badge.svg?branch=main)](https://github.com/Yikai-Liao/symusic/actions/workflows/wheel.yml) [![PyPI version](https://badge.fury.io/py/symusic.svg)](https://badge.fury.io/py/symusic) <a target="_blank" href="https://colab.research.google.com/github/Yikai-Liao/symusic/blob/main/tutorial.ipynb">
  <img src="https://colab.research.google.com/assets/colab-badge.svg" alt="Open In Colab"/>
</a>

**Sy**music("**Sy**bolic **Music**") is a cross platform `note level` midi decoding libaray with lightening speed, which is over 100 times faster than [mido](https://github.com/mido/mido), the main midi parsing library in python.

The libaray is written in cpp and based on [minimidi](https://github.com/lzqlzzq/minimidi/tree/main). It offers a python binding using pybind11.

Here, we have added a tutorial.ipynb for you to learn about how to use the libaray. 

Tutorial and Benchmark: <a target="_blank" href="https://colab.research.google.com/github/Yikai-Liao/symusic/blob/main/tutorial.ipynb">
  <img src="https://colab.research.google.com/assets/colab-badge.svg" alt="Open In Colab"/>
</a>

## Features

* You can just read a midi file like `score = symusic.Score("path to midi")`
* The time unit of the parsed data is the quarter note.
* The tempo attribute in the tempo event represents quarter per minute (qpm)
* We offer some batch operation functions for both `Score` and `Track` class:
  * shift_pitch(offset: int)
  * shift_velocity(offset: int)
  * shift_time(offset: float)
  * sort()
* You can operate each note just like you did before in python (like PrettyMidi)
* There are interfaces for generating pianoroll with any quantizations using Eigen in cpp:
  * Track.frame_pianoroll(quantization: int = 16)
  * Track.onset_pianoroll(quantization: int = 16)

## TODO

Notice that this libaray is under development:

* Writing back to midi files is currently not supported
* Not all features in midi are suppored, like lyrics and pitchbend

## Installation
### Use pre-compiled version
```bash
pip install symusic
```

### Build from source
> Make sure that your system has cmake and c++ compilers

```bash
git clone https://github.com/Yikai-Liao/symusic --recursive
pip install ./symusic
```

## Benchmark
### Parsing MIDI

* test using mahler.mid from minimid/example in colab
* mido is writen in pure python, and only parsing midi file to event level
* pretty_midi and miditoolkit is based on mido

|libarary| time |
| -- | -- |
|**symusic**|21.8 ms ± 11.7 ms|
|MIDI.jl|128.024 ms|
|mido|5.68 s ± 2 s|
|pretty_midi|5.59 s ± 844 ms|
|miditoolkit|6.27 s ± 1.79 s|
|music21|8.59 s ± 1.2 s|
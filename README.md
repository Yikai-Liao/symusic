# symusic
[![Build and upload to PyPI](https://github.com/Yikai-Liao/symusic/actions/workflows/wheel.yml/badge.svg?branch=main)](https://github.com/Yikai-Liao/symusic/actions/workflows/wheel.yml) 

**Sy**music("**Sy**bolic **Music**") is a cross platform `note level` midi decoding libaray with lightening speed, based on [minimidi](https://github.com/lzqlzzq/minimidi/tree/main).

The libaray is written in cpp and offers a python binding using pybind11.

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

> Make sure that your system has cmake and c++ compilers

```bash
git clone https://github.com/Yikai-Liao/symusic --recursive
pip install ./symusic
```


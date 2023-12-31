# [symusic](https://github.com/Yikai-Liao/symusic)
[![Build and upload to PyPI](https://github.com/Yikai-Liao/symusic/actions/workflows/wheel.yml/badge.svg?branch=main)](https://github.com/Yikai-Liao/symusic/actions/workflows/wheel.yml) [![PyPI version](https://badge.fury.io/py/symusic.svg)](https://badge.fury.io/py/symusic) [![Downloads](https://static.pepy.tech/badge/symusic)](https://pepy.tech/project/symusic) [![Page Views Count](https://badges.toozhao.com/badges/01HGE1345YAKN4YV7WF0JRKZJK/blue.svg)](https://badges.toozhao.com/stats/01HGE1345YAKN4YV7WF0JRKZJK "Get your own page views count badge on badges.toozhao.com") <a target="_blank" href="https://colab.research.google.com/github/Yikai-Liao/symusic/blob/main/tutorial.ipynb">
  <img src="https://colab.research.google.com/assets/colab-badge.svg" alt="Open In Colab"/>
</a>

**Sy**music("**Sy**bolic **Music**") is a cross-platform `note level` midi decoding library with lightening speed, which is over 100 times faster than [mido](https://github.com/mido/mido), the main midi parsing library in python.

The library is written in cpp and based on [minimidi](https://github.com/lzqlzzq/minimidi/tree/main). It offers a python binding using pybind11.

Here, we have added a tutorial.ipynb for you to learn about how to use the library.

Tutorial and Benchmark: <a target="_blank" href="https://colab.research.google.com/github/Yikai-Liao/symusic/blob/main/tutorial.ipynb">
  <img src="https://colab.research.google.com/assets/colab-badge.svg" alt="Open In Colab"/>
</a>

## Features

* You can just read a midi file like `score = symusic.Score("path to midi", ttype="tick")`
* Writing back to midi is now supported! `score.dump_midi("path")`
* Multiple `time unit (ttype)` is now supported (currently `tick` and `quarter`)
* The tempo attribute in the tempo event represents quarter per minute (qpm)
* We offer some batch operation functions for both `Score` and `Track` class:
  * shift_pitch(offset: int)
  * shift_velocity(offset: int)
  * shift_time(offset: float)
  * sort(key, reverse)
* You can operate each note just like you did before in python (like PrettyMidi)
* Extremely fast `pickle` is now supported
* `.numpy()` method for getting [SoA](https://en.wikipedia.org/wiki/AoS_and_SoA) data
* `.filter(func, inplace)` method for filtering objets in all the "List" in symusic

## Installation
### Use pre-compiled version
```bash
pip install symusic
```

### Build from source
> Make sure that your system has cmake and c++ compilers

```bash
git clone --recursive https://github.com/Yikai-Liao/symusic
pip install ./symusic
```

## Benchmark
### Parsing MIDI

* test using [mahler.mid](https://github.com/lzqlzzq/minimidi/blob/main/example/mahler.mid) from minimidi/example on my laptop (i7-10875H, 32GB 2666MHz DDR4 RAM, Linux 6.1.69-1-lts)
* `midifile` is writen in cpp, and could parse midi files to both `event level` and `note level`. It is slow mostly becuase of `iostream`.
* `mido` is writen in pure python, and only parses midi files to `event level`
* `pretty_midi` and `miditoolkit` is based on `mido`, and parse midi files to `note level`
* For libraries written in python or with python bindings, we use `timeit` to measure the time cost of parsing the midi file. `nanobench` for cpp libraries and `BenchmarkTools` for julia libraries.

| library                                                       | level | absolute time     | relative time |
|---------------------------------------------------------------|-------|-------------------|---------------|
| [**minimidi**](https://github.com/lzqlzzq/minimidi)           | event | 2.86 ms           | 1.0x          |
| [**symusic**](https://github.com/Yikai-Liao/symusic)          | note  | 3.47 ms ± 113 µs  | 1.2x          |
| [midifile](https://github.com/craigsapp/midifile)             | event | 44.0 ms           | 15.4x         |
| [midifile](https://github.com/craigsapp/midifile)             | note  | 45.6 ms           | 15.9x         |
| [MIDI.jl](https://github.com/JuliaMusic/MIDI.jl)              | note  | 109.707 ms        | 38.4x         |
| [mido](https://github.com/mido/mido)                          | event | 2.92 s ± 42.7 ms  | 1021.0x       |
| [miditoolkit](https://github.com/YatingMusic/miditoolkit)     | note  | 3.15 s ± 38.2 ms  | 1101.4x       |
| [pretty_midi](https://github.com/craffel/pretty-midi)         | note  | 3.16 s ± 9.56 ms  | 1104.9x       |
| [music21](https://github.com/cuthbertLab/music21)             | note  | 4.23 s ± 34.5 ms  | 1479.0x       |

## Acknowledgement

* [minimidi](https://github.com/lzqlzzq/minimidi) : A fast and lightweight midi parsing library written in cpp, which is the foundation of this project.
* [nanobind](https://github.com/wjakob/nanobind) : A efficient and lightweight library for binding C++ to Python, which is significantly faster than [pybind11](https://github.com/pybind/pybind11).
* [zpp_bits](https://github.com/eyalz800/zpp_bits) : An extraordinary fast and lightweight single header library for serialization and deserialization. I use it to support pickle.
* [geek_time_cpp](https://github.com/adah1972/geek_time_cpp/tree/master) The example code of the book "Modern C++ Programming Practice". We use the [metamacro.h](https://github.com/adah1972/geek_time_cpp/blob/master/40/metamacro.h#L1-L622) in it for shortening the code.

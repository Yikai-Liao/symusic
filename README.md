# symusic
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


## TODO

Notice that this library is under development:

* Writing back to midi files is currently not supported
* Re-implement `filter_notes` and `Note Array`.

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

* test using [mahler.mid](https://github.com/lzqlzzq/minimidi/blob/main/example/mahler.mid) from minimidi/example in colab
* `mido` is writen in pure python, and only parses midi files to `event level`
* `pretty_midi` and `miditoolkit` is based on `mido`, and parse midi files to `note level`

| library                                                   | time              |
|-----------------------------------------------------------|-------------------|
| [**symusic**](https://github.com/Yikai-Liao/symusic)      | 21.8 ms ± 11.7 ms |
| [MIDI.jl](https://github.com/JuliaMusic/MIDI.jl)          | 128.024 ms        |
| [mido](https://github.com/mido/mido)                      | 5.68 s ± 2 s      |
| [pretty_midi](https://github.com/craffel/pretty-midi)     | 5.59 s ± 844 ms   |
| [miditoolkit](https://github.com/YatingMusic/miditoolkit) | 6.27 s ± 1.79 s   |
| [music21](https://github.com/cuthbertLab/music21)         | 8.59 s ± 1.2 s    |

## Acknowledgement

* [minimidi](https://github.com/lzqlzzq/minimidi/tree/main) : A fast and lightweight midi parsing library written in cpp, which is the foundation of this project.
* [pybind11](https://github.com/pybind/pybind11) : A great header-only library to help you create python binding for your cpp code.
* [pybind11-stubgen](https://github.com/sizmailov/pybind11-stubgen) A nice tool to generate stub files for pybind11 projects.
* [zpp_bits](https://github.com/eyalz800/zpp_bits) : An extraordinary fast and lightweight single header library for serialization and deserialization. I use it to support pickle.
* [geek_time_cpp](https://github.com/adah1972/geek_time_cpp/tree/master) The example code of the book "Modern C++ Programming Practice". We use the [metamacro.h](https://github.com/adah1972/geek_time_cpp/blob/master/40/metamacro.h#L1-L622) in it for shortening the code.

  
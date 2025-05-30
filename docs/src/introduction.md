# Introduction

**Symusic** (Symbolic Music) is a high-performance cross-platform library for processing symbolic music data, with a primary focus on MIDI files. It offers lightning-fast processing capabilities that are hundreds of times faster (200x-500x) than traditional Python libraries like [mido](https://github.com/mido/mido).

## Project Overview

Symbolic music data, such as MIDI, Music XML, and ABC Notation, are essential formats for representing music in a machine-readable form. Symusic aims to provide an efficient backend for preprocessing these formats, particularly targeting applications in music information retrieval and deep learning.

The library is written in C++ with Python bindings via [nanobind](https://github.com/wjakob/nanobind), making it both performant and accessible to a wide range of users. It offers a note-level representation of MIDI files (similar to [miditoolkit](https://github.com/YatingMusic/miditoolkit)) rather than an event-level representation (as in [mido](https://github.com/mido/mido)), which is more suitable for many music processing tasks.

## Core Features

- **Lightning-fast processing**: 200-500 times faster than pure Python implementations
- **Multiple time unit support**: Work with ticks, quarter notes, or seconds
- **Comprehensive MIDI handling**: Load, manipulate, and save MIDI files
- **Piano roll conversion**: Convert symbolic music to piano roll representations
- **Music synthesis**: Generate audio from symbolic music using SoundFont technology
- **Cross-platform compatibility**: Works on Linux, macOS, and Windows
- **Pythonic API**: Easy to use for Python developers
- **Multithreading support**: Efficiently process large datasets with multiple CPU cores
- **Serialization**: Fast pickling for multiprocessing workflows

## Installation

### Pre-Built Wheels

The easiest way to install Symusic is using pip:

```bash
pip install symusic
```

We provide pre-built wheels for:
- **Operating Systems**: Linux, macOS, and Windows
- **Architectures**: X86 and ARM (except Windows ARM64)
- **Python Versions**: CPython 3.8, 3.9, 3.10, 3.11, 3.12, and PyPy 3.9

### Installing from Source

If you need to build from source, you can use the source distribution on PyPI:

```bash
pip install --no-binary symusic symusic
```

Or clone the repository and install it:

```bash
git clone --recursive git@github.com:Yikai-Liao/symusic.git
pip install ./symusic
```

### Compiler Requirements

Symusic is built with C++20 and requires a modern compiler:

- GCC: 11 or newer
- Clang: 15 or newer
- MSVC: Visual Studio 2022 recommended

If you don't have sudo permissions on Linux, you can install GCC through conda:

```bash
conda install conda-forge::gcc conda-forge::gxx
CC=/path_to_conda_root/envs/env_name/bin/gcc CXX=/path_to_conda_root/envs/env_name/bin/g++ pip install --no-binary symusic symusic
```

## Why Symusic?

The previously dominant MIDI parsing backends (like mido, used by pretty_midi and miditoolkit) are too slow for large-scale symbolic music data preprocessing in modern deep learning workflows. Symusic addresses this limitation with its high-performance C++ implementation, making it possible to process and tokenize music data in real-time during training.

Our event-level MIDI parsing code has been separated into a lightweight and efficient header-only C++ library called [minimidi](https://github.com/lzqlzzq/minimidi) for those who only need to parse MIDI files at the event level in C++.

Future plans include bindings for other languages such as Julia and Lua for even broader accessibility.

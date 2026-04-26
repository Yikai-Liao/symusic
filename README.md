# [symusic](https://github.com/Yikai-Liao/symusic)
[![Build and upload to PyPI](https://github.com/Yikai-Liao/symusic/actions/workflows/wheel.yml/badge.svg?branch=main)](https://github.com/Yikai-Liao/symusic/actions/workflows/wheel.yml) [![PyPI version](https://badge.fury.io/py/symusic.svg)](https://badge.fury.io/py/symusic) [![Downloads](https://static.pepy.tech/badge/symusic)](https://pepy.tech/project/symusic) [![codecov](https://codecov.io/gh/Yikai-Liao/symusic/branch/main/graph/badge.svg)](https://codecov.io/gh/Yikai-Liao/symusic) [![Tests](https://github.com/Yikai-Liao/symusic/actions/workflows/tests.yml/badge.svg)](https://github.com/Yikai-Liao/symusic/actions/workflows/tests.yml) [![Page Views Count](https://badges.toozhao.com/badges/01HGE1345YAKN4YV7WF0JRKZJK/blue.svg)](https://badges.toozhao.com/stats/01HGE1345YAKN4YV7WF0JRKZJK "Get your own page views count badge on badges.toozhao.com")

**ISMIR 2024 LBD Demo Video**: [YouTube](https://www.youtube.com/watch?v=ZGcyyUJ3P6Q) [Bilibili](https://www.bilibili.com/video/BV1mJUaYcEj1)

**Sy**music ("**Sy**mbolic **Music**") is a cross-platform symbolic music toolkit for note-level
MIDI and ABC workflows. The core is written in C++20 and exposed to Python through nanobind, which
keeps common parsing and transformation workloads hundreds of times faster than the pure-Python
stacks many MIR pipelines still rely on.

The project builds on top of [minimidi](https://github.com/lzqlzzq/minimidi/tree/main) for parsing,
[zpp_bits](https://github.com/eyalz800/zpp_bits) for fast pickling, and
[FluidLite](https://github.com/divideconcept/FluidLite) for SoundFont rendering.

The canonical documentation is hosted on Read the Docs at
[symusic.readthedocs.io](https://symusic.readthedocs.io/en/stable/). The legacy GitHub Pages mdBook
remains available at [yikai-liao.github.io/symusic](https://yikai-liao.github.io/symusic/) for
historical links only.

## Documentation

- Read the Docs is the primary public documentation target: <https://symusic.readthedocs.io/en/stable/>.
- The documentation source for RTD lives under `docs/` and is built with Sphinx + MyST.
- `CHANGELOG.md` tracks the current `v0.6.0` work and backfills published historical releases.
- The legacy mdBook stays online for historical reference but is no longer updated.
- Build the docs locally via `python -m venv .venv`, `source .venv/bin/activate`,
  `pip install -r docs/requirements.txt`, and `pip install . -C cmake.define.SYMUSIC_FAST_BUILD=ON`
  before running `sphinx-build -b html docs docs/_build/html`.

## Features

- Load MIDI and ABC files with `Score("path.mid")` or `Score("path.abc", fmt="abc")`.
- Convert scores between `tick`, `quarter`, and `second` time units.
- Export back to MIDI or ABC with `dump_midi`, `dump_abc`, and `dumps_abc`.
- Compute beats, downbeats, and piano rolls for analysis and model pipelines.
- Apply vectorized transformations on `Score` and `Track` objects such as `shift_pitch`,
  `shift_velocity`, `shift_time`, `sort`, and `filter`.
- Serialize containers efficiently with pickle and structured NumPy exports.
- Render audio through SoundFonts with `Synthesizer`, `BuiltInSF2`, `BuiltInSF3`, and `dump_wav`.

## Installation
### Use pre-built wheels
```bash
pip install symusic
```

### Build from source
Make sure your system has CMake and a C++20-capable compiler, then clone the repository with
submodules and install it through `pip`:

```bash
git clone --recursive https://github.com/Yikai-Liao/symusic
cd symusic
pip install .
```

Or install the source distribution from PyPI:

```bash
pip install symusic --no-binary symusic
```

On Windows, `pip install .` will prefer `Ninja + MSVC` when it runs inside a Visual Studio
Developer shell. In a regular shell it keeps using the default Visual Studio generator.

For debugging, you can pass `-Ccmake.define.MEM_LEAK_WARNING=True` to `pip install` to enable the
nanobind leak-warning instrumentation:

```bash
pip install -Ccmake.define.MEM_LEAK_WARNING=True .
```

## Python compatibility and CI coverage

The wheel publication workflow builds CPython wheels for Python `3.9` through `3.14` across the
Linux, macOS, and Windows targets listed in `.github/workflows/wheel.yml`, including `win_arm64`.
PyPy wheels are currently published for `pp311` on `manylinux_x86_64`, `macosx_x86_64`, and
`macosx_arm64`.

`+freethreaded` variants are not tested yet. CPython 3.12 can still emit nanobind leak warnings
even when the build is otherwise healthy.

## Benchmark

* `midifile` is written in C++ and can parse MIDI files to both `event level` and `note level`.
  It is still slowed down by `iostream`.
* `mido` is written in pure Python and only parses MIDI files to `event level`.
* `pretty_midi` and `miditoolkit` build on top of `mido` and expose note-level abstractions.
* For libraries written in Python or with Python bindings, we use `timeit` to measure parse time.
  `nanobench` is used for C++ libraries and `BenchmarkTools` for Julia libraries.
* The benchmark scripts live in [symusic-benchmark](https://github.com/Yikai-Liao/symusic-benchmark).
* The published chart was measured on GitHub Actions M1 runners.

![Image](https://github.com/user-attachments/assets/5f663e4e-9562-436e-8f97-5b62e96d0314)

## Citation

```bibtex
@inproceedings{symusic2024,
    title={symusic: A swift and unified toolkit for symbolic music processing},
    author={Yikai Liao, Zhongqi Luo, et al.},
    booktitle={Extended Abstracts for the Late-Breaking Demo Session of the 25th International Society for Music Information Retrieval Conference},
    year={2024},
    url={https://ismir2024program.ismir.net/lbd_426.html#lbd},
}
```

## Acknowledgement

* [minimidi](https://github.com/lzqlzzq/minimidi) : A fast and lightweight MIDI parsing library written in C++, which is the foundation of this project.
* [FluidLite](https://github.com/divideconcept/FluidLite) : A lightweight SoundFont synthesizer that powers Symusic's synthesis layer.
* [nanobind](https://github.com/wjakob/nanobind) : An efficient and lightweight library for binding C++ to Python.
* [zpp_bits](https://github.com/eyalz800/zpp_bits) : A fast single-header serialization library used for pickle support.
* [geek_time_cpp](https://github.com/adah1972/geek_time_cpp/tree/master) : Example code from the book "Modern C++ Programming Practice". We use [metamacro.h](https://github.com/adah1972/geek_time_cpp/blob/master/40/metamacro.h#L1-L622) from it to shorten repetitive code.
* [utfcpp](https://github.com/nemtrif/utfcpp) : A portable library for handling UTF-8 strings in C++.

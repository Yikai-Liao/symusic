# symusic

[![PyPI](https://img.shields.io/pypi/v/symusic)](https://pypi.org/project/symusic/)
[![Downloads](https://static.pepy.tech/badge/symusic)](https://pepy.tech/project/symusic)
[![CI](https://github.com/Yikai-Liao/symusic/actions/workflows/ci.yml/badge.svg?branch=main)](https://github.com/Yikai-Liao/symusic/actions/workflows/ci.yml)
[![Docs](https://readthedocs.org/projects/symusic/badge/?version=stable)](https://symusic.readthedocs.io/en/stable/)
[![codecov](https://codecov.io/gh/Yikai-Liao/symusic/branch/main/graph/badge.svg)](https://codecov.io/gh/Yikai-Liao/symusic)

**Sy**music (**Sy**mbolic **Music**) is a fast toolkit for note-level symbolic music. It reads and writes MIDI and ABC, converts between ticks, quarter notes and seconds, transforms scores, exports NumPy arrays and piano rolls, and renders audio from SoundFonts. The core is C++20 with a Python binding built on nanobind, and parses MIDI hundreds of times faster than pure-Python libraries.

## Install

```sh
pip install symusic
```

Wheels cover CPython 3.10 and newer on Linux, macOS and Windows, and PyPy 3.11 on Linux x86_64 and macOS; from CPython 3.12 on, one stable-ABI (`abi3`) wheel serves every later version. Free-threaded builds are not supported. Building from source needs a C++20 compiler; see [CONTRIBUTING.md](https://github.com/Yikai-Liao/symusic/blob/main/CONTRIBUTING.md#set-up).

## Example

```python
import pickle
from symusic import Score

score = Score("song.mid")  # format from the extension, time in ticks
print(score)  # tracks and event counts

shifted = score.shift_pitch(2)  # returns a new score
score.shift_time(480, inplace=True)  # or mutates in place

seconds = score.to("second")  # "tick", "quarter" or "second"
notes = seconds.tracks[0].notes.numpy()  # dict of NumPy arrays
roll = score.tracks[0].pianoroll(modes=["onset", "frame"], pitch_range=(0, 128))

score.dump_midi("out.mid")
restored = pickle.loads(pickle.dumps(score))  # exact binary round trip
```

## Features

- **I/O:** MIDI read and write; ABC through the bundled `abc2midi` and `midi2abc`; pickle support.
- **Time units:** every score, track and event exists in `Tick`, `Quarter` and `Second`, converted through the tempo map.
- **Transformations:** shift time, pitch and velocity; clip, trim, sort, filter, resample and adjust time, each as a copy or in place.
- **Analysis:** beat and downbeat grids, structure-of-arrays NumPy export and import, and piano rolls.
- **Synthesis:** Offline SoundFont 2/3 rendering with `Synthesizer`.
- **Typing:** generated stubs with generic types, so type checkers distinguish `Score[Tick]` from `Score[Second]`.

## Documentation

The [guide](https://symusic.readthedocs.io/en/stable/) covers time units, the data model, file formats, NumPy arrays and piano rolls, dataset pipelines, building scores and synthesis; the same pages are in [`skills/symusic`](https://github.com/Yikai-Liao/symusic/tree/main/skills/symusic). Method signatures and docstrings are in the installed package's type stubs, `symusic/core.pyi`. The [architecture](https://github.com/Yikai-Liao/symusic/blob/main/docs/design/architecture.md) explains how the code is organized and which behavior it guarantees.

## Agent skill

The guide pages are also an [agent skill](https://github.com/Yikai-Liao/symusic/tree/main/skills/symusic) for coding agents. Install it from GitHub with `npx skills add Yikai-Liao/symusic`, adding `@<tag>` to pin the release that matches your installed symusic, or copy the version bundled in the installed package with `python -m symusic.skill install DIR`, where `DIR` is the agent's skills directory, such as `~/.claude/skills`. `python -m symusic.skill path` prints the bundled copy's location.

## Performance

![MIDI parsing benchmark](https://github.com/user-attachments/assets/5f663e4e-9562-436e-8f97-5b62e96d0314)

Parse time on GitHub Actions M1 runners. `mido` parses to event level in pure Python; `pretty_midi` and `miditoolkit` build note-level objects on top of `mido`; `midifile` is C++. Python libraries were timed with `timeit`, C++ libraries with `nanobench` and Julia libraries with `BenchmarkTools`. The scripts are in [symusic-benchmark](https://github.com/Yikai-Liao/symusic-benchmark).

## Contributing

Bug reports, fixes and features are welcome. [CONTRIBUTING.md](https://github.com/Yikai-Liao/symusic/blob/main/CONTRIBUTING.md) covers setup, building, testing and pull requests; planned work and known issues are in [ROADMAP.md](https://github.com/Yikai-Liao/symusic/blob/main/ROADMAP.md). Coding agents start from [AGENTS.md](https://github.com/Yikai-Liao/symusic/blob/main/AGENTS.md).

## Citation

symusic was presented as an [ISMIR 2024 late-breaking demo](https://ismir2024program.ismir.net/lbd_426.html#lbd) ([video](https://www.youtube.com/watch?v=ZGcyyUJ3P6Q), [Bilibili](https://www.bilibili.com/video/BV1mJUaYcEj1)).

```bibtex
@inproceedings{symusic2024,
    title={symusic: A swift and unified toolkit for symbolic music processing},
    author={Yikai Liao, Zhongqi Luo, et al.},
    booktitle={Extended Abstracts for the Late-Breaking Demo Session of the 25th International Society for Music Information Retrieval Conference},
    year={2024},
    url={https://ismir2024program.ismir.net/lbd_426.html#lbd},
}
```

## Acknowledgements

- [minimidi](https://github.com/lzqlzzq/minimidi): fast, lightweight MIDI parsing, the foundation of this project.
- [FluidSynth](https://github.com/FluidSynth/fluidsynth): Bundled offline SoundFont synthesis.
- [nanobind](https://github.com/wjakob/nanobind): C++/Python binding.
- [zpp_bits](https://github.com/eyalz800/zpp_bits): serialization for pickling.
- [abcmidi](https://github.com/sshlien/abcmidi): ABC conversion.
- [geek_time_cpp](https://github.com/adah1972/geek_time_cpp): `metamacro.h` for repetitive code.
- [utfcpp](https://github.com/nemtrif/utfcpp): UTF-8 handling.

## License

[MIT](https://github.com/Yikai-Liao/symusic/blob/main/LICENSE)

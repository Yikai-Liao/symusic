# Motivation

**[Symusic](https://github.com/Yikai-Liao/symusic)** aims to offer fast and efficient symbolic music data (e.g. MIDI, Music XML and ABC Notation) preprocessing backend.

The former dominant MIDI parsing backend is [mido](https://github.com/mido/mido)
(used by [pretty_midi](https://github.com/craffel/pretty-midi) and [miditoolkit](https://github.com/YatingMusic/miditoolkit)), which is written in pure python.
However, it is too slow for the large-scale symbolic music data preprocessing task in the deep learning era,
which makes it impossible to tokenize the needed data in real-time while training.

Out of the need, we developed this library. It is written in C++, offers a python binding using pybind11, and is over 100 times faster than mido.
We parse the MIDI file to `note level`(similar to [miditoolkit](https://github.com/YatingMusic/miditoolkit)) instead of `event level`([mido](https://github.com/mido/mido)), which is more suitable for the symbolic music data preprocessing task.
More data formats like `Music XML` and `ABC Notation` will be supported in the future.

We separated the `event-level` MIDI parsing code into a lightweight and efficient header-only C++ library [minimidi](https://github.com/lzqlzzq/minimidi/tree/main)
for those who only need to parse MIDI files to `event level` in C++.

In the future, we will also bind [symusic](https://github.com/Yikai-Liao/symusic) to other languages like `Julia` and `Lua` for more convenient use.

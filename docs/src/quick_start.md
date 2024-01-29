# Quick Start

## Installation

```bash
pip install symusic
```

We have pre-built wheels for `Linux`, `macOS`, and `Windows`, with support for both `X86` and `ARM` architectures (Windows ARM64 is not supported yet).

The wheels are built with `3.8`, `3.9`, `3.10`, `3.11` and `3.12` versions of `CPython`, and `3.9` version of `PyPy`.

Note that there are some memory leaks in `CPython 3.12`, and they would be reported by the `nanobind` framework when you import `symusic` in python 3.12.
I hope that the `CPython` team would fix them soon.

## Load

If you pass a file path (str or pathlib.Path) to the `Score` constructor,
it would be dynamically dispatched to the `from_file` method. `symusic` will try to infer the file format from the file extension,
or you can specify the format by passing the `fmt` argument.

```python
from symusic import Score

Score("xxx.mid")
Score("xxx.txt", fmt="abc")

Score.from_file("xxx.mid")
Score.from_file("xxx.txt", fmt="abc")
```

You could also pass a `ttype` argument to the `Score` constructor to specify the type of time unit.
There are three types of time unit: `tick`, `quarter` and `second`, while the default is `tick`.

A member function `to` is also provided for converting the time unit of the score.

* `tick` (int32) is just the raw time unit in the MIDI file.
* `quarter` (float32) is a normalized time unit (directly normalized from `tick` using `ticks_per_quarter`), where the length of a quarter note is 1.
* `second` (float32) means the time unit is in seconds, the same as what `pretty_midi` does.



```python
from symusic import Score, TimeUnit

# you could specify the time unit when loading a score
Score("xxx.mid", ttype=TimeUnit.quarter)
Score("xxx.txt", fmt="abc", ttype='second')

# or convert the time unit after loading
# an additional argument `min_dur` is optional, which is the minimum duration of a note
Score("xxx.mid", "quarter", ).to("tick", min_dur=1)
Score("xxx.txt", fmt="abc").to(TimeUnit.second)
```

## Dump

Currently, we got two dumpers, `dump_midi` and `dump_abc`, which dump the score to a MIDI file and an ABC file respectively.

Before actually dumping the score, it would be automatically converted to the time unit `tick`, which is the raw time unit in the MIDI file.

So if you want to specify a minimum duration of a note, you need to call the `to` method manually before dumping.

```python
from symusic import Score
s = Score("xxx.mid")
s.dump_midi("out.mid")
s.dump_abc("out.abc")
```

## Pickle & Multi Processing
All the objects in `symusic` are pickleable (extremely fast because of the `zpp_bits` backend we use),
so you can use `multiprocessing` to accelerate the processing.

Note that because of `zpp_bits`, pickle would be faster than loading from or dumping to a MIDI file.

```python
from symusic import Score
import pickle
import multiprocessing as mp

s = Score("xxx.mid")
with open("out.pkl", "wb") as f:
    pickle.dump(s, f)
with open("out.pkl", "rb") as f:
    s = pickle.load(f)

with mp.Pool(4) as pool:
    print(pool.map(lambda x: x.to("second"), [s, s, s, s]))
```

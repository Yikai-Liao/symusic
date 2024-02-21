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

## Accessing the Score

### Basic Information

The data structure of `Score` is quite simple, just following what `miditoolkit` and `pretty_midi` do.

```python
from symusic import Score
s = Score("xxx.mid")

# ttype is a property of all the objects in symusic related to time
s.ttype             # the type of time unit, 'tick', 'quarter' or 'second'
s.ticks_per_quarter # or s.tpq


# Global Events
s.tempos            # list of tempo changes
s.time_signature    # list of time signature changes
s.key_signature     # list of key signature changes
s.lyrics            # list of lyrics
s.marker            # list of markers

s.start()           # return the start time of the score
s.end()             # return the end time of the score
s.empty()           # return True if the score is empty
s.note_num()        # return the number of notes in the score

# Tracks
s.tracks            # list of tracks

t = s.tracks[0]
t.ttype             # read-only property, the same as s.ttype
t.name              # track name
t.program           # program number
t.is_drum           # whether it's a drum track
t.notes             # list of notes
t.control_changes   # list of control changes
t.pitch_bends       # list of pitch bends
t.pedals            # list of pedals

t.start()           # return the start time of the track
t.end()             # return the end time of the track
t.empty()           # return True if the track is empty
t.note_num()        # return the number of notes in the track

# Note, the most important event
n = t.notes[0]
n.ttype             # read-only property, the same as s.ttype
n.time              # the NoteOn time or n.start
n.start             # the same as n.time
n.duration          # the duration of the note
n.end               # a read-only property, n.time + n.duration
n.end_time()        # a method that returns n.end
n.pitch             # the pitch of the note
n.velocity          # the velocity of the note
n.empty()           # duration <= 0 or velocity <= 0
```

The container of events in `symusic` like the NoteList is a binding of `std::vector` in c++, with a similar interface to list in python.

We also add some methods to the container, like `start`, `end`, `empty`:

```python
from symusic import Score
notes = Score("xxx.mid").tracks[0].notes

notes.empty()       # return True if the note list is empty
notes.start()       # return the start time of the note list
notes.end()         # return the end time of the note list

notes1 = notes.copy()   # copy the note list
# the default key is (time, duration, pitch) for notes, (time, duration) for pedals
# for other events, the default key is "time"
notes2 = notes.sort(reversed=False, inplace=False)              # sort the notes
notes3 = notes.sort(lambda x: x.velocity)                       # you could also customize the key, but it is slower
notes4 = notes.filter(lambda x: x.pitch > 60, inplace=False)    # filter the notes

# adjust the time of the notes, the first list is the old time, the second list is the new time
# the time unit of the two lists should be the same as the time unit of the note list
# the semantic of adjust_time is the same as the method in pretty_midi
notes5 = notes.adjust_time([0, 10, notes.end()], [0, 20, notes.end() / 2])
```


### Struct of Array (SOA)

`symusic` has a great numpy support. You could easily convert "a list of objects" in `symusic` to "a dict of numpy array`.

```python
from symusic import Score
import numpy as np
from typing import Dict
s = Score("xxx.mid")

# get the numpy arrays
tempos: Dict[str, np.ndarray] = s.tempos.numpy()
notes: Dict[str, np.ndarray] = s.tracks[0].notes.numpy()

# access the array in dict
mspq = tempos["mspq"]   # a 1D numpy array, dtype is int32. mspq for microsecond per quarter note
start = notes["time"]   # a 1D numpy array, the dtype is determined by the time unit(ttype) of the score

# you could also convert the numpy arrays back to the list of objects
from symusic import Note
note_list1 = Note.from_numpy(**notes, ttype=s.ttype)
note_list2 = Note.from_numpy(
    time=notes["time"], duration=notes["duration"], pitch=notes["pitch"], velocity=notes["velocity"], ttype=s.ttype
)
# The note_list you get here is the same as s.tracks[0].notes
```

### Piano Roll

`symusic` also supports a conversion to the piano roll, which is a 3D numpy array.
But convert the piano roll back to the score is not supported yet since the complexity of the piano roll.

```python
from symusic import Score
s = Score("xxx.mid")

# You'd better resample the score before converting it to the piano roll to reduce the size of it
# tpq=6 means that the minimum time unit is 1/6 quarter note or 1/24 note
# min_dur=1 means that the duration of a note is at least 1 time unit
s_quantized = s.resample(tpq=6, min_dur=1)

# 4D np array, [modes, tracks, pitch, time]
s_pianoroll = s_quantized.pianoroll(
    # the modes of the piano roll， which determines the "modes dim"
    # only the following modes are supported: "onset", "frame", "offset"
    # you could determine the order by yourself
    modes=["onset", "frame", "offset"], # List[str]
    pitch_range=[0, 128],   # the pitch range (half-open interval) of the piano roll, [0, 128) by default
    encode_velocity=True    # make the pianoroll binary or filled with velocity
)

# 3D np array, [modes, pitch, time]
t_pianoroll = s_quantized.tracks[0].pianoroll(
    modes=["onset", "frame", "offset"], pitch_range=[0, 128], encode_velocity=True
)
```
You could also visualize the piano roll using `matplotlib`：

```python
from symusic import Score
from matplotlib import pyplot as plt
s = Score("xxx.mid").resample(tpq=6, min_dur=1)
track = s.tracks[0]
pianoroll = track.pianoroll(modes=["onset", "frame"], pitch_range=[0, 128], encode_velocity=False)
# this will show the onset and frame of the piano roll in one figure
plt.imshow(pianoroll[0] + pianoroll[1], aspect="auto")
plt.show()
```



## Synthesis

From version 0.4.0, `symusic` supports a simple synthesis of the score using SoundFont(2/3).
It's our own implementation, called `prestosynth`, and you could find the repository [here](https://github.com/lzqlzzq/prestosynth).

```python
from symusic import Score, Synthesizer, BuiltInSF3 ,dump_wav

s = Score("xxx.mid")

# You could choose a builtin soundfont
# And the following one is the default soundfont if you don't specify it when creating a synthesizer
sf_path = BuiltInSF3.MuseScoreGeneral().path(download=True)

# sf3 and sf2 are both supported
sf_path = "path/to/your/soundfont.sf3"

synth = Synthesizer(
    sf_path = sf_path, # the path to the soundfont
    sample_rate = 44100, # the sample rate of the output wave, 44100 is the default value
)

# audio is a 2D numpy array of float32, [channels, time]
audio = synth.render(s, stereo=True) # stereo is True by default, which means you will get a stereo wave

# you could also dump the wave to a file
# use_int16 is True by default, which means the output wave is int16, otherwise float32
dump_wav("out.wav", audio, sample_rate=44100, use_int16=True)
```

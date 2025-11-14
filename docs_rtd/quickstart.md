# Quickstart

This quick tour shows the most common Python workflow: read a MIDI file, tweak a few events, and
export back to disk. All operations run on the fast nanobind-backed core, but you only need regular
Python packaging tools.

## Installation

```bash
pip install symusic
```

To build from source (required for bleeding-edge contributors), clone the repository with submodules
and install in editable mode:

```bash
git clone --recursive https://github.com/Yikai-Liao/symusic
cd symusic
pip install -e .
```

## Hello MIDI

```python
import symusic

score = symusic.Score.from_file("example.mid", ttype="tick")
print(score.summary())

# Shift everything up a semitone and add a swingy timing offset.
score.shift_pitch_inplace(1)
score.shift_time_inplace(20)

score.dump_midi("example_shifted.mid")
```

```{note}
`symusic.Score` is a factory—pass a path, time signature info, or another score and it dispatches to
an optimized backend automatically. See the API reference for the full list of helper constructors.
```

For more extensive walkthroughs (data pipelines, piano roll conversions, synthesis), keep an eye on
the Tutorials section once the legacy mdBook pages are migrated.

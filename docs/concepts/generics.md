# Generic Core Bindings

Symusic's Python bindings mirror a templated C++ API. Every musically meaningful
object (scores, tracks, and event vectors) is parameterized by a time unit so the
same algorithms work on ticks, musical quarters, or absolute seconds. The binding
module exposes that structure directly through `typing.Generic` placeholders and
concrete low-level classes.

## Typing helpers vs. concrete classes

When you import :mod:`symusic.core`, you will see names such as `Score`, `Track`,
`Note`, or `PyVec`. These are *typing placeholders* that behave like
`typing.Generic[TimeUnitT]` definitions. They make type checkers aware that a
`ScoreTick` and a `ScoreSecond` share the same API but are not interchangeable.

At runtime you always instantiate concrete classes such as `ScoreTick`,
`ScoreQuarter`, or `ScoreSecond`. The same pattern applies to events
(`NoteTick`, `PedalQuarter`, etc.) and their list wrappers (`NoteTickList`). The
factories inside :mod:`symusic.factory` simply route each call to the matching
core class based on the requested time unit.

## Low-level API surface

The :doc:`/api/index` page now includes a dedicated *Low-Level symusic.core*
section that documents the canonical Tick specializations (Tick, Quarter,
Second, ScoreTick, TrackTick, NoteTick, and related events). Quarter/Second
variants behave identically except for the time data type.

Because the documentation reuses the real docstrings injected inside each
binding, you can rely on the descriptions to stay aligned with the binary that
was built locally for Read the Docs. If you need to debug a runtime edge case,
the new section provides a stable entry point into the raw nanobind classes.

## Working across factories and generics

Factories deliver ergonomic constructors while still exposing the typed nature of
the backend. For example:

```python
from symusic import Note, Score, TimeUnit

score = Score(960, ttype=TimeUnit.tick)
track = score.tracks[0]
track.notes.append(
    Note(time=0, duration=240, pitch=60, velocity=100, ttype=TimeUnit.quarter)
)
```

The `ttype` argument instructs the factory which `symusic.core` specialization to
use. Thanks to the new docstrings, both the factory object (`symusic.Note`) and
the low-level class (`symusic.core.NoteQuarter`) clearly describe the expected
values and behaviors.

```{note}
`TimeUnitT` and `EventT` type variables are exported directly from
`symusic.core` for linters that understand :mod:`typing`. They mirror the same
generics described here.
```

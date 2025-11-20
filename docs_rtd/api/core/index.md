(core-low-level-api)=
# Low-Level symusic.core API

The `symusic.core` module provides high-performance C++ bindings for MIDI data structures.
Most classes come in three time-unit flavors: **Tick** (integers), **Quarter** (floats), and **Second** (floats).

## Time Units

```{eval-rst}
.. autoclass:: symusic.core.Tick

.. autoclass:: symusic.core.Quarter

.. autoclass:: symusic.core.Second
```

## Scores and Tracks

```{eval-rst}
.. autoclass:: symusic.core.ScoreTick

**Other variants:** :class:`symusic.core.ScoreQuarter`, :class:`symusic.core.ScoreSecond` mirror this API on quarter-note or second timelines. Convert between them via :meth:`symusic.core.ScoreTick.to`.

.. autoclass:: symusic.core.TrackTick

**Other variants:** :class:`symusic.core.TrackQuarter`, :class:`symusic.core.TrackSecond` expose identical per-track operations on alternate time bases.
```

## Event Primitives

```{eval-rst}
.. autoclass:: symusic.core.NoteTick

**Other variants:** :class:`symusic.core.NoteQuarter`, :class:`symusic.core.NoteSecond`

.. autoclass:: symusic.core.ControlChangeTick

**Other variants:** :class:`symusic.core.ControlChangeQuarter`, :class:`symusic.core.ControlChangeSecond`

.. autoclass:: symusic.core.TempoTick

**Other variants:** :class:`symusic.core.TempoQuarter`, :class:`symusic.core.TempoSecond`

.. autoclass:: symusic.core.PedalTick

**Other variants:** :class:`symusic.core.PedalQuarter`, :class:`symusic.core.PedalSecond`

.. autoclass:: symusic.core.PitchBendTick

**Other variants:** :class:`symusic.core.PitchBendQuarter`, :class:`symusic.core.PitchBendSecond`

.. autoclass:: symusic.core.KeySignatureTick

**Other variants:** :class:`symusic.core.KeySignatureQuarter`, :class:`symusic.core.KeySignatureSecond`

.. autoclass:: symusic.core.TimeSignatureTick

**Other variants:** :class:`symusic.core.TimeSignatureQuarter`, :class:`symusic.core.TimeSignatureSecond`

.. autoclass:: symusic.core.TextMetaTick

**Other variants:** :class:`symusic.core.TextMetaQuarter`, :class:`symusic.core.TextMetaSecond`
```

## Utilities

```{eval-rst}
.. autoclass:: symusic.core.Synthesizer
```

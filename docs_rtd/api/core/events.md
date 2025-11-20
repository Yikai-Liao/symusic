# Event Primitives

## Notes

::::{tab-set}
:::{tab-item} Tick
```{eval-rst}
.. autoclass:: symusic.core.NoteTick
   :show-inheritance:

.. rubric:: Attributes
.. autoattribute:: symusic.core.NoteTick.time
.. autoattribute:: symusic.core.NoteTick.ttype
.. autoattribute:: symusic.core.NoteTick.start
.. autoattribute:: symusic.core.NoteTick.duration
.. autoattribute:: symusic.core.NoteTick.pitch
.. autoattribute:: symusic.core.NoteTick.velocity
.. autoattribute:: symusic.core.NoteTick.end

.. rubric:: Methods
.. automethod:: symusic.core.NoteTick.copy
.. automethod:: symusic.core.NoteTick.shift_time
.. automethod:: symusic.core.NoteTick.empty
.. automethod:: symusic.core.NoteTick.shift_pitch
.. automethod:: symusic.core.NoteTick.shift_velocity
```
:::
:::{tab-item} Quarter
```{eval-rst}
.. autoclass:: symusic.core.NoteQuarter
   :show-inheritance:

.. rubric:: Attributes
.. autoattribute:: symusic.core.NoteQuarter.time
.. autoattribute:: symusic.core.NoteQuarter.ttype
.. autoattribute:: symusic.core.NoteQuarter.start
.. autoattribute:: symusic.core.NoteQuarter.duration
.. autoattribute:: symusic.core.NoteQuarter.pitch
.. autoattribute:: symusic.core.NoteQuarter.velocity
.. autoattribute:: symusic.core.NoteQuarter.end

.. rubric:: Methods
.. automethod:: symusic.core.NoteQuarter.copy
.. automethod:: symusic.core.NoteQuarter.shift_time
.. automethod:: symusic.core.NoteQuarter.empty
.. automethod:: symusic.core.NoteQuarter.shift_pitch
.. automethod:: symusic.core.NoteQuarter.shift_velocity
```
:::
:::{tab-item} Second
```{eval-rst}
.. autoclass:: symusic.core.NoteSecond
   :show-inheritance:

.. rubric:: Attributes
.. autoattribute:: symusic.core.NoteSecond.time
.. autoattribute:: symusic.core.NoteSecond.ttype
.. autoattribute:: symusic.core.NoteSecond.start
.. autoattribute:: symusic.core.NoteSecond.duration
.. autoattribute:: symusic.core.NoteSecond.pitch
.. autoattribute:: symusic.core.NoteSecond.velocity
.. autoattribute:: symusic.core.NoteSecond.end

.. rubric:: Methods
.. automethod:: symusic.core.NoteSecond.copy
.. automethod:: symusic.core.NoteSecond.shift_time
.. automethod:: symusic.core.NoteSecond.empty
.. automethod:: symusic.core.NoteSecond.shift_pitch
.. automethod:: symusic.core.NoteSecond.shift_velocity
```
:::
::::

## Control Changes

::::{tab-set}
:::{tab-item} Tick
```{eval-rst}
.. autoclass:: symusic.core.ControlChangeTick
   :show-inheritance:

.. rubric:: Attributes
.. autoattribute:: symusic.core.ControlChangeTick.time
.. autoattribute:: symusic.core.ControlChangeTick.ttype
.. autoattribute:: symusic.core.ControlChangeTick.number
.. autoattribute:: symusic.core.ControlChangeTick.value

.. rubric:: Methods
.. automethod:: symusic.core.ControlChangeTick.copy
.. automethod:: symusic.core.ControlChangeTick.shift_time
```
:::
:::{tab-item} Quarter
```{eval-rst}
.. autoclass:: symusic.core.ControlChangeQuarter
   :show-inheritance:

.. rubric:: Attributes
.. autoattribute:: symusic.core.ControlChangeQuarter.time
.. autoattribute:: symusic.core.ControlChangeQuarter.ttype
.. autoattribute:: symusic.core.ControlChangeQuarter.number
.. autoattribute:: symusic.core.ControlChangeQuarter.value

.. rubric:: Methods
.. automethod:: symusic.core.ControlChangeQuarter.copy
.. automethod:: symusic.core.ControlChangeQuarter.shift_time
```
:::
:::{tab-item} Second
```{eval-rst}
.. autoclass:: symusic.core.ControlChangeSecond
   :show-inheritance:

.. rubric:: Attributes
.. autoattribute:: symusic.core.ControlChangeSecond.time
.. autoattribute:: symusic.core.ControlChangeSecond.ttype
.. autoattribute:: symusic.core.ControlChangeSecond.number
.. autoattribute:: symusic.core.ControlChangeSecond.value

.. rubric:: Methods
.. automethod:: symusic.core.ControlChangeSecond.copy
.. automethod:: symusic.core.ControlChangeSecond.shift_time
```
:::
::::

## Tempo

::::{tab-set}
:::{tab-item} Tick
```{eval-rst}
.. autoclass:: symusic.core.TempoTick
   :show-inheritance:

.. rubric:: Attributes
.. autoattribute:: symusic.core.TempoTick.time
.. autoattribute:: symusic.core.TempoTick.ttype
.. autoattribute:: symusic.core.TempoTick.mspq
.. autoattribute:: symusic.core.TempoTick.tempo
.. autoattribute:: symusic.core.TempoTick.qpm

.. rubric:: Methods
.. automethod:: symusic.core.TempoTick.copy
.. automethod:: symusic.core.TempoTick.shift_time
```
:::
:::{tab-item} Quarter
```{eval-rst}
.. autoclass:: symusic.core.TempoQuarter
   :show-inheritance:

.. rubric:: Attributes
.. autoattribute:: symusic.core.TempoQuarter.time
.. autoattribute:: symusic.core.TempoQuarter.ttype
.. autoattribute:: symusic.core.TempoQuarter.mspq
.. autoattribute:: symusic.core.TempoQuarter.tempo
.. autoattribute:: symusic.core.TempoQuarter.qpm

.. rubric:: Methods
.. automethod:: symusic.core.TempoQuarter.copy
.. automethod:: symusic.core.TempoQuarter.shift_time
```
:::
:::{tab-item} Second
```{eval-rst}
.. autoclass:: symusic.core.TempoSecond
   :show-inheritance:

.. rubric:: Attributes
.. autoattribute:: symusic.core.TempoSecond.time
.. autoattribute:: symusic.core.TempoSecond.ttype
.. autoattribute:: symusic.core.TempoSecond.mspq
.. autoattribute:: symusic.core.TempoSecond.tempo
.. autoattribute:: symusic.core.TempoSecond.qpm

.. rubric:: Methods
.. automethod:: symusic.core.TempoSecond.copy
.. automethod:: symusic.core.TempoSecond.shift_time
```
:::
::::

## Pedals

::::{tab-set}
:::{tab-item} Tick
```{eval-rst}
.. autoclass:: symusic.core.PedalTick
   :show-inheritance:

.. rubric:: Attributes
.. autoattribute:: symusic.core.PedalTick.time
.. autoattribute:: symusic.core.PedalTick.ttype
.. autoattribute:: symusic.core.PedalTick.start
.. autoattribute:: symusic.core.PedalTick.duration
.. autoattribute:: symusic.core.PedalTick.end

.. rubric:: Methods
.. automethod:: symusic.core.PedalTick.copy
.. automethod:: symusic.core.PedalTick.shift_time
```
:::
:::{tab-item} Quarter
```{eval-rst}
.. autoclass:: symusic.core.PedalQuarter
   :show-inheritance:

.. rubric:: Attributes
.. autoattribute:: symusic.core.PedalQuarter.time
.. autoattribute:: symusic.core.PedalQuarter.ttype
.. autoattribute:: symusic.core.PedalQuarter.start
.. autoattribute:: symusic.core.PedalQuarter.duration
.. autoattribute:: symusic.core.PedalQuarter.end

.. rubric:: Methods
.. automethod:: symusic.core.PedalQuarter.copy
.. automethod:: symusic.core.PedalQuarter.shift_time
```
:::
:::{tab-item} Second
```{eval-rst}
.. autoclass:: symusic.core.PedalSecond
   :show-inheritance:

.. rubric:: Attributes
.. autoattribute:: symusic.core.PedalSecond.time
.. autoattribute:: symusic.core.PedalSecond.ttype
.. autoattribute:: symusic.core.PedalSecond.start
.. autoattribute:: symusic.core.PedalSecond.duration
.. autoattribute:: symusic.core.PedalSecond.end

.. rubric:: Methods
.. automethod:: symusic.core.PedalSecond.copy
.. automethod:: symusic.core.PedalSecond.shift_time
```
:::
::::

## Pitch Bends

::::{tab-set}
:::{tab-item} Tick
```{eval-rst}
.. autoclass:: symusic.core.PitchBendTick
   :show-inheritance:

.. rubric:: Attributes
.. autoattribute:: symusic.core.PitchBendTick.time
.. autoattribute:: symusic.core.PitchBendTick.ttype
.. autoattribute:: symusic.core.PitchBendTick.value

.. rubric:: Methods
.. automethod:: symusic.core.PitchBendTick.copy
.. automethod:: symusic.core.PitchBendTick.shift_time
```
:::
:::{tab-item} Quarter
```{eval-rst}
.. autoclass:: symusic.core.PitchBendQuarter
   :show-inheritance:

.. rubric:: Attributes
.. autoattribute:: symusic.core.PitchBendQuarter.time
.. autoattribute:: symusic.core.PitchBendQuarter.ttype
.. autoattribute:: symusic.core.PitchBendQuarter.value

.. rubric:: Methods
.. automethod:: symusic.core.PitchBendQuarter.copy
.. automethod:: symusic.core.PitchBendQuarter.shift_time
```
:::
:::{tab-item} Second
```{eval-rst}
.. autoclass:: symusic.core.PitchBendSecond
   :show-inheritance:

.. rubric:: Attributes
.. autoattribute:: symusic.core.PitchBendSecond.time
.. autoattribute:: symusic.core.PitchBendSecond.ttype
.. autoattribute:: symusic.core.PitchBendSecond.value

.. rubric:: Methods
.. automethod:: symusic.core.PitchBendSecond.copy
.. automethod:: symusic.core.PitchBendSecond.shift_time
```
:::
::::

## Key Signatures

::::{tab-set}
:::{tab-item} Tick
```{eval-rst}
.. autoclass:: symusic.core.KeySignatureTick
   :show-inheritance:

.. rubric:: Attributes
.. autoattribute:: symusic.core.KeySignatureTick.time
.. autoattribute:: symusic.core.KeySignatureTick.ttype
.. autoattribute:: symusic.core.KeySignatureTick.key
.. autoattribute:: symusic.core.KeySignatureTick.tonality

.. rubric:: Methods
.. automethod:: symusic.core.KeySignatureTick.copy
.. automethod:: symusic.core.KeySignatureTick.shift_time
```
:::
:::{tab-item} Quarter
```{eval-rst}
.. autoclass:: symusic.core.KeySignatureQuarter
   :show-inheritance:

.. rubric:: Attributes
.. autoattribute:: symusic.core.KeySignatureQuarter.time
.. autoattribute:: symusic.core.KeySignatureQuarter.ttype
.. autoattribute:: symusic.core.KeySignatureQuarter.key
.. autoattribute:: symusic.core.KeySignatureQuarter.tonality

.. rubric:: Methods
.. automethod:: symusic.core.KeySignatureQuarter.copy
.. automethod:: symusic.core.KeySignatureQuarter.shift_time
```
:::
:::{tab-item} Second
```{eval-rst}
.. autoclass:: symusic.core.KeySignatureSecond
   :show-inheritance:

.. rubric:: Attributes
.. autoattribute:: symusic.core.KeySignatureSecond.time
.. autoattribute:: symusic.core.KeySignatureSecond.ttype
.. autoattribute:: symusic.core.KeySignatureSecond.key
.. autoattribute:: symusic.core.KeySignatureSecond.tonality

.. rubric:: Methods
.. automethod:: symusic.core.KeySignatureSecond.copy
.. automethod:: symusic.core.KeySignatureSecond.shift_time
```
:::
::::

## Time Signatures

::::{tab-set}
:::{tab-item} Tick
```{eval-rst}
.. autoclass:: symusic.core.TimeSignatureTick
   :show-inheritance:

.. rubric:: Attributes
.. autoattribute:: symusic.core.TimeSignatureTick.time
.. autoattribute:: symusic.core.TimeSignatureTick.ttype
.. autoattribute:: symusic.core.TimeSignatureTick.numerator
.. autoattribute:: symusic.core.TimeSignatureTick.denominator

.. rubric:: Methods
.. automethod:: symusic.core.TimeSignatureTick.copy
.. automethod:: symusic.core.TimeSignatureTick.shift_time
```
:::
:::{tab-item} Quarter
```{eval-rst}
.. autoclass:: symusic.core.TimeSignatureQuarter
   :show-inheritance:

.. rubric:: Attributes
.. autoattribute:: symusic.core.TimeSignatureQuarter.time
.. autoattribute:: symusic.core.TimeSignatureQuarter.ttype
.. autoattribute:: symusic.core.TimeSignatureQuarter.numerator
.. autoattribute:: symusic.core.TimeSignatureQuarter.denominator

.. rubric:: Methods
.. automethod:: symusic.core.TimeSignatureQuarter.copy
.. automethod:: symusic.core.TimeSignatureQuarter.shift_time
```
:::
:::{tab-item} Second
```{eval-rst}
.. autoclass:: symusic.core.TimeSignatureSecond
   :show-inheritance:

.. rubric:: Attributes
.. autoattribute:: symusic.core.TimeSignatureSecond.time
.. autoattribute:: symusic.core.TimeSignatureSecond.ttype
.. autoattribute:: symusic.core.TimeSignatureSecond.numerator
.. autoattribute:: symusic.core.TimeSignatureSecond.denominator

.. rubric:: Methods
.. automethod:: symusic.core.TimeSignatureSecond.copy
.. automethod:: symusic.core.TimeSignatureSecond.shift_time
```
:::
::::

## Text Meta

::::{tab-set}
:::{tab-item} Tick
```{eval-rst}
.. autoclass:: symusic.core.TextMetaTick
   :show-inheritance:

.. rubric:: Attributes
.. autoattribute:: symusic.core.TextMetaTick.time
.. autoattribute:: symusic.core.TextMetaTick.ttype
.. autoattribute:: symusic.core.TextMetaTick.text

.. rubric:: Methods
.. automethod:: symusic.core.TextMetaTick.copy
.. automethod:: symusic.core.TextMetaTick.shift_time
```
:::
:::{tab-item} Quarter
```{eval-rst}
.. autoclass:: symusic.core.TextMetaQuarter
   :show-inheritance:

.. rubric:: Attributes
.. autoattribute:: symusic.core.TextMetaQuarter.time
.. autoattribute:: symusic.core.TextMetaQuarter.ttype
.. autoattribute:: symusic.core.TextMetaQuarter.text

.. rubric:: Methods
.. automethod:: symusic.core.TextMetaQuarter.copy
.. automethod:: symusic.core.TextMetaQuarter.shift_time
```
:::
:::{tab-item} Second
```{eval-rst}
.. autoclass:: symusic.core.TextMetaSecond
   :show-inheritance:

.. rubric:: Attributes
.. autoattribute:: symusic.core.TextMetaSecond.time
.. autoattribute:: symusic.core.TextMetaSecond.ttype
.. autoattribute:: symusic.core.TextMetaSecond.text

.. rubric:: Methods
.. automethod:: symusic.core.TextMetaSecond.copy
.. automethod:: symusic.core.TextMetaSecond.shift_time
```
:::
::::
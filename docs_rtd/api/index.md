# Python API Reference

This section is generated from the actual source tree so it stays in sync with releases. The entries
below focus on the user-facing factories that mirror the `symusic` namespace.

```{eval-rst}
.. currentmodule:: symusic

.. autosummary::
   :toctree: generated
   :caption: Core Factories
   :nosignatures:

   Score
   Track
   Note
   ControlChange
   Tempo
   Pedal
   PitchBend
   KeySignature
   TimeSignature
   TimeUnit
   Synthesizer
   dump_wav
```

```{note}
Most Python surfaces are thin factories built on top of nanobind classes defined in
`symusic.core`. Although the binary extension is mocked for documentation builds,
the signatures and docstrings come from `python/symusic/` and `python/symusic/core.pyi`.
```

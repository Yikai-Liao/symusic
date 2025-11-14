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
```

```{note}
Most Python surfaces are thin factories built on top of nanobind classes defined in
`symusic.core`. Although the binary extension is mocked for documentation builds,
the signatures and docstrings come from `python/symusic/` and `python/symusic/core.pyi`.

The section above lists the lightweight factories that live in the pure-Python package.
The nanobind classes/functions that actually execute the work are documented below so
their docstrings show up verbatim.
```

## Core bindings

```{eval-rst}
.. currentmodule:: symusic.core

.. autosummary::
   :toctree: generated/core
   :caption: Binding Classes

   Synthesizer
```

```{eval-rst}
.. currentmodule:: symusic.core

.. autofunction:: dump_wav
```

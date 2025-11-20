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

```{toctree}
:maxdepth: 1
:caption: Audio Synthesis

generated/symusic.Synthesizer
generated/symusic.dump_wav
```

```{toctree}
:maxdepth: 1
:caption: SoundFonts

generated/symusic.Soundfont
generated/symusic.BuiltInSF2
generated/symusic.BuiltInSF3
```

```{note}
Most Python surfaces are thin factories built on top of nanobind classes defined in
`symusic.core`. The docstrings you see here are injected at bind time so they
accurately describe the compiled extension even though the documentation focuses on
the high-level `symusic` namespace.
```

See {doc}`/concepts/generics` for a guided overview of how the factories relate to their
templated `symusic.core` counterparts, and use {doc}`/api/core/index` to explore the raw
Tick-based classes that power the binding.

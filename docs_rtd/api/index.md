# Python API Reference

This section is generated from the actual source tree so it stays in sync with releases. The entries
below focus on the user-facing factories that mirror the `symusic` namespace.

```{eval-rst}
.. autosummary::
   :toctree: _autosummary
   :caption: Core Factories
   :nosignatures:

   symusic.Score
   symusic.Track
   symusic.Note
   symusic.ControlChange
   symusic.Tempo
   symusic.Pedal
   symusic.PitchBend
   symusic.KeySignature
   symusic.TimeSignature
   symusic.TimeUnit

.. autosummary::
   :toctree: _autosummary
   :caption: Audio Synthesis

   symusic.Synthesizer
   symusic.dump_wav

.. autosummary::
   :toctree: _autosummary
   :caption: SoundFonts

   symusic.Soundfont
   symusic.BuiltInSF2
   symusic.BuiltInSF3

.. autosummary::
   :toctree: _autosummary
   :caption: Low-Level Modules
   :recursive:

   symusic.core
   symusic.factory
   symusic.soundfont
   symusic.types
   symusic.demo
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

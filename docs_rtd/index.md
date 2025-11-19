---
title: Symusic Docs
---

# Symusic Documentation (WIP)

This site hosts the in-progress Sphinx documentation for the upcoming **symusic 1.0** release. It
consolidates Python and C++ references, tutorials, and migration notes that will eventually replace
the legacy mdBook.

> **Heads up:** the old mdBook under `docs/src` is still served on GitHub Pages, but much of that
> content is stale. The sections below highlight the refreshed structure and call out legacy
> references explicitly.

```{toctree}
:maxdepth: 1
:caption: Start Here

introduction
quickstart
faq
project_notes
```

```{toctree}
:maxdepth: 1
:caption: Tutorials

tutorials/index
tutorials/midi_operations
tutorials/pianoroll
tutorials/synthesis
tutorials/data_processing
```

```{toctree}
:maxdepth: 1
:caption: Core Concepts

concepts/index
concepts/score
concepts/track
concepts/events
concepts/time_units
concepts/generics
concepts/synthesis
```

```{toctree}
:maxdepth: 1
:caption: API Reference

api/index
api/core/index
```

```{toctree}
:maxdepth: 1
:caption: Legacy Material

legacy
```

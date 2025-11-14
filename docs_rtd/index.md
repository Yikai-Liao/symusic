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
project_notes
```

```{toctree}
:maxdepth: 1
:caption: Core Concepts

concepts/score
concepts/track
concepts/events
concepts/time_units
concepts/synthesis
```

```{toctree}
:maxdepth: 1
:caption: API Reference

api/index
```

```{toctree}
:maxdepth: 1
:caption: Legacy Material

legacy
```

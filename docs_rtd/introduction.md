# Introduction

Symusic provides lightning-fast note-level MIDI processing via a modern C++20 core with nanobind
Python bindings. The 1.0 documentation program focuses on three goals:

1. A reliable home on Read the Docs with searchable, versioned pages.
2. Automatic API coverage from the source tree (Python docstrings and C++ Doxygen blocks).
3. Curated onboarding content so new users can complete real tasks without hunting through stale posts.

## Current status

- ✅ **New Sphinx stack** (this site) generated from `docs_rtd/` using MyST + Furo.
- ✅ **Python autodoc skeleton** powered directly from the `python/symusic` package.
- 🚧 **Tutorial migration** from the old mdBook and `tutorial.ipynb`.
- 🚧 **C++ internals** via Doxygen + Breathe.

Until all chapters are ported, you can still browse the legacy mdBook at
<https://yikai-liao.github.io/symusic/>. Expect discrepancies—feature behavior in the codebase takes
precedence over whatever the mdBook says.

## Building locally

```bash
pip install -r docs_rtd/requirements.txt
sphinx-build -b html docs_rtd docs_rtd/_build/html
```

This command mirrors what Read the Docs and CI will run. The generated HTML lands in
`docs_rtd/_build/html` for spot checks.

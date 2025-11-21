# Documentation with Pagefind Search

This documentation uses [Pagefind](https://pagefind.app/) for fast static search functionality.

## Prerequisites

Before building the documentation, ensure you have:

- **Python** with Sphinx and required extensions (see `docs/requirements.txt`)
- **Node.js and npm** (for Pagefind)

Install Pagefind globally (optional but recommended):
```bash
npm install -g pagefind
```

Or use `npx` to run it without installation (will download on first use).

## Building Documentation

### Important Note
**Always delete the `_build` directory before rebuilding** to ensure proper Pagefind integration:
```bash
rm -rf docs/_build
```

### Quick Build
```bash
./build_docs_with_pagefind.sh
```

### Manual Build
```bash
# 1. Clean previous build (important!)
rm -rf docs/_build

# 2. Build Sphinx documentation
sphinx-build -b html docs docs/_build/html

# 3. Generate Pagefind index
npx pagefind --site docs/_build/html --output-subdir _pagefind
```

## Local Preview
```bash
npx pagefind --serve --site docs/_build/html
```

Then visit: http://localhost:1414/search.html

## Configuration Files

- `pagefind.yml` - Pagefind configuration
- `_templates/search.html` - Custom search page template
- `conf.py` - Sphinx configuration with `html_additional_pages`

## Features

- **Static Search**: No backend server required
- **Fast Indexing**: ~0.17 seconds for 37 pages
- **Auto-fill**: Query parameters from sidebar search automatically populate the search box
- **Theme Integration**: Styled to match the Furo theme with proper dark mode support

## Docstring → Docs Pipeline

Follow this loop whenever you update nanobind docstrings so `.pyi` files, local previews, and Read the Docs stay aligned.

1. **Author docstrings at the binding site**  
   Use `nb::doc(R"pbdoc(...)pbdoc")` alongside each definition in `py_src/core.cpp` (or the relevant `.cpp`) so the compiled `symusic.core` exposes real descriptions.

2. **Install the extension into the docs virtualenv**  
   ```bash
   source .venv-docs/bin/activate
   pip install -e .
   ```
   Read the Docs can’t build from source on every run, so `readthedocs.yml` instead executes `pip install symusic` to fetch the published wheel. Ship a new wheel whenever docstrings change.

3. **Regenerate `.pyi` stubs (optional but recommended)**  
   ```bash
   nanobind-stubgen symusic.core --out .
   mv symusic.core.pyi python/symusic/core.pyi
   ```
   Editors and type-checkers then surface the exact same narratives.

4. **Rebuild Sphinx + Pagefind**  
   ```bash
   sphinx-build -b html docs docs/_build/html
   npx pagefind --site docs/_build/html --output-subdir _pagefind
   ```
   Verify under `docs/_build/html/api/_autosummary/symusic.core.html` that “Core bindings” pages show the updated text.

Repeat this cycle whenever bindings change to keep local previews and the hosted site consistent.

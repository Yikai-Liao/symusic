# Documentation with Pagefind Search

This documentation uses [Pagefind](https://pagefind.app/) for fast static search functionality.

## Prerequisites

Before building the documentation, ensure you have:

- **Python** with Sphinx and required extensions (see `docs_rtd/requirements.txt`)
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
rm -rf docs_rtd/_build
```

### Quick Build
```bash
./build_docs_with_pagefind.sh
```

### Manual Build
```bash
# 1. Clean previous build (important!)
rm -rf docs_rtd/_build

# 2. Build Sphinx documentation
sphinx-build -b html docs_rtd docs_rtd/_build/html

# 3. Generate Pagefind index
npx pagefind --site docs_rtd/_build/html --output-subdir _pagefind
```

## Local Preview
```bash
npx pagefind --serve --site docs_rtd/_build/html
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

# Documentation Notes

The documentation is migrating from the legacy mdBook to this Sphinx + MyST stack. This page
explains why the move happened and how contributors can preview the site locally before the Read the
Docs deployment.

## Why a new documentation site?

The mdBook served as a convenient scratchpad, but it lagged behind the code base. API changes in the
nanobind layer rarely reached the prose, and tutorials embedded in notebooks aged quickly. The Sphinx
stack addresses those pain points by:

1. Pulling signatures and docstrings directly from the Python package (stubs now, Doxygen bindings in
   Phase 3).
2. Running on Read the Docs so every release has a matching set of versioned pages.
3. Hosting tutorials, conceptual guides, and API references in a single navigable tree while still
   linking back to the mdBook for historical context.

## Building the docs locally

These steps mirror the Read the Docs build process:

```bash
python -m venv .venv-docs
source .venv-docs/bin/activate
pip install -r docs_rtd/requirements.txt
sphinx-build -b html docs_rtd docs_rtd/_build/html
```

Open `docs_rtd/_build/html/index.html` in your browser to preview the site. Remember to remove the
local virtual environment when you are done (`rm -rf .venv-docs`).

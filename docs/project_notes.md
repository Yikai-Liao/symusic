# Documentation Notes

The documentation now publishes primarily on Read the Docs at
<https://symusic.readthedocs.io/en/stable/>. This page explains why the move happened and how
contributors can preview the site locally before changes are published there.

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
python -m venv .venv
source .venv/bin/activate
pip install -r docs/requirements.txt
git submodule update --init --recursive --depth 1
pip install . -C cmake.define.SYMUSIC_FAST_BUILD=ON
sphinx-build -b html docs docs/_build/html
```

Open `docs/_build/html/index.html` in your browser to preview the site. Remember to remove the
local build artifacts when you are done if you want a clean tree (`rm -rf docs/_build`).

## Contributing to the documentation

When working on documentation and docstrings:

1. Edit files under `docs/` rather than the legacy mdBook in `docs/src`.
2. Keep prose concise and task-oriented; prefer small, composable pages over monolithic guides.
3. Run a local Sphinx build (`sphinx-build -b html docs docs/_build/html`) before opening a PR.

### Docstring conventions and nanobind bindings

- Python docstrings should follow Google- or NumPy-style conventions so that `sphinx.ext.napoleon`
  can render them cleanly.
- When adding or updating C++ bindings via nanobind, keep docstrings close to the binding site. Use
  `nb::doc("...")` helpers so that the eventual C++ reference (via Doxygen+Breathe) can reuse the same
  text.

### Notebooks and myst-nb

The historical tutorial notebook is not part of the active documentation workflow anymore. If we
bring a notebook back into the tree, refresh every cell against the current API first and confirm
that the examples are still endorsed before wiring it into MyST-NB.

Until then:

- Keep notebook experiments outside `docs/` and out of the navigation.
- Track any notebook-refresh work in `docs/TODO.md`.

## Legacy mdBook reference

While the mdBook remains online for historical context, the Sphinx tree is now the canonical source.
Use the table below when redirecting deep links or triaging stale references:

| Legacy mdBook (`docs/src`) | New Sphinx docs (`docs/`) |
| --- | --- |
| `introduction.md` | {doc}`/introduction` |
| `quick_start.md` | {doc}`/quickstart` |
| `tutorials/*` | {doc}`/tutorials/index` and its subpages |
| `examples/*` | {doc}`/examples/index` and its subpages |
| `api_reference*` | {doc}`/api/index` |
| `development.md` | {doc}`/development` |
| top-level overview | {doc}`/index` |

When updating downstream content:

- Prefer linking to the Sphinx site and include the appropriate version (Read the Docs will host tags).
- Flag discrepancies between the code and these docs via GitHub issues so we can keep both sources in
  sync during the transition period.

### Continuous integration / Read the Docs

The Read the Docs configuration lives in `readthedocs.yml`. It installs dependencies from
`docs/requirements.txt`, initializes the submodules, and then builds the local checkout with
`pip install . -C cmake.define.SYMUSIC_FAST_BUILD=ON` so the hosted pages reflect the current
nanobind bindings and docstrings from the repository.

When you add a docs-only dependency, update `docs/requirements.txt` rather than the project-wide
runtime dependencies. Once the Sphinx build passes locally, the same source tree should publish
cleanly on Read the Docs.

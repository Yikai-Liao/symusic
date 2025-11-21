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
pip install -r docs/requirements.txt
pip install -e .
sphinx-build -b html docs docs/_build/html
```

Open `docs/_build/html/index.html` in your browser to preview the site. Remember to remove the
local virtual environment when you are done (`rm -rf .venv-docs`).

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

The public tutorial notebook (`tutorial.ipynb` at the repo root) is currently **out of date**.
Before wiring it into the docs with myst-nb, refresh every cell to reflect the latest API and confirm
with the interface owners that the examples are still endorsed. While it remains stale:

- Keep the notebook outside `docs/` and do not include it in the navigation.
- Track the refresh work in `docs/TODO.md` (see the blocked entry for myst-nb integration).

Once the notebook is updated and approved, we can move it under `docs/`, enable myst-nb rendering,
and follow the usual hygiene guidelines (deterministic execution, concise outputs, no heavyweight
downloads).

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
| `development.md` | {doc}`/project_notes` (this page) |
| top-level overview | {doc}`/index` |

When updating downstream content:

- Prefer linking to the Sphinx site and include the appropriate version (Read the Docs will host tags).
- Flag discrepancies between the code and these docs via GitHub issues so we can keep both sources in
  sync during the transition period.

### Continuous integration / Read the Docs

The Read the Docs configuration lives in `readthedocs.yml` and installs dependencies from
`docs/requirements.txt`, followed by `pip install symusic` so it can pull the prebuilt wheels
from PyPI and expose the nanobind docstrings. When you add a new dependency for docs only, update
that requirements file rather than the project-wide dependencies.

Once the Sphinx build passes locally, CI should produce identical HTML on Read the Docs.

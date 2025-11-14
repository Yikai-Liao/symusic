# Documentation Migration Status
- Sphinx-based docs live under `docs_rtd/` (Furo theme, MyST/myst-nb). `index.md` organizes Start Here, Core Concepts, API Reference, Legacy.
- Introduction/Quickstart rewritten; Core Concepts (Score/Track/Events/Time Units/Synthesis) migrated from mdBook; `project_notes.md` holds background + local build steps.
- `docs_rtd/TODO.md` tracks remaining tasks: tutorials/examples migration, myst-nb for `tutorial.ipynb`, FAQ/migration guide, Doxygen+Breathe, contribution guidelines, switch to RTD badge once live.
- Virtual env suggestion: `.venv-docs` with deps listed in `docs_rtd/requirements.txt` (includes numpy, pySmartDL, platformdirs for doc builds).

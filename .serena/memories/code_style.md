# Code Style & Conventions
- C++: formatted via `.clang-format` (LLVM-derived, 4-space indent, braces mostly attached, align assignments/declarations) with C++20 standard and LTO defaults; keep headers under `include/symusic` and implementations mirroring paths in `src/`.
- Python: linted and auto-formatted by Ruff (`ruff` + `ruff-format` via pre-commit); target Python 3.8+, expect type hints and nanobind-exposed APIs declared in `python/symusic/*.py` plus `core.pyi` stubs.
- Pre-commit: install `pre-commit` and run `pre-commit run --all-files` (or `./pre_commit.sh`) before committing; enforces Ruff lint/format, EOF fixer, and trailing whitespace cleanup.
- Stub generation: run `./generate_stub.sh` (uses `nanobind-stubgen`) whenever bindings change, but manually fix import sections afterwards per `docs/src/development.md`.
- Documentation: mdBook sources under `docs/src`; follow existing Markdown tone until the new RTD pipeline is defined.

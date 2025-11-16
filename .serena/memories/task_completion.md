# Task completion checklist
- Ensure new or modified code builds for both the C++ core and Python bindings (`cmake --build build` or `pip install -e .`).
- Run the Python test suite (`pytest tests/`) and the C++ tests (`ctest --test-dir build`) after touching their respective layers.
- Regenerate nanobind stubs (`./generate_stub.sh`) and confirm `.pyi`/bindings remain lint-clean when API/types change.
- Execute `pre-commit run --all-files` (or the relevant hooks) to keep Ruff/clang-format consistent before sharing a patch.
- Update documentation (`docs`, `docs_rtd`, README) when behavior or configuration changes; rebuild/preview with `mdbook build docs`.
- Double-check packaging metadata (`py_src`, `python/symusic/__init__.py`, `pyproject.toml`) whenever interfaces or build scripts are touched.

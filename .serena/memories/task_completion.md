# Task completion checklist
- Ensure new/modified code builds for both core C++ and Python bindings when relevant (via `cmake --build` or `pip install -e .`).
- Run `pytest` for Python-facing changes; enable `ctest` when touching low-level C++/IO modules.
- Execute `pre-commit run --all-files` (Ruff, clang-format, etc.) or the relevant subset before handing off patches.
- Update docs (`docs`/`docs_rtd`) or README when behavior or configuration changes.
- Verify packaging metadata if interface changes impact Python API (`python/symusic/__init__.py`).

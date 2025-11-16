# Coding style and conventions
- C++: enforced via `.clang-format` (LLVM base tweaked for aligned assignments/declarations, block-indent after brackets). Targets C++20, PI-only, headers in `include/`, `src/` with descriptive namespaces. Avoid non-UTF8 when possible; MSVC build expects /utf-8.
- Python: linted/formatted with Ruff (pyproject config) targeting py38; numerous lint rules enabled, tests allowed prints/asserts. Type hints expected; package exposes clean public API via `python/symusic/__init__.py`. Keep modules import-clean and avoid unused code per Ruff.
- Build configs: `pyproject.toml` defines scikit-build + CMake options plus pytest strict markers/warnings-as-errors. Respect README guidance for reproducible builds and optional MEM_LEAK_WARNING flag.
- General principles: prefer simplicity/clarity (matches project's performance focus), keep serialization-friendly structures, document non-obvious logic inline sparingly.

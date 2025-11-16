# Useful commands
- Install Python package locally (with submodules): `pip install -e .`
- Build from source wheel/bindings manually: `cmake -S . -B build -DBUILD_SYMUSIC_PY=ON && cmake --build build -j` (add other `-D` flags such as `ENABLE_COVERAGE=ON` as needed).
- Enable nanobind memory leak warnings during pip install: `pip install -Ccmake.define.MEM_LEAK_WARNING=True ./symusic`
- Run Python test suite: `pytest` (strict warnings, configured paths under `tests`).
- Execute C++ tests (if configured): `cmake -S . -B build -DBUILD_SYMUSIC_TEST=ON && cmake --build build && ctest --test-dir build`
- Lint/format via pre-commit hooks: `pre-commit run --all-files` (wraps Ruff/clang-format etc.).

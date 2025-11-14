# Suggested Commands
- Install Python deps locally: `pip install -e .[test]` (root dir) to build the nanobind wheel and install pytest extras.
- Run Python tests: `pytest tests/` (use `-k` or file::test selectors for focus); respects `pyproject.toml` pytest config.
- Build C++ + Python binding via CMake: `cmake -S . -B build -DBUILD_SYMUSIC_PY=ON -DBUILD_SYMUSIC_TEST=ON && cmake --build build -j` (set `ENABLE_COVERAGE=ON` or `MEM_LEAK_WARNING=True` via `-D` flags as needed).
- Execute Catch2 C++ tests: `cmake -S . -B build -DBUILD_SYMUSIC_TEST=ON && cmake --build build -j && ./build/tests/cpp/symusic_tests` (or `./build/tests/cpp/symusic_tests "[score]"`).
- Static checks/formatting: `pre-commit run --all-files` or `./pre_commit.sh` to apply Ruff lint/format + whitespace fixes.
- Regenerate `.pyi`: `./generate_stub.sh` (requires `nanobind-stubgen`), then review diffs and re-run pre-commit.
- mdBook preview: `mdbook serve docs` (or `mdbook build docs`) to render the current documentation set.

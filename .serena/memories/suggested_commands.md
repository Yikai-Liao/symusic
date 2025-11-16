# Suggested Commands
- Install Python dependencies with test extras: `pip install -e .[test]` so pytest and binding tools rebuild automatically.
- Build the core C++/Python artifacts in one shot: `cmake -S . -B build -DBUILD_SYMUSIC_PY=ON -DBUILD_SYMUSIC_TEST=ON && cmake --build build -j` (add `-DENABLE_COVERAGE=ON`/`-DMEM_LEAK_WARNING=True` as needed).
- Enable nanobind memory leak warnings during install when debugging: `pip install -Ccmake.define.MEM_LEAK_WARNING=True ./symusic`.
- Run the Python test suite: `pytest tests/` (scope with `-k` if needed).
- Run the Catch2/ctest workloads: `cmake -S . -B build -DBUILD_SYMUSIC_TEST=ON && cmake --build build && ctest --test-dir build`.
- Enforce formatting/lint via `pre-commit run --all-files` (or `./pre_commit.sh`) before pushing changes.
- Regenerate nanobind stubs: `./generate_stub.sh` (requires `nanobind-stubgen`) and replay pre-commit afterwards.
- Preview docs via `mdbook serve docs` (or `mdbook build docs`) to validate links/SUMMARY.

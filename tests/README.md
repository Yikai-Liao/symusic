# symusic Test Suite

This directory contains regression coverage for both the C++ core and the Python package.

## Layout

- `cpp/`: Catch2-based C++ tests
- `testcases/`: MIDI / ABC fixtures and edge-case files
- `typecheck/`: mypy-based typing regressions
- `test_*.py`: pytest-based Python regressions

## Python tests

Build and install the package into an active virtual environment before running pytest. The test
suite imports the compiled extension, so stale installs will give misleading results.

```bash
python -m venv .venv
source .venv/bin/activate
pip install -e ".[test]"
pytest -s --cov=symusic --cov-report=xml -n auto --durations=0 -v tests
```

Useful targeted commands:

```bash
pytest tests/test_py_bindings.py
pytest tests/test_py_bindings.py::test_note_creation
pytest tests/typecheck/test_typing_mypy.py
pytest tests/test_beats.py tests/test_beats_pretty_midi.py
pytest tests/test_abc.py tests/test_malformed_midi.py
```

## C++ tests

Configure the project with `BUILD_SYMUSIC_TEST` and run the suite through `ctest`:

```bash
cmake -S . -B build -DBUILD_SYMUSIC_TEST:BOOL=ON -DCMAKE_BUILD_TYPE=Debug
cmake --build build --config Debug
ctest --test-dir build -C Debug --output-on-failure --verbose
```

If you want to invoke the Catch2 binary directly, the executable is named `symusic_test`.

## Coverage focus

The suite covers:

- Score / Track / Event behavior across `tick`, `quarter`, and `second`
- MIDI and ABC parsing, round-trips, and malformed-file regressions
- Beat and downbeat extraction
- Piano-roll generation and ownership semantics
- Pickle support and multiprocessing-friendly serialization
- Unicode and shell-sensitive path handling
- Python typing regressions under mypy

## Adding new tests

### C++ tests

1. Add a new header under `tests/cpp/`.
2. Include it from `tests/cpp/test_main.cpp`.
3. Keep fixtures in `tests/testcases/` when the case depends on external files.

### Python tests

1. Add a new `test_*.py` file under `tests/`.
2. Prefer focused regression cases over broad integration scripts.
3. Reinstall the package after changing compiled bindings before re-running pytest.

## Continuous integration

GitHub Actions runs both the Python and C++ suites. See `.github/workflows/tests.yml` and
`.github/workflows/test_wheel.yml` for the current matrix.

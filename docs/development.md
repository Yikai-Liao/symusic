# Development

## Create a local environment

Symusic development is easiest when you keep everything inside a project-local virtual environment.
Clone the repository with submodules, create `.venv`, and install the package through `pip` so the
compiled extension and Python package stay in sync.

```bash
git clone --recursive https://github.com/Yikai-Liao/symusic
cd symusic
python -m venv .venv
source .venv/bin/activate
python -m pip install --upgrade pip
pip install -e ".[test]"
pip install -r docs/requirements.txt
```

On Windows, activate the environment with `.venv\\Scripts\\activate` instead.

Symusic targets C++20. GCC 11+, Clang 15+, and MSVC 2022 are the supported compiler baselines.
CPython 3.12 can still emit harmless nanobind leak warnings during local work; 3.11 remains the
smoothest choice if you want less noise.

## Build the Python extension

The supported contributor workflow is to build the extension through `pip`, not by manually driving
the Python bindings from raw CMake targets.

```bash
pip install .
```

That command rebuilds `symusic.core`, regenerates the installed `core.pyi` / `py.typed` artifacts,
and refreshes the active environment. If you changed bindings, factories, or typing-related files,
reinstall before running tests. Otherwise you will be debugging stale binaries or stale stubs.

## Run the test suite

### Python tests

```bash
pytest -s --cov=symusic --cov-report=xml -n auto --durations=0 -v tests
```

If you only need the typing regressions:

```bash
pytest tests/typecheck/test_typing_mypy.py
```

### C++ tests

```bash
cmake -S . -B build -DBUILD_SYMUSIC_TEST:BOOL=ON -DCMAKE_BUILD_TYPE=Debug
cmake --build build --config Debug
ctest --test-dir build -C Debug --output-on-failure --verbose
```

Use a Visual Studio Developer shell on Windows if you want `pip install .` or CMake to pick up the
expected MSVC toolchain automatically.

## Stubs and type hints

nanobind generates the installed stubs as part of the normal build. Whenever `pip install .` or
`pip install -e .` rebuilds the package, the build also refreshes `symusic/core.pyi` and the
`py.typed` marker.

The typing surface intentionally mirrors the tick / quarter / second specializations exposed by the
compiled extension. When you need explicit annotations, prefer the generic helpers from
`symusic.types` and `symusic.core`, for example:

```python
import symusic.core as core
import symusic.types as smt

score: smt.Score[core.Tick] = core.ScoreTick(480)
notes: core.PyVec[core.Note[core.Tick]] = core.NoteTickList()
```

## Formatting and hooks

Install the project hooks with:

```bash
./scripts/install_git_hooks.sh
```

Run formatting and lint checks manually with:

```bash
pre-commit run --all-files
```

The pre-commit configuration lives in `.pre-commit-config.yaml`.

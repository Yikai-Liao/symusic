# FAQ & Troubleshooting

This page collects short answers to common setup and runtime issues when using Symusic.

## Installation problems

### `pip install symusic` falls back to a source build

Symusic publishes wheels for Linux, macOS, and Windows on CPython 3.8–3.12 (plus PyPy 3.9). If `pip`
starts compiling from source:

- Check that your Python version is supported (`python -m pip debug --verbose` is helpful).
- Upgrade `pip`, `setuptools`, and `wheel` in your environment.
- On ARM or less common platforms, prefer a recent CPython release before trying to build from source.

If a wheel is not available for your platform, follow the source build instructions in {doc}`/quickstart`.

### Compiler or toolchain errors when building from source

Symusic requires a C++20-capable toolchain:

- GCC 11 or newer
- Clang 15 or newer
- MSVC 2022 on Windows

When builds fail with compiler errors:

- Verify the compiler version with `g++ --version` / `clang++ --version` / `cl` and upgrade if needed.
- On Linux without sudo, install `gcc` from conda and set `CC`/`CXX` as shown in {doc}`/introduction`.
- Ensure the repository was cloned with submodules: `git clone --recursive ...`.

### Importing `symusic` fails after installation

If `python -c "import symusic"` raises an `ImportError` or complains about missing extension modules:

- Confirm you are running the same Python interpreter that you used for `pip install`.
- Check for multiple installations: `python -m pip show symusic` and `pip show symusic` should point to
  the same environment.
- Reinstall inside a clean virtual environment to rule out conflicts:

```bash
python -m venv .venv-symusic
source .venv-symusic/bin/activate
pip install symusic
python -c "import symusic; print(symusic.__version__)"
```

## Common runtime issues

### File cannot be opened or parsed

Errors such as `FileNotFoundError`, `IOError`, or format errors when creating a `Score` usually trace
back to one of:

- Wrong path (relative vs. absolute). Use `Path` objects or `pathlib.Path.resolve()` for clarity.
- Unsupported or corrupted files. Symusic focuses on MIDI and ABC; other formats may not be accepted.
- Permission issues when reading from network drives or temporary directories.

Printing `Path(path).resolve()` before loading often makes path issues obvious.

### Confusion around time units

Symusic works in three time domains: `tick`, `quarter`, and `second`. Mixing them accidentally can
lead to surprising results (e.g., unexpected durations or densities).

- Decide on a time unit at the start of a workflow and stick to it.
- Use `Score(..., ttype="second")` when computing rates or densities over wall-clock time.
- Convert explicitly with `score.to("second")` or `score.to("quarter")` before heavy analysis.

See {doc}`concepts/time_units` for a deeper discussion.

### Slow analysis over large datasets

If a pipeline over thousands of MIDI files is slower than expected:

- Avoid re-reading the same file repeatedly; reuse `Score` objects when possible.
- Use multiprocessing as shown in {doc}`tutorials/data_processing`.
- Prefer structured NumPy exports (`track.notes.numpy()`) over Python loops inside tight inner loops.

## Getting help

If you run into an issue that is not covered here:

- Check the open and closed issues on the GitHub repository.
- Open a new issue with:
  - OS, Python, and compiler versions
  - `symusic.__version__`
  - A minimal script and, if possible, a small MIDI file that reproduces the problem.

Clear, minimal reproductions make it much easier to fix bugs and improve the documentation.

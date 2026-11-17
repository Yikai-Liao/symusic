# Contributing to symusic

Bug reports with a failing file or snippet, fixes and new features are welcome. Discuss changes larger than a small fix in an issue first. Read the [architecture](docs/design/architecture.md) before changing code.

## Set up

Clone with submodules and work inside a virtual environment, never the system Python. The project uses [uv](https://docs.astral.sh/uv/) for environments and tools; plain `pip` inside a venv works too.

```sh
git clone --recursive https://github.com/Yikai-Liao/symusic
cd symusic                      # or: git submodule update --init --recursive --depth 1
uv venv -p 3.12 && source .venv/bin/activate
uv pip install ".[test]" -r docs/requirements.txt
uv tool install prek && prek install   # git hooks from prek.toml
```

The hooks format C++ with [clice](https://github.com/clice-io/clice), which must be on `PATH`: download a release, or on Linux x86_64 run `.github/scripts/install_clice.sh` and add the directory it prints to `PATH`.

Use a regular CPython build. Free-threaded builds (`3.13t`, `3.14t`) are not supported yet: importing the extension emits a GIL `RuntimeWarning`, which pytest turns into an error. The compiler must support C++20 (GCC 11+, Clang 15+ or MSVC 2022). On Windows, run from a Visual Studio Developer shell so `pip install .` uses Ninja and MSVC.

## Build

Build and install the extension with pip, not by driving CMake targets by hand:

```sh
pip install . -C cmake.define.SYMUSIC_FAST_BUILD=ON   # -O1, no LTO: for local iteration
pip install .                                         # release flags
```

Reinstall after every change to `src/`, `include/`, `py_src/`, `python/` or the stub script. The installed package, not the source tree, is what tests import, so a stale install tests old code. Installing also regenerates `symusic/core.pyi`. `-C cmake.define.MEM_LEAK_WARNING=ON` enables nanobind leak reports.

## Validate

Run the checks for every area you touched:

| Change | Checks |
|---|---|
| Core (`include/`, `src/`) | C++ tests below, then reinstall and run pytest |
| Binding (`py_src/`) or package (`python/`) | Reinstall, then `pytest -n auto tests` |
| Typing or stubs | Reinstall, then `pytest tests/typecheck` |
| Documentation | `python -m sphinx -b html docs docs/_build/html` with the package installed |
| Formatting and lint | `prek run --all-files` |
| CI workflows or scripts | `pytest .github/scripts`; see [CI](docs/development/ci.md) |

C++ tests use Catch2 and run from any build directory outside the source tree:

```sh
cmake -S . -B ../symusic-build -DBUILD_SYMUSIC_TEST=ON -DCMAKE_BUILD_TYPE=Debug
cmake --build ../symusic-build -j
ctest --test-dir ../symusic-build --output-on-failure
```

`clice lint` runs clang-tidy (checks in `.clang-tidy`) and needs a compilation database in `build/`, the one in-tree build directory, which Git ignores:

```sh
cmake -S . -B build -DCMAKE_EXPORT_COMPILE_COMMANDS=ON -DBUILD_SYMUSIC_TEST=ON -DBUILD_SYMUSIC_PY=ON -DBUILD_SYMUSIC_EXAMPLE=ON
clice lint
```

It is not yet a CI gate; do not add findings in code you touch. Report which commands you ran and what you left to CI.

### Tests

Test public behavior, the [contracts](docs/design/architecture.md#contracts), historical bugs and malformed input. Prefer one round-trip test plus focused boundary cases over repeating the same assertion at every layer. Cover all three time units when behavior depends on the unit. A test without assertions is not a test: record missing coverage in the [roadmap](ROADMAP.md) instead of committing a placeholder. Regression files go in `tests/testcases/`, named after the issue they reproduce.

### Code and comments

Formatters own style: clice with `.clang-format` for C++, Ruff (configured in `pyproject.toml`) for Python. `.git-blame-ignore-revs` lists the bulk formatting commits; run `git config blame.ignoreRevsFile .git-blame-ignore-revs` once. Use the project aliases (`i32`, `vec`, `shared`, `pyvec`) in the core. Comments explain invariants, units, ownership and non-obvious choices; they do not narrate history or keep commented-out code. Write all code, comments, logs and documentation in English.

### Dependencies

C++ dependencies are submodules under `3rdparty/`, pinned to release tags. A dependency without releases (`pdqsort`, `prestosynth`, `pyvec`, and `minimidi`, whose pin is newer than its only release) stays at a reviewed commit; never track a branch head. To update, check out the new tag in the submodule, run the full checks and commit the new pin with its version in the title.

Python runtime dependencies are listed in `pyproject.toml` and kept few. Add one only when it solves a recurring, non-trivial problem better than a small in-repo helper, builds on every wheel platform and has a clear license and maintained upstream. Optional workflows go in extras, not runtime dependencies. Explain the choice and the rejected alternative in the pull request.

## Documentation

| Content | Location |
|---|---|
| Agent instructions | [`AGENTS.md`](AGENTS.md) |
| Contribution procedure | This file |
| Design and contracts | [`docs/design/`](docs/design/architecture.md) |
| User guides and tutorials | `docs/` (Sphinx + MyST, published on [Read the Docs](https://symusic.readthedocs.io/)) |
| Python API reference | Docstrings at the binding site, written with `nb::doc` or `R"pbdoc(...)pbdoc"` in `py_src/bindings/` |
| Planned work and accepted debt | [`ROADMAP.md`](ROADMAP.md) |

Edit the existing page that owns a topic before adding a new one. Delete obsolete instructions instead of marking them legacy; Git history keeps superseded decisions, so do not add separate decision records or dated reports. New Markdown paragraphs occupy one line. Read the Docs runs `readthedocs.yml`, which installs the package from source, builds the site and indexes it with Pagefind; `./build_docs_with_pagefind.sh` reproduces that locally.

## Commits and pull requests

Titles follow [Conventional Commits](https://www.conventionalcommits.org/en/v1.0.0/): `type(scope): summary`, imperative, lowercase, no final period, at most 64 characters.

| Type | Change |
|---|---|
| `feat` | New public API or capability |
| `fix` | Incorrect behavior |
| `perf` | Measured performance change |
| `refactor` | Structure without behavior change |
| `docs` | Documentation, docstrings or comments |
| `test` | Tests only |
| `ci` | Workflows or CI scripts |
| `build` | Build system, packaging or dependencies |
| `style` | Formatting only |
| `chore`, `revert` | Other maintenance or a revert |

Optional scopes: `core`, `io`, `abc`, `binding`, `typing`, `synth`, `pianoroll`, `docs`, `deps`. Keep behavior changes, refactors and performance changes in separate pull requests. Describe user-visible changes in the pull request; release notes are written from merged pull requests.

## AI-assisted contributions

AI assistance is welcome, and a person stays accountable for the whole change. Read and understand every line you submit, run the checks above rather than predicting their results, and disclose the tools you used in the pull request. Coding agents follow [`AGENTS.md`](AGENTS.md).

## Releasing

1. Set `__version__` in `python/symusic/__init__.py`; the build reads the version from there.
2. Run the **Wheels** workflow manually on that commit and wait for it to pass. Pull requests build only one wheel, so this is the first run on every platform.
3. Publish a GitHub release with a `vX.Y.Z` tag and notes generated from merged pull requests. Publishing runs `.github/workflows/release.yml`, which builds the wheels and the sdist again and uploads them to PyPI.

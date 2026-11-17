# Instructions for coding agents

symusic is a C++20 symbolic-music library (MIDI, ABC, piano rolls, synthesis) with a nanobind Python binding. Keep code, comments, logs and documentation in English.

## Read only for your task

Batch the reads for your task in one step. Each link points to a section; read the section, not the whole file, and follow deeper links only when the task needs them.

| Task | Read first |
|---|---|
| Understand the code layout or plan a refactor | [Architecture](docs/design/architecture.md) |
| Add a feature or change behavior | [Adding a feature](docs/design/architecture.md#adding-a-feature), [contracts](docs/design/architecture.md#contracts), [validation](CONTRIBUTING.md#validate) |
| Fix a bug | [Contracts](docs/design/architecture.md#contracts), [tests](CONTRIBUTING.md#tests) |
| Change bindings, typing or stubs | [Python typing](docs/design/architecture.md#python-typing), [build](CONTRIBUTING.md#build) |
| Change MIDI, ABC or pickle I/O | [Data flow](docs/design/architecture.md#data-flow) |
| Add or update a dependency | [Dependencies](CONTRIBUTING.md#dependencies) |
| Edit documentation or docstrings | [Documentation](CONTRIBUTING.md#documentation) |
| Work on known debt or plan work | [Roadmap](ROADMAP.md) |
| Change CI, or add a top-level file or directory | [CI](docs/development/ci.md) |
| Commit, open a PR or release | [Commits and pull requests](CONTRIBUTING.md#commits-and-pull-requests), [releasing](CONTRIBUTING.md#releasing) |

The [documentation index](docs/README.md) covers user-facing topics.

## Commands

Run from the repository root inside a virtual environment:

```sh
git submodule update --init --recursive --depth 1     # once per clone or worktree
uv pip install ".[test]" --config-settings-package symusic:cmake.define.SYMUSIC_FAST_BUILD=ON
pytest -n auto tests
cmake -S . -B ../symusic-build -DSYMUSIC_BUILD_TESTS=ON -DCMAKE_BUILD_TYPE=Debug \
  && cmake --build ../symusic-build -j && ctest --test-dir ../symusic-build --output-on-failure
prek run --all-files                                   # needs clice on PATH
```

- Reinstall after any change to `src/`, `include/`, `py_src/`, `python/` or `scripts/lift_generic_stubs.py`; tests import the installed package and its generated `core.pyi`.
- Use a regular CPython. Free-threaded builds (`3.13t`, `3.14t`) fail `tests/test_abc.py` with a GIL `RuntimeWarning` that pytest turns into an error; `uv venv` may pick one by default, so pass `-p 3.12`.
- Keep build directories outside the source tree; only `build/`, used by `clice lint`, lives inside it.

## Always preserve

- The [contracts](docs/design/architecture.md#contracts): the same semantics for `Tick`, `Quarter` and `Second`; copy and in-place pairs; stable Python references into containers; MIDI and pickle round trips; explicit errors on invalid input.
- Algorithms live in the C++ core. The binding adapts types; `symusic.factory` only dispatches.
- Docstrings live at the binding site in `py_src/bindings/`. Reinstall to regenerate stubs; never edit generated `core.pyi`.
- Never edit `3rdparty/` submodules in place. Update the pinned commit, or fix upstream.
- Every behavior change comes with a test that fails without it. Never commit placeholder tests; record missing coverage in the roadmap.
- Comments explain invariants, units and ownership, not editing history. Delete dead code instead of commenting it out.
- Each fact lives in one document. Update the owning topic when behavior changes, delete obsolete text, and do not add dated reports or decision records. Planned work and accepted debt go to the [roadmap](ROADMAP.md).
- Commit titles follow [Conventional Commits](CONTRIBUTING.md#commits-and-pull-requests). Keep behavior changes, refactors and performance changes in separate commits.

Report the commands you ran, what passed, and what you did not verify.

# Continuous integration

CI runs on GitHub Actions. Every pull request reports one required check, `ci-ok`, which passes when every lane the change selected has passed. Lanes the change did not select are skipped and count as passed.

## Workflows

| Workflow | Trigger | Purpose |
|---|---|---|
| `ci.yml` | Pull requests, pushes to `main`, manual | Lint, tests, docs and a wheel smoke build, routed by changed paths |
| `pr-title.yml` | Pull requests | Checks the title against the [commit convention](https://github.com/Yikai-Liao/symusic/blob/main/CONTRIBUTING.md#commits-and-pull-requests) |
| `wheels.yml` | Manual, called by the other workflows | Builds and tests every wheel and the sdist with cibuildwheel |
| `release.yml` | GitHub release published | Runs the full wheel build and uploads to PyPI |

cibuildwheel reads its configuration from `[tool.cibuildwheel]` in `pyproject.toml`; the workflow only picks the runner and architecture.

## Lanes

The `changes` job runs `.github/scripts/ci_changes.py`, which maps each path changed against the base branch to lanes. The first matching rule wins, and a path that matches no rule selects every lane. Pushes to `main` and manual runs select every lane.

| Lane | Selected by | Runs |
|---|---|---|
| `lint` | Always | `prek run --all-files` (whitespace, YAML/TOML, Ruff, clice format), action pin check, CI script tests |
| `python` | Core, binding, package, tests, build files | `pytest` on Linux (CPython 3.10 and 3.14, PyPy 3.11), macOS and Windows; Python coverage from Linux 3.14 |
| `cpp` | Core, `tests/cpp/`, `examples/`, build files | Catch2 tests through CTest on Linux, macOS and Windows; C++ coverage from Linux |
| `docs` | `docs/`, core, binding, package | Sphinx with warnings as errors |
| `wheels` | Core, binding, build and packaging files | One cibuildwheel build and test (CPython 3.12, manylinux x86_64) |

Changes to `.github/workflows/` or `.github/scripts/` select every lane. Markdown files outside `docs/` select only `lint`. When you add a top-level file or directory, add a rule to `ci_changes.py`; a test fails until every tracked file matches one.

## Reproduce locally

```sh
uvx prek run --all-files                       # lint lane; needs clice on PATH
pytest .github/scripts                          # CI script tests
python .github/scripts/ci_changes.py origin/main  # lanes for your branch
```

The test lanes use the commands in [CONTRIBUTING.md](https://github.com/Yikai-Liao/symusic/blob/main/CONTRIBUTING.md#validate). `.github/scripts/install_clice.sh` installs the clice release CI uses on Linux x86_64.

## Before a release

Pull requests build a single wheel, so the [release steps](https://github.com/Yikai-Liao/symusic/blob/main/CONTRIBUTING.md#releasing) start with a manual full **Wheels** run.

## Pinning

Third-party actions are pinned to a full commit SHA with the version in a comment; `actions/*` may use a major tag. `check_action_pins.py` enforces this. Dependabot updates actions weekly and submodules monthly. The clice version and checksum live in `install_clice.sh`, and prek's version in `ci.yml`.

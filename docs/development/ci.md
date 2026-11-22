# Continuous integration

CI runs on GitHub Actions. Every pull request reports an aggregate check, `ci-ok`, which passes when every lane the change selected has passed. Lanes the change did not select are skipped and count as passed. Configure `ci-ok` as a required status check in GitHub branch protection to enforce it before merging; the workflow alone does not enforce that rule.

## Workflows

| Workflow | Trigger | Purpose |
|---|---|---|
| `ci.yml` | Pull requests, pushes to `main`, weekly, manual | Small daily checks; the compatibility matrix runs weekly or on request |
| `pr-title.yml` | Pull requests | Checks the title against the [commit convention](../CONTRIBUTING.md#commits-and-pull-requests) |
| `wheels.yml` | Manual, called by the other workflows | Builds and tests every wheel and the sdist with cibuildwheel |
| `release.yml` | GitHub release published | Runs the full wheel build and uploads to PyPI |
| `bench.yml` | Manual | On-demand [A/B benchmark](../CONTRIBUTING.md#benchmarks) of two git refs on Linux; writes the table to the job summary |
| `cross-validation.yml` | Manual Wheels run with `cross_validation` enabled | Experimental xclang builds on Linux, executed on native target runners |
| `cross-wheels.yml` | Manual Wheels run with `cross_wheels` enabled | Experimental abi3 wheels built on Linux and installed on target runners |

cibuildwheel reads its configuration from `[tool.cibuildwheel]` in `pyproject.toml`; the workflow picks the runner, architecture and libc. The cibuildwheel test commands run the runtime suite. The CI Python lane runs the typing tests.

## Lanes

The `changes` job runs `.github/scripts/ci_changes.py`, which maps each path changed against the base branch to lanes. The first matching rule wins, and a path that matches no rule selects every lane. Pushes to `main` and manual runs select every lane.

| Lane | Selected by | Runs |
|---|---|---|
| `lint` | Always | `prek run --all-files` (whitespace, YAML/TOML, Ruff, clice format), action pin check, CI script tests |
| `python` | Core, binding, package, tests, `skills/`, build files | `pytest` on Linux CPython 3.12 with Python coverage, and on Windows CPython 3.15 |
| `cpp` | Core, `tests/cpp/`, `examples/`, build files | Catch2 tests through CTest on Linux, with C++ coverage; a separate `sanitize` job runs the same tests with ASan and UBSan |
| `docs` | `docs/`, `skills/`, `CONTRIBUTING.md`, `ROADMAP.md` | Sphinx HTML with warnings as errors; the site does not import the package |
| `wheels` | Build, packaging, dependency and CI configuration, `skills/` | One stable-ABI wheel (manylinux x86_64), tested on CPython 3.12 and 3.15 |

Changes to `.github/workflows/` or `.github/scripts/` select every lane. Root Markdown files included in the website select `docs`; other Markdown files outside `docs/` and `skills/` select only `lint`. Renames select both the old and new paths' lanes. When you add a top-level file or directory, add a rule to `ci_changes.py`; a test fails until every tracked file matches one.

Ordinary core changes select the Python and C++ lanes without rebuilding documentation or running Docker packaging tests. Matrices and path routing both live in `ci_changes.py`.

The C++ lane also compiles examples when their sources or shared CMake configuration change. Full runs compile them on every selected platform; ordinary core PRs keep them off.

The `sanitize` job builds the Catch2 tests with `-DSYMUSIC_SANITIZE="address;undefined"` (GCC, Debug, Ninja) and runs CTest with `halt_on_error`, stack traces and leak detection. UBSan findings abort. It shares the sccache setup described under [compiler caches](#compiler-caches), so only changed objects rebuild. Reproduce it locally with the same CMake options and `ASAN_OPTIONS`/`UBSAN_OPTIONS` from `ci.yml`. The Python tests do not run under ASan yet; see the [roadmap](../ROADMAP.md#memory-safety).

## Compatibility checks

Every Monday at 08:00 UTC, or when a manual run enables `compatibility`, CI selects all lanes and adds Linux CPython 3.10/3.15 and PyPy 3.11, macOS CPython 3.15, and C++ tests on macOS and Windows. The daily checks remain, with coverage collected only there. External linkcheck runs in this mode; network failures do not delay ordinary PRs.

## Reproduce locally

```sh
uvx prek run --all-files                       # lint lane; needs clice on PATH
pytest .github/scripts                          # CI script tests
python .github/scripts/ci_changes.py origin/main  # lanes for your branch
python .github/scripts/ci_changes.py --compatibility  # full matrices
```

The test lanes use the commands in [CONTRIBUTING.md](../CONTRIBUTING.md#validate). `.github/scripts/install_clice.sh` installs the clice release CI uses on Linux x86_64.

## Before a release

Packaging changes build a single wheel in PRs, so the [release steps](../CONTRIBUTING.md#releasing) start with a manual full **Wheels** run and a manual **CI** run with `compatibility` enabled.

Each wheel includes FluidSynth, libsndfile and the abcmidi converters. Linux and macOS repair resolve the libraries within the wheel. Windows repair also scans the bundled DLLs for external dependencies. Release CI attaches the sdist, which contains the corresponding native sources and build recipes, to the GitHub release.

The full build keeps every interpreter for one platform in the same job. It builds separate wheels for CPython 3.10/3.11 and supported PyPy targets, then builds one `cp312-abi3` wheel and reuses it for tests on 3.13, 3.14 and 3.15. Splitting those interpreters into separate jobs would lose reuse.

## Compiler caches

Three caches keep compiler output across runs. Each is keyed so that it never mixes incompatible toolchains.

- The `python`, `cpp` and `sanitize` jobs use sccache with the GitHub Actions cache backend (`mozilla-actions/sccache-action`) and hand it to CMake through the `CMAKE_C_COMPILER_LAUNCHER` and `CMAKE_CXX_COMPILER_LAUNCHER` environment variables, which scikit-build-core and CMake both honor. sccache hashes the compiler, flags and preprocessed source of every object, so entries from different toolchains, operating systems or Python versions simply miss. Pull requests read entries written on `main` and write only to their own scope, so they cannot overwrite or evict cache written by `main`. The post step prints hit statistics.
- The Windows `python` build runs sccache through Ninja (the generator the MSVC developer shell selects). The extension is built with `/O1` and no `/Zi`, which sccache cannot cache. The `cpp` job skips the cache on Windows because its Debug configuration uses `/Zi`.
- The `smoke` wheel job keeps ccache's directory on the host and mounts it into the manylinux container with cibuildwheel's `CIBW_CONTAINER_ENGINE` `create_args`. `actions/cache` restores it before the build and saves it afterwards, only on pushes to `main` and manual runs. The key names the OS, architecture and libc of the container image, so the cache never crosses those boundaries; the newest entry with that prefix is restored.

Linux and macOS wheel jobs use ccache for both C and C++. The core library, synthesizer and ABC converter objects can be reused across Python builds when their inputs match; binding objects are recompiled when their Python headers or flags differ. Linux installs a checksum-pinned static ccache binary inside each wheel container; macOS uses Homebrew. The cache lives only for that container or runner (the smoke job's host-mounted cache is described above), so it never crosses OS, architecture, libc or job boundaries. Build isolation can discard CMake's object directory, which is why a compiler cache is used instead of sharing an unchecked build tree. Cache statistics are printed before wheel tests.

The Python test lane keys its uv dependency cache on the root `pyproject.toml`, OS and interpreter, so unrelated vendored documentation changes do not discard source-built PyPy dependencies.

## Cross-compilation experiment

A manual **Wheels** run with `cross_validation` enabled skips packaging and calls `cross-validation.yml`. Linux x64 builds the full C++ test executable for macOS 13+ (arm64 and x86_64), Linux ARM64 (glibc and static musl), and Windows (MSVC ABI, x64 and ARM64). Native GitHub runners then run each artifact against the test fixtures. The checksum-pinned xclang installer and vendor SDK presets make the build reproducible; SDK downloads require the maintainer's accepted Apple and Microsoft licenses. SDKs are never uploaded as artifacts.

This experiment validates the C++ core and target runtime only. Release wheels retain native builders until target Python headers/libraries, stub generation, wheel repair and installed-package tests also pass.

A manual **Wheels** run with `cross_wheels` enabled tests the packaging path separately. A native Linux build generates `_core.pyi` from the same revision; Linux x64 then builds `cp312-abi3` wheels for both macOS architectures, both Windows MSVC architectures, and Linux AArch64 with glibc. `.github/scripts/cross_wheels.py` owns the target Python distributions, wheel tags and runner mapping. Cross builds use target Python headers and Windows import libraries, while scikit-build-core runs under the host Python. Windows targets use the SDK's Microsoft STL with `-stdlib=platform`; other targets use xclang's libc++. Linux wheels pass auditwheel repair, and every wheel passes strict abi3audit.

Each target runner installs its wheel, regenerates the stub with the pinned nanobind submodule and compares it with the bundled file, then runs the runtime and typing tests. A separate Python 3.15 environment tests the same wheel again. CMake configuration tests are excluded from these runtime jobs and remain in the normal Python lane. This experiment does not publish packages or replace the release workflow. Python 3.10/3.11, PyPy, Windows x86 and musllinux retain native packaging; xclang's bundled musl sysroot supports static executables and needs a separate dynamic sysroot for Python extensions.

## Pinning

Third-party actions are pinned to a full commit SHA with the version in a comment; `actions/*` may use a major tag. `check_action_pins.py` enforces this. Dependabot updates actions weekly; submodules follow release tags and are updated by hand (see [Dependencies](../CONTRIBUTING.md#dependencies)). The clice and Linux ccache versions and checksums live in their installer scripts, and prek's version in `ci.yml`.

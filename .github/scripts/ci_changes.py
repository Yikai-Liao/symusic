"""Select the CI lanes a change needs.

Usage: ci_changes.py [BASE_REF] [--compatibility] (writes GITHUB_OUTPUT values)

Each changed path selects the lanes listed for the first matching rule. A path
that matches no rule selects every lane, so new top-level files are tested
until someone routes them. Without a base ref (push, manual run) every lane runs.
Daily runs use Linux plus one Windows Python job, because MSVC breaks most
often; compatibility runs add other interpreters and operating systems.
"""

from __future__ import annotations

import argparse
import fnmatch
import json
import subprocess
import sys

LANES = ("python", "cpp", "docs", "wheels")
ALL = frozenset(LANES)
CORE = frozenset({"python", "cpp"})

PYTHON_BASELINE = (
    {"os": "ubuntu-latest", "python": "3.12", "coverage": True},
    {"os": "windows-latest", "python": "3.15"},
)
CPP_BASELINE = {"os": "ubuntu-latest", "coverage": True}
PYTHON_COMPATIBILITY = (
    {"os": "ubuntu-latest", "python": "3.10"},
    {"os": "ubuntu-latest", "python": "3.15"},
    {"os": "ubuntu-latest", "python": "pypy3.11"},
    {"os": "macos-latest", "python": "3.15"},
)
CPP_COMPATIBILITY = ({"os": "macos-latest"}, {"os": "windows-latest"})

# First match wins. Patterns use fnmatch, where `*` also matches `/`.
RULES: tuple[tuple[str, frozenset[str]], ...] = (
    (".github/workflows/*", ALL),
    (".github/scripts/*", ALL),
    (".github/*", frozenset()),
    ("benchmarks/*", frozenset()),
    ("include/*", CORE),
    ("src/*", CORE),
    ("3rdparty/*", ALL),
    ("cmake/*", ALL),
    ("CMakeLists.txt", ALL),
    (".gitmodules", ALL),
    ("tests/cpp/*", frozenset({"cpp"})),
    ("examples/*", frozenset({"cpp"})),
    ("py_src/CMakeLists.txt", frozenset({"python", "wheels"})),
    ("py_src/*", frozenset({"python"})),
    ("pyproject.toml", frozenset({"python", "wheels"})),
    ("MANIFEST.in", frozenset({"wheels"})),
    ("scripts/*", frozenset({"python", "wheels"})),
    ("python/*", frozenset({"python"})),
    # pytest runs the guide pages' examples, the site includes them and the
    # wheel bundles them. This rule must precede "*.md", which would otherwise
    # match them.
    ("skills/*", frozenset({"python", "docs", "wheels"})),
    ("tests/*", frozenset({"python"})),
    ("docs/*", frozenset({"docs"})),
    ("readthedocs.yml", frozenset({"docs"})),
    ("pagefind.yml", frozenset({"docs"})),
    ("build_docs_with_pagefind.sh", frozenset({"docs"})),
    ("CONTRIBUTING.md", frozenset({"docs"})),
    ("ROADMAP.md", frozenset({"docs"})),
    ("*.md", frozenset()),
    ("LICENSE", frozenset()),
    (".codecov.yml", frozenset()),
    (".gitignore", frozenset()),
    (".gitattributes", frozenset()),
    (".git-blame-ignore-revs", frozenset()),
    # Lint configuration: the lint job always runs.
    ("prek.toml", frozenset()),
    ("clice.toml", frozenset()),
    (".clang-format", frozenset()),
    (".clang-tidy", frozenset()),
)


def lanes_for(path: str) -> frozenset[str]:
    for pattern, lanes in RULES:
        if fnmatch.fnmatchcase(path, pattern):
            return lanes
    return ALL


def select(paths: list[str]) -> frozenset[str]:
    selected: set[str] = set()
    for path in paths:
        selected |= lanes_for(path)
    return frozenset(selected)


def changed_paths(base_ref: str) -> list[str]:
    out = subprocess.run(
        ["git", "diff", "--no-renames", "--name-only", "-z", f"{base_ref}...HEAD"],
        check=True,
        capture_output=True,
        text=True,
    ).stdout
    # A move affects both the old and new lanes; NULs preserve literal paths.
    return [path for path in out.split("\0") if path]


def main(argv: list[str]) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("base_ref", nargs="?", default="")
    parser.add_argument("--compatibility", action="store_true")
    args = parser.parse_args(argv[1:])
    paths = changed_paths(args.base_ref) if args.base_ref else None
    selected = select(paths) if paths is not None else ALL
    for lane in LANES:
        print(f"{lane}={'true' if lane in selected else 'false'}")
    python_jobs = list(PYTHON_BASELINE)
    cpp_jobs = [CPP_BASELINE]
    if args.compatibility:
        python_jobs.extend(PYTHON_COMPATIBILITY)
        cpp_jobs.extend(CPP_COMPATIBILITY)
    build_examples = (
        paths is None
        or args.compatibility
        or any(
            path.startswith("examples/")
            or path
            in {
                "CMakeLists.txt",
                "cmake/BuildOptions.cmake",
                "cmake/Dependencies.cmake",
            }
            for path in paths
        )
    )
    cpp_jobs = [{**job, "examples": build_examples} for job in cpp_jobs]
    for name, jobs in (("python", python_jobs), ("cpp", cpp_jobs)):
        print(f"{name}_matrix={json.dumps({'include': jobs}, separators=(',', ':'))}")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))

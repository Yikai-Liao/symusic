"""Select the CI lanes a change needs.

Usage: ci_changes.py BASE_REF  (prints `lane=true|false` lines for GITHUB_OUTPUT)

Each changed path selects the lanes listed for the first matching rule. A path
that matches no rule selects every lane, so new top-level files are tested
until someone routes them. Without a base ref (push, manual run) every lane runs.
"""

from __future__ import annotations

import fnmatch
import subprocess
import sys

LANES = ("python", "cpp", "docs", "wheels")
ALL = frozenset(LANES)
NATIVE = frozenset(LANES)  # the C++ core feeds every lane

# First match wins. Patterns use fnmatch, where `*` also matches `/`.
RULES: tuple[tuple[str, frozenset[str]], ...] = (
    (".github/workflows/*", ALL),
    (".github/scripts/*", ALL),
    (".github/*", frozenset()),
    ("include/*", NATIVE),
    ("src/*", NATIVE),
    ("3rdparty/*", NATIVE),
    ("cmake/*", NATIVE),
    ("CMakeLists.txt", NATIVE),
    (".gitmodules", NATIVE),
    ("tests/cpp/*", frozenset({"cpp"})),
    ("examples/*", frozenset({"cpp"})),
    ("py_src/*", frozenset({"python", "docs", "wheels"})),
    ("pyproject.toml", frozenset({"python", "docs", "wheels"})),
    ("MANIFEST.in", frozenset({"wheels"})),
    ("scripts/*", frozenset({"python", "docs", "wheels"})),
    ("python/*", frozenset({"python", "docs"})),
    ("tests/*", frozenset({"python"})),
    ("docs/*", frozenset({"docs"})),
    ("readthedocs.yml", frozenset({"docs"})),
    ("pagefind.yml", frozenset({"docs"})),
    ("build_docs_with_pagefind.sh", frozenset({"docs"})),
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
        ["git", "diff", "--name-only", f"{base_ref}...HEAD"],
        check=True,
        capture_output=True,
        text=True,
    ).stdout
    return [line for line in out.splitlines() if line]


def main(argv: list[str]) -> int:
    base_ref = argv[1] if len(argv) > 1 else ""
    selected = select(changed_paths(base_ref)) if base_ref else ALL
    for lane in LANES:
        print(f"{lane}={'true' if lane in selected else 'false'}")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))

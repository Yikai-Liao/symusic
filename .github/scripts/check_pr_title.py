"""Check a pull request title against the convention in CONTRIBUTING.md.

Usage: PR_TITLE="..." check_pr_title.py
"""

from __future__ import annotations

import os
import re
import sys

TYPES = (
    "feat",
    "fix",
    "perf",
    "refactor",
    "docs",
    "test",
    "ci",
    "build",
    "style",
    "chore",
    "revert",
)
SCOPES = (
    "core",
    "io",
    "abc",
    "binding",
    "typing",
    "synth",
    "pianoroll",
    "docs",
    "deps",
)
MAX_LENGTH = 64

TITLE = re.compile(r"^(?P<type>[a-z]+)(?:\((?P<scope>[a-z]+)\))?!?: (?P<summary>\S.*)$")


def problems(title: str) -> list[str]:
    found: list[str] = []
    if len(title) > MAX_LENGTH:
        found.append(f"title has {len(title)} characters; the limit is {MAX_LENGTH}")
    match = TITLE.match(title)
    if match is None:
        found.append("title must look like `type(scope): summary` or `type: summary`")
        return found
    if match["type"] not in TYPES:
        found.append(f"unknown type `{match['type']}`; use one of {', '.join(TYPES)}")
    if match["scope"] is not None and match["scope"] not in SCOPES:
        found.append(
            f"unknown scope `{match['scope']}`; use one of {', '.join(SCOPES)}"
        )
    summary = match["summary"]
    if summary[0].isupper() and not summary.split()[0].isupper():
        found.append("summary starts lowercase unless it begins with an acronym")
    if summary.endswith("."):
        found.append("summary has no final period")
    return found


def main() -> int:
    title = os.environ.get("PR_TITLE", "")
    found = problems(title)
    for problem in found:
        print(f"::error title=PR title::{problem}")
    if found:
        print(
            "See https://github.com/Yikai-Liao/symusic/blob/main/CONTRIBUTING.md#commits-and-pull-requests"
        )
    return 1 if found else 0


if __name__ == "__main__":
    sys.exit(main())

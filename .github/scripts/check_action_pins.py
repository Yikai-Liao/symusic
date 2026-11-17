"""Require third-party actions in workflows to be pinned to a full commit SHA.

Actions under `actions/` and local actions (`./`) may use a version tag.
"""

from __future__ import annotations

import re
import sys
from pathlib import Path

USES = re.compile(r"^\s*(?:-\s+)?uses:\s*([^\s#]+)")
SHA = re.compile(r"[0-9a-f]{40}")


def problems(workflow: Path) -> list[str]:
    found: list[str] = []
    for number, line in enumerate(workflow.read_text().splitlines(), start=1):
        match = USES.match(line)
        if match is None:
            continue
        ref = match.group(1)
        if ref.startswith(("actions/", "./")):
            continue
        _, _, version = ref.partition("@")
        if SHA.fullmatch(version) is None:
            found.append(f"{workflow}:{number}: pin {ref} to a full commit SHA")
    return found


def main(argv: list[str]) -> int:
    root = Path(argv[1]) if len(argv) > 1 else Path(".github/workflows")
    found = [p for workflow in sorted(root.glob("*.yml")) for p in problems(workflow)]
    print("\n".join(found))
    return 1 if found else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))

"""Tests for the CI helper scripts. Run with `pytest .github/scripts`."""

from __future__ import annotations

import sys
from pathlib import Path

import pytest

sys.path.insert(0, str(Path(__file__).parent))

import check_action_pins
import check_pr_title
import ci_changes


@pytest.mark.parametrize(
    ("paths", "expected"),
    [
        (["README.md", "ROADMAP.md"], set()),
        (["docs/quickstart.md"], {"docs"}),
        (["tests/test_pickle.py"], {"python"}),
        (["tests/cpp/test_io.hpp"], {"cpp"}),
        (["python/symusic/factory.py"], {"python", "docs"}),
        (["py_src/bindings/score.cpp"], {"python", "docs", "wheels"}),
        (["src/score.cpp"], set(ci_changes.LANES)),
        (["3rdparty/minimidi"], set(ci_changes.LANES)),
        ([".github/workflows/ci.yml"], set(ci_changes.LANES)),
        ([".github/pull_request_template.md"], set()),
        (["somewhere/new.txt"], set(ci_changes.LANES)),
        (["docs/index.md", "tests/cpp/test_main.cpp"], {"docs", "cpp"}),
    ],
)
def test_ci_changes_routes_paths(paths: list[str], expected: set[str]) -> None:
    assert ci_changes.select(paths) == expected


def test_ci_changes_routes_every_tracked_file() -> None:
    import subprocess

    root = Path(__file__).resolve().parents[2]
    files = subprocess.run(
        ["git", "ls-files", "-z"], cwd=root, check=True, capture_output=True, text=True
    ).stdout.split("\0")
    unrouted = [
        f
        for f in files
        if f
        and not any(ci_changes.fnmatch.fnmatchcase(f, p) for p, _ in ci_changes.RULES)
    ]
    assert unrouted == []


@pytest.mark.parametrize(
    "title",
    [
        "feat(io): read ABC from bytes",
        "fix: keep track names on resample",
        "ci: route jobs by changed paths",
        "refactor(core)!: drop the legacy sort",
        "docs: explain MIDI round trips",
    ],
)
def test_pr_title_accepts(title: str) -> None:
    assert check_pr_title.problems(title) == []


@pytest.mark.parametrize(
    "title",
    [
        "Fix the parser",
        "feat: Add a thing",
        "feature: add a thing",
        "fix(parser): handle eof",
        "fix: handle eof.",
        "fix: " + "x" * 64,
        "fix:missing space",
    ],
)
def test_pr_title_rejects(title: str) -> None:
    assert check_pr_title.problems(title) != []


def test_action_pins(tmp_path: Path) -> None:
    sha = "0" * 40
    workflow = tmp_path / "w.yml"
    workflow.write_text(
        "steps:\n"
        "  - uses: actions/checkout@v6\n"
        f"  - uses: astral-sh/setup-uv@{sha}  # v10\n"
        "  - uses: pypa/cibuildwheel@v4.3.5\n"
        "    uses: codecov/codecov-action\n"
    )
    found = check_action_pins.problems(workflow)
    assert [line.split(":")[1] for line in found] == ["4", "5"]


def test_repository_workflows_are_pinned() -> None:
    root = Path(__file__).resolve().parents[1] / "workflows"
    assert check_action_pins.main(["", str(root)]) == 0

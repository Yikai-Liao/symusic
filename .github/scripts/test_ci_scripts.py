"""Tests for the CI helper scripts. Run with `pytest .github/scripts`."""

from __future__ import annotations

import json
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
        (["README.md"], set()),
        (["CONTRIBUTING.md"], {"docs"}),
        (["ROADMAP.md"], {"docs"}),
        (["docs/quickstart.md"], {"docs"}),
        (["tests/test_pickle.py"], {"python"}),
        (["tests/cpp/test_io.hpp"], {"cpp"}),
        (["python/symusic/factory.py"], {"python", "docs"}),
        (["py_src/bindings/score.cpp"], {"python", "docs"}),
        (["py_src/CMakeLists.txt"], {"python", "docs", "wheels"}),
        (["src/score.cpp"], {"python", "cpp"}),
        (["CMakeLists.txt"], set(ci_changes.LANES)),
        (["3rdparty/minimidi"], set(ci_changes.LANES)),
        ([".github/workflows/ci.yml"], set(ci_changes.LANES)),
        ([".github/pull_request_template.md"], set()),
        (["somewhere/new.txt"], set(ci_changes.LANES)),
        (["docs/index.md", "tests/cpp/test_main.cpp"], {"docs", "cpp"}),
    ],
)
def test_path_routes(paths: list[str], expected: set[str]) -> None:
    assert ci_changes.select(paths) == expected


def test_tracked_paths() -> None:
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


@pytest.mark.parametrize("compatibility", [False, True])
def test_ci_matrices(compatibility: bool, capsys) -> None:
    argv = ["ci_changes.py"]
    if compatibility:
        argv.append("--compatibility")
    assert ci_changes.main(argv) == 0
    outputs = dict(line.split("=", 1) for line in capsys.readouterr().out.splitlines())
    python = json.loads(outputs["python_matrix"])["include"]
    cpp = json.loads(outputs["cpp_matrix"])["include"]
    assert python[0]["python"] == "3.12"
    assert sum(bool(job.get("coverage")) for job in python) == 1
    if compatibility:
        assert {job["os"] for job in python} == {
            "ubuntu-latest",
            "macos-latest",
            "windows-latest",
        }
        assert {job["python"] for job in python} >= {"3.10", "3.15", "pypy3.11"}
        assert len(cpp) == 3
    else:
        assert [job["os"] for job in python] == ["ubuntu-latest", "windows-latest"]
        assert [job["os"] for job in cpp] == ["ubuntu-latest"]


@pytest.mark.parametrize(
    ("paths", "expected"),
    [
        (["src/score.cpp"], False),
        (["examples/cpp/note_count.cpp"], True),
        (["examples/cpp/CMakeLists.txt"], True),
        (["CMakeLists.txt"], True),
    ],
)
def test_example_builds(paths, expected, monkeypatch, capsys) -> None:
    monkeypatch.setattr(ci_changes, "changed_paths", lambda _: paths)
    assert ci_changes.main(["ci_changes.py", "origin/main"]) == 0
    outputs = dict(line.split("=", 1) for line in capsys.readouterr().out.splitlines())
    jobs = json.loads(outputs["cpp_matrix"])["include"]
    assert all(job["examples"] == expected for job in jobs)


def test_rename_paths(tmp_path: Path, monkeypatch) -> None:
    import subprocess

    monkeypatch.chdir(tmp_path)

    def git(*args: str) -> None:
        subprocess.run(["git", *args], check=True, capture_output=True)

    git("init")
    git("config", "user.name", "CI test")
    git("config", "user.email", "ci@example.com")
    (tmp_path / "tests" / "cpp").mkdir(parents=True)
    (tmp_path / "docs").mkdir()
    old_path = "tests/cpp/example.hpp"
    new_path = "docs/example.md"
    (tmp_path / old_path).write_text("An unchanged file.\n")
    git("add", ".")
    git("commit", "-m", "initial")
    git("branch", "base")
    git("mv", old_path, new_path)
    git("commit", "-m", "move file")

    paths = ci_changes.changed_paths("base")
    assert set(paths) == {old_path, new_path}
    assert ci_changes.select(paths) == {"cpp", "docs"}


def test_docs_build_failure(tmp_path: Path, monkeypatch) -> None:
    import os
    import subprocess

    bin_dir = tmp_path / "bin"
    bin_dir.mkdir()
    for name, body in (
        ("python", "exit 23"),
        ("sphinx-build", "exit 23"),
        ("npx", "touch indexed"),
    ):
        executable = bin_dir / name
        executable.write_text(f"#!/bin/sh\n{body}\n")
        executable.chmod(0o755)
    monkeypatch.setenv("PATH", str(bin_dir) + os.pathsep + os.environ["PATH"])
    script = Path(__file__).resolve().parents[2] / "build_docs_with_pagefind.sh"
    result = subprocess.run(
        ["bash", str(script)], cwd=tmp_path, capture_output=True, text=True
    )
    assert result.returncode == 23
    assert not (tmp_path / "indexed").exists()


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


def test_workflow_pins() -> None:
    root = Path(__file__).resolve().parents[1] / "workflows"
    assert check_action_pins.main(["", str(root)]) == 0

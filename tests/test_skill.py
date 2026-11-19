"""Tests for the agent skill bundled in the package."""

from __future__ import annotations

import re
import subprocess
import sys
from pathlib import Path

import pytest
from symusic import skill

SOURCE = Path(__file__).resolve().parents[1] / "skills" / "symusic"


def files(root: Path) -> dict[str, bytes]:
    return {
        p.relative_to(root).as_posix(): p.read_bytes()
        for p in root.rglob("*")
        if p.is_file()
    }


def test_bundled_skill_matches_source():
    assert files(skill.path()) == files(SOURCE)


def test_skill_frontmatter():
    """`npx skills` and agents require a name matching the directory and a description."""
    text = (SOURCE / "SKILL.md").read_text("utf-8")
    match = re.match(r"---\n(.*?)\n---\n", text, re.DOTALL)
    assert match is not None
    fields = dict(line.split(": ", 1) for line in match.group(1).splitlines())
    assert fields["name"] == SOURCE.name
    assert fields["description"]


def test_install(tmp_path: Path):
    target = skill.install(tmp_path)
    assert target == tmp_path / "symusic"
    assert files(target) == files(SOURCE)

    (target / "stale.md").write_text("old", "utf-8")
    with pytest.raises(FileExistsError):
        skill.install(tmp_path)
    assert (target / "stale.md").exists()

    skill.install(tmp_path, force=True)
    assert files(target) == files(SOURCE)


def test_command_line(tmp_path: Path):
    def run(*args: str) -> subprocess.CompletedProcess[str]:
        return subprocess.run(
            [sys.executable, "-m", "symusic.skill", *args],
            capture_output=True,
            text=True,
            check=False,
        )

    result = run("path")
    assert result.returncode == 0
    assert Path(result.stdout.strip()) == skill.path()

    result = run("install", str(tmp_path))
    assert result.returncode == 0
    assert Path(result.stdout.strip()) == tmp_path / "symusic"

    result = run("install", str(tmp_path))
    assert result.returncode == 1
    assert "exists" in result.stderr

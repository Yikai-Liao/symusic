"""Run the Python examples and check the links in the user guide pages.

The guide lives in ``skills/symusic`` and is shared by the agent skill and the
website. Each page's examples run in order as one script, in a temporary
directory that holds the files the examples load. A block preceded by a
``<!-- docs-test: skip, <reason> -->`` line is not run.
"""

from __future__ import annotations

import re
import shutil
import subprocess
import sys
from pathlib import Path

import pytest

ROOT = Path(__file__).resolve().parents[1]
SKILL = ROOT / "skills" / "symusic"
PAGES = sorted([SKILL / "SKILL.md", *(SKILL / "references").glob("*.md")])
TESTCASES = Path(__file__).parent / "testcases"
FIXTURES = {
    "song.mid": TESTCASES / "Multitrack_MIDIs" / "Funkytown.mid",
    "tune.abc": TESTCASES / "abc_files" / "flowers_of_edinburgh.abc",
}

BLOCK = re.compile(r"(<!-- docs-test: skip[^>]*-->\n)?```python\n(.*?)```", re.DOTALL)
LINK = re.compile(r"\]\(([^)\s]+)\)")
HEADING = re.compile(r"^#+ (.+)$", re.MULTILINE)


def page_id(page: Path) -> str:
    return page.relative_to(SKILL).as_posix()


def anchor(heading: str) -> str:
    """Return the anchor GitHub and MyST generate for a heading."""
    text = re.sub(r"[^\w\- ]", "", heading.strip().lower())
    return text.replace(" ", "-")


@pytest.mark.parametrize("page", PAGES, ids=page_id)
def test_examples_run(page: Path, tmp_path: Path):
    blocks = [code for skip, code in BLOCK.findall(page.read_text("utf-8")) if not skip]
    if not blocks:
        pytest.skip("no runnable examples")
    for name, source in FIXTURES.items():
        shutil.copy(source, tmp_path / name)
    script = tmp_path / "examples.py"
    script.write_text("\n\n".join(blocks), "utf-8")
    # The examples run as a file, not through exec, so that multiprocessing
    # workers can import the functions they define.
    result = subprocess.run(
        [sys.executable, script.name],
        cwd=tmp_path,
        capture_output=True,
        text=True,
        timeout=300,
        check=False,
    )
    assert result.returncode == 0, result.stderr


@pytest.mark.parametrize("page", PAGES, ids=page_id)
def test_links_resolve(page: Path):
    for target in LINK.findall(page.read_text("utf-8")):
        if "://" in target:
            continue
        path, _, fragment = target.partition("#")
        linked = (page.parent / path).resolve() if path else page
        assert linked.is_file(), f"{target}: no such file"
        if fragment:
            headings = {anchor(h) for h in HEADING.findall(linked.read_text("utf-8"))}
            assert fragment in headings, f"{target}: no such heading"


@pytest.mark.parametrize("page", PAGES, ids=page_id)
def test_guide_uses_plain_markdown(page: Path):
    text = page.read_text("utf-8")
    forbidden = re.compile(
        r"^\s*(?:```|~~~|:::)\{[^}]+\}|^\s*\.\.\s+[\w-]+::"
        r"|^\s*\([^)\n]+\)=\s*$|\{[\w:-]+\}`[^`]+`|:[\w:-]+:`[^`]+`",
        re.MULTILINE,
    )
    matches = forbidden.findall(text)
    assert not matches, f"Use plain Markdown in {page_id(page)}: {matches}"

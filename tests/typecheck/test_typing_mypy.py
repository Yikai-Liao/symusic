from __future__ import annotations

import shutil
import subprocess
import sys
from pathlib import Path

import pytest
import textwrap


MYPY_BIN = shutil.which("mypy")


def _run_mypy(tmp_path: Path, content: str) -> subprocess.CompletedProcess[str]:
    script = tmp_path / "typing_sample.py"
    script.write_text(
        textwrap.dedent(
            content
        ).strip()
    )
    return subprocess.run(
        [sys.executable, "-m", "mypy", str(script)],
        capture_output=True,
        text=True,
        check=False,
    )


def _assert_success(result: subprocess.CompletedProcess[str]) -> None:
    assert (
        result.returncode == 0
    ), f"mypy failed ({result.returncode}):\nSTDOUT:\n{result.stdout}\nSTDERR:\n{result.stderr}"


@pytest.mark.skipif(MYPY_BIN is None, reason="mypy is not installed")
def test_score_typing(tmp_path: Path) -> None:
    content = """
        import symusic.core as core


        score_tick: core.Score[core.Tick]
        score_tick = core.ScoreTick(480)
        score_tick_generic: core.Score[core.Tick] = score_tick

        def needs_tick(score: core.Score[core.Tick]) -> None:
            pass

        needs_tick(score_tick_generic)
    """
    _assert_success(_run_mypy(tmp_path, content))


@pytest.mark.skipif(MYPY_BIN is None, reason="mypy is not installed")
def test_pyvec_typing(tmp_path: Path) -> None:
    content = """
        import symusic.core as core


        note_tick_list: core.NoteTickList = core.NoteTickList()
        pyvec_note_tick: core.PyVec[core.NoteTick]
        pyvec_note_tick = note_tick_list
    """
    _assert_success(_run_mypy(tmp_path, content))


@pytest.mark.skipif(MYPY_BIN is None, reason="mypy is not installed")
def test_event_and_track_typing(tmp_path: Path) -> None:
    content = """
        import symusic.core as core


        note_tick: core.Note[core.Tick]
        note_tick = core.NoteTick(0, 10, 60, 70)
        note_tick_generic: core.Note[core.Tick] = note_tick

        track_tick: core.Track[core.Tick]
        track_tick = core.TrackTick()
        track_tick_generic: core.Track[core.Tick] = track_tick
    """
    _assert_success(_run_mypy(tmp_path, content))


@pytest.mark.skipif(MYPY_BIN is None, reason="mypy is not installed")
def test_nested_pyvec_typing(tmp_path: Path) -> None:
    content = """
        import symusic.core as core
        from typing import cast


        nested_vec = cast(core.PyVec[core.Note[core.Tick]], core.NoteTickList())

        def consume(vec: core.PyVec[core.Note[core.Tick]]) -> None:
            pass

        consume(nested_vec)
    """
    _assert_success(_run_mypy(tmp_path, content))


@pytest.mark.skipif(MYPY_BIN is None, reason="mypy is not installed")
def test_types_module_aliases(tmp_path: Path) -> None:
    content = """
        import symusic.core as core
        import symusic.types as smt
        from typing import cast


        score_tick: smt.Score[core.Tick]
        score_tick = core.ScoreTick(480)

        pyvec_note_tick = cast(smt.PyVec[smt.Note[core.Tick]], core.NoteTickList())

        track_tick: smt.Track[core.Tick]
        track_tick = core.TrackTick()
    """
    _assert_success(_run_mypy(tmp_path, content))

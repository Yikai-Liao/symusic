"""Runtime compatibility tests for Python 3.14 JIT and free-threaded builds."""

from __future__ import annotations

import os
import sys
import sysconfig
from concurrent.futures import ThreadPoolExecutor

import pytest
from symusic import Note, Score, Track


def _is_gil_disabled_build() -> bool:
    """Return whether the current interpreter is built in free-threaded mode."""
    return bool(sysconfig.get_config_var("Py_GIL_DISABLED"))


def test_expected_gil_build_mode_matches_ci_matrix():
    """Validate interpreter mode expected by CI matrix."""
    expected = os.getenv("SYMUSIC_EXPECT_GIL_DISABLED")
    if expected is None:
        pytest.skip("SYMUSIC_EXPECT_GIL_DISABLED not set")
    assert _is_gil_disabled_build() is (expected == "1")


@pytest.mark.skipif(sys.version_info < (3, 14), reason="Requires Python 3.14+")
def test_jit_flag_is_safe_to_enable_when_available():
    """Exercise import and object creation under optional PYTHON_JIT=1."""
    if os.getenv("PYTHON_JIT") != "1":
        pytest.skip("PYTHON_JIT is not enabled for this run")

    jit_module = getattr(sys, "_jit", None)
    if jit_module is None:
        pytest.skip("sys._jit is unavailable on this interpreter")

    score = Score(480)
    track = Track("jit-track", 0, False)
    track.notes.append(Note(0, 120, 60, 90))
    score.tracks.append(track)
    assert score.note_num() == 1


@pytest.mark.skipif(sys.version_info < (3, 14), reason="Requires Python 3.14+")
def test_free_threaded_parallel_score_operations():
    """Run parallel score operations as a free-threaded smoke test."""
    if not _is_gil_disabled_build():
        pytest.skip("This smoke test is only meaningful on free-threaded builds")

    def _worker(seed: int) -> int:
        score = Score(480)
        track = Track(f"track-{seed}", 0, False)
        for i in range(400):
            track.notes.append(Note(i * 10, 5, (seed + i) % 128, 80))
        score.tracks.append(track)
        return score.note_num()

    with ThreadPoolExecutor(max_workers=8) as pool:
        results = list(pool.map(_worker, range(32)))

    assert results == [400] * 32

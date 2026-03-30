from __future__ import annotations

import subprocess
import sys

import pytest
from symusic import Score

from tests.utils import MIDI_PATHS_ALL, TESTCASES_PATH

ISSUE_104_PATH = TESTCASES_PATH / "MIDIs_corrupted" / "Issue104_Bizarre_Love_Triangle2.mid"
ABC_FIXTURE_PATH = TESTCASES_PATH / "abc_files" / "a_morning_in_summer.abc"


def test_malformed_midi_default_load_is_strict():
    with pytest.raises(RuntimeError):
        Score(ISSUE_104_PATH)


@pytest.mark.parametrize("sanitize_data", [False, True], ids=["strict", "sanitize"])
def test_malformed_midi_raises_runtime_error(sanitize_data: bool):
    with pytest.raises(RuntimeError):
        Score(ISSUE_104_PATH, sanitize_data=sanitize_data)

    with pytest.raises(RuntimeError):
        Score.from_file(ISSUE_104_PATH, format="midi", sanitize_data=sanitize_data)


def test_valid_midi_path_loading_accepts_sanitize_data():
    midi_path = MIDI_PATHS_ALL[0]

    default_score = Score(midi_path)
    sanitized_score = Score(midi_path, sanitize_data=True)
    sanitized_from_file = Score.from_file(midi_path, format="midi", sanitize_data=True)

    assert sanitized_score.note_num() == default_score.note_num()
    assert sanitized_from_file.note_num() == default_score.note_num()
    assert sanitized_score.ticks_per_quarter == default_score.ticks_per_quarter
    assert sanitized_from_file.ticks_per_quarter == default_score.ticks_per_quarter


def test_abc_rejects_sanitize_data():
    with pytest.raises(ValueError, match="sanitize_data"):
        Score.from_file(ABC_FIXTURE_PATH, format="abc", sanitize_data=True)


def test_malformed_midi_raises_in_subprocess():
    script = f"""
from pathlib import Path
from symusic import Score

path = Path({str(ISSUE_104_PATH)!r})
try:
    Score(path)
except RuntimeError:
    raise SystemExit(0)
raise SystemExit(1)
"""
    result = subprocess.run(
        [sys.executable, "-c", script],
        check=False,
        capture_output=True,
        text=True,
    )

    assert result.returncode == 0, result.stderr or result.stdout

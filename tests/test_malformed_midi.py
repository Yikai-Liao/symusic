from __future__ import annotations

import subprocess
import sys

import pytest
from symusic import Score

from tests.utils import MIDI_PATHS_ALL, TESTCASES_PATH

ISSUE_104_PATH = (
    TESTCASES_PATH / "MIDIs_corrupted" / "Issue104_Bizarre_Love_Triangle2.mid"
)
ABC_FIXTURE_PATH = TESTCASES_PATH / "abc_files" / "a_morning_in_summer.abc"


def build_single_track_midi(track_bytes: bytes, tpq: int = 480) -> bytes:
    header = b"MThd" + (6).to_bytes(4, "big")
    header += (0).to_bytes(2, "big")
    header += (1).to_bytes(2, "big")
    header += tpq.to_bytes(2, "big")
    track = b"MTrk" + len(track_bytes).to_bytes(4, "big") + track_bytes
    return header + track


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


@pytest.mark.parametrize(
    ("case_name", "midi_bytes"),
    [
        pytest.param(
            "zero-length-sysex",
            build_single_track_midi(
                bytes(
                    [
                        0x00,
                        0xF0,
                        0x00,
                        0x00,
                        0x90,
                        0x3C,
                        0x40,
                        0x60,
                        0x80,
                        0x3C,
                        0x40,
                        0x00,
                        0xFF,
                        0x2F,
                        0x00,
                    ]
                )
            ),
            id="zero-length-sysex",
        ),
        pytest.param(
            "zero-length-meta",
            build_single_track_midi(
                bytes(
                    [
                        0x00,
                        0xFF,
                        0x01,
                        0x00,
                        0x00,
                        0x90,
                        0x3C,
                        0x40,
                        0x60,
                        0x80,
                        0x3C,
                        0x40,
                        0x00,
                        0xFF,
                        0x2F,
                        0x00,
                    ]
                )
            ),
            id="zero-length-meta",
        ),
    ],
)
def test_minimidi_vlq_cursor_regressions_parse_minimal_sequences(
    case_name: str,
    midi_bytes: bytes,
):
    # These byte sequences exercise the sequencing-sensitive VLQ parsing paths in minimidi.
    score = Score.from_midi(midi_bytes)

    assert case_name
    assert score.ticks_per_quarter == 480
    assert len(score.tracks) == 1
    assert score.note_num() == 1


def test_minimidi_unknown_status_reports_status_byte():
    midi_bytes = build_single_track_midi(bytes([0x00, 0xF4, 0x00, 0xFF, 0x2F, 0x00]))

    with pytest.raises(RuntimeError, match=r"Unknown or unsupported status byte: 0xf4"):
        Score.from_midi(midi_bytes)


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
        [sys.executable, "-c", script],  # noqa: S603
        check=False,
        capture_output=True,
        text=True,
    )

    assert result.returncode == 0, result.stderr or result.stdout

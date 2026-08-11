from __future__ import annotations

import platform
import subprocess
import sys
import textwrap
from pathlib import Path

import pytest

MYPY_SUPPORTED = platform.python_implementation() != "PyPy"
MYPY_SKIP_REASON = "mypy checks run on the CPython CI jobs"

POSITIVE_SAMPLE = """
    from symusic import Score
    import symusic.core as core
    import symusic.types as smt


    public_score = Score(ttype="second")
    public_pitch: int = public_score.tracks[0].notes[0].pitch

    tick_score: core.Score[core.Tick] = core.ScoreTick(480)
    tick_track: core.Track[core.Tick] = tick_score.tracks[0]
    tick_note: core.Note[core.Tick] = tick_track.notes[0]
    tick_score_copy: core.Score[core.Tick] = tick_score.copy()
    tick_track_copy: core.Track[core.Tick] = tick_track.copy()
    tick_note_copy: core.Note[core.Tick] = tick_note.copy()
    tick_pitch: int = tick_note.pitch

    second_score: core.Score[core.Second] = core.ScoreSecond(480)
    second_track: core.Track[core.Second] = second_score.tracks[0]
    second_note: core.Note[core.Second] = second_track.notes[0]
    second_control: core.ControlChange[core.Second] = second_track.controls[0]
    second_pedal: core.Pedal[core.Second] = second_track.pedals[0]
    second_bend: core.PitchBend[core.Second] = second_track.pitch_bends[0]
    second_lyric: core.TextMeta[core.Second] = second_track.lyrics[0]
    second_tempo: core.Tempo[core.Second] = second_score.tempos[0]
    second_key: core.KeySignature[core.Second] = second_score.key_signatures[0]
    second_meter: core.TimeSignature[core.Second] = second_score.time_signatures[0]
    second_marker: core.TextMeta[core.Second] = second_score.markers[0]

    note_pitch: int = second_note.pitch
    control_number: int = second_control.number
    pedal_duration: int | float = second_pedal.duration
    bend_value: int = second_bend.value
    lyric_text: str = second_lyric.text
    qpm: float = second_tempo.qpm
    key_value: int = second_key.key
    numerator: int = second_meter.numerator
    marker_text: str = second_marker.text

    quarter_score: core.Score[core.Quarter] = core.ScoreQuarter(480)
    tick_clipped: core.Score[core.Tick] = tick_score.clip(0, 10)
    quarter_clipped: core.Score[core.Quarter] = quarter_score.clip(0.0, 1.0)
    second_clipped: core.Score[core.Second] = second_score.clip(0.0, 1.0)
    tick_score.get_beats(0)
    quarter_score.get_beats(0.0)
    second_score.get_beats(0.0)
    tick_resampled: core.Score[core.Tick] = tick_score.resample(960, 1)
    quarter_resampled: core.Score[core.Tick] = quarter_score.resample(960, 0.25)
    tick_score.pianoroll()

    alias_score: smt.Score[core.Tick] = core.ScoreTick(480)
    alias_track: smt.Track[core.Tick] = alias_score.tracks[0]
    alias_note: smt.Note[core.Tick] = alias_track.notes[0]
    notes: smt.PyVec[smt.Note[core.Tick]] = alias_track.notes
    notes.append(core.NoteTick(0, 1, 60, 100))
    filtered: smt.PyVec[smt.Note[core.Tick]] = notes.filter(
        lambda item: item.pitch > 60
    )
    filtered_pitch: int = filtered[0].pitch

    signatures: smt.TimeSignatureList
    signatures = core.TimeSignatureTickList()
    signatures = core.TimeSignatureQuarterList()
    signatures = core.TimeSignatureSecondList()
"""

NEGATIVE_SAMPLE = """
    import symusic.core as core


    tick: core.Score[core.Tick] = core.ScoreTick(480)
    second: core.Score[core.Second] = core.ScoreSecond(480)

    tick.clip(0.5, 1.5)
    tick.get_beats(0.5)
    second.pianoroll()
    wrong_unit: core.Score[core.Tick] = core.ScoreSecond(480)
"""


def _write_sample(path: Path, content: str) -> None:
    path.write_text(
        f"from __future__ import annotations\n\n{textwrap.dedent(content).strip()}\n"
    )


@pytest.mark.skipif(not MYPY_SUPPORTED, reason=MYPY_SKIP_REASON)
def test_complete_typing_contract(tmp_path: Path) -> None:
    positive = tmp_path / "typing_positive.py"
    negative = tmp_path / "typing_negative.py"
    _write_sample(positive, POSITIVE_SAMPLE)
    _write_sample(negative, NEGATIVE_SAMPLE)

    result = subprocess.run(
        [
            sys.executable,
            "-m",
            "mypy",
            "--strict",
            "--no-incremental",
            str(positive),
            str(negative),
        ],
        capture_output=True,
        text=True,
        check=False,
    )

    assert result.returncode != 0, "mypy unexpectedly accepted the negative sample"
    assert str(positive) not in result.stdout, result.stdout
    assert str(negative) in result.stdout, result.stdout
    assert 'incompatible type "float"; expected "int"' in result.stdout
    assert "Invalid self argument" in result.stdout
    assert "Incompatible types in assignment" in result.stdout

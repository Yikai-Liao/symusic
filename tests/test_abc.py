from __future__ import annotations

from pathlib import Path

import pytest
from symusic import Note, Score, Tempo, TimeSignature, Track

from tests.utils import TESTCASES_PATH

ABC_PATHS = sorted((TESTCASES_PATH / "abc_files").glob("*.abc"))


def make_roundtrip_score() -> Score:
    score = Score(480)
    track = Track("Roundtrip", 0, False)
    track.notes.append(Note(0, 480, 60, 96))
    track.notes.append(Note(480, 480, 62, 96))
    track.notes.append(Note(960, 960, 64, 96))
    score.tracks.append(track)
    score.tempos.append(Tempo(0, 120))
    score.time_signatures.append(TimeSignature(0, 4, 4))
    return score


@pytest.mark.parametrize("abc_path", ABC_PATHS, ids=lambda path: path.name)
def test_read_abc(abc_path: Path):
    score = Score(abc_path, fmt="abc")

    assert len(score.tracks) > 0
    assert score.note_num() > 0


def test_dump_abc_roundtrip(tmp_path: Path):
    score = make_roundtrip_score()
    output_dir = tmp_path / "abc output"
    output_dir.mkdir()
    output_path = output_dir / "round trip score.abc"

    abc_text = score.dumps_abc()
    assert abc_text.strip()

    score.dump_abc(output_path)

    assert output_path.exists()
    assert output_path.stat().st_size > 0

    loaded_with_ctor = Score(output_path, fmt="abc")
    loaded_with_factory = Score.from_file(output_path, format="abc")

    assert loaded_with_ctor.note_num() == score.note_num()
    assert loaded_with_factory.note_num() == score.note_num()
    assert len(loaded_with_ctor.tracks) == 1
    assert len(loaded_with_factory.tracks) == 1

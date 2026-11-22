import pytest
from symusic import Note, Score, Tempo, Track
from symusic.core import NoteQuarterList


def test_trim_notes():
    track = Track(
        notes=[
            Note(time=0, duration=1, pitch=72, velocity=100, ttype="Quarter"),
            Note(time=0, duration=3, pitch=72, velocity=100, ttype="Quarter"),
            Note(time=1, duration=1, pitch=72, velocity=100, ttype="Quarter"),
            Note(time=1, duration=2, pitch=72, velocity=100, ttype="Quarter"),
            Note(time=2.91, duration=1.11, pitch=72, velocity=100, ttype="Quarter"),
            Note(time=2, duration=3, pitch=72, velocity=100, ttype="Quarter"),
            Note(time=4, duration=1, pitch=72, velocity=100, ttype="Quarter"),
        ],
        ttype="Quarter",
    )

    track = track.trim(
        start=1,
        end=3,
        min_overlap=0.1,
        start_mode="retain",
        end_mode="truncate",
    )
    trimmed = NoteQuarterList(
        [
            Note(0.00, 3.00, 72, 100, "Quarter"),
            Note(1.00, 1.00, 72, 100, "Quarter"),
            Note(1.00, 2.00, 72, 100, "Quarter"),
            Note(2.00, 1.00, 72, 100, "Quarter"),
        ],
    )
    assert track.notes == trimmed


@pytest.mark.parametrize("ttype", ["tick", "quarter", "second"])
def test_score_trim_returns_independent_score(ttype):
    score = Score(ttype=ttype)
    track = Track(ttype=ttype)
    track.notes = [Note(0, 4, 72, 100, ttype)]
    score.tracks.append(track)
    score.tempos.append(Tempo(0, 120, ttype=ttype))

    trimmed = score.trim(1, 3, start_mode="retain", end_mode="truncate")

    assert type(trimmed) is type(score)
    assert trimmed.tracks[0].notes[0].time == 0
    assert trimmed.tracks[0].notes[0].duration == 3
    assert trimmed.tempos[0].time == 1
    assert score.tracks[0].notes[0].duration == 4
    assert score.tempos[0].time == 0
    with pytest.raises(TypeError):
        score.trim(1, 3, inplace=True)
    with pytest.raises(ValueError, match="trim modes"):
        score.trim(1, 3, end_mode="unknown")

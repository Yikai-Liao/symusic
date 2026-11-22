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


@pytest.mark.parametrize("boundary", ["start_mode", "end_mode"])
@pytest.mark.parametrize("inplace", [False, True])
def test_track_rejects_unknown_trim_mode(boundary, inplace):
    track = Track(notes=[Note(1, 2, 60, 80)])
    with pytest.raises(ValueError, match=rf"{boundary}.*unknown"):
        track.trim(2, 4, inplace=inplace, **{boundary: "unknown"})
    assert track.notes[0].start == 1


@pytest.mark.parametrize("ttype", ["tick", "quarter", "second"])
def test_score_trim(ttype):
    score = Score(480, ttype=ttype)
    track = Track(notes=[Note(1, 5, 60, 80, ttype)], ttype=ttype)
    score.tracks.append(track)
    score.tempos.append(Tempo(0, 120, ttype=ttype))

    trimmed = score.trim(2, 4, start_mode="truncate", end_mode="truncate")
    assert trimmed.tracks[0] == track.trim(
        2, 4, start_mode="truncate", end_mode="truncate"
    )
    assert trimmed.tempos[0].time == 2
    assert score.tracks[0].notes[0].start == 1
    updated = score.trim(2, 4, start_mode="truncate", end_mode="truncate", inplace=True)
    assert updated == score == trimmed
    updated.ticks_per_quarter = 960
    assert score.ticks_per_quarter == 960


def test_empty_score_rejects_unknown_trim_mode():
    with pytest.raises(ValueError, match=r"end_mode.*unknown"):
        Score(480).trim(0, 1, end_mode="unknown")

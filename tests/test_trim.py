import pytest
from symusic import (
    ControlChange,
    Note,
    NoteList,
    Quarter,
    Score,
    Second,
    Tempo,
    Tick,
    Track,
)


def test_trim_notes():
    track = Track(
        notes=[
            Note(time=0, duration=1, pitch=72, velocity=100, ttype=Quarter),
            Note(time=0, duration=3, pitch=72, velocity=100, ttype=Quarter),
            Note(time=1, duration=1, pitch=72, velocity=100, ttype=Quarter),
            Note(time=1, duration=2, pitch=72, velocity=100, ttype=Quarter),
            Note(time=2.91, duration=1.11, pitch=72, velocity=100, ttype=Quarter),
            Note(time=2, duration=3, pitch=72, velocity=100, ttype=Quarter),
            Note(time=4, duration=1, pitch=72, velocity=100, ttype=Quarter),
        ],
        ttype=Quarter,
    )

    track = track.trim(
        start=1,
        end=3,
        min_overlap=0.1,
        start_mode="retain",
        end_mode="truncate",
    )
    trimmed = NoteList(
        [
            Note(0.00, 3.00, 72, 100, ttype=Quarter),
            Note(1.00, 1.00, 72, 100, ttype=Quarter),
            Note(1.00, 2.00, 72, 100, ttype=Quarter),
            Note(2.00, 1.00, 72, 100, ttype=Quarter),
        ],
        ttype=Quarter,
    )
    assert track.notes == trimmed


@pytest.mark.parametrize("boundary", ["start_mode", "end_mode"])
def test_track_rejects_unknown_trim_mode(boundary):
    track = Track(notes=[Note(1, 2, 60, 80)])
    with pytest.raises(ValueError, match=rf"{boundary}.*unknown"):
        track.trim(2, 4, **{boundary: "unknown"})
    assert track.notes[0].time == 1


@pytest.mark.parametrize("ttype", [Tick, Quarter, Second])
def test_score_trim(ttype):
    score = Score(ticks_per_quarter=480, ttype=ttype)
    track = Track(notes=[Note(1, 5, 60, 80, ttype=ttype)], ttype=ttype)
    score.tracks.append(track)
    score.tempos.append(Tempo(0, 120, ttype=ttype))

    trimmed = score.trim(2, 4, start_mode="truncate", end_mode="truncate")
    assert trimmed.tracks[0] == track.trim(
        2, 4, start_mode="truncate", end_mode="truncate"
    )
    assert trimmed.tempos[0].time == 2
    trimmed.tracks[0].notes[0].time = 3
    assert score.tracks[0].notes[0].time == 1
    assert score.tempos[0].time == 0


@pytest.mark.parametrize("ttype", [Tick, Quarter, Second])
@pytest.mark.parametrize("mode", ["truncate", "retain"])
def test_score_trim_checks_window_overlap(ttype, mode):
    score = Score(ticks_per_quarter=480, ttype=ttype)
    track = Track(notes=[Note(0, 100, 60, 80, ttype=ttype)], ttype=ttype)
    track.controls.extend(
        [ControlChange(0, 64, 127, ttype=ttype), ControlChange(100, 64, 0, ttype=ttype)]
    )
    score.tracks.append(track)

    trimmed = score.trim(10, 12, 3, start_mode=mode, end_mode=mode)
    assert trimmed.tracks[0].empty()

    trimmed = score.trim(10, 12, 2, start_mode=mode, end_mode=mode)
    expected = (10, 2) if mode == "truncate" else (0, 100)
    assert [(note.time, note.duration) for note in trimmed.tracks[0].notes] == [
        expected
    ]
    assert trimmed.tracks[0].pedals() == ()


def test_score_trim_checks_contained_note_duration():
    score = Score(ticks_per_quarter=480)
    score.tracks.append(Track(notes=[Note(11, 1, 60, 80), Note(11, 2, 61, 80)]))

    trimmed = score.trim(10, 20, min_overlap=2)

    assert [note.pitch for note in trimmed.tracks[0].notes] == [61]


@pytest.mark.parametrize("ttype", [Tick, Quarter, Second])
def test_score_trim_rejects_inplace(ttype):
    with pytest.raises(TypeError, match="incompatible function arguments"):
        Score(ticks_per_quarter=480, ttype=ttype).trim(0, 1, inplace=True)


def test_empty_score_rejects_unknown_trim_mode():
    with pytest.raises(ValueError, match=r"end_mode.*unknown"):
        Score(ticks_per_quarter=480).trim(0, 1, end_mode="unknown")


@pytest.mark.parametrize("ttype", [Tick, Quarter, Second])
@pytest.mark.parametrize("has_track", [False, True])
def test_score_trim_rejects_reversed_bounds(ttype, has_track):
    score = Score(ttype=ttype)
    if has_track:
        score.tracks.append(Track(ttype=ttype))
    with pytest.raises(ValueError, match="start <= end"):
        score.trim(4, 2)


@pytest.mark.parametrize("ttype", [Quarter, Second])
@pytest.mark.parametrize("bounds", [(float("nan"), 2), (0, float("nan"))])
def test_score_trim_rejects_nan_bounds(ttype, bounds):
    with pytest.raises(ValueError, match="start <= end"):
        Score(ttype=ttype).trim(*bounds)

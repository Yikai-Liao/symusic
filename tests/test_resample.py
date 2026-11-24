"""`Score.resample` rescales times to a new ticks-per-quarter and rounds half away from zero."""

import pytest
from symusic import (
    ControlChange,
    Note,
    Quarter,
    Score,
    Second,
    Tempo,
    Tick,
    TimeSignature,
    Track,
)


def make_score(tpq, notes, ttype=Tick):
    score = Score(tpq, ttype=ttype)
    score.tempos.append(Tempo(0, mspq=500000, ttype=ttype))
    score.time_signatures.append(TimeSignature(0, 4, 4, ttype=ttype))
    track = Track("Piano", 3, False, ttype=ttype)
    for time, duration in notes:
        track.notes.append(Note(time, duration, 60, 100, ttype=ttype))
    score.tracks.append(track)
    return score


def note_times(score):
    return [(n.time, n.duration) for n in score.tracks[0].notes]


def test_resample_upsample_exact():
    out = make_score(480, [(480, 480)]).resample(960)
    assert out.ticks_per_quarter == 960
    assert note_times(out) == [(960, 960)]
    note = out.tracks[0].notes[0]
    assert (note.pitch, note.velocity) == (60, 100)
    assert out.tempos[0].time == 0
    assert out.time_signatures[0].time == 0
    assert (out.tracks[0].name, out.tracks[0].program) == ("Piano", 3)


def test_resample_downsample_exact():
    out = make_score(480, [(480, 480)]).resample(240)
    assert out.ticks_per_quarter == 240
    assert note_times(out) == [(240, 240)]


def test_resample_upsample_rounds_half_up():
    # 4 -> 6 ticks per quarter scales by 1.5: 1 -> 1.5 -> 2 and 3 -> 4.5 -> 5.
    out = make_score(4, [(1, 3), (3, 1)]).resample(6)
    assert note_times(out) == [(2, 5), (5, 2)]


def test_resample_downsample_rounds_half_up():
    # 480 -> 100 scales by 5/24: 12 -> 2.5 -> 3, 11 -> 2.29 -> 2, 14 -> 2.92 -> 3.
    out = make_score(480, [(12, 11), (14, 24)]).resample(100)
    assert note_times(out) == [(3, 2), (3, 5)]


def test_resample_downsample_min_dur():
    # Durations that round to zero are raised to `min_dur`; times are untouched by it.
    notes = [(100, 1), (480, 3)]
    assert note_times(make_score(480, notes).resample(100)) == [(21, 0), (100, 1)]
    out = make_score(480, notes).resample(100, min_dur=1)
    assert note_times(out) == [(21, 1), (100, 1)]
    out = make_score(480, notes).resample(100, min_dur=5)
    assert note_times(out) == [(21, 5), (100, 5)]


def test_resample_keeps_source():
    score = make_score(480, [(480, 480)])
    score.resample(960)
    assert score.ticks_per_quarter == 480
    assert note_times(score) == [(480, 480)]


def test_resample_other_events():
    score = make_score(4, [])
    score.tempos.append(Tempo(1, mspq=400000))
    score.tracks[0].controls.append(ControlChange(3, 7, 100, ttype=Tick))
    out = score.resample(6)
    assert [t.time for t in out.tempos] == [0, 2]
    assert [c.time for c in out.tracks[0].controls] == [5]
    assert out.tempos[1].mspq == 400000


@pytest.mark.parametrize("ttype", [Quarter, Second])
def test_resample_other_units_returns_ticks(ttype):
    # At 120 BPM one quarter lasts 0.5 s; the result is always a tick score.
    time, duration = (1.0, 2.0) if ttype == Quarter else (0.5, 1.0)
    out = make_score(480, [(time, duration)], ttype).resample(960)
    assert out.ttype is Tick
    assert out.ticks_per_quarter == 960
    assert note_times(out) == [(960, 1920)]


@pytest.mark.parametrize("tpq", [0, -480])
def test_resample_rejects_tpq(tpq):
    with pytest.raises(ValueError, match="ticks_per_quarter"):
        make_score(480, [(0, 1)]).resample(tpq)


def test_resample_rejects_negative_min_dur():
    with pytest.raises(ValueError, match="min_dur"):
        make_score(480, [(0, 1)]).resample(960, min_dur=-1)


@pytest.mark.parametrize("ttype", [Quarter, Second])
def test_resample_min_dur_uses_target_ticks(ttype):
    score = make_score(480, [(0.0, 0.0001)], ttype)
    out = score.resample(100, min_dur=5)
    assert note_times(out) == [(0, 5)]
    with pytest.raises(TypeError):
        score.resample(100, min_dur=0.5)

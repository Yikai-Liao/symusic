from pathlib import Path

import pytest
from symusic import ControlChange, Note, Quarter, Score, Second, Tick, Track

TPQ = 480
# At 120 qpm (the default tempo), one quarter lasts 0.5 s.
SECONDS_PER_QUARTER = 0.5


def _build_score(n_channels: int) -> Score:
    # dump_midi gives non-drum tracks channels 0-8 and 10-15 by position, so the first
    # 15 tracks cover every non-drum channel and the 16th reuses channel 0.
    score = Score(ticks_per_quarter=TPQ)
    for i in range(n_channels):
        track = Track(program=0, is_drum=False, name=f"t{i}")
        track.notes.append(Note(0, 4 * TPQ, 60, 100))
        track.controls.append(ControlChange(TPQ, 64, 127))
        track.controls.append(ControlChange(2 * TPQ, 64, 0))
        # A pedal-off without a preceding pedal-on must not create a pedal.
        track.controls.append(ControlChange(3 * TPQ, 64, 0))
        score.tracks.append(track)
    return score


@pytest.mark.parametrize(
    ("ttype", "scale"),
    [(Tick, TPQ), (Quarter, 1), (Second, SECONDS_PER_QUARTER)],
)
def test_pedal_starts_at_cc64_time_on_every_channel(
    tmp_path: Path, ttype: str, scale: float
):
    path = tmp_path / "pedals.mid"
    _build_score(16).dump(path)

    tracks = Score(path, ttype=ttype).tracks
    assert len(tracks) == 16
    for track in tracks:
        assert len(track.pedals()) == 1, track.name
        pedal = track.pedals()[0]
        assert pedal.time == pytest.approx(1 * scale), track.name
        assert pedal.duration == pytest.approx(1 * scale), track.name


def test_pedal_off_without_on_creates_no_pedal(tmp_path: Path):
    score = Score(ticks_per_quarter=TPQ)
    for i in range(3):
        track = Track(program=0, is_drum=False, name=f"t{i}")
        track.notes.append(Note(0, TPQ, 60, 100))
        track.controls.append(ControlChange(TPQ, 64, 0))
        score.tracks.append(track)
    path = tmp_path / "off_only.mid"
    score.dump(path)

    for track in Score(path).tracks:
        assert len(track.pedals()) == 0, track.name


def test_pedals_are_detached_from_controls():
    track = _build_score(1).tracks[0]
    snapshot = track.pedals()
    assert isinstance(snapshot, tuple)
    assert (snapshot[0].time, snapshot[0].end) == (TPQ, 2 * TPQ)
    assert not hasattr(snapshot[0], "start")
    track.controls[1].time += 7
    assert track.pedals()[0].end == 2 * TPQ + 7
    assert snapshot[0].end == 2 * TPQ
    assert track.shift_time(-2 * TPQ) is None
    assert track.pedals()[0].time == -TPQ
    assert track.clip(-TPQ, 0).pedals() == ()


def test_pedal_promoted_after_program_change():
    track_data = bytes.fromhex(
        "00 B0 40 7F"  # Sustain on at tick 0, before any note.
        " 01 C0 05"  # Program 5 at tick 1.
        " 01 90 3C 64"  # Note on at tick 2.
        " 02 80 3C 00"  # Note off at tick 4.
        " 01 B0 40 00"  # Sustain off at tick 5.
        " 00 FF 2F 00"
    )
    midi = (
        b"MThd\x00\x00\x00\x06\x00\x00\x00\x01\x01\xe0MTrk"
        + len(track_data).to_bytes(4, "big")
        + track_data
    )
    score = Score.from_midi(midi)
    assert len(score.tracks) == 1
    track = score.tracks[0]
    assert track.program == 5
    assert [(pedal.time, pedal.end) for pedal in track.pedals()] == [(0, 5)]
    restored = Score.from_midi(score.to_midi())
    assert restored == score
    assert restored.tracks[0].pedals() == track.pedals()


@pytest.mark.parametrize("reverse", [False, True])
@pytest.mark.parametrize("scope", ["score", "track", "controls"])
def test_sort_preserves_same_tick_controls(reverse, scope):
    score = Score()
    track = Track(notes=[Note(0, 20, 60, 80)])
    track.controls.extend(
        [
            ControlChange(10, 64, 127),
            ControlChange(10, 1, 75),
            ControlChange(10, 64, 0),
            ControlChange(10, 1, 25),
        ]
    )
    score.tracks.append(track)
    before = track.pedals()
    assert [(pedal.time, pedal.duration) for pedal in before] == [(10, 0)]

    {"score": score.sort, "track": track.sort, "controls": track.controls.sort}[scope](
        reverse=reverse
    )

    assert track.controls.is_sorted(reverse=reverse)
    expected = (
        [(64, 127), (64, 0), (1, 75), (1, 25)]
        if reverse
        else [(1, 75), (1, 25), (64, 127), (64, 0)]
    )
    assert [(c.number, c.value) for c in track.controls] == expected
    assert track.pedals() == before
    restored = Score.from_midi(score.to_midi()).tracks[0]
    assert restored.controls == track.controls
    assert restored.pedals() == before

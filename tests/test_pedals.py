from pathlib import Path

import pytest
from symusic import ControlChange, Note, Quarter, Score, Second, Tick, Track

TPQ = 480
# At 120 qpm (the default tempo), one quarter lasts 0.5 s.
SECONDS_PER_QUARTER = 0.5


def _build_score(n_channels: int) -> Score:
    # dump_midi gives non-drum tracks channels 0-8 and 10-15 by position, so the first
    # 15 tracks cover every non-drum channel and the 16th reuses channel 0.
    score = Score(TPQ)
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
    _build_score(16).dump_midi(path)

    tracks = Score(path, ttype=ttype).tracks
    assert len(tracks) == 16
    for track in tracks:
        assert len(track.pedals) == 1, track.name
        pedal = track.pedals[0]
        assert pedal.time == pytest.approx(1 * scale), track.name
        assert pedal.duration == pytest.approx(1 * scale), track.name


def test_pedal_off_without_on_creates_no_pedal(tmp_path: Path):
    score = Score(TPQ)
    for i in range(3):
        track = Track(program=0, is_drum=False, name=f"t{i}")
        track.notes.append(Note(0, TPQ, 60, 100))
        track.controls.append(ControlChange(TPQ, 64, 0))
        score.tracks.append(track)
    path = tmp_path / "off_only.mid"
    score.dump_midi(path)

    for track in Score(path).tracks:
        assert len(track.pedals) == 0, track.name

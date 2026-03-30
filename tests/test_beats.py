from __future__ import annotations

import numpy as np
from symusic import Note, Score, Tempo, TimeSignature, Track


def make_tick_score(
    end_quarters: float,
    tempos: list[tuple[float, float]] | None = None,
    time_signatures: list[tuple[float, int, int]] | None = None,
) -> Score:
    score = Score(480)
    track = Track("Beats", 0, False)
    track.notes.append(Note(0, int(round(end_quarters * 480)), 60, 100))
    score.tracks.append(track)

    for quarter_time, qpm in tempos or []:
        score.tempos.append(Tempo(int(round(quarter_time * 480)), qpm))
    for quarter_time, numerator, denominator in time_signatures or []:
        score.time_signatures.append(
            TimeSignature(int(round(quarter_time * 480)), numerator, denominator)
        )
    return score


def test_get_beats_and_downbeats_default_sentinels_tick():
    score = make_tick_score(8)

    beats = score.get_beats()
    downbeats = score.get_downbeats()

    assert isinstance(beats, np.ndarray)
    assert beats.dtype == np.int32
    np.testing.assert_array_equal(beats, np.arange(0, 8 * 480, 480, dtype=np.int32))
    np.testing.assert_array_equal(downbeats, np.array([0, 1920], dtype=np.int32))


def test_get_beats_simple_meter_quarter():
    score = make_tick_score(8, time_signatures=[(0, 4, 4)]).to("quarter")

    beats = score.get_beats()
    downbeats = score.get_downbeats()

    assert beats.dtype == np.float32
    np.testing.assert_allclose(beats, np.arange(0, 8, dtype=np.float32))
    np.testing.assert_allclose(downbeats, np.array([0, 4], dtype=np.float32))


def test_get_beats_compound_meter_quarter():
    score = make_tick_score(12, time_signatures=[(0, 6, 8)]).to("quarter")

    beats = score.get_beats()
    downbeats = score.get_downbeats()

    np.testing.assert_allclose(
        beats,
        np.array([0, 1.5, 3, 4.5, 6, 7.5, 9, 10.5], dtype=np.float32),
    )
    np.testing.assert_allclose(downbeats, np.array([0, 3, 6, 9], dtype=np.float32))


def test_get_downbeats_across_time_signature_changes():
    score = make_tick_score(10, time_signatures=[(0, 4, 4), (4, 3, 4)]).to("quarter")

    beats = score.get_beats()
    downbeats = score.get_downbeats()

    np.testing.assert_allclose(beats, np.arange(0, 10, dtype=np.float32))
    np.testing.assert_allclose(downbeats, np.array([0, 4, 7], dtype=np.float32))


def test_get_beats_and_downbeats_with_pickup_start_tick():
    score = make_tick_score(8, time_signatures=[(0, 4, 4)])

    beats = score.get_beats(start_time=240)
    downbeats = score.get_downbeats(start_time=240)

    np.testing.assert_array_equal(
        beats,
        np.array([240, 720, 1200, 1680, 2160, 2640, 3120, 3600], dtype=np.int32),
    )
    np.testing.assert_array_equal(downbeats, np.array([240, 2160], dtype=np.int32))


def test_get_beats_and_downbeats_in_second_time():
    score = make_tick_score(
        8,
        tempos=[(0, 120), (4, 60)],
        time_signatures=[(0, 4, 4)],
    ).to("second")

    beats = score.get_beats()
    downbeats = score.get_downbeats()

    assert beats.dtype == np.float32
    np.testing.assert_allclose(
        beats,
        np.array([0, 0.5, 1.0, 1.5, 2.0, 3.0, 4.0, 5.0], dtype=np.float32),
    )
    np.testing.assert_allclose(downbeats, np.array([0, 2.0], dtype=np.float32))

from __future__ import annotations

import warnings
from operator import attrgetter

import numpy as np
import pytest
from symusic import Note, Score, Tempo, TimeSignature, Track

from tests.utils import MIDI_PATHS_ALL

pretty_midi = pytest.importorskip("pretty_midi")


def _assert_times_match_pretty_midi(
    pretty_midi_times: np.ndarray,
    symusic_times: np.ndarray,
    *,
    end_time: float,
    label: str,
    atol: float = 1e-4,
) -> None:
    if pretty_midi_times.shape == symusic_times.shape:
        np.testing.assert_allclose(
            pretty_midi_times,
            symusic_times,
            atol=atol,
            rtol=1e-6,
            err_msg=label,
        )
        return

    # pretty_midi occasionally keeps a final event exactly at the piece end when working in
    # floating-point seconds. Treat that as equivalent to the canonical unit-consistent grid.
    if pretty_midi_times.size == symusic_times.size + 1:
        np.testing.assert_allclose(
            pretty_midi_times[:-1],
            symusic_times,
            atol=atol,
            rtol=1e-6,
            err_msg=f"{label} (prefix mismatch)",
        )
        assert pretty_midi_times[-1] == pytest.approx(end_time, abs=atol), label
        return

    if symusic_times.size == pretty_midi_times.size + 1:
        np.testing.assert_allclose(
            pretty_midi_times,
            symusic_times[:-1],
            atol=atol,
            rtol=1e-6,
            err_msg=f"{label} (prefix mismatch)",
        )
        assert symusic_times[-1] == pytest.approx(end_time, abs=atol), label
        return

    msg = (
        f"{label}: pretty_midi has shape {pretty_midi_times.shape}, "
        f"symusic has shape {symusic_times.shape}"
    )
    raise AssertionError(msg)


def _load_pretty_midi(path) -> pretty_midi.PrettyMIDI:
    with warnings.catch_warnings():
        warnings.simplefilter("ignore", RuntimeWarning)
        return pretty_midi.PrettyMIDI(str(path))


@pytest.mark.parametrize("midi_path", MIDI_PATHS_ALL, ids=attrgetter("name"))
def test_beats_and_downbeats_match_pretty_midi_on_real_midis(midi_path):
    pm = _load_pretty_midi(midi_path)
    score = Score(midi_path, ttype="second")

    _assert_times_match_pretty_midi(
        pm.get_beats(),
        score.get_beats(),
        end_time=float(score.end()),
        label=f"{midi_path.name} beats",
    )
    _assert_times_match_pretty_midi(
        pm.get_downbeats(),
        score.get_downbeats(),
        end_time=float(score.end()),
        label=f"{midi_path.name} downbeats",
    )


@pytest.mark.parametrize("midi_path", MIDI_PATHS_ALL, ids=attrgetter("name"))
def test_beats_and_downbeats_are_consistent_across_ttypes_on_real_midis(midi_path):
    score_tick = Score(midi_path, ttype="tick")
    score_quarter = Score(midi_path, ttype="quarter")
    score_second = Score(midi_path, ttype="second")

    np.testing.assert_allclose(
        score_quarter.get_beats(),
        score_tick.to("quarter").get_beats(),
        atol=1e-5,
        err_msg=f"{midi_path.name} quarter vs tick->quarter beats",
    )
    np.testing.assert_allclose(
        score_quarter.get_beats(),
        score_second.to("quarter").get_beats(),
        atol=1e-5,
        err_msg=f"{midi_path.name} quarter vs second->quarter beats",
    )
    np.testing.assert_allclose(
        score_quarter.get_downbeats(),
        score_tick.to("quarter").get_downbeats(),
        atol=1e-5,
        err_msg=f"{midi_path.name} quarter vs tick->quarter downbeats",
    )
    np.testing.assert_allclose(
        score_quarter.get_downbeats(),
        score_second.to("quarter").get_downbeats(),
        atol=1e-5,
        err_msg=f"{midi_path.name} quarter vs second->quarter downbeats",
    )


def test_mid_beat_tempo_change_preserves_remaining_beat_ratio_in_second_time():
    score = Score(480)

    lead = Track("Lead", 0, False)
    lead.notes.append(Note(0, 24 * 480, 60, 100))
    lead.notes.append(Note(9 * 480, 2 * 480, 64, 88))
    score.tracks.append(lead)

    drums = Track("Drums", 0, True)
    drums.notes.append(Note(2 * 480, 480, 36, 96))
    drums.notes.append(Note(11 * 480, 480, 38, 96))
    score.tracks.append(drums)

    score.tempos.append(Tempo(0, 120))
    score.tempos.append(Tempo(int(8.8 * 480), 93))
    score.time_signatures.append(TimeSignature(0, 4, 4))

    second_score = score.to("second")
    beats = second_score.get_beats()
    downbeats = second_score.get_downbeats()

    np.testing.assert_allclose(beats[:9], np.arange(0, 4.5, 0.5, dtype=np.float32))

    crossed_beat = 4.4 + ((4.5 - 4.4) / 0.5) * (60.0 / 93.0)
    assert beats[9] == pytest.approx(crossed_beat, abs=1e-6)
    np.testing.assert_allclose(
        np.diff(beats[9:13]),
        np.full(3, 60.0 / 93.0, dtype=np.float32),
        atol=1e-6,
    )

    expected_downbeats = np.array(
        [
            0.0,
            2.0,
            4.0,
            crossed_beat + 3 * (60.0 / 93.0),
            crossed_beat + 7 * (60.0 / 93.0),
        ],
        dtype=np.float32,
    )
    np.testing.assert_allclose(downbeats[:5], expected_downbeats, atol=3e-6)

    np.testing.assert_allclose(
        score.to("quarter").get_beats(),
        second_score.to("quarter").get_beats(),
        atol=1e-5,
    )
    np.testing.assert_allclose(
        score.to("quarter").get_downbeats(),
        second_score.to("quarter").get_downbeats(),
        atol=1e-5,
    )

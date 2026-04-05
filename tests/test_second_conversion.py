from __future__ import annotations

from operator import attrgetter
from typing import TYPE_CHECKING

import numpy as np
import pytest
from symusic import Note, Score, Tempo, Track

from tests.utils import MIDI_PATHS_ALL, TESTCASES_PATH

if TYPE_CHECKING:
    from pathlib import Path


class ScoreTimeMapper:
    def __init__(self, score: Score):
        self.score = score
        self.tempos = self.compute_tempo_times(score)

    @staticmethod
    def compute_tempo_times(score: Score) -> np.ndarray:
        prev_tempo_tick, prev_tempo_time = 0, 0
        scale_factor = 60 / float(score.ticks_per_quarter)
        seconds_per_tick = scale_factor / 120.0

        tempo_data = []
        for tempo in score.tempos:
            tempo_time = prev_tempo_time + seconds_per_tick * (
                tempo.time - prev_tempo_tick
            )

            seconds_per_tick = scale_factor / tempo.qpm
            tempo_data.append([tempo.qpm, tempo.time, tempo_time, seconds_per_tick])
            prev_tempo_tick, prev_tempo_time = tempo.time, tempo_time

        return np.stack(tempo_data, axis=1)

    def t2s(self, ticks):
        ids = np.searchsorted(self.tempos[1], ticks, side="right") - 1
        tempo_ticks, tempo_times, seconds_per_tick = self.tempos[1:4, ids]
        return tempo_times + seconds_per_tick * (ticks - tempo_ticks)

    def s2t(self, seconds: float | np.ndarray) -> int | np.ndarray:
        ids = np.searchsorted(self.tempos[2], seconds, side="right") - 1
        tempo_ticks, tempo_times, seconds_per_tick = self.tempos[1:4, ids]
        return (tempo_ticks + (seconds - tempo_times) / seconds_per_tick).astype(int)


@pytest.mark.parametrize("midi_path", MIDI_PATHS_ALL, ids=attrgetter("name"))
def test_second_conversion(midi_path: Path):
    s_tick = Score(midi_path, ttype="tick", fmt="midi")
    s_sec = s_tick.to("second")
    mapper = ScoreTimeMapper(s_tick)
    max_delta = 0
    max_delta_rel = 0
    for track_tick, track_sec in zip(s_tick.tracks, s_sec.tracks):
        if len(track_tick.notes) == 0:
            continue
        track_tick.notes.sort(key=lambda x: (x.start, x.pitch, x.end, x.velocity))
        track_sec.notes.sort(key=lambda x: (x.start, x.pitch, x.end, x.velocity))
        notes_tick = track_tick.notes.numpy()
        start_tick = notes_tick["time"]
        end_tick = start_tick + notes_tick["duration"]

        notes_sec = track_sec.notes.numpy()
        start_sec = notes_sec["time"]
        end_sec = start_sec + notes_sec["duration"]

        start_sec_gt = mapper.t2s(start_tick)
        delta = np.abs(start_sec_gt - start_sec)
        delta_rel = delta / (start_sec_gt + 1e-6)
        max_delta = max(max_delta, np.max(delta))
        max_delta_rel = max(max_delta_rel, np.max(delta_rel))
        assert (
            max_delta_rel < 1e-5
        ), f"max_delta_rel={max_delta_rel}, max_delta={max_delta}"
        end_sec_gt = mapper.t2s(end_tick)
        delta = np.abs(end_sec_gt - end_sec)
        delta_rel = delta / (end_sec_gt + 1e-6)
        max_delta = max(max_delta, np.max(delta))
        max_delta_rel = max(max_delta_rel, np.max(delta_rel))
        assert (
            max_delta_rel < 1e-5
        ), f"max_delta_rel={max_delta_rel}, max_delta={max_delta}"


def test_quarter_parse_matches_tick_conversion(tmp_path):
    score_tick = Score(480)
    track = Track("quarter-check", 0, False)
    track.notes.append(Note(0, 480, 60, 80))
    track.notes.append(Note(480, 960, 64, 80))
    score_tick.tracks.append(track)
    score_tick.tempos.append(Tempo(0, 120))
    score_tick.tempos.append(Tempo(960, 60))

    midi_path = tmp_path / "quarter_time_check.mid"
    score_tick.dump_midi(midi_path)

    quarter_direct = Score(midi_path, ttype="quarter")
    quarter_from_tick = Score(midi_path, ttype="tick").to("quarter")

    notes_direct = quarter_direct.tracks[0].notes
    notes_from_tick = quarter_from_tick.tracks[0].notes

    assert len(notes_direct) == 2
    assert len(notes_from_tick) == 2

    # Explicitly validate quarter-unit note times/durations.
    assert notes_direct[0].time == pytest.approx(0.0)
    assert notes_direct[0].duration == pytest.approx(1.0)
    assert notes_direct[1].time == pytest.approx(1.0)
    assert notes_direct[1].duration == pytest.approx(2.0)

    for note_direct, note_from_tick in zip(notes_direct, notes_from_tick):
        assert note_direct.time == pytest.approx(note_from_tick.time)
        assert note_direct.duration == pytest.approx(note_from_tick.duration)

    # Non-note time events should remain in quarter units as well.
    assert quarter_direct.tempos[0].time == pytest.approx(0.0)
    assert quarter_direct.tempos[1].time == pytest.approx(2.0)


# ---------------------------------------------------------------------------
# Regression test for GitHub issue #90:
#   Score(path, ttype="second") produced all-1.0 durations instead of the
#   correct per-note values.  The root cause was in the MIDI parsing path
#   when converting directly to seconds during load.
# ---------------------------------------------------------------------------

ISSUE_90_MIDI = TESTCASES_PATH / "issue90.mid"


def test_issue90_direct_second_load_durations_not_all_one():
    """Loading with ttype='second' must not produce uniform 1.0 durations."""
    score = Score(ISSUE_90_MIDI, ttype="second")
    notes = score.tracks[0].notes
    assert len(notes) > 0, "Expected at least one note in track 0"
    durations = [n.duration for n in notes]
    unique_durations = set(round(d, 6) for d in durations)
    assert len(unique_durations) > 1, (
        f"All durations are identical ({durations[0]}); "
        f"expected varying values (issue #90 regression)"
    )


def test_issue90_direct_load_matches_tick_conversion():
    """Score(path, ttype='second') must match Score(path).to('second')."""
    score_direct = Score(ISSUE_90_MIDI, ttype="second")
    score_convert = Score(ISSUE_90_MIDI, ttype="tick").to("second")

    assert len(score_direct.tracks) == len(score_convert.tracks)

    for track_d, track_c in zip(score_direct.tracks, score_convert.tracks):
        notes_d = track_d.notes
        notes_c = track_c.notes
        assert len(notes_d) == len(notes_c), (
            f"Note count mismatch: direct={len(notes_d)}, convert={len(notes_c)}"
        )

        for i, (nd, nc) in enumerate(zip(notes_d, notes_c)):
            assert nd.time == pytest.approx(nc.time, abs=1e-4), (
                f"Note {i} time mismatch: direct={nd.time}, convert={nc.time}"
            )
            assert nd.duration == pytest.approx(nc.duration, abs=1e-4), (
                f"Note {i} duration mismatch: direct={nd.duration}, convert={nc.duration}"
            )
            assert nd.pitch == nc.pitch
            assert nd.velocity == nc.velocity


def test_issue90_first_notes_have_expected_relative_durations():
    """The first two notes should be roughly twice as long as the next three."""
    score = Score(ISSUE_90_MIDI, ttype="second")
    notes = score.tracks[0].notes[:5]
    assert len(notes) >= 5, "Expected at least 5 notes in track 0"

    dur = [n.duration for n in notes]
    # Notes 0-1 (~0.95s) should be roughly 2x notes 2-4 (~0.5s).
    long_avg = (dur[0] + dur[1]) / 2
    short_avg = (dur[2] + dur[3] + dur[4]) / 3
    ratio = long_avg / short_avg
    assert 1.5 < ratio < 2.5, (
        f"Duration ratio of first two vs next three notes is {ratio:.2f}; "
        f"expected ~2.0 (durations: {dur})"
    )

from operator import attrgetter
from pathlib import Path

import pytest

from symusic import Score
from typing import Union
import numpy as np
from tests.utils import MIDI_PATHS


class ScoreTimeMapper:
    def __init__(self, score: Score):
        self.score = score
        self.tempos = self.compute_tempo_times(score)

    @staticmethod
    def compute_tempo_times(score: Score) -> np.ndarray:
        prev_tempo_tick, prev_tempo_time = 0, 0
        scale_factor = 60 / float(score.ticks_per_quarter)
        seconds_per_tick = scale_factor / 120.

        tempo_data = []
        for tempo in score.tempos:
            tempo_time = prev_tempo_time + seconds_per_tick * (tempo.time - prev_tempo_tick)

            seconds_per_tick = scale_factor / tempo.qpm
            tempo_data.append([tempo.qpm, tempo.time, tempo_time, seconds_per_tick])
            prev_tempo_tick, prev_tempo_time = tempo.time, tempo_time

        tempo_data = np.stack(tempo_data, axis=1)
        return tempo_data

    def t2s(self, ticks):
        ids = np.searchsorted(self.tempos[1], ticks, side="right") - 1
        tempo_ticks, tempo_times, seconds_per_tick = self.tempos[1:4, ids]
        return tempo_times + seconds_per_tick * (ticks - tempo_ticks)

    def s2t(self, seconds: Union[float, np.ndarray]) -> Union[int, np.ndarray]:
        ids = np.searchsorted(self.tempos[2], seconds, side="right") - 1
        tempo_ticks, tempo_times, seconds_per_tick = self.tempos[1:4, ids]
        return (tempo_ticks + (seconds - tempo_times) / seconds_per_tick).astype(int)


@pytest.mark.parametrize("midi_path", MIDI_PATHS, ids=attrgetter("name"))
def test_second_conversion(midi_path: Path, tmp_path: Path):
    """Test that a MIDI loaded and saved unchanged is indeed the save as before."""
    s_tick = Score(midi_path, ttype="tick", fmt="mid")
    s_sec = s_tick.to("second")
    mapper = ScoreTimeMapper(s_tick)
    for track_tick, track_sec in zip(s_tick.tracks, s_sec.tracks):
        notes_tick = track_tick.notes.numpy()
        start_tick = notes_tick["start"]
        end_tick = start_tick + notes_tick["duration"]

        notes_sec = track_sec.notes.numpy()
        start_sec = notes_sec["start"]
        end_sec = start_sec + notes_sec["duration"]

        start_sec_gt = mapper.t2s(start_tick)
        assert np.allclose(start_sec_gt, start_sec, rtol=1e-3)
        end_sec_gt = mapper.t2s(end_tick)
        assert np.allclose(end_sec_gt, end_sec, rtol=1e-3)

from operator import attrgetter
from pathlib import Path
from typing import Union

import numpy as np
import pytest

from symusic import Score
from tests.utils import MIDI_PATHS


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
def test_second_conversion(midi_path: Path):
    s_tick = Score(midi_path, ttype="tick", fmt="midi")
    s_sec = s_tick.to("second")
    mapper = ScoreTimeMapper(s_tick)
    max_delta = 0
    max_delta_rel = 0
    for track_tick, track_sec in zip(s_tick.tracks, s_sec.tracks):
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

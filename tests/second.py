from symusic import Score
from typing import Union
import numpy as np
from utils import MIDI_PATHS
from pandas import DataFrame
import os
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


def test_second_conversion(midi_path):
    """Test that a MIDI loaded and saved unchanged is indeed the save as before."""
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
        # assert np.allclose(start_sec_gt, start_sec, rtol=1e-2)
        delta = np.abs(start_sec_gt - start_sec)
        delta_rel = delta / (start_sec_gt + 1e-6)
        max_delta = max(max_delta, np.max(delta))
        max_delta_rel = max(max_delta_rel, np.max(delta_rel))
        end_sec_gt = mapper.t2s(end_tick)
        # assert np.allclose(end_sec_gt, end_sec, rtol=1e-3)
        delta = np.abs(end_sec_gt - end_sec)
        delta_rel = delta / (end_sec_gt + 1e-6)
        max_delta = max(max_delta, np.max(delta))
        max_delta_rel = max(max_delta_rel, np.max(delta_rel))

    print(f"Max delta[{midi_path}]: /t/t {max_delta:.6e}, {max_delta_rel:.6e}")
    return max_delta, max_delta_rel

if __name__ == "__main__":
    deltas, deltas_rel = [], []
    MIDI_PATHS.append("C:/Users/nhy/code/symusic-libra/3rdparty/minimidi/example/mahler.mid")
    for p in MIDI_PATHS:
        d, d_rel = test_second_conversion(p)
        deltas.append(d)
        deltas_rel.append(d_rel)
    table = DataFrame({"delta": deltas, "delta_rel": deltas_rel}, index=list(map(os.path.basename, MIDI_PATHS)))
    print(table)

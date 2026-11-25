# pyright: strict
from typing import BinaryIO

from symusic import Note, Score, Second, Tick

score = Score()
seconds = Score(ttype=Second)
second = Note(0.0, 0.5, 60, ttype=Second)
score.tracks[0].notes.append(second)  # rejected
score.tracks[0].notes = [second]  # rejected
score.shift_time(0.5)  # rejected
score.to("second")  # rejected
Score(ttype="tick")  # rejected
Score.from_abc("X:1", tune="7")  # rejected
score.resample(480, min_duration=0.5)  # rejected
seconds.resample(480, min_duration=0.5)  # rejected
seconds.pianoroll()  # rejected
score.trim(0, 1, start_mode="unknown")  # rejected
score.trim(0, 1, end_mode="unknown")  # rejected
notes = score.tracks[0].notes
notes += [second]  # rejected

score.clip(0.5, 1.5)  # rejected
score.get_beats(0.5)  # rejected
wrong_unit: Score[Tick] = seconds  # rejected
score.pianoroll(["unknown"])  # rejected
Score("input.mid", format="unknown")  # rejected
Score("input.mid", ticks_per_quarter=480)  # rejected
Score(None, format="midi")  # rejected
Score("input.mid", strict="clamp")  # rejected
Score.from_midi(b"", strict="clamp")  # rejected
Score.from_abc("X:1", strict="clamp")  # rejected
score.tracks[0].trim(0, 12, end_mode="unknown")  # rejected


def read_and_write_without_format(source: BinaryIO, target: BinaryIO) -> None:
    Score(source)  # rejected
    Score(source, ttype=Second)  # rejected
    Score(source, format=None)  # rejected
    score.dump(target)  # rejected
    score.dump(target, format=None)  # rejected

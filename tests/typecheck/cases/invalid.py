# pyright: strict
from symusic import Note, Score, Second

score = Score()
second = Note(0.0, 0.5, 60, ttype=Second)
score.tracks[0].notes.append(second)  # rejected
score.tracks[0].notes = [second]  # rejected
score.shift_time(0.5)  # rejected
score.to("second")  # rejected
Score(ttype="tick")  # rejected
Score.from_abc("X:1", tune="7")  # rejected
score.resample(480, min_dur=0.5)  # rejected
seconds.resample(480, min_dur=0.5)  # rejected
notes = score.tracks[0].notes
notes += [second]  # rejected

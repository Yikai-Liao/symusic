# pyright: strict
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
score.resample(480, min_dur=0.5)  # rejected
seconds.resample(480, min_dur=0.5)  # rejected
seconds.pianoroll()  # rejected
score.trim(0, 1, start_mode="unknown")  # rejected
score.trim(0, 1, end_mode="unknown")  # rejected
notes = score.tracks[0].notes
notes += [second]  # rejected

score.clip(0.5, 1.5)  # rejected
score.get_beats(0.5)  # rejected
wrong_unit: Score[Tick] = seconds  # rejected

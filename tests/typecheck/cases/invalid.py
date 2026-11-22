# pyright: strict
from symusic import Note, Score, Second

score = Score()
second = Note(0.0, 0.5, 60, ttype=Second)
score.tracks[0].notes.append(second)  # rejected
score.tracks[0].notes = [second]  # rejected
score.shift_time(0.5)  # rejected
score.to("second")  # rejected
Score(ttype="tick")  # rejected
score.resample(480, min_duration=0.5)  # rejected
notes = score.tracks[0].notes
notes += [second]  # rejected
score.pianoroll(["unknown"])  # rejected
Score("input.mid", format="unknown")  # rejected
Score.from_midi(b"", invalid_data_bytes="sanitize")  # rejected
score.tracks[0].trim(0, 12, end_mode="unknown")  # rejected

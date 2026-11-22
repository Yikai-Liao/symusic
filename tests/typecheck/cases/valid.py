# pyright: strict
from collections.abc import Iterator

from symusic import (
    ControlChange,
    Note,
    NoteList,
    Quarter,
    Score,
    Second,
    Tempo,
    Tick,
    Track,
    TrackList,
)
from typing_extensions import assert_type, reveal_type

ticks = Score()
seconds = Score(ttype=Second)
assert_type(reveal_type(ticks), Score[Tick])
assert_type(reveal_type(seconds), Score[Second])
assert_type(reveal_type(ticks.to(Second)), Score[Second])
assert_type(seconds.to(Tick, min_dur=1), Score[Tick])
assert_type(Score("input.mid", ttype=Quarter), Score[Quarter])
assert_type(Score.from_midi(b"", ttype=Second), Score[Second])
abc_score = Score.from_abc("X:1", ttype=Quarter, tune=1)
assert_type(abc_score, Score[Quarter])
assert_type(Score.from_abc_file("score.abc"), Score[Tick])
assert_type(Score.from_file("input.mid"), Score[Tick])
assert_type(ticks.tracks, TrackList[Tick])
assert_type(seconds.tracks[0], Track[Second])
assert_type(ticks.tracks[0].notes, NoteList[Tick])
assert_type(seconds.ttype, type[Second])
assert_type(ticks.tracks[0].notes[0].time, int)
assert_type(seconds.tracks[0].notes[0].duration, float)
assert_type(seconds.tracks[0].notes[0].end, float)
assert_type(seconds.tracks[0].controls[0].time, float)
assert_type(ticks.tracks[0].controls[0], ControlChange[Tick])
assert_type(ticks.tempos[0].time, int)
assert_type(ticks.clip(0, 1), Score[Tick])
assert_type(seconds.shift_time(0.5), Score[Second])
assert_type(seconds.tracks[0].trim(0.0, 1.0), Track[Second])
assert_type(ticks.copy(), Score[Tick])
assert_type(ticks.resample(480, min_dur=1), Score[Tick])
assert_type(ticks.to(Quarter).resample(480, min_dur=0.5), Score[Tick])
assert_type(seconds.resample(480, min_dur=0.5), Score[Tick])
assert_type(iter(seconds.tracks[0].notes), Iterator[Note[Second]])
assert_type(ticks.tracks[0].notes[:1], NoteList[Tick])
assert_type(NoteList(ttype=Quarter), NoteList[Quarter])
assert_type(Note(0, 1, 60), Note[Tick])
assert_type(Note(0.0, 1.0, 60, ttype=Quarter), Note[Quarter])
assert_type(Tempo(0, qpm=120, ttype=Second), Tempo[Second])
ticks.tracks[0].notes = [Note(0, 1, 60)]
seconds.tracks[0].notes.append(Note(0, 1, 60, ttype=Second))
seconds.tracks[0].notes.sort(key=lambda note: note.time)


def pattern(note: Note[Tick], score: Score[Second] | str) -> None:
    match note:
        case Note(onset):
            assert_type(onset, int)
    match score:
        case Score(ttype=marker):
            assert_type(marker, type[Second])
        case str():
            pass

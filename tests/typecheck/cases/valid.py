# pyright: strict
from collections.abc import Iterator, MutableSequence, Sequence
from os import PathLike
from typing import BinaryIO

from symusic import (
    ControlChange,
    Note,
    NoteList,
    Pedal,
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
assert_type(seconds.to(Tick, min_duration=1), Score[Tick])
assert_type(Score("input.mid", ttype=Quarter), Score[Quarter])
assert_type(Score.from_midi(b"", ttype=Second, strict=False), Score[Second])
assert_type(Score.from_abc("X:1", ttype=Quarter, strict=False), Score[Quarter])
abc_score = Score.from_abc("X:1", ttype=Quarter, tune=1)
assert_type(abc_score, Score[Quarter])
assert_type(Score("score.abc", format="abc"), Score[Tick])
assert_type(Score("input.mid"), Score[Tick])
assert_type(Score("input.mid", strict=False), Score[Tick])
ticks.dump("output.mid")
assert_type(ticks.tracks, TrackList[Tick])
assert_type(seconds.tracks.filter(lambda track: not track.is_drum), TrackList[Second])
assert_type(seconds.tracks[0], Track[Second])
assert_type(ticks.tracks[0].notes, NoteList[Tick])
assert_type(
    seconds.tracks[0].notes.filter(lambda note: note.time > 0), NoteList[Second]
)
assert_type(seconds.ttype, type[Second])
assert_type(ticks.tracks[0].notes[0].time, int)
assert_type(seconds.tracks[0].notes[0].duration, float)
assert_type(seconds.tracks[0].notes[0].end, float)
assert_type(seconds.tracks[0].controls[0].time, float)
assert_type(ticks.tracks[0].controls[0], ControlChange[Tick])
assert_type(ticks.tracks[0].pedals(), tuple[Pedal[Tick], ...])
assert_type(ticks.tempos[0].time, int)
assert_type(ticks.clip(0, 1), Score[Tick])
ticks.pianoroll()
assert_type(ticks.trim(0, 1, start_mode="truncate", end_mode="retain"), Score[Tick])
assert_type(seconds.shift_time(0.5), None)
assert_type(seconds.tracks[0].trim(0.0, 1.0), Track[Second])
assert_type(ticks.copy(), Score[Tick])
assert_type(ticks.resample(480, min_duration=1), Score[Tick])
assert_type(ticks.to(Quarter).resample(480, min_duration=1), Score[Tick])
assert_type(seconds.resample(480, min_duration=1), Score[Tick])
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


def inspect_tracks(tracks: Sequence[Track[Tick]]) -> None:
    assert_type(tracks[0], Track[Tick])


def edit_notes(notes: MutableSequence[Note[Tick]]) -> None:
    notes.reverse()


inspect_tracks(ticks.tracks)
notes = ticks.tracks[0].notes
edit_notes(notes)
assert_type(reversed(notes), Iterator[Note[Tick]])
notes += [Note(0, 1, 60)]
assert_type(notes, NoteList[Tick])


def abc_paths(text: PathLike[str], binary: PathLike[bytes]) -> None:
    assert_type(Score(text), Score[Tick])
    assert_type(Score(binary), Score[Tick])
    assert_type(Score(binary, format="abc"), Score[Tick])
    assert_type(abc_score.to_abc(), str)
    Score(text).dump(binary, format="abc")
    Score(binary, format="abc").dump(text, format="abc")


def read_and_write_midi(source: BinaryIO, target: BinaryIO) -> None:
    assert_type(Score(source, format="midi"), Score[Tick])
    score = Score(source, ttype=Quarter, format="midi", strict=False)
    assert_type(score, Score[Quarter])
    score.dump(path_or_file=target, format="midi")

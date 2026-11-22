"""Python references into containers stay valid and alias their element.

`notes[i]` returns a view of the stored event, not a copy. The view keeps the
event alive after the list is modified, sorted, cleared or deleted.
"""

import copy
import gc

import pytest
from symusic import Note, Quarter, Score, Second, Tick, Track

TTYPES = [Tick, Quarter, Second]


def make_track(ttype):
    notes = [Note(t, 1, 60 + t, 100, ttype=ttype) for t in (0, 1, 2, 3)]
    return Track("t", 0, False, notes=notes, ttype=ttype)


@pytest.mark.parametrize("ttype", TTYPES)
def test_ref_aliases_element(ttype):
    notes = make_track(ttype).notes
    note = notes[1]
    note.pitch = 90
    assert notes[1].pitch == 90
    notes[1].velocity = 7
    assert note.velocity == 7
    assert note == notes[1]


@pytest.mark.parametrize("ttype", TTYPES)
def test_ref_survives_append(ttype):
    notes = make_track(ttype).notes
    note = notes[1]
    for i in range(1000):
        notes.append(Note(i, 1, 20, 1, ttype=ttype))
    assert note.pitch == 61
    note.pitch = 5
    assert notes[1].pitch == 5


@pytest.mark.parametrize("ttype", TTYPES)
def test_ref_survives_insert(ttype):
    notes = make_track(ttype).notes
    note = notes[1]
    notes.insert(0, Note(9, 1, 20, 1, ttype=ttype))
    assert notes[2].pitch == 61
    note.pitch = 5
    assert notes[2].pitch == 5
    assert notes[1].pitch == 60


@pytest.mark.parametrize("ttype", TTYPES)
def test_ref_survives_removal(ttype):
    notes = make_track(ttype).notes
    note = notes[1]
    del notes[1]
    assert len(notes) == 3
    assert note.pitch == 61
    note.pitch = 5
    assert all(n.pitch != 5 for n in notes)

    popped = notes.pop(0)
    assert popped.pitch == 60
    notes.remove(notes[0])
    del notes[:]
    assert popped.pitch == 60
    assert len(notes) == 0


@pytest.mark.parametrize("ttype", TTYPES)
def test_ref_survives_clear(ttype):
    notes = make_track(ttype).notes
    refs = list(notes)
    notes.clear()
    assert len(notes) == 0
    assert [n.pitch for n in refs] == [60, 61, 62, 63]
    refs[0].pitch = 1
    assert refs[0].pitch == 1


@pytest.mark.parametrize("ttype", TTYPES)
def test_ref_survives_setitem(ttype):
    notes = make_track(ttype).notes
    note = notes[1]
    notes[1] = Note(5, 1, 99, 1, ttype=ttype)
    assert notes[1].pitch == 99
    assert note.pitch == 61
    note.pitch = 5
    assert notes[1].pitch == 99


@pytest.mark.parametrize("ttype", TTYPES)
def test_ref_follows_sort(ttype):
    notes = make_track(ttype).notes
    note = notes[0]
    notes.sort(key=lambda n: n.time, reverse=True, inplace=True)
    assert notes[3].pitch == 60
    note.pitch = 5
    assert notes[3].pitch == 5
    assert notes[0].pitch == 63


@pytest.mark.parametrize("ttype", TTYPES)
def test_ref_survives_filter(ttype):
    notes = make_track(ttype).notes
    kept, dropped = notes[3], notes[0]
    notes.filter(lambda n: n.pitch > 60, inplace=True)
    assert len(notes) == 3
    assert dropped.pitch == 60
    kept.pitch = 5
    assert notes[2].pitch == 5


@pytest.mark.parametrize("ttype", TTYPES)
def test_ref_survives_parent_deletion(ttype):
    track = make_track(ttype)
    notes = track.notes
    note = notes[1]
    del track
    gc.collect()
    assert note.pitch == 61
    assert notes[1].pitch == 61
    del notes
    gc.collect()
    assert note.pitch == 61
    note.pitch = 3
    assert note.pitch == 3


@pytest.mark.parametrize("ttype", TTYPES)
def test_ref_survives_score_deletion(ttype):
    score = Score(480, ttype=ttype)
    score.tracks.append(make_track(ttype))
    track = score.tracks[0]
    note = track.notes[2]
    del score
    gc.collect()
    assert track.name == "t"
    assert note.pitch == 62
    del track
    gc.collect()
    assert note.pitch == 62


@pytest.mark.parametrize("ttype", TTYPES)
def test_track_ref_aliases_in_score(ttype):
    score = Score(480, ttype=ttype)
    score.tracks.append(make_track(ttype))
    track = score.tracks[0]
    track.name = "renamed"
    track.notes.append(Note(9, 1, 70, 1, ttype=ttype))
    assert score.tracks[0].name == "renamed"
    assert len(score.tracks[0].notes) == 5
    removed = score.tracks.pop()
    assert removed.name == "renamed"
    assert len(score.tracks) == 0
    removed.name = "orphan"
    assert track.name == "orphan"


@pytest.mark.parametrize("ttype", TTYPES)
def test_shallow_copy_shares_events(ttype):
    track = make_track(ttype)
    shallow = track.copy(deep=False)
    shallow.notes[0].pitch = 5
    assert track.notes[0].pitch == 5
    deep = copy.deepcopy(track)
    deep.notes[0].pitch = 6
    assert track.notes[0].pitch == 5
    assert deep.notes[0].pitch == 6


@pytest.mark.parametrize("ttype", TTYPES)
def test_ref_survives_list_replacement(ttype):
    track = make_track(ttype)
    note = track.notes[1]
    track.notes = [Note(0, 1, 10, 1, ttype=ttype)]
    assert [n.pitch for n in track.notes] == [10]
    assert note.pitch == 61


@pytest.mark.parametrize("ttype", [Quarter, Second])
def test_score_equality_shares_tracks(ttype):
    score = Score(ttype=ttype)
    track = Track(ttype=ttype, notes=[Note(0, 1, 60, 80, ttype=ttype)])
    track.notes[0].time = float("nan")
    score.tracks.append(track)

    other = Score(ttype=ttype)
    other.tracks.append(track)
    assert score == score.copy(deep=False)
    assert score == other
    assert score != score.copy(deep=True)

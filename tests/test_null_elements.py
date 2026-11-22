"""Containers reject `None` and foreign objects as elements with ``TypeError``."""

import pytest
from symusic import Note, NoteList, Tick, Track, TrackList

BAD_ITEMS = [None, 1, "note"]


@pytest.fixture
def notes():
    return NoteList([Note(0, 1, 60, 100, ttype=Tick), Note(1, 1, 62, 100, ttype=Tick)])


@pytest.fixture
def tracks():
    return TrackList([Track("a", 0, False, ttype=Tick)])


@pytest.mark.parametrize("bad", BAD_ITEMS)
def test_note_list_ctor_rejects(bad):
    with pytest.raises(TypeError):
        NoteList([Note(0, 1, 60, 100, ttype=Tick), bad])


@pytest.mark.parametrize("bad", BAD_ITEMS)
def test_track_list_ctor_rejects(bad):
    with pytest.raises(TypeError):
        TrackList([Track("a", 0, False, ttype=Tick), bad])


@pytest.mark.parametrize("bad", BAD_ITEMS)
def test_note_list_mutators_reject(notes, bad):
    before = len(notes)
    with pytest.raises(TypeError):
        notes.append(bad)
    with pytest.raises(TypeError):
        notes.insert(0, bad)
    with pytest.raises(TypeError):
        notes[0] = bad
    with pytest.raises(TypeError):
        notes.extend([bad])
    with pytest.raises(TypeError):
        notes[0:1] = [bad]
    assert len(notes) == before
    assert notes[0].pitch == 60


@pytest.mark.parametrize("bad", BAD_ITEMS)
def test_track_list_mutators_reject(tracks, bad):
    with pytest.raises(TypeError):
        tracks.append(bad)
    with pytest.raises(TypeError):
        tracks.insert(0, bad)
    with pytest.raises(TypeError):
        tracks[0] = bad
    with pytest.raises(TypeError):
        tracks.extend([bad])
    assert len(tracks) == 1
    assert tracks[0].name == "a"


@pytest.mark.parametrize("bad", BAD_ITEMS)
def test_lookup_rejects_or_misses(notes, tracks, bad):
    # Membership tests never raise and never match.
    assert bad not in notes
    assert bad not in tracks
    for container in (notes, tracks):
        with pytest.raises(TypeError):
            container.count(bad)
        with pytest.raises(TypeError):
            container.remove(bad)


@pytest.mark.parametrize("bad", BAD_ITEMS)
def test_list_equality_unequal(notes, tracks, bad):
    assert notes != [notes[0], bad]
    assert (notes == [notes[0], bad]) is False
    assert notes != [bad, bad]
    assert tracks != [bad]
    assert (tracks == [bad]) is False


def test_track_assignment_rejects():
    track = Track("a", 0, False, ttype=Tick)
    with pytest.raises(TypeError):
        track.notes = [None]
    assert len(track.notes) == 0

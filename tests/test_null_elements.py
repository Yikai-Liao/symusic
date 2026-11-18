"""Containers reject `None` and foreign objects as elements with ``TypeError``."""

import pytest
from symusic import Note, Track
from symusic.core import NoteTickList, TrackTickList

BAD_ITEMS = [None, 1, "note"]


@pytest.fixture
def notes():
    return NoteTickList([Note(0, 1, 60, 100, "tick"), Note(1, 1, 62, 100, "tick")])


@pytest.fixture
def tracks():
    return TrackTickList([Track("a", 0, False, ttype="tick")])


@pytest.mark.parametrize("bad", BAD_ITEMS)
def test_note_list_ctor_rejects(bad):
    with pytest.raises(TypeError):
        NoteTickList([Note(0, 1, 60, 100, "tick"), bad])


@pytest.mark.parametrize("bad", BAD_ITEMS)
def test_track_list_ctor_rejects(bad):
    with pytest.raises(TypeError):
        TrackTickList([Track("a", 0, False, ttype="tick"), bad])


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
    track = Track("a", 0, False, ttype="tick")
    with pytest.raises(TypeError):
        track.notes = [None]
    with pytest.raises(TypeError):
        track.pedals = [None]
    assert len(track.notes) == 0

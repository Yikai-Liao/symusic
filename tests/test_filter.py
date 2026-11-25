from symusic import Note, Quarter, Score, Track


def test_filter_notes():
    original = [Note(0, 4, 72, 72), Note(4, 6, 72, 72), Note(8, 4, 72, 72)]
    track = Track(notes=original)

    selected = track.notes.filter(lambda n: n.time < 6)
    assert type(selected) is type(track.notes)
    assert selected.ttype is track.notes.ttype
    assert list(selected) == original[:2]
    assert list(track.notes) == original

    selected[0].pitch = 73
    assert track.notes[0].pitch == 73
    assert track.notes[:1][0].pitch == 73


def test_filter_tracks():
    score = Score(ttype=Quarter)
    original = [Track(name, ttype=Quarter) for name in ["A", "B", "C"]]
    score.tracks.extend(original)

    selected = score.tracks.filter(lambda t: t.name in {"A", "C"})
    assert type(selected) is type(score.tracks)
    assert selected.ttype is Quarter
    assert list(selected) == [original[0], original[2]]
    assert list(score.tracks) == original

    selected[1].name = "Kept"
    assert score.tracks[2].name == "Kept"
    assert score.tracks[1:][1].name == "Kept"


def test_assign_filtered_lists():
    track = Track("Keep", notes=[Note(0, 2, 72, 72), Note(2, 2, 73, 72)])
    score = Score()
    score.tracks.extend([Track("Drop"), track])

    track.notes = track.notes.filter(lambda n: n.pitch == 73)
    score.tracks = score.tracks.filter(lambda t: t.name == "Keep")

    assert list(score.tracks) == [track]
    assert list(score.tracks[0].notes) == [Note(2, 2, 73, 72)]

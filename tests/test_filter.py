from symusic import Note, Score, Track


def test_filter_notes():
    track = Track(notes=[Note(0, 4, 72, 72), Note(4, 6, 72, 72), Note(8, 4, 72, 72)])

    notes1 = track.notes.filter(lambda n: n.start < 6, inplace=False)
    assert list(notes1) == [Note(0, 4, 72, 72), Note(4, 6, 72, 72)]
    assert list(track.notes) == [
        Note(0, 4, 72, 72),
        Note(4, 6, 72, 72),
        Note(8, 4, 72, 72),
    ]

    notes2 = track.notes.filter(lambda n: n.start > 6, inplace=True)
    assert list(notes2) == [Note(8, 4, 72, 72)]
    assert list(track.notes) == [Note(8, 4, 72, 72)]


def test_filter_tracks():
    t_a = Track(name="A")
    t_b = Track(name="B")
    t_c = Track(name="C")
    score = Score()
    score.tracks.extend([t_a, t_b, t_c])

    tracks1 = score.tracks.filter(lambda t: t.name in ["A", "C"], inplace=False)
    assert list(tracks1) == [t_a, t_c]
    assert list(score.tracks) == [t_a, t_b, t_c]

    tracks2 = score.tracks.filter(lambda t: t.name in ["A", "C"], inplace=True)
    assert list(tracks2) == [t_a, t_c]
    assert list(score.tracks) == [t_a, t_c]

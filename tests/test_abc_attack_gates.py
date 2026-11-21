from symusic import Score

HEADER = "X:1\nT:Attack gates\nM:4/4\nL:1/4\nK:C\n%%MIDI beat 105 95 80 2\n"


def test_trim_and_expand_follow_the_attack_channel():
    music = "%%MIDI trim 1/8\nC [I:MIDI expand 1/8] D z2 |]"
    for channel, durations in [(1, [419, 539]), (10, [479, 479])]:
        track = Score.from_abc(HEADER + f"%%MIDI channel {channel}\n" + music).tracks[0]
        assert track.is_drum == (channel == 10)
        assert [n.duration for n in track.notes] == durations


def test_expand_preserves_the_tied_release():
    score = Score.from_abc(HEADER + "%%MIDI expand 1/24\nC-C D z |]")
    assert [(n.time, n.duration) for n in score.tracks[0].notes] == [
        (1, 959),
        (961, 499),
    ]


def test_tied_trill_uses_its_opening_channel_for_each_gate():
    music = "%%MIDI expand 1/24\nTC- [I:MIDI channel 1] C D z |]"
    score = Score.from_abc(HEADER + "%%MIDI channel 10\n" + music)
    melody, drums = score.tracks
    assert drums.is_drum
    assert [(n.time, n.duration) for n in drums.notes] == [
        (1 + 60 * pulse, 59) for pulse in range(16)
    ]
    assert [(n.time, n.duration) for n in melody.notes] == [(961, 499)]

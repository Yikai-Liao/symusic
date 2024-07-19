"""Test methods."""

from __future__ import annotations

from copy import deepcopy
from typing import TYPE_CHECKING

import pytest
from symusic import (
    ControlChange,
    KeySignature,
    Note,
    Pedal,
    PitchBend,
    Score,
    Tempo,
    TimeSignature,
)

from tests.utils import (
    MIDI_PATHS_MULTITRACK,
    MIDI_PATHS_ONE_TRACK,
    concat_scores,
    merge_tracks,
)

if TYPE_CHECKING:
    from pathlib import Path


def test_containers_assertions():
    note1 = [Note(0, 30, 50, 120), Note(0, 30, 49, 120)]
    note2 = [Note(0, 30, 51, 120), Note(0, 30, 49, 120)]
    note3 = [Note(0, 30, 51, 120), Note(0, 30, 49, 120)]
    assert note1 != note2
    assert note2 == note3

    tc1 = [Tempo(2, 120), Tempo(0, 110)]
    tc2 = [Tempo(3, 120), Tempo(0, 110)]
    tc3 = [Tempo(3, 120), Tempo(0, 110)]
    assert tc1 != tc2
    assert tc2 == tc3

    ts1 = [TimeSignature(0, 4, 4), TimeSignature(10, 6, 4)]
    ts2 = [TimeSignature(0, 2, 4), TimeSignature(10, 6, 4)]
    ts3 = [TimeSignature(0, 2, 4), TimeSignature(10, 6, 4)]
    assert ts1 != ts2
    assert ts2 == ts3

    sp1 = [Pedal(0, 2), Pedal(10, 20)]
    sp2 = [Pedal(1, 2), Pedal(15, 20)]
    sp3 = [Pedal(1, 2), Pedal(15, 20)]
    assert sp1 != sp2
    assert sp2 == sp3

    pb1 = [PitchBend(2, 120), PitchBend(0, 110)]
    pb2 = [PitchBend(3, 120), PitchBend(0, 110)]
    pb3 = [PitchBend(3, 120), PitchBend(0, 110)]
    assert pb1 != pb2
    assert pb2 == pb3

    ks1 = [KeySignature(2, 0, 1), KeySignature(0, 0, 1)]
    ks2 = [KeySignature(20, 0, 1), KeySignature(0, 0, 1)]
    ks3 = [KeySignature(20, 0, 1), KeySignature(0, 0, 1)]
    assert ks1 != ks2
    assert ks2 == ks3

    cc1 = [ControlChange(2, 120, 50), ControlChange(0, 110, 50)]
    cc2 = [ControlChange(2, 120, 50), ControlChange(10, 110, 50)]
    cc3 = [ControlChange(2, 120, 50), ControlChange(10, 110, 50)]
    assert cc1 != cc2
    assert cc2 == cc3


@pytest.mark.parametrize("file_path", MIDI_PATHS_ONE_TRACK)
def test_check_scores_equals(file_path: Path):
    score = Score(file_path)
    score_copy = deepcopy(score)

    # Check when midi is untouched
    assert score == score_copy

    # Altering notes
    i = 0
    while i < len(score_copy.tracks):
        if len(score_copy.tracks[i].notes) > 0:
            score_copy.tracks[i].notes[-1].pitch += 5
            assert score != score_copy
            break
        i += 1

    # Altering track events
    if len(score_copy.tracks) > 0:
        # Altering pedals
        score_copy = deepcopy(score)
        if len(score_copy.tracks[0].pedals) == 0:
            score_copy.tracks[0].pedals.append(Pedal(0, 10))
        else:
            score_copy.tracks[0].pedals[-1].duration += 10
        assert score != score_copy

        # Altering pitch bends
        score_copy = deepcopy(score)
        if len(score_copy.tracks[0].pitch_bends) == 0:
            score_copy.tracks[0].pitch_bends.append(PitchBend(50, 10))
        else:
            score_copy.tracks[0].pitch_bends[-1].value += 10
        assert score != score_copy

    # Altering tempos
    score_copy = deepcopy(score)
    if len(score_copy.tempos) == 0:
        score_copy.tempos.append(Tempo(50, 10))
    else:
        score_copy.tempos[-1].time += 10
    assert score != score_copy

    # Altering time signatures
    score_copy = deepcopy(score)
    if len(score_copy.time_signatures) == 0:
        score_copy.time_signatures.append(TimeSignature(10, 4, 4))
    else:
        score_copy.time_signatures[-1].time += 10
    assert score != score_copy


def test_merge_tracks(
    file_path: str | Path = MIDI_PATHS_ONE_TRACK[0],
):
    # Load music file and only keep the first track
    score = Score(file_path)
    score.tracks = [score.tracks[0]]

    # Duplicate the track and merge it
    original_track = deepcopy(score.tracks[0])
    score.tracks.append(deepcopy(score.tracks[0]))

    # Test merge with effects
    merge_tracks(score, effects=True)
    assert len(score.tracks[0].notes) == 2 * len(original_track.notes)
    assert len(score.tracks[0].pedals) == 2 * len(original_track.pedals)
    assert len(score.tracks[0].controls) == 2 * len(original_track.controls)
    assert len(score.tracks[0].pitch_bends) == 2 * len(original_track.pitch_bends)


@pytest.mark.parametrize("file_path", MIDI_PATHS_MULTITRACK)
def test_split_concat_score(file_path: Path):
    score = Score(file_path)
    if score.end() == 0:
        pytest.skip("empty file")
    tick_split = score.tpq * 4
    score_1 = score.clip(0, tick_split)
    score_2 = score.clip(tick_split, score.end() + 1).shift_time(-tick_split)

    # Concatenate split MIDIs and assert its equal to original one
    score_concat = concat_scores((score_1, score_2), [tick_split])

    # Assert the concatenated Score is identical to the original one
    # We do not test tempos, time signatures and key signature as they are duplicated
    # in score_concat (same consecutive ones for each chunk).
    assert score.tracks == score_concat.tracks
    assert score.markers == score_concat.markers

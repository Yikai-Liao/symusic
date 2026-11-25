from __future__ import annotations

from operator import attrgetter
from typing import TYPE_CHECKING

import pytest
from symusic import Note, Score, Track

from tests.utils import MIDI_PATHS_ALL

if TYPE_CHECKING:
    from pathlib import Path

# as tuples (original notes, original times, new times, expected notes)
NOTES_OG = [Note(0, 4, 72, 72), Note(4, 6, 72, 72), Note(8, 4, 72, 72)]
TEST_CASES = (
    # Both note endpoints on an anchor must map to its exact replacement.
    (
        [Note(0, 7, 72, 72), Note(7, 2, 72, 72)],
        [0, 7, 9],
        [0, 61, 63],
        [Note(0, 61, 72, 72), Note(61, 2, 72, 72)],
    ),
    # Only shkrinks the [0,4] to [0,2]
    (
        NOTES_OG,
        [0, 4, 12],
        [0, 2, 10],
        [
            Note(0, 2, 72, 72),
            Note(2, 6, 72, 72),
            Note(6, 4, 72, 72),
        ],
    ),
    # Same as previous without the first point
    (
        NOTES_OG,
        [4, 12],
        [2, 10],
        [
            Note(2, 6, 72, 72),
            Note(6, 4, 72, 72),
        ],
    ),
    # Stretching the first section crossing the first note which will be discarded
    (
        NOTES_OG,
        [2, 12],
        [4, 14],
        [
            Note(6, 6, 72, 72),
            Note(10, 4, 72, 72),
        ],
    ),
)

# For tests on MIDIs, sections are in quarter
TIMES = (([2, 4], [1, 3]),)


@pytest.mark.parametrize("test_case", TEST_CASES)
def test_adjust_time(test_case: tuple[list[Note], list[int], list[int], list[Note]]):
    """Test the `adjust_time` method with hardcoded test cases."""
    notes_og, times_og, times_new, notes_expected = test_case
    midi = Score()
    midi.tracks.append(Track(notes=notes_og))
    midi_adjusted = midi.adjust_time(times_og, times_new)
    assert midi_adjusted.tracks[0].notes == notes_expected


@pytest.mark.parametrize("midi_path", MIDI_PATHS_ALL, ids=attrgetter("name"))
@pytest.mark.parametrize("times", TIMES)
def test_adjust_time_midi(midi_path: Path, times: tuple[list[int], list[int]]):
    """Test the `adjust_time` method just by running it on test MIDIs."""
    midi = Score(midi_path)
    times_og = [time * midi.ticks_per_quarter for time in times[0]]
    times_new = [time * midi.ticks_per_quarter for time in times[1]]
    _ = midi.adjust_time(times_og, times_new)


@pytest.mark.parametrize("inplace", [False, True])
@pytest.mark.parametrize(
    ("times_og", "times_new"),
    [([0, 10], [0]), ([0], [0]), ([10, 0], [0, 5]), ([0, 10], [5, 0])],
    ids=["unequal-size", "one-anchor", "unsorted-original", "unsorted-new"],
)
def test_score_adjust_time_rejects_invalid_anchors(
    times_og: list[int], times_new: list[int], inplace: bool
):
    """Copy and in-place adjust_time validate the anchors the same way."""
    score = Score()
    score.tracks.append(Track(notes=NOTES_OG))
    with pytest.raises(ValueError, match="adjust_time"):
        score.adjust_time(times_og, times_new, inplace=inplace)


@pytest.mark.parametrize("ttype", ["tick", "quarter", "second"])
@pytest.mark.parametrize("inplace", [False, True])
def test_repeated_original_anchor(ttype, inplace):
    score = Score(480, ttype=ttype)
    score.tracks.append(Track(notes=[Note(2, 1, 60, 80, ttype)], ttype=ttype))
    with pytest.raises(
        ValueError, match=r"original_times must be strictly increasing, got 1, 1\b"
    ):
        score.adjust_time([0, 1, 1, 3], [0, 1, 2, 4], inplace=inplace)
    assert score.tracks[0].notes[0].start == 2


def test_repeated_new_anchor():
    score = Score(480)
    score.tracks.append(Track(notes=[Note(1, 1, 60, 80)]))
    adjusted = score.adjust_time([0, 1, 2, 3], [0, 1, 1, 3])
    assert [(note.start, note.duration) for note in adjusted.tracks[0].notes] == [
        (1, 0)
    ]


def test_adjust_time_checks_both_note_endpoints():
    score = Score(480)
    score.tracks.append(Track(notes=[Note(2, -10, 60, 80)]))
    adjusted = score.adjust_time([0, 4], [0, 8])
    assert not adjusted.tracks[0].notes


def test_adjust_time_wide_tick_span_in_range():
    int32_min = -(2**31)
    int32_max = 2**31 - 1
    score = Score(480)
    score.tracks.append(Track(notes=[Note(6, 0, 60, 80)]))
    adjusted = score.adjust_time([0, 10], [int32_min, int32_max])
    expected = int32_min + int(((int32_max - int32_min) / 10.0) * 6.0)
    assert adjusted.tracks[0].notes[0].time == expected


@pytest.mark.parametrize("ttype", ["quarter", "second"])
@pytest.mark.parametrize(
    ("times_og", "times_new", "parameter", "value"),
    [
        ([0.0, 1.0], [0.0, float("nan")], "new_times", "nan"),
        ([0.0, 1.0], [float("nan"), 1.0], "new_times", "nan"),
        ([float("-inf"), float("inf")], [0.0, 1.0], "original_times", "-inf"),
        ([0.0, 1.0], [0.0, float("inf")], "new_times", "inf"),
    ],
)
def test_adjust_time_rejects_non_finite_anchors(
    ttype, times_og, times_new, parameter, value
):
    score = Score(480, ttype=ttype)
    with pytest.raises(ValueError, match=rf"{parameter} must be finite, got {value}\b"):
        score.adjust_time(times_og, times_new)


@pytest.mark.parametrize("ttype", ["quarter", "second"])
@pytest.mark.parametrize("inplace", [False, True])
def test_adjust_time_empty_tracks(ttype, inplace):
    tracks = Score(480, ttype=ttype).tracks
    with pytest.raises(ValueError, match=r"new_times must be finite, got inf\b"):
        tracks.adjust_time([0, 1], [0, float("inf")], inplace=inplace)
    assert not tracks.adjust_time([0, 1], [0, 2], inplace=inplace)
    assert not tracks

from __future__ import annotations

from operator import attrgetter
from pathlib import Path

import pytest

from symusic import Note, Score, Track
from tests.utils import MIDI_PATHS

# as tuples (original notes, original times, new times, expected notes)
NOTES_OG = [Note(0, 4, 72, 72), Note(4, 6, 72, 72), Note(8, 4, 72, 72)]
TEST_CASES = (
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


@pytest.mark.parametrize("midi_path", MIDI_PATHS, ids=attrgetter("name"))
@pytest.mark.parametrize("times", TIMES)
def test_adjust_time_midi(midi_path: Path, times: tuple[list[int], list[int]]):
    """Test the `adjust_time` method just by running it on test MIDIs."""
    midi = Score(midi_path)
    times_og = [time * midi.ticks_per_quarter for time in times[0]]
    times_new = [time * midi.ticks_per_quarter for time in times[1]]
    _ = midi.adjust_time(times_og, times_new)

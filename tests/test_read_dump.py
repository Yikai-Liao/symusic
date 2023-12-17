from operator import attrgetter

import pytest

from symusic import Score
from tests.utils import MIDI_PATHS


@pytest.mark.parametrize("midi_path", MIDI_PATHS, ids=attrgetter("name"))
def test_load_dump(midi_path, tmp_path):
    """Test that a MIDI loaded and saved unchanged is indeed the save as before."""
    midi1 = Score(midi_path)
    dump_path = tmp_path / midi_path.name
    midi1.dump_midi(dump_path)  # Writing it unchanged
    midi2 = Score(dump_path)  # Loading it back

    # Sorting the notes, as after dump the order might have changed
    for track1, track2 in zip(midi1.tracks, midi2.tracks):
        track1.notes.sort(key=lambda x: (x.start, x.pitch, x.end, x.velocity))
        track2.notes.sort(key=lambda x: (x.start, x.pitch, x.end, x.velocity))

    assert midi1 == midi2

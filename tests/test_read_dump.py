from operator import attrgetter
from pathlib import Path

import pytest

from symusic import Score
from tests.utils import MIDI_PATHS


@pytest.mark.parametrize("midi_path", MIDI_PATHS, ids=attrgetter("name"))
def test_load_dump(midi_path: Path, tmp_path: Path):
    """Test that a MIDI loaded and saved unchanged is indeed the save as before."""
    midi1 = Score(midi_path)
    dump_path = tmp_path / midi_path.name
    midi1.dump_midi(dump_path)  # Writing it unchanged
    midi2 = Score(dump_path)  # Loading it back

    # Sorting MIDI and tracks events, as after dump the order might have changed
    midi1.tempos.sort(key=lambda x: (x.time, x.tempo))
    midi2.tempos.sort(key=lambda x: (x.time, x.tempo))
    midi1.markers.sort(key=lambda x: (x.time, x.text))
    midi2.markers.sort(key=lambda x: (x.time, x.text))
    midi1.lyrics.sort(key=lambda x: (x.time, x.text))
    midi2.lyrics.sort(key=lambda x: (x.time, x.text))
    for track1, track2 in zip(midi1.tracks, midi2.tracks):
        track1.notes.sort(key=lambda x: (x.start, x.pitch, x.end, x.velocity))
        track2.notes.sort(key=lambda x: (x.start, x.pitch, x.end, x.velocity))
        track1.controls.sort(key=lambda x: (x.time, x.number, x.value))
        track2.controls.sort(key=lambda x: (x.time, x.number, x.value))
        track1.pitch_bends.sort(key=lambda x: (x.time, x.value))
        track2.pitch_bends.sort(key=lambda x: (x.time, x.value))

    # Check the objects are equal
    midi_equals = midi1 == midi2

    # If not, look for what's different and prints it
    if not midi_equals:
        track_attrs = ["notes", "controls", "pitch_bends", "pedals"]
        for track1, track2 in zip(midi1.tracks, midi2.tracks):
            for track_attr in track_attrs:
                if getattr(track1, track_attr) != getattr(track2, track_attr):
                    # For debug
                    """for i, (obj1, obj2) in enumerate(zip(
                            getattr(track1, track_attr), getattr(track2, track_attr))
                    ):
                        if obj1 != obj2:
                            list1 = getattr(track1, track_attr)[i-2:i+2]
                            list2 = getattr(track2, track_attr)[i-2:i+2]
                            break"""
                    print(f"{track_attr} are not equal for {track1.name}")
        midi_attrs = [
            "tempos",
            "time_signatures",
            "key_signatures",
            "markers",
            "lyrics",
            "ticks_per_quarter",
        ]
        for midi_attr in midi_attrs:
            if getattr(midi1, midi_attr) != getattr(midi2, midi_attr):
                print(f"{midi_attr} are not equals")

    assert midi_equals

"""
Test Python bindings for the C++ library.

This test module verifies that Python bindings correctly expose C++ functionality.
"""

from __future__ import annotations

import math
import struct
import tempfile
from pathlib import Path

import numpy as np
import pytest
from symusic import (
    ControlChange,
    KeySignature,
    Note,
    Pedal,
    PitchBend,
    Quarter,
    Score,
    Second,
    Tempo,
    TextMeta,
    Tick,
    TimeSignature,
    Track,
    TrackList,
)


def test_note_creation():
    """Test creating Note objects and accessing their properties."""
    # Create a note
    note = Note(100, 200, 60, 80)

    # Check basic properties
    assert note.time == 100
    assert note.duration == 200
    assert note.pitch == 60
    assert note.velocity == 80

    # Check derived properties
    assert note.time == 100
    assert note.time + note.duration == 300

    # Test empty check
    assert not note.empty()
    empty_note = Note(100, 0, 60, 80)
    assert empty_note.empty()

    assert note.shift_time(50) is None
    assert note.shift_pitch(12) is None
    assert note.shift_velocity(10) is None
    assert (note.time, note.pitch, note.velocity) == (150, 72, 90)


def test_track_creation():
    """Test creating Track objects and manipulating their contents."""
    # Create a track
    track = Track("Piano", 0, False)

    # Check basic properties
    assert track.name == "Piano"
    assert track.program == 0
    assert track.is_drum is False
    assert len(track.notes) == 0

    # Add notes
    track.notes.append(Note(100, 200, 60, 80))
    track.notes.append(Note(300, 200, 64, 80))
    assert len(track.notes) == 2

    # Test track operations
    assert track.start() == 100
    assert track.end() == 500
    assert not hasattr(track, "note_count")
    assert len(track.notes) == 2
    assert not track.empty()

    assert track.shift_time(50) is None
    assert track.shift_pitch(12) is None
    assert len(track.clip(200, 400).notes) == 1
    assert len(track.notes) == 2
    assert (track.notes[0].time, track.notes[0].pitch) == (150, 72)


def test_score_creation():
    """Test creating Score objects and manipulating their contents."""
    # Create a score
    score = Score(ticks_per_quarter=480)  # 480 ticks per quarter

    # Check basic properties
    assert score.ticks_per_quarter == 480
    assert len(score.tracks) == 0

    # Add a track
    track = Track("Piano", 0, False)
    track.notes.append(Note(480, 240, 60, 80))
    track.notes.append(Note(720, 240, 64, 80))
    score.tracks.append(track)

    # Add a tempo and time signature
    score.tempos.append(Tempo(0, 120))  # 120 BPM
    score.time_signatures.append(TimeSignature(0, 4, 4))

    # Test score operations - score.start() returns 0, not the earliest note time
    assert score.start() == 0
    assert score.end() == 960
    assert score.note_count() == 2
    assert len(score.tracks) == 1

    assert score.shift_time(100) is None
    assert score.clip(600, 900).note_count() == 1
    assert score.tracks[0].notes[0].time == 580


def test_event_types():
    """Test different event types work correctly."""
    # Test Tempo
    tempo = Tempo(0, 120)  # 120 BPM
    assert abs(tempo.qpm - 120.0) < 0.001
    tempo2 = Tempo(100, 120.0)
    assert tempo2.time == 100
    assert tempo2.qpm == 120.0

    # Test TimeSignature
    ts = TimeSignature(200, 3, 4)
    assert ts.time == 200
    assert ts.numerator == 3
    assert ts.denominator == 4

    # Test KeySignature
    ks = KeySignature(300, 2, False)  # D major
    assert ks.time == 300
    assert ks.key == 2
    assert ks.minor == 0

    # Test ControlChange
    cc = ControlChange(50, 7, 100)  # Volume
    assert cc.time == 50
    assert cc.number == 7
    assert cc.value == 100

    # Test PitchBend
    pb = PitchBend(400, 0)  # Center position
    assert pb.time == 400
    assert pb.value == 0

    # Test Pedal
    pedal = Pedal(100, 200)
    assert pedal.time == 100
    assert pedal.duration == 200

    # Test TextMeta
    text = TextMeta(500, "Test Text")
    assert text.time == 500
    assert text.text == "Test Text"


def test_file_io():
    """Test file I/O operations (reading and writing MIDI files)."""
    # Create a simple score
    score = Score(ticks_per_quarter=480)

    # Add a track with notes
    track = Track("Piano", 0, False)
    track.notes.append(Note(480, 240, 60, 80))
    track.notes.append(Note(720, 240, 64, 80))
    track.notes.append(Note(960, 240, 67, 80))
    score.tracks.append(track)

    # Add tempo and time signature
    score.tempos.append(Tempo(0, 120))  # 120 BPM
    score.time_signatures.append(TimeSignature(0, 4, 4))

    # Write to a temporary MIDI file
    with tempfile.TemporaryDirectory() as temp_dir:
        midi_path = Path(temp_dir) / "test.mid"
        score.dump(midi_path)

        # Read the file back
        read_score = Score(midi_path)

        # Verify the read score
        assert read_score.ticks_per_quarter == 480
        assert len(read_score.tracks) == 1
        assert len(read_score.tracks[0].notes) == 3
        assert len(read_score.tempos) == 1
        assert abs(read_score.tempos[0].qpm - 120.0) < 0.001
        assert len(read_score.time_signatures) == 1
        assert read_score.time_signatures[0].numerator == 4
        assert read_score.time_signatures[0].denominator == 4


def test_tick_operations():
    """Test operations specific to tick-based notes."""
    # Create tick-based notes
    tick_note = Note(480, 240, 60, 80)

    tick_note.shift_time(100)
    tick_note.shift_pitch(12)
    assert (tick_note.time, tick_note.pitch, tick_note.velocity) == (580, 72, 80)


def test_equality_comparison():
    """Test equality comparison for all event types."""
    # Test Note equality
    note1 = Note(100, 200, 60, 80)
    note2 = Note(100, 200, 60, 80)
    note3 = Note(100, 200, 61, 80)
    assert note1 == note2
    assert note1 != note3

    # Test Track equality
    track1 = Track("Piano", 0, False)
    track1.notes.append(Note(100, 200, 60, 80))

    track2 = Track("Piano", 0, False)
    track2.notes.append(Note(100, 200, 60, 80))

    track3 = Track("Piano", 0, False)
    track3.notes.append(Note(100, 200, 61, 80))

    assert track1 == track2
    assert track1 != track3

    # Test Score equality
    score1 = Score(ticks_per_quarter=480)
    score1.tracks.append(track1)

    score2 = Score(ticks_per_quarter=480)
    score2.tracks.append(track2)

    score3 = Score(ticks_per_quarter=480)
    score3.tracks.append(track3)

    assert score1 == score2
    assert score1 != score3


@pytest.mark.parametrize(
    "midi_file",
    [
        # Add paths to test MIDI files
        pytest.param(Path("tests/testcases/test1.mid"), id="basic_midi"),
        pytest.param(Path("tests/testcases/test2.mid"), id="complex_midi"),
        # Add more test files if available
    ],
)
def test_midi_roundtrip(midi_file):
    """Test round-trip conversion from MIDI file to Score and back."""
    if not midi_file.exists():
        pytest.skip(f"Test file {midi_file} not found")

    # Read MIDI file
    original_score = Score(str(midi_file))

    # Write to a temporary file
    with tempfile.TemporaryDirectory() as temp_dir:
        temp_midi = Path(temp_dir) / "round_trip.mid"
        original_score.dump(str(temp_midi))

        # Read back
        round_trip_score = Score(str(temp_midi))

        # Verify the scores are identical
        assert original_score == round_trip_score


def test_numpy_integration():
    """Test integration with numpy arrays."""
    # Create a track with notes
    track = Track("Piano", 0, False)
    for i in range(8):
        track.notes.append(Note(i * 480, 240, 60 + i, 80))  # C major scale

    # Convert to pianoroll
    pianoroll = track.pianoroll(modes=["frame"], encode_velocity=True)

    # Check it's a numpy array
    assert isinstance(pianoroll, np.ndarray)

    # Verify basic properties
    assert pianoroll.shape[0] == 1  # One mode
    assert pianoroll.shape[1] == 128  # 128 pitches
    assert pianoroll.shape[2] >= 3600  # Time dimension (at least enough for our notes)

    # Check a few values
    for i in range(8):
        pitch = 60 + i
        time = i * 480 + 120  # Middle of note
        assert pianoroll[0, pitch, time] == 80  # Velocity value


@pytest.mark.parametrize("ttype", [Tick, Quarter, Second])
def test_list_equals_python_list(ttype):
    note = Note(0, 1, 60, 100, ttype=ttype)
    track = Track("a", 0, False, notes=[note], ttype=ttype)
    other = Track("b", 0, False, ttype=ttype)

    assert track.notes == [note]
    assert track.notes != [Note(0, 1, 61, 100, ttype=ttype)]
    assert type(track.notes)([track.notes[0]], ttype=ttype) == [note]

    tracks = Score(ticks_per_quarter=480, ttype=ttype).tracks
    tracks.append(track)
    assert tracks == [track]
    assert tracks != [other]
    assert tracks != []


def _score_for_conversion() -> Score:
    score = Score(ticks_per_quarter=480)
    score.tempos.append(Tempo(0, 120.0))
    track = Track(name="t", program=0, is_drum=False)
    track.notes.append(Note(0, 10, 60, 100))
    track.notes.append(Note(480, 960, 64, 100))
    score.tracks.append(track)
    return score


def test_score_conversion_is_independent():
    original = _score_for_conversion()
    duplicate = original.to(Tick)
    assert duplicate == original
    assert duplicate is not original
    duplicate.tracks[0].notes[0].pitch = 1
    assert original.tracks[0].notes[0].pitch == 60


@pytest.mark.parametrize(
    ("ttype", "min_duration"), [(Tick, 100), (Quarter, 2.0), (Second, 0.5)]
)
def test_score_conversion_enforces_minimum_duration(ttype, min_duration):
    original = _score_for_conversion()
    converted = original.to(ttype, min_duration=min_duration)
    assert converted != original.to(ttype)
    assert converted.tracks[0].notes[0].duration == min_duration


if __name__ == "__main__":
    pytest.main(["-xvs", __file__])


def test_flat_key_degree():
    from symusic import KeySignature

    assert "degree=7" in repr(KeySignature(0, -1, False))
    assert "degree=19" in repr(KeySignature(0, -1, True))


@pytest.mark.parametrize("ttype", [Tick, Quarter, Second])
def test_note_end_binding(ttype):
    from symusic import Note

    note = Note(1, 2, 60, 80, ttype=ttype)
    assert note.end == 3
    assert not hasattr(note, "ent_time")


def test_track_slice_with_large_step():
    import sys

    tracks = Score(ticks_per_quarter=480).tracks
    tracks.extend([Track("first"), Track("second"), Track("third")])
    selection = slice(1, None, sys.maxsize)
    assert [track.name for track in tracks[selection]] == ["second"]
    tracks[selection] = [Track("replacement")]
    assert tracks[1].name == "replacement"
    del tracks[selection]
    assert [track.name for track in tracks] == ["first", "third"]


@pytest.mark.parametrize("name", ["pitch", "velocity"])
@pytest.mark.parametrize("offset", [-61, 68])
def test_note_shift_rejects_midi_range(name, offset):
    note = Note(0, 1, 60, 60)
    shift = getattr(note, f"shift_{name}")
    with pytest.raises(ValueError, match=rf"{name} must be in 0\.\.127"):
        shift(offset)
    assert getattr(note, name) == 60
    assert shift(1) is None
    assert getattr(note, name) == 61


@pytest.mark.parametrize("ttype", [Tick, Quarter, Second])
def test_track_constructor_copies_event_lists(ttype):
    source = Track(ttype=ttype)
    events = {
        "notes": Note(1, 2, 60, 80, ttype=ttype),
        "controls": ControlChange(1, 7, 80, ttype=ttype),
        "pitch_bends": PitchBend(1, 2, ttype=ttype),
        "lyrics": TextMeta(1, "lyric", ttype=ttype),
    }
    for field, event in events.items():
        getattr(source, field).append(event)
    lists = {field: getattr(source, field) for field in events}
    track = Track(ttype=ttype, **lists)
    from_iterables = Track(
        ttype=ttype, **{field: iter(items) for field, items in lists.items()}
    )
    assert from_iterables == track

    for field, event in events.items():
        original = lists[field]
        owned = getattr(track, field)
        assert owned.ttype is ttype
        original[0].time = 3
        assert owned[0].time == 1
        original.clear()
        assert list(owned) == [event]
        owned.append(event)
        assert len(original) == 0
        original.append(event)
        owned.clear()
        assert list(original) == [event]


@pytest.mark.parametrize("ttype", [Quarter, Second])
@pytest.mark.parametrize("nan_time", [False, True])
@pytest.mark.parametrize("operation", ["count", "contains", "remove"])
def test_track_list_matches_identity_and_value(ttype, nan_time, operation):
    track = Track(notes=[Note(1, 2, 60, 80, ttype=ttype)], ttype=ttype)
    if nan_time:
        # Pickle restoration reads the note's native float bytes directly.
        unit, data = track.__getstate__()
        time = struct.pack("=f", 1)
        assert data.count(time) == 1
        track = Track.__new__(Track)
        track.__setstate__((unit, data.replace(time, struct.pack("=f", math.nan))))
        assert math.isnan(track.notes[0].time)
    tracks = TrackList([track], ttype=ttype)
    target = track if nan_time else Track(track)

    if operation == "count":
        assert tracks.count(target) == 1
    elif operation == "contains":
        assert target in tracks
    else:
        tracks.remove(target)
        assert len(tracks) == 0


@pytest.mark.parametrize("ttype", [Quarter, Second])
@pytest.mark.parametrize("nan_time", [False, True])
@pytest.mark.parametrize("operation", ["count", "contains", "remove"])
def test_note_list_matches_identity_and_value(ttype, nan_time, operation):
    note = Note(1, 2, 60, 80, ttype=ttype)
    if nan_time:
        import math
        import struct

        unit, data = note.__getstate__()
        time = struct.pack("=f", 1)
        assert data.count(time) == 1
        target = Note.__new__(Note)
        target.__setstate__((unit, data.replace(time, struct.pack("=f", math.nan))))
        assert math.isnan(target.time)
        notes = Track(notes=[target, note], ttype=ttype).notes
        target = notes[0]
    else:
        notes = Track(notes=[note, note], ttype=ttype).notes
        target = note.copy()
    assert notes.index(target) == 0

    if operation == "count":
        assert notes.count(target) == (1 if nan_time else 2)
    elif operation == "contains":
        assert target in notes
    else:
        notes.remove(target)
        assert list(notes) == [note]


@pytest.mark.parametrize(
    "selection", [slice(2048), slice(1, None, 2), slice(None, None, -2)]
)
def test_track_list_slice_deletion(selection):
    original = [Track(str(i)) for i in range(4096)]
    tracks = TrackList(original)
    expected = original.copy()

    del tracks[selection]
    del expected[selection]

    assert [track.name for track in tracks] == [track.name for track in expected]
    for track, reference in zip(tracks, expected, strict=True):
        reference.program = 1
        assert track.program == 1

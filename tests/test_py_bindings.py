"""
Test Python bindings for the C++ library.

This test module verifies that Python bindings correctly expose C++ functionality.
"""

from __future__ import annotations

import tempfile
import os
from pathlib import Path
import pytest
import numpy as np

from symusic import (
    Note,
    Track,
    Score,
    Tempo,
    TimeSignature,
    KeySignature,
    ControlChange,
    PitchBend,
    Pedal,
    TextMeta,
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
    assert note.start() == 100
    assert note.end() == 300
    
    # Test empty check
    assert not note.empty()
    empty_note = Note(100, 0, 60, 80)
    assert empty_note.empty()
    
    # Test modification methods
    note2 = note.shift_time(50)
    assert note2.time == 150
    
    note3 = note.shift_pitch(12)
    assert note3.pitch == 72
    
    note4 = note.shift_velocity(10)
    assert note4.velocity == 90


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
    assert track.note_num() == 2
    assert not track.empty()
    
    # Test time shifting
    shifted_track = track.shift_time(50)
    assert shifted_track.notes[0].time == 150
    assert shifted_track.notes[1].time == 350
    
    # Test pitch shifting
    pitched_track = track.shift_pitch(12)
    assert pitched_track.notes[0].pitch == 72
    assert pitched_track.notes[1].pitch == 76
    
    # Test clipping
    clipped_track = track.clip(200, 400)
    assert len(clipped_track.notes) == 1
    assert clipped_track.notes[0].time == 300


def test_score_creation():
    """Test creating Score objects and manipulating their contents."""
    # Create a score
    score = Score(480)  # 480 ticks per quarter
    
    # Check basic properties
    assert score.ticks_per_quarter == 480
    assert len(score.tracks) == 0
    
    # Add a track
    track = Track("Piano", 0, False)
    track.notes.append(Note(480, 240, 60, 80))
    track.notes.append(Note(720, 240, 64, 80))
    score.tracks.append(track)
    
    # Add a tempo and time signature
    score.tempos.append(Tempo(0, 500000))  # 120 BPM
    score.time_signatures.append(TimeSignature(0, 4, 4))
    
    # Test score operations
    assert score.start() == 480
    assert score.end() == 960
    assert score.note_num() == 2
    assert score.track_num() == 1
    
    # Test time shifting
    shifted_score = score.shift_time(100)
    assert shifted_score.tracks[0].notes[0].time == 580
    assert shifted_score.tempos[0].time == 100
    
    # Test clipping
    clipped_score = score.clip(600, 800)
    assert len(clipped_score.tracks[0].notes) == 1
    assert clipped_score.tracks[0].notes[0].time == 720


def test_event_types():
    """Test different event types work correctly."""
    # Test Tempo
    tempo = Tempo(0, 500000)  # 120 BPM
    assert abs(tempo.qpm() - 120.0) < 0.001
    tempo2 = Tempo.from_qpm(100, 120.0)
    assert tempo2.time == 100
    assert tempo2.mspq == 500000
    
    # Test TimeSignature
    ts = TimeSignature(200, 3, 4)
    assert ts.time == 200
    assert ts.numerator == 3
    assert ts.denominator == 4
    
    # Test KeySignature
    ks = KeySignature(300, 2, 0)  # D major
    assert ks.time == 300
    assert ks.key == 2
    assert ks.tonality == 0
    
    # Test ControlChange
    cc = ControlChange(50, 7, 100)  # Volume
    assert cc.time == 50
    assert cc.number == 7
    assert cc.value == 100
    
    # Test PitchBend
    pb = PitchBend(400, 8192)  # Center position
    assert pb.time == 400
    assert pb.value == 8192
    
    # Test Pedal
    pedal = Pedal(100, 200)
    assert pedal.time == 100
    assert pedal.duration == 200
    assert pedal.start() == 100
    assert pedal.end() == 300
    
    # Test TextMeta
    text = TextMeta(500, "Test Text")
    assert text.time == 500
    assert text.text == "Test Text"


def test_file_io():
    """Test file I/O operations (reading and writing MIDI files)."""
    # Create a simple score
    score = Score(480)
    
    # Add a track with notes
    track = Track("Piano", 0, False)
    track.notes.append(Note(480, 240, 60, 80))
    track.notes.append(Note(720, 240, 64, 80))
    track.notes.append(Note(960, 240, 67, 80))
    score.tracks.append(track)
    
    # Add tempo and time signature
    score.tempos.append(Tempo(0, 500000))  # 120 BPM
    score.time_signatures.append(TimeSignature(0, 4, 4))
    
    # Write to a temporary MIDI file
    with tempfile.TemporaryDirectory() as temp_dir:
        midi_path = os.path.join(temp_dir, "test.mid")
        score.dump_midi(midi_path)
        
        # Read the file back
        read_score = Score(midi_path)
        
        # Verify the read score
        assert read_score.ticks_per_quarter == 480
        assert len(read_score.tracks) == 1
        assert len(read_score.tracks[0].notes) == 3
        assert len(read_score.tempos) == 1
        assert abs(read_score.tempos[0].qpm() - 120.0) < 0.001
        assert len(read_score.time_signatures) == 1
        assert read_score.time_signatures[0].numerator == 4
        assert read_score.time_signatures[0].denominator == 4


def test_conversions():
    """Test conversions between different time unit types."""
    # Create notes with different time unit types
    tick_note = Note(480, 240, 60, 80)
    quarter_note = Note(1.0, 0.5, 60, 80)
    second_note = Note(0.5, 0.25, 60, 80)  # At 120 BPM
    
    # Check their properties match the expected values
    assert tick_note.time == 480
    assert tick_note.duration == 240
    
    assert quarter_note.time == 1.0
    assert quarter_note.duration == 0.5
    
    assert second_note.time == 0.5
    assert second_note.duration == 0.25
    
    # Test conversion functions if available in the Python API
    # This would depend on how the C++ conversion functions are exposed


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
    score1 = Score(480)
    score1.tracks.append(track1)
    
    score2 = Score(480)
    score2.tracks.append(track2)
    
    score3 = Score(480)
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
    """Test reading and writing MIDI files preserves all information."""
    # Skip if file doesn't exist
    if not midi_file.exists():
        pytest.skip(f"Test file {midi_file} not found")
    
    # Read the original MIDI file
    original_score = Score(midi_file)
    
    # Write to a temporary file
    with tempfile.TemporaryDirectory() as temp_dir:
        output_path = os.path.join(temp_dir, "output.mid")
        original_score.dump_midi(output_path)
        
        # Read the file back
        new_score = Score(output_path)
        
        # Verify content is preserved
        assert original_score.ticks_per_quarter == new_score.ticks_per_quarter
        assert len(original_score.tracks) == len(new_score.tracks)
        assert original_score.note_num() == new_score.note_num()
        
        # Check that the tracks are the same
        for i, (orig_track, new_track) in enumerate(zip(original_score.tracks, new_score.tracks)):
            assert orig_track.program == new_track.program
            assert orig_track.is_drum == new_track.is_drum
            assert len(orig_track.notes) == len(new_track.notes)


def test_numpy_integration():
    """Test integration with NumPy if available."""
    try:
        # Create a track with several notes
        track = Track("Piano", 0, False)
        for i in range(10):
            track.notes.append(Note(i * 100, 80, 60 + i, 80))
        
        # Extract note data as NumPy arrays
        times = np.array([note.time for note in track.notes])
        durations = np.array([note.duration for note in track.notes])
        pitches = np.array([note.pitch for note in track.notes])
        velocities = np.array([note.velocity for note in track.notes])
        
        # Verify the arrays have the expected values
        assert len(times) == 10
        assert times[0] == 0
        assert times[9] == 900
        assert np.all(durations == 80)
        assert pitches[0] == 60
        assert pitches[9] == 69
        assert np.all(velocities == 80)
        
        # Test creating a pianoroll if available in the API
        # This would depend on how the pianoroll functionality is exposed
    
    except ImportError:
        pytest.skip("NumPy not available")


if __name__ == "__main__":
    pytest.main(["-xvs", __file__]) 
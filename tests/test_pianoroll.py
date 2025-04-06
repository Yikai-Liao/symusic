"""
Test pianoroll functionality of the symusic library.

These tests verify that the pianoroll creation, manipulation, and conversion work correctly.
"""

from __future__ import annotations

import pytest
import numpy as np
from symusic import Score, Track, Note, PianoRoll

@pytest.fixture
def simple_track():
    """Create a simple track with a few notes for testing."""
    track = Track("Piano", 0, False)
    # C4 (MIDI note 60) for 1 beat at time 0
    track.notes.append(Note(0, 480, 60, 100))
    # E4 (MIDI note 64) for 1 beat at time 480
    track.notes.append(Note(480, 480, 64, 80))
    # G4 (MIDI note 67) for 1 beat at time 960
    track.notes.append(Note(960, 480, 67, 60))
    # C5 (MIDI note 72) for 2 beats at time 1440
    track.notes.append(Note(1440, 960, 72, 100))
    return track

@pytest.fixture
def simple_score(simple_track):
    """Create a simple score with one track."""
    score = Score(480)  # 480 ticks per quarter note
    score.tracks.append(simple_track)
    return score


def test_pianoroll_creation():
    """Test creating a pianoroll from scratch."""
    # Create an empty pianoroll
    pianoroll = PianoRoll(480)  # 480 ticks per quarter note
    
    # Verify initial properties
    assert pianoroll.ticks_per_quarter == 480
    assert pianoroll.get_shape() == (0, 128)  # Empty pianoroll, 128 MIDI notes
    
    # Add a note
    pianoroll.add_note(0, 480, 60, 100)
    assert pianoroll.get_shape() == (480, 128)  # Now has 480 time steps
    
    # Add a second note
    pianoroll.add_note(480, 480, 64, 80)
    assert pianoroll.get_shape() == (960, 128)  # Now has 960 time steps


def test_track_to_pianoroll(simple_track):
    """Test converting a track to a pianoroll."""
    # Convert the track to a pianoroll
    pianoroll = PianoRoll.from_track(simple_track, 480)
    
    # Verify the pianoroll shape
    assert pianoroll.ticks_per_quarter == 480
    assert pianoroll.get_shape() == (2400, 128)  # Last note ends at 2400 (1440+960)
    
    # Verify note placement
    assert pianoroll.get_value(240, 60) == 100  # Middle of first note
    assert pianoroll.get_value(720, 64) == 80   # Middle of second note
    assert pianoroll.get_value(1200, 67) == 60  # Middle of third note
    assert pianoroll.get_value(1920, 72) == 100 # Middle of fourth note
    
    # Check that the pianoroll is empty where no notes exist
    assert pianoroll.get_value(240, 61) == 0    # No note at pitch 61 at time 240


def test_score_to_pianoroll(simple_score):
    """Test converting a score to a pianoroll."""
    # Convert the score to a pianoroll
    pianoroll = PianoRoll.from_score(simple_score)
    
    # Verify the pianoroll shape
    assert pianoroll.ticks_per_quarter == 480
    assert pianoroll.get_shape() == (2400, 128)  # Last note ends at 2400 (1440+960)
    
    # Verify note placement
    assert pianoroll.get_value(240, 60) == 100  # Middle of first note
    assert pianoroll.get_value(720, 64) == 80   # Middle of second note
    assert pianoroll.get_value(1200, 67) == 60  # Middle of third note
    assert pianoroll.get_value(1920, 72) == 100 # Middle of fourth note


def test_pianoroll_to_numpy(simple_track):
    """Test converting a pianoroll to a NumPy array."""
    # Convert the track to a pianoroll
    pianoroll = PianoRoll.from_track(simple_track, 480)
    
    # Get the NumPy array representation
    array = pianoroll.to_array()
    
    # Verify the array shape
    assert array.shape == (2400, 128)
    
    # Verify note placement in the array
    assert array[240, 60] == 100  # Middle of first note
    assert array[720, 64] == 80   # Middle of second note
    assert array[1200, 67] == 60  # Middle of third note
    assert array[1920, 72] == 100 # Middle of fourth note
    
    # Check that the array is zeros where no notes exist
    assert array[240, 61] == 0    # No note at pitch 61 at time 240


def test_pianoroll_to_track():
    """Test converting a pianoroll back to a track."""
    # Create a pianoroll
    pianoroll = PianoRoll(480)
    
    # Add some notes
    pianoroll.add_note(0, 480, 60, 100)
    pianoroll.add_note(480, 480, 64, 80)
    pianoroll.add_note(960, 480, 67, 60)
    
    # Convert the pianoroll to a track
    track = pianoroll.to_track("Piano", 0, False)
    
    # Verify the track properties
    assert track.name == "Piano"
    assert track.program == 0
    assert track.is_drum == False
    
    # Verify the notes
    assert len(track.notes) == 3
    
    # Sort the notes by time to ensure consistent testing
    sorted_notes = sorted(track.notes, key=lambda n: n.time)
    
    assert sorted_notes[0].time == 0
    assert sorted_notes[0].duration == 480
    assert sorted_notes[0].pitch == 60
    assert sorted_notes[0].velocity == 100
    
    assert sorted_notes[1].time == 480
    assert sorted_notes[1].duration == 480
    assert sorted_notes[1].pitch == 64
    assert sorted_notes[1].velocity == 80
    
    assert sorted_notes[2].time == 960
    assert sorted_notes[2].duration == 480
    assert sorted_notes[2].pitch == 67
    assert sorted_notes[2].velocity == 60


def test_pianoroll_save_load():
    """Test saving and loading a pianoroll."""
    import tempfile
    import os
    
    # Create a pianoroll
    pianoroll = PianoRoll(480)
    pianoroll.add_note(0, 480, 60, 100)
    pianoroll.add_note(480, 480, 64, 80)
    
    # Save to a temporary file
    with tempfile.TemporaryDirectory() as temp_dir:
        file_path = os.path.join(temp_dir, "test_pianoroll.npy")
        pianoroll.save(file_path)
        
        # Load the saved pianoroll
        loaded_pianoroll = PianoRoll.load(file_path)
        
        # Verify the loaded pianoroll matches the original
        assert loaded_pianoroll.ticks_per_quarter == 480
        assert loaded_pianoroll.get_shape() == pianoroll.get_shape()
        
        # Compare the underlying arrays
        original_array = pianoroll.to_array()
        loaded_array = loaded_pianoroll.to_array()
        assert np.array_equal(original_array, loaded_array)


def test_pianoroll_visualization():
    """Test visualization functionality of pianoroll if available."""
    pianoroll = PianoRoll(480)
    pianoroll.add_note(0, 480, 60, 100)
    pianoroll.add_note(480, 480, 64, 80)
    pianoroll.add_note(960, 480, 67, 60)
    
    # Just test that the method exists and runs without error
    # The actual visualization can be tested manually
    try:
        pianoroll.show()
        # If we got here without an error, the method exists and ran
        # We can't verify the visualization itself in an automated test
        pass
    except (AttributeError, NotImplementedError):
        # Method might not exist or be implemented
        pytest.skip("Visualization method not available or not implemented")


def test_pianoroll_operations():
    """Test operations on pianorolls (e.g., transpose, time-shift)."""
    # Create a pianoroll
    pianoroll = PianoRoll(480)
    pianoroll.add_note(0, 480, 60, 100)
    
    # Test transpose operation if available
    try:
        transposed = pianoroll.transpose(12)  # Up one octave
        assert transposed.get_value(240, 72) == 100  # Note should be at pitch 72 now
    except (AttributeError, NotImplementedError):
        # Method might not exist or be implemented
        pytest.skip("Transpose method not available or not implemented")
    
    # Test time-shift operation if available
    try:
        shifted = pianoroll.shift_time(480)  # Shift forward by 480 ticks
        assert shifted.get_value(720, 60) == 100  # Note should be at time 720 now
    except (AttributeError, NotImplementedError):
        # Method might not exist or be implemented
        pytest.skip("Time-shift method not available or not implemented")


if __name__ == "__main__":
    pytest.main(["-xvs", __file__])

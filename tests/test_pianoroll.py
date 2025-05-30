"""
Test pianoroll functionality of the symusic library.

These tests verify that the pianoroll creation, manipulation, and conversion work correctly.
"""

from __future__ import annotations

import numpy as np
import pytest
from symusic import Note, Score, Track


@pytest.fixture()
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


@pytest.fixture()
def simple_score(simple_track):
    """Create a simple score with one track."""
    score = Score(480)  # 480 ticks per quarter note
    score.tracks.append(simple_track)
    return score


def test_pianoroll_creation(simple_track):
    """Test creating a pianoroll from a track."""
    # Create a pianoroll using the pianoroll method
    pianoroll = simple_track.pianoroll(
        modes=["frame"],
        pitch_range=(0, 128),
        encode_velocity=True,
    )

    # Verify the shape
    assert pianoroll.shape[0] == 1  # One mode (frame)
    assert pianoroll.shape[1] == 128  # 128 MIDI notes
    assert pianoroll.shape[2] >= 2400  # At least 2400 time steps

    # Check a few values in the pianoroll
    assert pianoroll[0, 60, 240] == 100  # Middle of first note (C4)
    assert pianoroll[0, 64, 720] == 80  # Middle of second note (E4)
    assert pianoroll[0, 67, 1200] == 60  # Middle of third note (G4)
    assert pianoroll[0, 72, 1920] == 100  # Middle of fourth note (C5)


def test_track_to_pianoroll(simple_track):
    """Test converting a track to a pianoroll with multiple modes."""
    # Convert the track to a pianoroll with multiple modes
    pianoroll = simple_track.pianoroll(
        modes=["onset", "frame", "offset"],
        pitch_range=(0, 128),
        encode_velocity=True,
    )

    # Verify the pianoroll shape
    assert pianoroll.shape[0] == 3  # Three modes
    assert pianoroll.shape[1] == 128  # 128 MIDI notes
    assert pianoroll.shape[2] >= 2400  # At least 2400 time steps

    # Verify onset mode (only start of notes has values)
    assert pianoroll[0, 60, 0] == 100  # Start of first note
    assert pianoroll[0, 60, 1] == 0  # Not the start
    assert pianoroll[0, 64, 480] == 80  # Start of second note

    # Verify frame mode (entire duration has values)
    assert pianoroll[1, 60, 0] == 100  # Start of first note
    assert pianoroll[1, 60, 240] == 100  # Middle of first note
    assert pianoroll[1, 60, 479] == 100  # End of first note

    # Verify offset mode (only end of notes has values)
    assert pianoroll[2, 60, 0] == 0  # Not the end
    assert pianoroll[2, 60, 479] == 0  # Not the exact end
    assert pianoroll[2, 60, 480] == 100  # End of first note


def test_score_to_pianoroll(simple_score):
    """Test converting a score to a pianoroll."""
    # Convert the score to a pianoroll
    pianoroll = simple_score.pianoroll(
        modes=["frame"],
        pitch_range=(0, 128),
        encode_velocity=True,
    )

    # Verify the pianoroll shape
    assert pianoroll.shape[0] == 1  # One mode
    assert pianoroll.shape[1] == 1  # One track
    assert pianoroll.shape[2] == 128  # 128 MIDI notes
    assert pianoroll.shape[3] >= 2400  # At least 2400 time steps

    # Verify note placement
    assert pianoroll[0, 0, 60, 240] == 100  # Middle of first note
    assert pianoroll[0, 0, 64, 720] == 80  # Middle of second note
    assert pianoroll[0, 0, 67, 1200] == 60  # Middle of third note
    assert pianoroll[0, 0, 72, 1920] == 100  # Middle of fourth note


def test_pianoroll_array_properties(simple_track):
    """Test that the pianoroll is properly a numpy array with correct properties."""
    # Get the pianoroll as a numpy array
    pianoroll = simple_track.pianoroll(
        modes=["frame"],
        encode_velocity=True,
    )

    # Verify it's a numpy array
    assert isinstance(pianoroll, np.ndarray)

    # Check properties of the array
    assert pianoroll.dtype == np.uint8

    # Verify array shape
    assert len(pianoroll.shape) == 3  # [modes, pitch, time]

    # Verify note placement in the array
    assert pianoroll[0, 60, 240] == 100  # Middle of first note
    assert pianoroll[0, 64, 720] == 80  # Middle of second note
    assert pianoroll[0, 67, 1200] == 60  # Middle of third note
    assert pianoroll[0, 72, 1920] == 100  # Middle of fourth note

    # Check that the array is zeros where no notes exist
    assert pianoroll[0, 61, 240] == 0  # No note at pitch 61 at time 240


def test_pianoroll_with_custom_pitch_range(simple_track):
    """Test creating a pianoroll with a custom pitch range."""
    # Create a pianoroll with limited pitch range
    pianoroll = simple_track.pianoroll(
        modes=["frame"],
        pitch_range=(60, 73),  # Only include C4 to C5
        encode_velocity=True,
    )

    # Verify the shape
    assert pianoroll.shape[0] == 1  # One mode
    assert pianoroll.shape[1] == 13  # 13 notes in the range

    # Check values in the limited range
    assert pianoroll[0, 0, 240] == 100  # C4 (60) is now at index 0
    assert pianoroll[0, 4, 720] == 80  # E4 (64) is now at index 4
    assert pianoroll[0, 7, 1200] == 60  # G4 (67) is now at index 7
    assert pianoroll[0, 12, 1920] == 100  # C5 (72) is now at index 12


def test_pianoroll_binary_encoding(simple_track):
    """Test creating a pianoroll with binary encoding (no velocity)."""
    # Create a pianoroll with binary encoding
    pianoroll = simple_track.pianoroll(
        modes=["frame"],
        encode_velocity=False,
    )

    # Verify note values are binary (1 instead of velocity)
    assert pianoroll[0, 60, 240] == 1  # Middle of first note
    assert pianoroll[0, 64, 720] == 1  # Middle of second note
    assert pianoroll[0, 67, 1200] == 1  # Middle of third note
    assert pianoroll[0, 72, 1920] == 1  # Middle of fourth note


if __name__ == "__main__":
    pytest.main(["-xvs", __file__])

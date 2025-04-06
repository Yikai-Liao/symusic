"""
Test various operations on MIDI data: filtering, timing adjustments, etc.

These tests verify that operations on MIDI data work correctly.
"""

from __future__ import annotations

import pytest
import copy
from symusic import Score, Track, Note, Tempo, TimeSignature

@pytest.fixture
def complex_score():
    """Create a complex score with multiple tracks and notes for testing."""
    score = Score(480)  # 480 ticks per quarter
    
    # Track 1: Piano
    piano_track = Track("Piano", 0, False)
    # Notes spanning different pitch ranges
    piano_track.notes.append(Note(0, 480, 60, 80))    # Middle C
    piano_track.notes.append(Note(480, 480, 64, 80))  # E
    piano_track.notes.append(Note(960, 480, 67, 80))  # G
    piano_track.notes.append(Note(1440, 480, 48, 80)) # C below middle C
    piano_track.notes.append(Note(1920, 480, 72, 80)) # C above middle C
    score.tracks.append(piano_track)
    
    # Track 2: Bass
    bass_track = Track("Bass", 32, False)
    bass_track.notes.append(Note(0, 960, 36, 100))    # C2
    bass_track.notes.append(Note(960, 960, 43, 100))  # G2
    score.tracks.append(bass_track)
    
    # Track 3: Drums (varied velocities)
    drum_track = Track("Drums", 0, True)
    drum_track.notes.append(Note(0, 240, 36, 120))    # Bass drum (forte)
    drum_track.notes.append(Note(240, 240, 36, 80))   # Bass drum (medium)
    drum_track.notes.append(Note(480, 240, 36, 40))   # Bass drum (piano)
    drum_track.notes.append(Note(720, 240, 38, 100))  # Snare
    drum_track.notes.append(Note(960, 240, 42, 90))   # Hi-hat closed
    score.tracks.append(drum_track)
    
    # Add tempo changes
    score.tempos.append(Tempo(0, 500000))       # 120 BPM
    score.tempos.append(Tempo(1920, 600000))    # 100 BPM
    
    # Add time signature
    score.time_signatures.append(TimeSignature(0, 4, 4))
    
    return score


def test_filter_by_pitch_range():
    """Test filtering notes by pitch range."""
    # Create a track with notes across multiple octaves
    track = Track("Piano", 0, False)
    track.notes.append(Note(0, 480, 36, 80))     # C2
    track.notes.append(Note(480, 480, 48, 80))   # C3
    track.notes.append(Note(960, 480, 60, 80))   # C4 (middle C)
    track.notes.append(Note(1440, 480, 72, 80))  # C5
    track.notes.append(Note(1920, 480, 84, 80))  # C6
    
    # Filter notes in the middle range (C3 to C5)
    filtered_track = track.filter(pitch_range=(48, 72))
    
    # Verify filtering worked correctly
    assert len(filtered_track.notes) == 3
    pitches = [note.pitch for note in filtered_track.notes]
    assert 36 not in pitches  # C2 should be removed
    assert 48 in pitches      # C3 should be included
    assert 60 in pitches      # C4 should be included
    assert 72 in pitches      # C5 should be included
    assert 84 not in pitches  # C6 should be removed


def test_filter_by_velocity_range():
    """Test filtering notes by velocity range."""
    # Create a track with notes of different velocities
    track = Track("Piano", 0, False)
    track.notes.append(Note(0, 480, 60, 20))     # Very soft
    track.notes.append(Note(480, 480, 60, 50))   # Medium soft
    track.notes.append(Note(960, 480, 60, 80))   # Medium loud
    track.notes.append(Note(1440, 480, 60, 110)) # Very loud
    
    # Filter notes in the medium range (velocity 40-90)
    filtered_track = track.filter(velocity_range=(40, 90))
    
    # Verify filtering worked correctly
    assert len(filtered_track.notes) == 2
    velocities = [note.velocity for note in filtered_track.notes]
    assert 20 not in velocities  # Very soft should be removed
    assert 50 in velocities      # Medium soft should be included
    assert 80 in velocities      # Medium loud should be included
    assert 110 not in velocities # Very loud should be removed


def test_filter_by_time_range():
    """Test filtering notes by time range."""
    # Create a track with notes at different times
    track = Track("Piano", 0, False)
    track.notes.append(Note(0, 480, 60, 80))      # 0-480
    track.notes.append(Note(480, 480, 62, 80))    # 480-960
    track.notes.append(Note(960, 480, 64, 80))    # 960-1440
    track.notes.append(Note(1440, 480, 65, 80))   # 1440-1920
    
    # Filter notes in the middle time range (400-1200)
    filtered_track = track.filter(time_range=(400, 1200))
    
    # Verify filtering worked correctly - should include notes that overlap with the range
    assert len(filtered_track.notes) == 2
    times = [note.time for note in filtered_track.notes]
    assert 480 in times  # Second note should be included
    assert 960 in times  # Third note should be included
    assert 0 not in times    # First note should be removed
    assert 1440 not in times # Fourth note should be removed


def test_score_filter(complex_score):
    """Test filtering an entire score."""
    # Create a copy of the score to filter
    score = copy.deepcopy(complex_score)
    
    # Filter out all notes below middle C (MIDI note 60)
    filtered_score = score.filter(pitch_range=(60, 127))
    
    # Verify filtering worked correctly
    all_filtered_notes = []
    for track in filtered_score.tracks:
        all_filtered_notes.extend(track.notes)
    
    # Check that no notes below middle C remain
    for note in all_filtered_notes:
        assert note.pitch >= 60
        
    # Check that we have fewer notes than before
    assert len(all_filtered_notes) < sum(len(track.notes) for track in score.tracks)


def test_adjust_velocities():
    """Test adjusting velocities of notes."""
    # Create a track with notes of different velocities
    track = Track("Piano", 0, False)
    track.notes.append(Note(0, 480, 60, 64))
    track.notes.append(Note(480, 480, 62, 64))
    
    # Scale velocities by 1.5
    adjusted_track = track.adjust_velocity(factor=1.5)
    
    # Verify velocity adjustment
    assert adjusted_track.notes[0].velocity == 96  # 64 * 1.5 = 96
    assert adjusted_track.notes[1].velocity == 96
    
    # Test with a minimum threshold
    track.notes[0].velocity = 20
    track.notes[1].velocity = 80
    adjusted_track = track.adjust_velocity(factor=1.2, min_velocity=30)
    
    # Verify velocity adjustment with minimum threshold
    assert adjusted_track.notes[0].velocity == 30  # 20 * 1.2 = 24, but min is 30
    assert adjusted_track.notes[1].velocity == 96  # 80 * 1.2 = 96
    
    # Test with a maximum threshold
    adjusted_track = track.adjust_velocity(factor=1.5, max_velocity=100)
    
    # Verify velocity adjustment with maximum threshold
    assert adjusted_track.notes[0].velocity == 30  # 20 * 1.5 = 30
    assert adjusted_track.notes[1].velocity == 100  # 80 * 1.5 = 120, but max is 100


def test_quantize_timing():
    """Test quantizing note timings."""
    # Create a track with notes at irregular timings
    track = Track("Piano", 0, False)
    track.notes.append(Note(10, 230, 60, 80))    # Slightly off from beat
    track.notes.append(Note(485, 230, 62, 80))   # Slightly off from beat
    track.notes.append(Note(1005, 230, 64, 80))  # Slightly off from beat
    
    # Quantize to the nearest quarter note (480 ticks)
    quantized_track = track.quantize(480)
    
    # Verify quantization
    assert quantized_track.notes[0].time == 0    # Closest to 0
    assert quantized_track.notes[1].time == 480  # Closest to 480
    assert quantized_track.notes[2].time == 960  # Closest to 960
    
    # Test quantizing with different grid sizes
    track = Track("Piano", 0, False)
    track.notes.append(Note(10, 230, 60, 80))
    track.notes.append(Note(130, 230, 62, 80))
    track.notes.append(Note(250, 230, 64, 80))
    
    # Quantize to the nearest eighth note (480/2 = 240 ticks)
    quantized_track = track.quantize(240)
    
    # Verify quantization
    assert quantized_track.notes[0].time == 0    # Closest to 0
    assert quantized_track.notes[1].time == 240  # Closest to 240
    assert quantized_track.notes[2].time == 240  # Closest to 240


def test_adjust_tempo(complex_score):
    """Test adjusting the tempo of a score."""
    # Create a copy of the score to modify
    score = copy.deepcopy(complex_score)
    
    # Original tempos: 120 BPM at time 0, 100 BPM at time 1920
    
    # Double the tempo
    adjusted_score = score.adjust_tempo(factor=2.0)
    
    # Verify tempo adjustment
    assert len(adjusted_score.tempos) == 2
    assert abs(adjusted_score.tempos[0].qpm() - 240.0) < 0.1  # 120 * 2 = 240 BPM
    assert abs(adjusted_score.tempos[1].qpm() - 200.0) < 0.1  # 100 * 2 = 200 BPM
    
    # Verify note timings are adjusted
    # When tempo is doubled, note positions should be halved
    assert adjusted_score.tracks[0].notes[0].time == 0       # Original: 0
    assert adjusted_score.tracks[0].notes[1].time == 240     # Original: 480
    assert adjusted_score.tracks[0].notes[2].time == 480     # Original: 960
    
    # Verify the tempo change point is also adjusted
    assert adjusted_score.tempos[1].time == 960  # Original: 1920


def test_invert_notes():
    """Test inverting notes around a center pitch."""
    # Create a track with a C major triad (C-E-G)
    track = Track("Piano", 0, False)
    track.notes.append(Note(0, 480, 60, 80))    # C
    track.notes.append(Note(0, 480, 64, 80))    # E
    track.notes.append(Note(0, 480, 67, 80))    # G
    
    # Invert around middle C (60)
    inverted_track = track.invert(center_pitch=60)
    
    # Verify inversion
    # C (60) stays as C (60)
    # E (64) becomes A-flat (60 - (64 - 60) = 56)
    # G (67) becomes F (60 - (67 - 60) = 53)
    pitches = sorted([note.pitch for note in inverted_track.notes])
    assert pitches == [53, 56, 60]
    
    # Test with a different center pitch
    inverted_track = track.invert(center_pitch=64)
    
    # Verify inversion around E (64)
    # C (60) becomes G-sharp (64 + (64 - 60) = 68)
    # E (64) stays as E (64)
    # G (67) becomes B (64 - (67 - 64) = 61)
    pitches = sorted([note.pitch for note in inverted_track.notes])
    assert pitches == [61, 64, 68]


def test_transpose_notes():
    """Test transposing notes by a specified interval."""
    # Create a track with a C major chord
    track = Track("Piano", 0, False)
    track.notes.append(Note(0, 480, 60, 80))    # C
    track.notes.append(Note(0, 480, 64, 80))    # E
    track.notes.append(Note(0, 480, 67, 80))    # G
    
    # Transpose up a perfect fifth (7 semitones)
    transposed_track = track.shift_pitch(7)
    
    # Verify transposition
    # C (60) -> G (67)
    # E (64) -> B (71)
    # G (67) -> D (74)
    pitches = sorted([note.pitch for note in transposed_track.notes])
    assert pitches == [67, 71, 74]
    
    # Test transposing down an octave
    transposed_track = track.shift_pitch(-12)
    
    # Verify transposition
    # C (60) -> C (48)
    # E (64) -> E (52)
    # G (67) -> G (55)
    pitches = sorted([note.pitch for note in transposed_track.notes])
    assert pitches == [48, 52, 55]


if __name__ == "__main__":
    pytest.main(["-xvs", __file__]) 
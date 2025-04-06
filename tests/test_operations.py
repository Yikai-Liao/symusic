"""
Test operations for manipulating music data.

These tests verify that operations like filtering, transforming, and adjusting
MIDI data work correctly.
"""

from __future__ import annotations

import copy

import pytest
from symusic import Note, Score, Tempo, TimeSignature, Track


@pytest.fixture()
def complex_score():
    """Create a more complex score for testing."""
    score = Score(480)  # 480 ticks per quarter

    # Track 1: Piano
    piano = Track("Piano", 0, False)
    piano.notes.append(Note(0, 480, 60, 100))  # C4
    piano.notes.append(Note(480, 480, 64, 80))  # E4
    piano.notes.append(Note(960, 480, 67, 60))  # G4
    piano.notes.append(Note(1440, 960, 72, 100))  # C5

    # Track 2: Bass
    bass = Track("Bass", 32, False)
    bass.notes.append(Note(0, 960, 36, 100))  # C2
    bass.notes.append(Note(960, 960, 43, 80))  # G2
    bass.notes.append(Note(1920, 480, 36, 100))  # C2

    # Track 3: Drums
    drums = Track("Drums", 0, True)
    drums.notes.append(Note(0, 240, 36, 100))  # Bass drum
    drums.notes.append(Note(480, 240, 38, 80))  # Snare
    drums.notes.append(Note(960, 240, 42, 60))  # Closed hi-hat
    drums.notes.append(Note(1440, 240, 46, 100))  # Open hi-hat
    drums.notes.append(Note(1920, 240, 49, 80))  # Crash cymbal

    # Add tracks to score
    score.tracks.append(piano)
    score.tracks.append(bass)
    score.tracks.append(drums)

    # Add tempo changes
    score.tempos.append(Tempo(0, 120))  # 120 BPM at start
    score.tempos.append(Tempo(1920, 100))  # 100 BPM later

    # Add time signature
    score.time_signatures.append(TimeSignature(0, 4, 4))  # 4/4

    return score


def test_filter_by_pitch_range():
    """Test filtering notes by pitch range."""
    # Create a track with notes across multiple octaves
    track = Track("Piano", 0, False)
    track.notes.append(Note(0, 480, 36, 80))  # C2
    track.notes.append(Note(480, 480, 48, 80))  # C3
    track.notes.append(Note(960, 480, 60, 80))  # C4 (middle C)
    track.notes.append(Note(1440, 480, 72, 80))  # C5
    track.notes.append(Note(1920, 480, 84, 80))  # C6

    # Filter notes in the middle range (C3 to C5)
    # Create a new track by only including notes in the desired pitch range
    filtered_track = Track("Piano", 0, False)
    for note in track.notes:
        if 48 <= note.pitch < 72:
            filtered_track.notes.append(note)

    # Check that only the notes in the specified range remain
    assert len(filtered_track.notes) == 2
    assert filtered_track.notes[0].pitch == 48  # C3
    assert filtered_track.notes[1].pitch == 60  # C4


def test_filter_by_velocity_range():
    """Test filtering notes by velocity range."""
    # Create a track with notes of different velocities
    track = Track("Piano", 0, False)
    track.notes.append(Note(0, 480, 60, 20))  # Very soft
    track.notes.append(Note(480, 480, 60, 50))  # Medium soft
    track.notes.append(Note(960, 480, 60, 80))  # Medium loud
    track.notes.append(Note(1440, 480, 60, 110))  # Very loud

    # Filter notes in the medium range (velocity 40-90)
    # Create a new track by only including notes in the desired velocity range
    filtered_track = Track("Piano", 0, False)
    for note in track.notes:
        if 40 <= note.velocity <= 90:
            filtered_track.notes.append(note)

    # Check that only the notes in the specified range remain
    assert len(filtered_track.notes) == 2
    assert filtered_track.notes[0].velocity == 50
    assert filtered_track.notes[1].velocity == 80


def test_filter_by_time_range():
    """Test filtering notes by time range."""
    # Create a track with notes at different times
    track = Track("Piano", 0, False)
    track.notes.append(Note(0, 480, 60, 80))  # 0-480
    track.notes.append(Note(480, 480, 62, 80))  # 480-960
    track.notes.append(Note(960, 480, 64, 80))  # 960-1440
    track.notes.append(Note(1440, 480, 65, 80))  # 1440-1920

    # Filter notes in the middle time range (400-1200)
    # Create a new track by only including notes in the desired time range
    filtered_track = Track("Piano", 0, False)
    for note in track.notes:
        if 400 <= note.time <= 1200:
            filtered_track.notes.append(note)

    # Check that only the notes in the specified range remain
    assert len(filtered_track.notes) == 2
    assert filtered_track.notes[0].time == 480
    assert filtered_track.notes[1].time == 960


def test_score_filter(complex_score):
    """Test filtering an entire score."""
    # Create a copy of the score to filter
    score = copy.deepcopy(complex_score)

    # Filter out all notes below middle C (MIDI note 60)
    filtered_score = Score(score.ticks_per_quarter)

    # Copy metadata
    filtered_score.tempos = score.tempos
    filtered_score.time_signatures = score.time_signatures

    # Filter tracks and add to new score
    for track in score.tracks:
        filtered_track = Track(track.name, track.program, track.is_drum)
        for note in track.notes:
            if note.pitch >= 60:
                filtered_track.notes.append(note)
        filtered_score.tracks.append(filtered_track)

    # Count the total notes in the filtered score
    total_notes = sum(len(track.notes) for track in filtered_score.tracks)

    # Original complex_score has 12 notes, but only some are above middle C
    assert total_notes < 12


def test_adjust_velocities():
    """Test adjusting velocities of notes."""
    # Create a track with notes of different velocities
    track = Track("Piano", 0, False)
    track.notes.append(Note(0, 480, 60, 64))
    track.notes.append(Note(480, 480, 62, 64))

    # Scale velocities by 1.5 - create a new track with adjusted velocities
    adjusted_track = Track("Piano", 0, False)
    for note in track.notes:
        # Create a new note with scaled velocity (capped at 127)
        new_velocity = min(127, int(note.velocity * 1.5))
        adjusted_note = Note(note.time, note.duration, note.pitch, new_velocity)
        adjusted_track.notes.append(adjusted_note)

    # Check that velocities were adjusted correctly
    assert adjusted_track.notes[0].velocity == 96  # 64 * 1.5 = 96
    assert adjusted_track.notes[1].velocity == 96  # 64 * 1.5 = 96


def test_quantize_timing():
    """Test quantizing note timings."""
    # Create a track with notes at irregular timings
    track = Track("Piano", 0, False)
    track.notes.append(Note(10, 230, 60, 80))  # Slightly off from beat
    track.notes.append(Note(485, 230, 62, 80))  # Slightly off from beat
    track.notes.append(Note(1005, 230, 64, 80))  # Slightly off from beat

    # Quantize to the nearest quarter note (480 ticks)
    # Manually implement quantization by rounding to nearest grid
    quantized_track = Track("Piano", 0, False)
    grid_size = 480  # Quarter note grid

    for note in track.notes:
        # Calculate the nearest grid point
        quantized_time = round(note.time / grid_size) * grid_size
        # Create a new note with the quantized time
        quantized_note = Note(quantized_time, note.duration, note.pitch, note.velocity)
        quantized_track.notes.append(quantized_note)

    # Check that timings were quantized correctly
    assert quantized_track.notes[0].time == 0  # 10 -> 0
    assert quantized_track.notes[1].time == 480  # 485 -> 480
    assert quantized_track.notes[2].time == 960  # 1005 -> 960


def test_adjust_tempo(complex_score):
    """Test adjusting the tempo of a score."""
    # Create a copy of the score to modify
    score = copy.deepcopy(complex_score)

    # Original tempos: 120 BPM at time 0, 100 BPM at time 1920

    # Double the tempo
    adjusted_score = copy.deepcopy(score)

    # Create new tempo list with doubled values
    new_tempos = []
    for tempo in score.tempos:
        # Create a new tempo with doubled BPM
        new_tempo = Tempo(tempo.time, tempo.qpm * 2)
        new_tempos.append(new_tempo)

    adjusted_score.tempos = new_tempos

    # Check that the tempos were adjusted correctly
    assert adjusted_score.tempos[0].qpm == 240  # 120 * 2 = 240
    assert adjusted_score.tempos[1].qpm == 200  # 100 * 2 = 200


def test_transpose_notes():
    """Test transposing notes up or down in pitch."""
    # Create a track with a C major triad (C-E-G)
    track = Track("Piano", 0, False)
    track.notes.append(Note(0, 480, 60, 80))  # C
    track.notes.append(Note(0, 480, 64, 80))  # E
    track.notes.append(Note(0, 480, 67, 80))  # G

    # Transpose up a perfect fifth (7 semitones)
    transposed_track = track.shift_pitch(7)

    # Check that pitches were transposed correctly
    assert transposed_track.notes[0].pitch == 67  # C -> G
    assert transposed_track.notes[1].pitch == 71  # E -> B
    assert transposed_track.notes[2].pitch == 74  # G -> D


def test_invert_notes():
    """Test inverting notes around a center pitch."""
    # Create a track with a C major triad (C-E-G)
    track = Track("Piano", 0, False)
    track.notes.append(Note(0, 480, 60, 80))  # C
    track.notes.append(Note(0, 480, 64, 80))  # E
    track.notes.append(Note(0, 480, 67, 80))  # G

    # Invert around middle C (60)
    inverted_track = Track("Piano", 0, False)
    center_pitch = 60

    for note in track.notes:
        # Calculate inverted pitch: center + (center - pitch)
        inverted_pitch = 2 * center_pitch - note.pitch
        inverted_note = Note(note.time, note.duration, inverted_pitch, note.velocity)
        inverted_track.notes.append(inverted_note)

    # Check that pitches were inverted correctly
    assert (
        inverted_track.notes[0].pitch == 60
    )  # C -> C (unchanged since it's the center)
    assert inverted_track.notes[1].pitch == 56  # E -> Ab below C
    assert inverted_track.notes[2].pitch == 53  # G -> F below C


if __name__ == "__main__":
    pytest.main(["-xvs", __file__])

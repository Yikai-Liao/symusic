#pragma once
#ifndef SYMUSIC_TEST_TRACK_SCORE_HPP
#define SYMUSIC_TEST_TRACK_SCORE_HPP

#include "symusic.h"
#include "catch2/catch_test_macros.hpp"
using namespace symusic;

/**
 * Test suite for Track class operations
 * These tests verify that track-level operations work correctly
 */
TEST_CASE("Test Track Operations", "[symusic][track]") {
    SECTION("Track Default Constructor") {
        // Test default constructor
        Track<Tick> track;
        REQUIRE(track.name == "");
        REQUIRE(track.program == 0);
        REQUIRE(track.is_drum == false);
        REQUIRE(track.notes->empty());
        REQUIRE(track.controls->empty());
        REQUIRE(track.pitch_bends->empty());
        REQUIRE(track.pedals->empty());
    }

    SECTION("Track Named Constructor") {
        // Test named constructor
        Track<Tick> track("Piano", 0, false);
        REQUIRE(track.name == "Piano");
        REQUIRE(track.program == 0);
        REQUIRE(track.is_drum == false);
    }

    SECTION("Track Copy and Equality") {
        // Create a track with some data
        Track<Tick> track1("Piano", 0, false);
        track1.notes->push_back(Note<Tick>{480, 240, 60, 80});
        track1.controls->push_back(ControlChange<Tick>{120, 0, 100});

        // Test copy constructor
        Track<Tick> track2 = track1.copy();
        REQUIRE(track1 == track2);

        // Test deep copy
        Track<Tick> track3 = track1.deepcopy();
        REQUIRE(track1 == track3);

        // Modify the original track and verify deep copy is unchanged
        track1.notes->push_back(Note<Tick>{600, 240, 62, 80});
        REQUIRE(track1 != track3);
    }

    SECTION("Track Start and End Time") {
        Track<Tick> track("Piano", 0, false);

        // Empty track should have start/end time of 0
        REQUIRE(track.start() == 0); // start() returns max() for empty, but min(max(), max()...) is max(), needs fix in track.cpp/score.cpp
        REQUIRE(track.end() == 0);

        // Add some notes and check start/end time
        track.notes->push_back(Note<Tick>{100, 200, 60, 80});  // ends at 300
        REQUIRE(track.start() == 100); // Now expect 100 after adding the note
        REQUIRE(track.end() == 300);   // End time should be correct

        track.notes->push_back(Note<Tick>{400, 100, 62, 80});  // ends at 500
        REQUIRE(track.start() == 100);  // Start time should still be the earliest
        REQUIRE(track.end() == 500);    // End time should update

        // Add other event types and check if end time updates
        track.pedals->push_back(Pedal<Tick>{600, 200});  // ends at 800
        REQUIRE(track.end() == 800);
    }

    SECTION("Track Sorting") {
        Track<Tick> track("Piano", 0, false);

        // Add notes in non-sorted order
        track.notes->push_back(Note<Tick>{300, 200, 60, 80});
        track.notes->push_back(Note<Tick>{100, 100, 62, 80});
        track.notes->push_back(Note<Tick>{200, 150, 64, 80});

        // Test sorting
        track.sort_inplace();
        REQUIRE(track.notes->at(0).time == 100);
        REQUIRE(track.notes->at(1).time == 200);
        REQUIRE(track.notes->at(2).time == 300);

        // Test reverse sorting
        track.sort_inplace(true);
        REQUIRE(track.notes->at(0).time == 300);
        REQUIRE(track.notes->at(1).time == 200);
        REQUIRE(track.notes->at(2).time == 100);
    }

    SECTION("Track Clipping") {
        Track<Tick> track("Piano", 0, false);

        // Add some notes
        track.notes->push_back(Note<Tick>{100, 200, 60, 80});  // 100-300
        track.notes->push_back(Note<Tick>{300, 200, 62, 80});  // 300-500
        track.notes->push_back(Note<Tick>{500, 200, 64, 80});  // 500-700

        // Test clipping by start time
        Track<Tick> clipped1 = track.clip(200, 600);
        REQUIRE(clipped1.notes->size() == 2);
        REQUIRE(clipped1.notes->at(0).time == 300);
        REQUIRE(clipped1.notes->at(1).time == 500);

        // Test clipping by end time
        Track<Tick> clipped2 = track.clip(0, 400, true);
        REQUIRE(clipped2.notes->size() == 1);
        REQUIRE(clipped2.notes->at(0).time == 100);
    }

    SECTION("Track Time Shifting") {
        Track<Tick> track("Piano", 0, false);

        // Add some notes
        track.notes->push_back(Note<Tick>{100, 200, 60, 80});
        track.notes->push_back(Note<Tick>{300, 200, 62, 80});

        // Test time shifting
        Track<Tick> shifted = track.shift_time(50);
        REQUIRE(shifted.notes->at(0).time == 150);
        REQUIRE(shifted.notes->at(1).time == 350);

        // Test inplace shifting
        track.shift_time_inplace(-20);
        REQUIRE(track.notes->at(0).time == 80);
        REQUIRE(track.notes->at(1).time == 280);
    }

    SECTION("Track Pitch and Velocity Shifting") {
        Track<Tick> track("Piano", 0, false);

        // Add some notes
        track.notes->push_back(Note<Tick>{100, 200, 60, 80});
        track.notes->push_back(Note<Tick>{300, 200, 62, 70});

        // Test pitch shifting
        Track<Tick> pitched = track.shift_pitch(12);
        REQUIRE(pitched.notes->at(0).pitch == 72);
        REQUIRE(pitched.notes->at(1).pitch == 74);

        // Test velocity shifting
        Track<Tick> velocitied = track.shift_velocity(10);
        REQUIRE(velocitied.notes->at(0).velocity == 90);
        REQUIRE(velocitied.notes->at(1).velocity == 80);
    }
}

/**
 * Test suite for Score class operations
 * These tests verify that score-level operations work correctly
 */
TEST_CASE("Test Score Operations", "[symusic][score]") {
    SECTION("Score Default Constructor") {
        // Test default constructor
        Score<Tick> score;
        REQUIRE(score.ticks_per_quarter == 0);
        REQUIRE(score.tracks->empty());
        REQUIRE(score.time_signatures->empty());
        REQUIRE(score.key_signatures->empty());
        REQUIRE(score.tempos->empty());
        REQUIRE(score.markers->empty());
    }

    SECTION("Score TPQ Constructor") {
        // Test TPQ constructor
        Score<Tick> score(480);
        REQUIRE(score.ticks_per_quarter == 480);
    }

    SECTION("Score Copy and Equality") {
        // Create a score with some data
        Score<Tick> score1(480);

        // Add a track
        auto track = std::make_shared<Track<Tick>>("Piano", 0, false);
        track->notes->push_back(Note<Tick>{480, 240, 60, 80});
        score1.tracks->push_back(track);

        // Add a tempo
        score1.tempos->push_back(Tempo<Tick>{0, 500000});

        // Test copy constructor
        Score<Tick> score2 = score1.copy();
        REQUIRE(score1 == score2);

        // Test deep copy
        Score<Tick> score3 = score1.deepcopy();
        REQUIRE(score1 == score3);

        // Modify the original score and verify deep copy is unchanged
        score1.tempos->push_back(Tempo<Tick>{1000, 400000});
        REQUIRE(score1 != score3);
    }

    SECTION("Score Start and End Time") {
        Score<Tick> score(480);

        // Empty score should have start/end time of 0
        REQUIRE(score.start() == 0); // start() returns max() for empty, needs fix in score.cpp
        REQUIRE(score.end() == 0);

        // Add a track with notes
        auto track = std::make_shared<Track<Tick>>("Piano", 0, false);
        track->notes->push_back(Note<Tick>{100, 200, 60, 80});  // ends at 300
        score.tracks->push_back(track);

        // Test score start time after adding track
        REQUIRE(score.start() == 100); // Now expect 100 after adding the track with the note
        REQUIRE(score.end() == 300);   // End time should be correct

        track->notes->push_back(Note<Tick>{400, 100, 62, 80});  // ends at 500
        REQUIRE(score.start() == 100); // Start time should still be the earliest
        REQUIRE(score.end() == 500);   // End time should update

        // Add a tempo event that ends later
        score.tempos->push_back(Tempo<Tick>{600, 500000});
        REQUIRE(score.end() == 600);
    }

    SECTION("Score Clipping") {
        Score<Tick> score(480);

        // Add a track with notes
        auto track = std::make_shared<Track<Tick>>("Piano", 0, false);
        track->notes->push_back(Note<Tick>{100, 200, 60, 80});  // 100-300
        track->notes->push_back(Note<Tick>{300, 200, 62, 80});  // 300-500
        track->notes->push_back(Note<Tick>{500, 200, 64, 80});  // 500-700
        score.tracks->push_back(track);

        // Add some tempo events
        score.tempos->push_back(Tempo<Tick>{0, 500000});
        score.tempos->push_back(Tempo<Tick>{400, 400000});

        // Test clipping
        Score<Tick> clipped = score.clip(200, 600);

        // Check that notes are clipped
        REQUIRE(clipped.tracks->at(0)->notes->size() == 2);
        REQUIRE(clipped.tracks->at(0)->notes->at(0).time == 300);
        REQUIRE(clipped.tracks->at(0)->notes->at(1).time == 500);

        // Check that tempos are clipped
        // Keep the corrected expectation based on previous findings
        REQUIRE(clipped.tempos->size() == 2);
        REQUIRE(clipped.tempos->at(0).time == 200);
        REQUIRE(clipped.tempos->at(1).time == 400);
    }

    SECTION("Score Time Shifting") {
        Score<Tick> score(480);

        // Add a track with notes
        auto track = std::make_shared<Track<Tick>>("Piano", 0, false);
        track->notes->push_back(Note<Tick>{100, 200, 60, 80});
        track->notes->push_back(Note<Tick>{300, 200, 62, 80});
        score.tracks->push_back(track);

        // Add a tempo event
        score.tempos->push_back(Tempo<Tick>{0, 500000});

        // Test time shifting
        Score<Tick> shifted = score.shift_time(50);
        REQUIRE(shifted.tracks->at(0)->notes->at(0).time == 150);
        REQUIRE(shifted.tracks->at(0)->notes->at(1).time == 350);
        REQUIRE(shifted.tempos->at(0).time == 50);

        // Test inplace shifting
        score.shift_time_inplace(-20);
        REQUIRE(score.tracks->at(0)->notes->at(0).time == 80);
        REQUIRE(score.tracks->at(0)->notes->at(1).time == 280);
        REQUIRE(score.tempos->at(0).time == -20);
    }

    SECTION("Score Pitch and Velocity Shifting") {
        Score<Tick> score(480);

        // Add a track with notes
        auto track = std::make_shared<Track<Tick>>("Piano", 0, false);
        track->notes->push_back(Note<Tick>{100, 200, 60, 80});
        track->notes->push_back(Note<Tick>{300, 200, 62, 70});
        score.tracks->push_back(track);

        // Test pitch shifting
        Score<Tick> pitched = score.shift_pitch(12);
        REQUIRE(pitched.tracks->at(0)->notes->at(0).pitch == 72);
        REQUIRE(pitched.tracks->at(0)->notes->at(1).pitch == 74);

        // Test velocity shifting
        Score<Tick> velocitied = score.shift_velocity(10);
        REQUIRE(velocitied.tracks->at(0)->notes->at(0).velocity == 90);
        REQUIRE(velocitied.tracks->at(0)->notes->at(1).velocity == 80);
    }

    SECTION("Score Note Count") {
        Score<Tick> score(480);

        // Add two tracks with notes
        auto track1 = std::make_shared<Track<Tick>>("Piano", 0, false);
        track1->notes->push_back(Note<Tick>{100, 200, 60, 80});
        track1->notes->push_back(Note<Tick>{300, 200, 62, 80});
        score.tracks->push_back(track1);

        auto track2 = std::make_shared<Track<Tick>>("Guitar", 24, false);
        track2->notes->push_back(Note<Tick>{100, 200, 48, 80});
        score.tracks->push_back(track2);

        // Test note count
        REQUIRE(score.note_num() == 3);
        REQUIRE(score.track_num() == 2);
    }
}

#endif // SYMUSIC_TEST_TRACK_SCORE_HPP

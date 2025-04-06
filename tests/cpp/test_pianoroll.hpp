#pragma once
#ifndef SYMUSIC_TEST_PIANOROLL_HPP
#define SYMUSIC_TEST_PIANOROLL_HPP

#include <vector>
#include <tuple>
#include "symusic.h"
#include "catch2/catch_test_macros.hpp"
#include "catch2/matchers/catch_matchers_floating_point.hpp"
#include <catch2/catch_all.hpp>
#include "symusic/pianoroll.h"
#include "symusic/track.h"
#include "symusic/score.h"
using namespace symusic;

/**
 * Test suite for pianoroll functionality
 * These tests verify the TrackPianoroll and ScorePianoroll classes
 */
TEST_CASE("Test TrackPianoroll", "[symusic][pianoroll]") {

    SECTION("TrackPianoroll Creation") {
        // Create a pianoroll with dimensions
        TrackPianoroll pianoroll(2, 10, 5);

        // Verify dimensions
        auto dims = pianoroll.dims();
        REQUIRE(std::get<0>(dims) == 2);  // modeDim
        REQUIRE(std::get<1>(dims) == 10); // pitchDim
        REQUIRE(std::get<2>(dims) == 5);  // timeDim
    }

    SECTION("TrackPianoroll From Track") {
        // Create a track with notes
        Track<Tick> track("Test Track", 0, false);
        track.notes->push_back(Note<Tick>(480, 240, 60, 80));  // C4
        track.notes->push_back(Note<Tick>(720, 240, 64, 80));  // E4
        track.notes->push_back(Note<Tick>(960, 240, 67, 80));  // G4

        // Define pianoroll modes
        std::vector<PianorollMode> modes = {PianorollMode::onset};

        // Define pitch range (C4 to G4+1)
        std::pair<uint8_t, uint8_t> pitchRange = {60, 68};

        // Create a pianoroll from track
        TrackPianoroll pianoroll = TrackPianoroll::from_track(track, modes, pitchRange, true);

        // Verify dimensions
        auto dims = pianoroll.dims();
        REQUIRE(std::get<0>(dims) == 1);   // modeDim (1 mode: onset)
        REQUIRE(std::get<1>(dims) == 8);   // pitchDim (8 pitches: 60-67)
        REQUIRE(std::get<2>(dims) == 1201); // timeDim (track end + 1, end is 1200)

        // Verify specific notes
        REQUIRE(pianoroll.get(0, 0, 480) == 80);  // C4 (60-60=0) at onset 480, with velocity 80
        REQUIRE(pianoroll.get(0, 4, 720) == 80);  // E4 (64-60=4) at onset 720, with velocity 80
        REQUIRE(pianoroll.get(0, 7, 960) == 80);  // G4 (67-60=7) at onset 960, with velocity 80
    }

    SECTION("Set and Get Values") {
        // Create a pianoroll
        TrackPianoroll pianoroll(1, 10, 5);

        // Set some values
        pianoroll.set(0, 5, 2, 1, 100);  // Mode 0, pitch 5, time 2, duration 1, value 100

        // Get values
        REQUIRE(pianoroll.get(0, 5, 2) == 100);
        REQUIRE(pianoroll.get(0, 5, 1) == 0);  // Should be 0 (not set)

        // Set a longer duration
        pianoroll.set(0, 3, 1, 3, 80);  // Mode 0, pitch 3, time 1, duration 3, value 80

        // Check all time steps
        REQUIRE(pianoroll.get(0, 3, 1) == 80);
        REQUIRE(pianoroll.get(0, 3, 2) == 80);
        REQUIRE(pianoroll.get(0, 3, 3) == 80);
        REQUIRE(pianoroll.get(0, 3, 4) == 0);  // Outside the duration
    }

    SECTION("Clear Pianoroll") {
        // Create a pianoroll
        TrackPianoroll pianoroll(1, 5, 5);

        // Set some values
        pianoroll.set(0, 0, 0, 1, 100);
        pianoroll.set(0, 1, 1, 1, 100);

        // Verify values are set
        REQUIRE(pianoroll.get(0, 0, 0) == 100);
        REQUIRE(pianoroll.get(0, 1, 1) == 100);

        // Clear the pianoroll
        pianoroll.clear();

        // Verify all values are cleared
        REQUIRE(pianoroll.get(0, 0, 0) == 0);
        REQUIRE(pianoroll.get(0, 1, 1) == 0);
    }

    SECTION("Data Access") {
        // Create a pianoroll
        TrackPianoroll pianoroll(1, 3, 3);

        // Set some values
        pianoroll.set(0, 1, 1, 1, 100);

        // Access data pointer
        const pianoroll_t* data = pianoroll.data();
        REQUIRE(data != nullptr);

        // Check that the data at the correct index matches the set value
        // Index calculation: mode * pitchDim * timeDim + pitch * timeDim + time
        REQUIRE(data[0 * 3 * 3 + 1 * 3 + 1] == 100);
    }

    SECTION("Release Data") {
        // Create a pianoroll
        TrackPianoroll pianoroll(1, 2, 2);

        // Set a value
        pianoroll.set(0, 0, 0, 1, 50);

        // Release data
        pianoroll_t* data = pianoroll.release();

        // Check the released data still has the value
        REQUIRE(data[0] == 50);

        // Clean up the released data
        delete[] data;
    }
}

TEST_CASE("Test ScorePianoroll", "[symusic][pianoroll]") {

    SECTION("ScorePianoroll Creation") {
        // Create a pianoroll with dimensions (1 mode, 2 tracks, 10 pitches, 5 timesteps)
        ScorePianoroll pianoroll(1, 2, 10, 5);

        // Verify dimensions
        auto dims = pianoroll.dims();
        REQUIRE(std::get<0>(dims) == 1);  // modeDim
        REQUIRE(std::get<1>(dims) == 2);  // trackDim
        REQUIRE(std::get<2>(dims) == 10); // pitchDim
        REQUIRE(std::get<3>(dims) == 5);  // timeDim
    }

    SECTION("ScorePianoroll From Score") {
        // Create a score with multiple tracks
        Score<Tick> score(480);

        // Add a bass track
        auto bass_track = std::make_shared<Track<Tick>>("Bass", 32, false);
        bass_track->notes->push_back(Note<Tick>(0, 480, 40, 80));    // E2
        bass_track->notes->push_back(Note<Tick>(480, 480, 43, 80));  // G2
        score.tracks->push_back(bass_track);

        // Add a melody track
        auto melody_track = std::make_shared<Track<Tick>>("Melody", 0, false);
        melody_track->notes->push_back(Note<Tick>(0, 240, 64, 100));   // E4
        melody_track->notes->push_back(Note<Tick>(240, 240, 67, 100)); // G4
        melody_track->notes->push_back(Note<Tick>(480, 480, 72, 100)); // C5
        score.tracks->push_back(melody_track);

        // Define pianoroll modes
        std::vector<PianorollMode> modes = {PianorollMode::onset};

        // Define pitch range (E2 to C5+1)
        std::pair<uint8_t, uint8_t> pitchRange = {40, 73};

        // Create a pianoroll from score
        ScorePianoroll pianoroll = ScorePianoroll::from_score(score, modes, pitchRange, true);

        // Verify dimensions
        auto dims = pianoroll.dims();
        REQUIRE(std::get<0>(dims) == 1);   // modeDim (1 mode: onset)
        REQUIRE(std::get<1>(dims) == 2);   // trackDim (2 tracks)
        REQUIRE(std::get<2>(dims) == 33);  // pitchDim (33 pitches: 40-72)
        REQUIRE(std::get<3>(dims) == 961); // timeDim (score end + 1)

        // Verify specific notes on track 0 (bass)
        REQUIRE(pianoroll.get(0, 0, 0, 0) == 80);     // E2 (40-40=0) at onset 0, with velocity 80
        REQUIRE(pianoroll.get(0, 0, 3, 480) == 80);   // G2 (43-40=3) at onset 480, with velocity 80

        // Verify specific notes on track 1 (melody)
        REQUIRE(pianoroll.get(0, 1, 24, 0) == 100);    // E4 (64-40=24) at onset 0, with velocity 100
        REQUIRE(pianoroll.get(0, 1, 27, 240) == 100);  // G4 (67-40=27) at onset 240, with velocity 100
        REQUIRE(pianoroll.get(0, 1, 32, 480) == 100);  // C5 (72-40=32) at onset 480, with velocity 100
    }

    SECTION("Set and Get Values") {
        // Create a pianoroll
        ScorePianoroll pianoroll(1, 2, 5, 5);

        // Set some values
        pianoroll.set(0, 1, 3, 2, 1, 100);  // Mode 0, track 1, pitch 3, time 2, duration 1, value 100

        // Get values
        REQUIRE(pianoroll.get(0, 1, 3, 2) == 100);
        REQUIRE(pianoroll.get(0, 1, 3, 1) == 0);  // Should be 0 (not set)
        REQUIRE(pianoroll.get(0, 0, 3, 2) == 0);  // Different track

        // Set a longer duration
        pianoroll.set(0, 0, 2, 1, 3, 80);  // Mode 0, track 0, pitch 2, time 1, duration 3, value 80

        // Check all time steps
        REQUIRE(pianoroll.get(0, 0, 2, 1) == 80);
        REQUIRE(pianoroll.get(0, 0, 2, 2) == 80);
        REQUIRE(pianoroll.get(0, 0, 2, 3) == 80);
        REQUIRE(pianoroll.get(0, 0, 2, 4) == 0);  // Outside the duration
    }

    SECTION("Clear Pianoroll") {
        // Create a pianoroll
        ScorePianoroll pianoroll(1, 2, 5, 5);

        // Set some values
        pianoroll.set(0, 0, 0, 0, 1, 100);
        pianoroll.set(0, 1, 1, 1, 1, 100);

        // Verify values are set
        REQUIRE(pianoroll.get(0, 0, 0, 0) == 100);
        REQUIRE(pianoroll.get(0, 1, 1, 1) == 100);

        // Clear the pianoroll
        pianoroll.clear();

        // Verify all values are cleared
        REQUIRE(pianoroll.get(0, 0, 0, 0) == 0);
        REQUIRE(pianoroll.get(0, 1, 1, 1) == 0);
    }

    SECTION("Data Access") {
        // Create a pianoroll
        ScorePianoroll pianoroll(1, 2, 3, 3);

        // Set some values
        pianoroll.set(0, 1, 1, 1, 1, 100);

        // Access data pointer
        const pianoroll_t* data = pianoroll.data();
        REQUIRE(data != nullptr);

        // Check that the data at the correct index matches the set value
        // Index calculation: mode * trackDim * pitchDim * timeDim + track * pitchDim * timeDim + pitch * timeDim + time
        REQUIRE(data[0 * 2 * 3 * 3 + 1 * 3 * 3 + 1 * 3 + 1] == 100);
    }

    SECTION("Release Data") {
        // Create a pianoroll
        ScorePianoroll pianoroll(1, 1, 2, 2);

        // Set a value
        pianoroll.set(0, 0, 0, 0, 1, 50);

        // Release data
        pianoroll_t* data = pianoroll.release();

        // Check the released data still has the value
        REQUIRE(data[0] == 50);

        // Clean up the released data
        delete[] data;
    }
}

#endif // SYMUSIC_TEST_PIANOROLL_HPP

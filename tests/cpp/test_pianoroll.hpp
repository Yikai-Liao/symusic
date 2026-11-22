#pragma once
#ifndef SYMUSIC_TEST_PIANOROLL_HPP
#define SYMUSIC_TEST_PIANOROLL_HPP

#include <catch2/catch_all.hpp>
#include "catch2/catch_test_macros.hpp"
#include "catch2/matchers/catch_matchers_floating_point.hpp"
#include <memory>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>
#include "symusic.h"
#include "symusic/score.h"
#include "symusic/pianoroll.h"
#include "symusic/track.h"
using namespace symusic;

static_assert(!std::is_copy_constructible_v<TrackPianoroll>);
static_assert(!std::is_copy_assignable_v<TrackPianoroll>);
static_assert(std::is_move_constructible_v<TrackPianoroll>);
static_assert(std::is_move_assignable_v<TrackPianoroll>);
static_assert(!std::is_copy_constructible_v<ScorePianoroll>);
static_assert(!std::is_copy_assignable_v<ScorePianoroll>);
static_assert(std::is_move_constructible_v<ScorePianoroll>);
static_assert(std::is_move_assignable_v<ScorePianoroll>);

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
        REQUIRE(std::get<0>(dims) == 2);    // modeDim
        REQUIRE(std::get<1>(dims) == 10);   // pitchDim
        REQUIRE(std::get<2>(dims) == 5);    // timeDim
    }

    SECTION("TrackPianoroll From Track") {
        // Create a track with notes
        Track<Tick> track("Test Track", 0, false);
        track.notes.push_back(Note<Tick>(480, 240, 60, 80));   // C4
        track.notes.push_back(Note<Tick>(720, 240, 64, 80));   // E4
        track.notes.push_back(Note<Tick>(960, 240, 67, 80));   // G4

        // Define pianoroll modes
        std::vector<PianorollMode> modes = {PianorollMode::onset};

        // Define pitch range (C4 to G4+1)
        std::pair<uint8_t, uint8_t> pitchRange = {60, 68};

        // Create a pianoroll from track
        TrackPianoroll pianoroll = TrackPianoroll::from_track(track, modes, pitchRange, true);

        // Verify dimensions
        auto dims = pianoroll.dims();
        REQUIRE(std::get<0>(dims) == 1);      // modeDim (1 mode: onset)
        REQUIRE(std::get<1>(dims) == 8);      // pitchDim (8 pitches: 60-67)
        REQUIRE(std::get<2>(dims) == 1201);   // timeDim (track end + 1, end is 1200)

        // Verify specific notes
        REQUIRE(pianoroll.get(0, 0, 480) == 80);   // C4 (60-60=0) at onset 480, with velocity 80
        REQUIRE(pianoroll.get(0, 4, 720) == 80);   // E4 (64-60=4) at onset 720, with velocity 80
        REQUIRE(pianoroll.get(0, 7, 960) == 80);   // G4 (67-60=7) at onset 960, with velocity 80
    }

    SECTION("Set and Get Values") {
        // Create a pianoroll
        TrackPianoroll pianoroll(1, 10, 5);

        // Set some values
        pianoroll.set(0, 5, 2, 1, 100);   // Mode 0, pitch 5, time 2, duration 1, value 100

        // Get values
        REQUIRE(pianoroll.get(0, 5, 2) == 100);
        REQUIRE(pianoroll.get(0, 5, 1) == 0);   // Should be 0 (not set)

        // Set a longer duration
        pianoroll.set(0, 3, 1, 3, 80);   // Mode 0, pitch 3, time 1, duration 3, value 80

        // Check all time steps
        REQUIRE(pianoroll.get(0, 3, 1) == 80);
        REQUIRE(pianoroll.get(0, 3, 2) == 80);
        REQUIRE(pianoroll.get(0, 3, 3) == 80);
        REQUIRE(pianoroll.get(0, 3, 4) == 0);   // Outside the duration
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
        std::unique_ptr<pianoroll_t[]> data(pianoroll.release());

        // Check the released data still has the value
        REQUIRE(data[0] == 50);

        REQUIRE_THROWS_AS(pianoroll.data(), std::runtime_error);
        REQUIRE_THROWS_AS(pianoroll.release(), std::runtime_error);
    }

    SECTION("Move Construction Transfers Ownership") {
        TrackPianoroll source(1, 2, 2);
        source.set(0, 1, 1, 1, 77);

        const pianoroll_t* original_data = source.data();
        TrackPianoroll     moved(std::move(source));

        REQUIRE(moved.data() == original_data);
        REQUIRE(moved.get(0, 1, 1) == 77);
        REQUIRE_THROWS_AS(source.data(), std::runtime_error);
        REQUIRE_THROWS_AS(source.release(), std::runtime_error);
    }

    SECTION("Move Assignment Transfers Ownership") {
        TrackPianoroll source(1, 2, 2);
        source.set(0, 0, 1, 1, 91);

        const pianoroll_t* original_data = source.data();
        TrackPianoroll     target(1, 1, 1);
        target.set(0, 0, 0, 1, 12);

        target = std::move(source);

        REQUIRE(target.data() == original_data);
        REQUIRE(target.get(0, 0, 1) == 91);
        REQUIRE_THROWS_AS(source.data(), std::runtime_error);
        REQUIRE_THROWS_AS(source.release(), std::runtime_error);
    }
}

TEST_CASE("Test ScorePianoroll", "[symusic][pianoroll]") {

    SECTION("ScorePianoroll Creation") {
        // Create a pianoroll with dimensions (1 mode, 2 tracks, 10 pitches, 5 timesteps)
        ScorePianoroll pianoroll(1, 2, 10, 5);

        // Verify dimensions
        auto dims = pianoroll.dims();
        REQUIRE(std::get<0>(dims) == 1);    // modeDim
        REQUIRE(std::get<1>(dims) == 2);    // trackDim
        REQUIRE(std::get<2>(dims) == 10);   // pitchDim
        REQUIRE(std::get<3>(dims) == 5);    // timeDim
    }

    SECTION("ScorePianoroll From Score") {
        // Create a score with multiple tracks
        Score<Tick> score(480);

        // Add a bass track
        auto bass_track = Track<Tick>("Bass", 32, false);
        bass_track.notes.push_back(Note<Tick>(0, 480, 40, 80));     // E2
        bass_track.notes.push_back(Note<Tick>(480, 480, 43, 80));   // G2
        score.tracks.push_back(bass_track);

        // Add a melody track
        auto melody_track = Track<Tick>("Melody", 0, false);
        melody_track.notes.push_back(Note<Tick>(0, 240, 64, 100));     // E4
        melody_track.notes.push_back(Note<Tick>(240, 240, 67, 100));   // G4
        melody_track.notes.push_back(Note<Tick>(480, 480, 72, 100));   // C5
        score.tracks.push_back(melody_track);

        // Define pianoroll modes
        std::vector<PianorollMode> modes = {PianorollMode::onset};

        // Define pitch range (E2 to C5+1)
        std::pair<uint8_t, uint8_t> pitchRange = {40, 73};

        // Create a pianoroll from score
        ScorePianoroll pianoroll = ScorePianoroll::from_score(score, modes, pitchRange, true);

        // Verify dimensions
        auto dims = pianoroll.dims();
        REQUIRE(std::get<0>(dims) == 1);     // modeDim (1 mode: onset)
        REQUIRE(std::get<1>(dims) == 2);     // trackDim (2 tracks)
        REQUIRE(std::get<2>(dims) == 33);    // pitchDim (33 pitches: 40-72)
        REQUIRE(std::get<3>(dims) == 961);   // timeDim (score end + 1)

        // Verify specific notes on track 0 (bass)
        REQUIRE(pianoroll.get(0, 0, 0, 0) == 80);     // E2 (40-40=0) at onset 0, with velocity 80
        REQUIRE(pianoroll.get(0, 0, 3, 480) == 80);   // G2 (43-40=3) at onset 480, with velocity 80

        // Verify specific notes on track 1 (melody)
        REQUIRE(pianoroll.get(0, 1, 24, 0) == 100);   // E4 (64-40=24) at onset 0, with velocity 100
        REQUIRE(
            pianoroll.get(0, 1, 27, 240) == 100
        );   // G4 (67-40=27) at onset 240, with velocity 100
        REQUIRE(
            pianoroll.get(0, 1, 32, 480) == 100
        );   // C5 (72-40=32) at onset 480, with velocity 100
    }

    SECTION("Set and Get Values") {
        // Create a pianoroll
        ScorePianoroll pianoroll(1, 2, 5, 5);

        // Set some values
        pianoroll.set(
            0, 1, 3, 2, 1, 100
        );   // Mode 0, track 1, pitch 3, time 2, duration 1, value 100

        // Get values
        REQUIRE(pianoroll.get(0, 1, 3, 2) == 100);
        REQUIRE(pianoroll.get(0, 1, 3, 1) == 0);   // Should be 0 (not set)
        REQUIRE(pianoroll.get(0, 0, 3, 2) == 0);   // Different track

        // Set a longer duration
        pianoroll.set(
            0, 0, 2, 1, 3, 80
        );   // Mode 0, track 0, pitch 2, time 1, duration 3, value 80

        // Check all time steps
        REQUIRE(pianoroll.get(0, 0, 2, 1) == 80);
        REQUIRE(pianoroll.get(0, 0, 2, 2) == 80);
        REQUIRE(pianoroll.get(0, 0, 2, 3) == 80);
        REQUIRE(pianoroll.get(0, 0, 2, 4) == 0);   // Outside the duration
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
        // Index calculation: mode * trackDim * pitchDim * timeDim + track * pitchDim * timeDim +
        // pitch * timeDim + time
        REQUIRE(data[0 * 2 * 3 * 3 + 1 * 3 * 3 + 1 * 3 + 1] == 100);
    }

    SECTION("Release Data") {
        // Create a pianoroll
        ScorePianoroll pianoroll(1, 1, 2, 2);

        // Set a value
        pianoroll.set(0, 0, 0, 0, 1, 50);

        // Release data
        std::unique_ptr<pianoroll_t[]> data(pianoroll.release());

        // Check the released data still has the value
        REQUIRE(data[0] == 50);

        REQUIRE_THROWS_AS(pianoroll.data(), std::runtime_error);
        REQUIRE_THROWS_AS(pianoroll.release(), std::runtime_error);
    }

    SECTION("Move Construction Transfers Ownership") {
        ScorePianoroll source(1, 1, 2, 2);
        source.set(0, 0, 1, 1, 1, 65);

        const pianoroll_t* original_data = source.data();
        ScorePianoroll     moved(std::move(source));

        REQUIRE(moved.data() == original_data);
        REQUIRE(moved.get(0, 0, 1, 1) == 65);
        REQUIRE_THROWS_AS(source.data(), std::runtime_error);
        REQUIRE_THROWS_AS(source.release(), std::runtime_error);
    }

    SECTION("Move Assignment Transfers Ownership") {
        ScorePianoroll source(1, 1, 2, 2);
        source.set(0, 0, 0, 1, 1, 33);

        const pianoroll_t* original_data = source.data();
        ScorePianoroll     target(1, 1, 1, 1);
        target.set(0, 0, 0, 0, 1, 14);

        target = std::move(source);

        REQUIRE(target.data() == original_data);
        REQUIRE(target.get(0, 0, 0, 1) == 33);
        REQUIRE_THROWS_AS(source.data(), std::runtime_error);
        REQUIRE_THROWS_AS(source.release(), std::runtime_error);
    }
}

TEST_CASE("Pianoroll rejects invalid input", "[symusic][pianoroll][bounds]") {
    const std::vector<PianorollMode>  modes = {PianorollMode::onset, PianorollMode::frame};
    const std::pair<uint8_t, uint8_t> range = {0, 128};

    auto track_with = [](Note<Tick> note) {
        Track<Tick> track("t", 0, false);
        track.notes.push_back(Note<Tick>(0, 10, 60, 80));
        track.notes.push_back(note);
        return track;
    };

    SECTION("Negative note time") {
        const auto track = track_with(Note<Tick>(-5, 10, 61, 80));
        REQUIRE_THROWS_AS(
            TrackPianoroll::from_track(track, modes, range, true), std::invalid_argument
        );
        Score<Tick> score(480);
        score.tracks.push_back(Track<Tick>(track));
        REQUIRE_THROWS_AS(
            ScorePianoroll::from_score(score, modes, range, true), std::invalid_argument
        );
    }

    SECTION("Negative note duration") {
        const auto track = track_with(Note<Tick>(20, -5, 61, 80));
        REQUIRE_THROWS_AS(
            TrackPianoroll::from_track(track, modes, range, true), std::invalid_argument
        );
        Score<Tick> score(480);
        score.tracks.push_back(Track<Tick>(track));
        REQUIRE_THROWS_AS(
            ScorePianoroll::from_score(score, modes, range, true), std::invalid_argument
        );
    }

    SECTION("Track whose end is negative") {
        Track<Tick> track("t", 0, false);
        track.notes.push_back(Note<Tick>(-20, 5, 60, 80));
        REQUIRE_THROWS_AS(
            TrackPianoroll::from_track(track, modes, range, true), std::invalid_argument
        );
    }

    SECTION("Invalid notes outside the pitch range are ignored") {
        const auto track = track_with(Note<Tick>(-5, 10, 100, 80));
        const auto roll  = TrackPianoroll::from_track(track, modes, {0, 64}, true);
        REQUIRE(roll.dims() == std::make_tuple<size_t, size_t, size_t>(2, 64, 11));
    }

    SECTION("Zero duration is valid") {
        const auto track = track_with(Note<Tick>(5, 0, 61, 80));
        auto       roll  = TrackPianoroll::from_track(
            track, {PianorollMode::frame, PianorollMode::onset}, range, true
        );
        REQUIRE(roll.get(0, 61, 5) == 0);
        REQUIRE(roll.get(1, 61, 5) == 80);
    }

    SECTION("A note ending at the largest tick fits the axis computation") {
        // Only the time-axis size is computed in 64 bits; the allocation would be huge, so
        // exercise the constructor overflow check instead.
        REQUIRE_THROWS_AS(TrackPianoroll(3, 128, static_cast<size_t>(1) << 62), std::length_error);
        REQUIRE_THROWS_AS(
            ScorePianoroll(std::numeric_limits<size_t>::max(), 2, 2, 2), std::length_error
        );
        REQUIRE_THROWS_AS(ScorePianoroll(1 << 20, 1 << 20, 1 << 20, 1 << 20), std::length_error);
    }

    SECTION("Out-of-range get, set and operator()") {
        TrackPianoroll roll(2, 3, 4);
        REQUIRE_THROWS_AS(roll.get(2, 0, 0), std::out_of_range);
        REQUIRE_THROWS_AS(roll.get(0, 3, 0), std::out_of_range);
        REQUIRE_THROWS_AS(roll.get(0, 0, 4), std::out_of_range);
        REQUIRE_THROWS_AS(roll.get(0, 0, static_cast<size_t>(-1)), std::out_of_range);
        REQUIRE_THROWS_AS(roll(0, 0, 4), std::out_of_range);
        REQUIRE_THROWS_AS(roll.set(0, 0, 3, 2, 1), std::out_of_range);
        REQUIRE_THROWS_AS(roll.set(0, 0, 0, static_cast<size_t>(-1), 1), std::out_of_range);
        REQUIRE_THROWS_AS(roll.set(0, 0, 5, 0, 1), std::out_of_range);
        REQUIRE_THROWS_AS(roll.set(2, 0, 0, 0, 1), std::out_of_range);
        roll.set(1, 2, 2, 2, 7);   // exactly reaches the end of the axis
        REQUIRE(roll.get(1, 2, 3) == 7);
        roll.set(0, 0, 4, 0, 1);   // empty span at the end is a no-op

        ScorePianoroll sroll(1, 2, 3, 4);
        REQUIRE_THROWS_AS(sroll.get(1, 0, 0, 0), std::out_of_range);
        REQUIRE_THROWS_AS(sroll.get(0, 2, 0, 0), std::out_of_range);
        REQUIRE_THROWS_AS(sroll.get(0, 0, 3, 0), std::out_of_range);
        REQUIRE_THROWS_AS(sroll.get(0, 0, 0, 4), std::out_of_range);
        REQUIRE_THROWS_AS(sroll.set(0, 0, 0, 3, 2, 1), std::out_of_range);
        REQUIRE_THROWS_AS(sroll.set(0, 0, 0, 0, static_cast<size_t>(-1), 1), std::out_of_range);
    }
}

#endif   // SYMUSIC_TEST_PIANOROLL_HPP

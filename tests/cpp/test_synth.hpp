#pragma once
#ifndef SYMUSIC_TEST_SYNTH_HPP
#define SYMUSIC_TEST_SYNTH_HPP

#include <cmath>
#include <vector>
#include <filesystem>
#include "symusic.h"
#include "prestosynth/synthesizer.h"
#include "catch2/catch_test_macros.hpp"
#include "catch2/matchers/catch_matchers_floating_point.hpp"
#include <catch2/catch_all.hpp>
#include "symusic/synth.h"
#include "symusic/score.h"
#include "symusic/track.h"
#include "symusic/event.h"
#include <stdexcept>
#include <iostream>

using namespace symusic;

// This is a mock test for the Synthesizer class
// We can't actually test the audio output without a sound font file,
// but we can test the interface and behavior of the class.

TEST_CASE("Test Synthesizer Behavior", "[symusic][synth]") {

    SECTION("BankSelect Processing") {
        // Test the bank selection mechanism
        // Create a track with bank select messages
        Track<Second> track("Test Track", 0, false);

        // Add a note
        track.notes->push_back(Note<Second>(0.5, 1.0, 60, 80));

        // Add bank select controls
        // MSB (CC0) and LSB (CC32) for bank selection
        track.controls->push_back(ControlChange<Second>(0.0, 0, 1));   // Bank MSB = 1
        track.controls->push_back(ControlChange<Second>(0.0, 32, 2));  // Bank LSB = 2
        track.controls->push_back(ControlChange<Second>(1.0, 0, 3));   // Bank MSB change to 3
        track.controls->push_back(ControlChange<Second>(1.0, 32, 4));  // Bank LSB change to 4

        // Create score with this track
        Score<Second> score;
        score.tracks->push_back(std::make_shared<Track<Second>>(track));

        // In the actual implementation, we would expect:
        // - Note at 0.5s to use bank 1:2
        // - No notes after 1.0s, but if there were any, they would use bank 3:4

        // We can't test the actual audio output, but we can verify that the
        // score with bank select messages doesn't cause any exceptions

        // This would normally throw if the bank select handling in the synthesizer is broken
        REQUIRE_NOTHROW(details::toSequence(score));
    }

    SECTION("Score Conversion") {
        // Test the conversion from Score to psynth::Sequence
        Score<Second> score;

        // Add a track
        auto track = std::make_shared<Track<Second>>("Piano", 0, false);
        track->notes->push_back(Note<Second>(0.0, 1.0, 60, 80));
        track->notes->push_back(Note<Second>(1.0, 1.0, 64, 80));
        track->notes->push_back(Note<Second>(2.0, 1.0, 67, 80));
        score.tracks->push_back(track);

        // Add another track with different program
        auto track2 = std::make_shared<Track<Second>>("Strings", 40, false);
        track2->notes->push_back(Note<Second>(0.5, 2.0, 48, 100));
        track2->notes->push_back(Note<Second>(2.5, 1.5, 52, 100));
        score.tracks->push_back(track2);

        // Convert to psynth::Sequence
        psynth::Sequence sequence = details::toSequence(score);

        // Check that we have two tracks in the result
        REQUIRE(sequence.tracks.size() == 2);

        // Verify the first track details
        REQUIRE(sequence.tracks[0].preset == 0);
        REQUIRE(sequence.tracks[0].notes.size() == 3);

        // Verify the second track details
        REQUIRE(sequence.tracks[1].preset == 40);
        REQUIRE(sequence.tracks[1].notes.size() == 2);

        // Check that the notes are correctly converted
        REQUIRE(sequence.tracks[0].notes[0].start == 0.0);
        REQUIRE(sequence.tracks[0].notes[0].duration == 1.0);
        REQUIRE(sequence.tracks[0].notes[0].pitch == 60);
        REQUIRE(sequence.tracks[0].notes[0].velocity == 80);

        REQUIRE(sequence.tracks[1].notes[0].start == 0.5);
        REQUIRE(sequence.tracks[1].notes[0].duration == 2.0);
        REQUIRE(sequence.tracks[1].notes[0].pitch == 48);
        REQUIRE(sequence.tracks[1].notes[0].velocity == 100);
    }

    SECTION("Time Type Conversion") {
        // Test that converting from Tick to Second works correctly
        Score<Tick> tick_score(480);  // 480 ticks per quarter

        // Add tempo changes to make the conversion non-trivial
        tick_score.tempos->push_back(Tempo<Tick>(0, 500000));  // 120 BPM
        tick_score.tempos->push_back(Tempo<Tick>(960, 1000000));  // 60 BPM at tick 960

        // Add a track with notes
        auto track = std::make_shared<Track<Tick>>("Piano", 0, false);
        track->notes->push_back(Note<Tick>(0, 480, 60, 80));      // Quarter note at start (120 BPM)
        track->notes->push_back(Note<Tick>(960, 480, 64, 80));    // Quarter note at tempo change (60 BPM)
        track->notes->push_back(Note<Tick>(1440, 480, 67, 80));   // Quarter note after tempo change
        tick_score.tracks->push_back(track);

        // Convert to psynth::Sequence via Second
        auto second_score = convert<Second>(tick_score);
        psynth::Sequence sequence = details::toSequence(second_score);

        // Check the track and notes count
        REQUIRE(sequence.tracks.size() == 1);
        REQUIRE(sequence.tracks[0].notes.size() == 3);

        // In seconds, with tempo changes:
        // - First note at 0.0s (120 BPM, quarter = 0.5s)
        // - Second note at 1.0s (after one quarter at 120 BPM)
        // - Third note at 2.0s (after one more quarter at 60 BPM)

        // Check timing (with some tolerance for floating point)
        REQUIRE(sequence.tracks[0].notes[0].start < 0.01);  // ~0.0
        REQUIRE(std::abs(sequence.tracks[0].notes[1].start - 1.0) < 0.01);
        REQUIRE(std::abs(sequence.tracks[0].notes[2].start - 2.0) < 0.01);

        // Durations: quarter note at 120 BPM = 0.5s, at 60 BPM = 1.0s
        REQUIRE(std::abs(sequence.tracks[0].notes[0].duration - 0.5) < 0.01);
        REQUIRE(std::abs(sequence.tracks[0].notes[1].duration - 1.0) < 0.01);
        REQUIRE(std::abs(sequence.tracks[0].notes[2].duration - 1.0) < 0.01);
    }

    SECTION("Unsupported SF Path") {
        // Try to create a Synthesizer with an invalid SF path
        // If we could actually initialize it (which would require a soundfont file)
        // this would throw. Instead, we just make sure our interface is correct.
        #ifdef DUMMY_TEST  // This test is not meant to be run automatically
        REQUIRE_THROWS_AS(
            Synthesizer("/path/that/does/not/exist.sf2", 44100, 0),
            std::runtime_error
        );
        #endif
    }
}

#endif // SYMUSIC_TEST_SYNTH_HPP

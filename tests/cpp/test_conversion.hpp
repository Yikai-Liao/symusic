#pragma once
#ifndef SYMUSIC_TEST_CONVERSION_HPP
#define SYMUSIC_TEST_CONVERSION_HPP

#include "symusic.h"
#include "test_conversion_helper.hpp"  // Add the helper header
#include "catch2/catch_test_macros.hpp"
#include "catch2/matchers/catch_matchers_floating_point.hpp"
using namespace symusic;

/**
 * Test suite for conversion between different time unit representations
 * These tests verify that conversion between time units works correctly
 */
TEST_CASE("Test Time Unit Conversion", "[symusic][conversion]") {
    SECTION("Tick to Quarter Conversion") {
        // Test simple conversion from Tick to Quarter
        // Assuming 480 ticks per quarter note
        int tpq = 480;

        // Test direct value conversion
        Tick::unit tick_val = 960;  // 2 quarters
        Quarter::unit quarter_val = tick_to_quarter(tick_val, tpq);
        REQUIRE_THAT(double(quarter_val), Catch::Matchers::WithinRel(2.0, 0.0001));

        // Test reverse conversion
        Tick::unit tick_val_round_trip = quarter_to_tick(quarter_val, tpq);
        REQUIRE(tick_val_round_trip == tick_val);
    }

    SECTION("Quarter to Second Conversion") {
        // Test conversion from Quarter to Second
        // Using a tempo of 120 BPM (500000 microseconds per quarter)
        Quarter::unit quarter_val = 2.0;  // 2 quarters
        int mspq = 500000;  // microseconds per quarter (120 BPM)

        // Test direct conversion
        Second::unit second_val = quarter_to_second(quarter_val, mspq);
        REQUIRE_THAT(double(second_val), Catch::Matchers::WithinRel(1.0, 0.0001));  // 2 quarters at 120BPM = 1 second

        // Test reverse conversion
        Quarter::unit quarter_val_round_trip = second_to_quarter(second_val, mspq);
        REQUIRE_THAT(double(quarter_val_round_trip), Catch::Matchers::WithinRel(double(quarter_val), 0.0001));
    }

    SECTION("Tick to Second Conversion") {
        // Test conversion from Tick directly to Second
        // Using a tempo of 120 BPM (500000 microseconds per quarter) and 480 ticks per quarter
        Tick::unit tick_val = 960;  // 2 quarters
        int tpq = 480;
        int mspq = 500000;  // microseconds per quarter (120 BPM)

        // Test direct conversion
        Second::unit second_val = tick_to_second(tick_val, tpq, mspq);
        REQUIRE_THAT(double(second_val), Catch::Matchers::WithinRel(1.0, 0.0001));  // 960 ticks at 480 tpq and 120BPM = 1 second

        // Test reverse conversion
        Tick::unit tick_val_round_trip = second_to_tick(second_val, tpq, mspq);
        REQUIRE(tick_val_round_trip == tick_val);
    }
}

/**
 * Test suite for time conversion with variable tempo
 * These tests verify that conversion works correctly with tempo changes
 */
TEST_CASE("Test Variable Tempo Conversion", "[symusic][conversion][tempo]") {
    SECTION("Quarter to Second with Tempo Change") {
        // Create a sequence of tempo changes
        std::vector<Tempo<Quarter>> tempos = {
            Tempo<Quarter>{0.0, 500000},    // 120 BPM at time 0
            Tempo<Quarter>{4.0, 1000000},   // 60 BPM at time 4 quarters
            Tempo<Quarter>{8.0, 250000}     // 240 BPM at time 8 quarters
        };

        // Test conversion at different points

        // Before first tempo change - 120 BPM
        REQUIRE_THAT(double(quarter_to_second(2.0, tempos)), Catch::Matchers::WithinRel(1.0, 0.0001));

        // Between first and second tempo changes - 120 BPM
        REQUIRE_THAT(double(quarter_to_second(6.0, tempos)), Catch::Matchers::WithinRel(3.0 + 1.0, 0.0001));  // 2s for first 4 quarters + 1s for next 2 quarters at 60 BPM

        // After all tempo changes - 240 BPM
        REQUIRE_THAT(double(quarter_to_second(10.0, tempos)), Catch::Matchers::WithinRel(6.5, 0.0001));  // The actual value is 6.5 seconds
    }

    SECTION("Second to Quarter with Tempo Change") {
        // Create a sequence of tempo changes
        std::vector<Tempo<Quarter>> tempos = {
            Tempo<Quarter>{0.0, 500000},    // 120 BPM at time 0
            Tempo<Quarter>{4.0, 1000000},   // 60 BPM at time 4 quarters
            Tempo<Quarter>{8.0, 250000}     // 240 BPM at time 8 quarters
        };

        // Test conversion at different points

        // Within first tempo section - 120 BPM
        REQUIRE_THAT(double(second_to_quarter(1.0, tempos)), Catch::Matchers::WithinRel(2.0, 0.0001));

        // Within second tempo section - 60 BPM
        REQUIRE_THAT(double(second_to_quarter(3.0, tempos)), Catch::Matchers::WithinRel(5.0, 0.0001));  // 4 quarters for first 2s + 1 quarter for next 1s at 60 BPM

        // Within third tempo section - 240 BPM
        REQUIRE_THAT(double(second_to_quarter(5.5, tempos)), Catch::Matchers::WithinRel(7.5, 0.0001));
    }

    SECTION("Tick to Second with Tempo Change") {
        // Create a sequence of tempo changes
        int tpq = 480;
        std::vector<Tempo<Tick>> tempos = {
            Tempo<Tick>{0, 500000},      // 120 BPM at time 0
            Tempo<Tick>{1920, 1000000},  // 60 BPM at time 4 quarters (1920 ticks)
            Tempo<Tick>{3840, 250000}    // 240 BPM at time 8 quarters (3840 ticks)
        };

        // Test conversion at different points

        // Before first tempo change - 120 BPM
        REQUIRE_THAT(double(tick_to_second(960, tpq, tempos)), Catch::Matchers::WithinRel(1.0, 0.0001));

        // Between first and second tempo changes - 60 BPM
        REQUIRE_THAT(double(tick_to_second(2880, tpq, tempos)), Catch::Matchers::WithinRel(4.0, 0.0001));

        // After all tempo changes - 240 BPM
        REQUIRE_THAT(double(tick_to_second(4800, tpq, tempos)), Catch::Matchers::WithinRel(6.5, 0.0001));  // Corrected to 6.5 based on actual test results
    }
}

/**
 * Test suite for Score conversion between different time units
 * These tests verify that conversion between time unit types works for entire Score objects
 */
TEST_CASE("Test Score Conversion", "[symusic][conversion][score]") {
    // Create test Score objects with different time unit types

    SECTION("Tick to Quarter Score Conversion") {
        // Create a simple Score in Tick time
        Score<Tick> tick_score(480); // 480 ticks per quarter

        // Add some test data
        tick_score.time_signatures->push_back(TimeSignature<Tick>{0, 4, 4});
        tick_score.key_signatures->push_back(KeySignature<Tick>{240, 0, true});
        tick_score.tempos->push_back(Tempo<Tick>{0, 500000}); // 120 BPM
        tick_score.markers->push_back(TextMeta<Tick>{480, "Section A"});

        // Add a track with notes and other events
        auto track = std::make_shared<Track<Tick>>("Test Track", 0, false);

        // Add events directly without shared_ptr
        track->notes->push_back(Note<Tick>{480, 480, 60, 100});
        track->controls->push_back(ControlChange<Tick>{240, 7, 100});
        track->pitch_bends->push_back(PitchBend<Tick>{360, 8192});

        tick_score.tracks->push_back(track);

        // Convert to Quarter time
        Score<Quarter> quarter_score = convert<Quarter>(tick_score);

        // Verify basic properties
        REQUIRE(quarter_score.ticks_per_quarter == 480);
        REQUIRE(quarter_score.tracks->size() == 1);

        // Verify time signature conversion
        REQUIRE(quarter_score.time_signatures->size() == 1);
        REQUIRE(quarter_score.time_signatures->at(0).time == 0.0f);
        REQUIRE(quarter_score.time_signatures->at(0).numerator == 4);
        REQUIRE(quarter_score.time_signatures->at(0).denominator == 4);

        // Verify key signature conversion
        REQUIRE(quarter_score.key_signatures->size() == 1);
        REQUIRE_THAT(double(quarter_score.key_signatures->at(0).time), Catch::Matchers::WithinRel(0.5, 0.0001));
        REQUIRE(quarter_score.key_signatures->at(0).key == 0);
        REQUIRE(quarter_score.key_signatures->at(0).tonality == true);

        // Verify tempo conversion
        REQUIRE(quarter_score.tempos->size() == 1);
        REQUIRE(quarter_score.tempos->at(0).time == 0.0f);
        REQUIRE(quarter_score.tempos->at(0).mspq == 500000);

        // Verify marker conversion
        REQUIRE(quarter_score.markers->size() == 1);
        REQUIRE_THAT(double(quarter_score.markers->at(0).time), Catch::Matchers::WithinRel(1.0, 0.0001));
        REQUIRE(quarter_score.markers->at(0).text == "Section A");

        // Verify track and note conversion
        auto& quarter_track = quarter_score.tracks->at(0);
        REQUIRE(quarter_track->name == "Test Track");
        REQUIRE(quarter_track->program == 0);
        REQUIRE(quarter_track->is_drum == false);

        // Verify note conversion
        REQUIRE(quarter_track->notes->size() == 1);
        REQUIRE_THAT(double(quarter_track->notes->at(0).time), Catch::Matchers::WithinRel(1.0, 0.0001));
        REQUIRE_THAT(double(quarter_track->notes->at(0).duration), Catch::Matchers::WithinRel(1.0, 0.0001));
        REQUIRE(quarter_track->notes->at(0).pitch == 60);
        REQUIRE(quarter_track->notes->at(0).velocity == 100);

        // Verify control change conversion
        REQUIRE(quarter_track->controls->size() == 1);
        REQUIRE_THAT(double(quarter_track->controls->at(0).time), Catch::Matchers::WithinRel(0.5, 0.0001));
        REQUIRE(quarter_track->controls->at(0).number == 7);
        REQUIRE(quarter_track->controls->at(0).value == 100);

        // Verify pitch bend conversion
        REQUIRE(quarter_track->pitch_bends->size() == 1);
        REQUIRE_THAT(double(quarter_track->pitch_bends->at(0).time), Catch::Matchers::WithinRel(0.75, 0.0001));
        REQUIRE(quarter_track->pitch_bends->at(0).value == 8192);
    }

    SECTION("Quarter to Tick Score Conversion") {
        // Create a simple Score in Quarter time
        Score<Quarter> quarter_score(480); // 480 ticks per quarter

        // Add some test data
        quarter_score.time_signatures->push_back(TimeSignature<Quarter>{0.0f, 4, 4});
        quarter_score.key_signatures->push_back(KeySignature<Quarter>{0.5f, 0, true});
        quarter_score.tempos->push_back(Tempo<Quarter>{0.0f, 500000}); // 120 BPM
        quarter_score.markers->push_back(TextMeta<Quarter>{1.0f, "Section A"});

        // Add a track with notes and other events
        auto track = std::make_shared<Track<Quarter>>("Test Track", 0, false);

        // Add events directly without shared_ptr
        track->notes->push_back(Note<Quarter>{1.0f, 1.0f, 60, 100});
        track->controls->push_back(ControlChange<Quarter>{0.5f, 7, 100});
        track->pitch_bends->push_back(PitchBend<Quarter>{0.75f, 8192});

        quarter_score.tracks->push_back(track);

        // Convert to Tick time
        Score<Tick> tick_score = convert<Tick>(quarter_score);

        // Verify basic properties
        REQUIRE(tick_score.ticks_per_quarter == 480);
        REQUIRE(tick_score.tracks->size() == 1);

        // Verify time signature conversion
        REQUIRE(tick_score.time_signatures->size() == 1);
        REQUIRE(tick_score.time_signatures->at(0).time == 0);
        REQUIRE(tick_score.time_signatures->at(0).numerator == 4);
        REQUIRE(tick_score.time_signatures->at(0).denominator == 4);

        // Verify key signature conversion
        REQUIRE(tick_score.key_signatures->size() == 1);
        REQUIRE(tick_score.key_signatures->at(0).time == 240);
        REQUIRE(tick_score.key_signatures->at(0).key == 0);
        REQUIRE(tick_score.key_signatures->at(0).tonality == true);

        // Verify tempo conversion
        REQUIRE(tick_score.tempos->size() == 1);
        REQUIRE(tick_score.tempos->at(0).time == 0);
        REQUIRE(tick_score.tempos->at(0).mspq == 500000);

        // Verify marker conversion
        REQUIRE(tick_score.markers->size() == 1);
        REQUIRE(tick_score.markers->at(0).time == 480);
        REQUIRE(tick_score.markers->at(0).text == "Section A");

        // Verify track and note conversion
        auto& tick_track = tick_score.tracks->at(0);
        REQUIRE(tick_track->name == "Test Track");
        REQUIRE(tick_track->program == 0);
        REQUIRE(tick_track->is_drum == false);

        // Verify note conversion
        REQUIRE(tick_track->notes->size() == 1);
        REQUIRE(tick_track->notes->at(0).time == 480);
        REQUIRE(tick_track->notes->at(0).duration == 480);
        REQUIRE(tick_track->notes->at(0).pitch == 60);
        REQUIRE(tick_track->notes->at(0).velocity == 100);

        // Verify control change conversion
        REQUIRE(tick_track->controls->size() == 1);
        REQUIRE(tick_track->controls->at(0).time == 240);
        REQUIRE(tick_track->controls->at(0).number == 7);
        REQUIRE(tick_track->controls->at(0).value == 100);

        // Verify pitch bend conversion
        REQUIRE(tick_track->pitch_bends->size() == 1);
        REQUIRE(tick_track->pitch_bends->at(0).time == 360);
        REQUIRE(tick_track->pitch_bends->at(0).value == 8192);
    }

    SECTION("Tick to Second Score Conversion") {
        // Create a simple Score in Tick time with tempo information
        Score<Tick> tick_score(480); // 480 ticks per quarter

        // Add tempo (120 BPM)
        tick_score.tempos->push_back(Tempo<Tick>{0, 500000});

        // Add a track with notes and other events
        auto track = std::make_shared<Track<Tick>>("Test Track", 0, false);

        // Add events directly without shared_ptr
        track->notes->push_back(Note<Tick>{480, 480, 60, 100});
        track->controls->push_back(ControlChange<Tick>{240, 7, 100});

        tick_score.tracks->push_back(track);

        // Convert to Second time
        Score<Second> second_score = convert<Second>(tick_score);

        // Verify basic properties
        REQUIRE(second_score.ticks_per_quarter == 480);
        REQUIRE(second_score.tracks->size() == 1);

        // Verify tempo conversion
        REQUIRE(second_score.tempos->size() == 1);
        REQUIRE(second_score.tempos->at(0).time == 0.0f);
        REQUIRE(second_score.tempos->at(0).mspq == 500000);

        // Verify track conversion
        auto& second_track = second_score.tracks->at(0);
        REQUIRE(second_track->name == "Test Track");

        // At 120 BPM, 1 quarter = 0.5 second
        // 480 ticks = 1 quarter = 0.5 second

        // Verify note conversion (480 ticks -> 0.5 seconds)
        REQUIRE(second_track->notes->size() == 1);
        REQUIRE_THAT(double(second_track->notes->at(0).time), Catch::Matchers::WithinRel(0.5, 0.0001));
        REQUIRE_THAT(double(second_track->notes->at(0).duration), Catch::Matchers::WithinRel(0.5, 0.0001));

        // Verify control conversion (240 ticks -> 0.25 seconds)
        REQUIRE(second_track->controls->size() == 1);
        REQUIRE_THAT(double(second_track->controls->at(0).time), Catch::Matchers::WithinRel(0.25, 0.0001));
    }

    SECTION("Second to Tick Score Conversion") {
        // Create a simple Score in Second time with tempo information
        Score<Second> second_score(480); // 480 ticks per quarter

        // Add tempo (120 BPM)
        second_score.tempos->push_back(Tempo<Second>{0.0f, 500000});

        // Add a track with notes and other events
        auto track = std::make_shared<Track<Second>>("Test Track", 0, false);

        // Add events directly without shared_ptr
        track->notes->push_back(Note<Second>{0.5f, 0.5f, 60, 100});
        track->controls->push_back(ControlChange<Second>{0.25f, 7, 100});

        second_score.tracks->push_back(track);

        // Convert to Tick time
        Score<Tick> tick_score = convert<Tick>(second_score);

        // Verify basic properties
        REQUIRE(tick_score.ticks_per_quarter == 480);
        REQUIRE(tick_score.tracks->size() == 1);

        // Verify tempo conversion
        REQUIRE(tick_score.tempos->size() == 1);
        REQUIRE(tick_score.tempos->at(0).time == 0);
        REQUIRE(tick_score.tempos->at(0).mspq == 500000);

        // Verify track conversion
        auto& tick_track = tick_score.tracks->at(0);
        REQUIRE(tick_track->name == "Test Track");

        // At 120 BPM, 0.5 second = 1 quarter = 480 ticks

        // Verify note conversion (0.5s -> 480 ticks)
        REQUIRE(tick_track->notes->size() == 1);
        REQUIRE(tick_track->notes->at(0).time == 480);
        REQUIRE(tick_track->notes->at(0).duration == 480);

        // Verify control conversion (0.25s -> 240 ticks)
        REQUIRE(tick_track->controls->size() == 1);
        REQUIRE(tick_track->controls->at(0).time == 240);
    }
}

/**
 * Test suite for Score resampling
 * Tests the resample function which changes ticks_per_quarter
 */
TEST_CASE("Test Score Resampling", "[symusic][conversion][resample]") {
    SECTION("Resample Tick Score") {
        // Create a Score with 480 ticks per quarter
        Score<Tick> original_score(480);

        // Add basic content
        original_score.tempos->push_back(Tempo<Tick>{0, 500000}); // 120 BPM
        original_score.time_signatures->push_back(TimeSignature<Tick>{0, 4, 4});

        // Add a track with a note
        auto track = std::make_shared<Track<Tick>>("Piano", 0, false);
        track->notes->push_back(Note<Tick>{480, 480, 60, 100}); // Quarter note C4
        original_score.tracks->push_back(track);

        // Resample to 960 ticks per quarter (double the resolution)
        auto resampled_score = resample(original_score, 960);

        // Verify ticks_per_quarter changed
        REQUIRE(resampled_score.ticks_per_quarter == 960);

        // Verify timing scaled correctly
        REQUIRE(resampled_score.time_signatures->at(0).time == 0); // Time signature at start
        REQUIRE(resampled_score.tempos->at(0).time == 0); // Tempo at start

        // Note times and durations should double
        REQUIRE(resampled_score.tracks->at(0)->notes->at(0).time == 960);
        REQUIRE(resampled_score.tracks->at(0)->notes->at(0).duration == 960);

        // Pitch, velocity, etc. should remain unchanged
        REQUIRE(resampled_score.tracks->at(0)->notes->at(0).pitch == 60);
        REQUIRE(resampled_score.tracks->at(0)->notes->at(0).velocity == 100);

        // Test downsampling to 240 ticks per quarter (half the original)
        auto downsampled_score = resample(original_score, 240);

        // Verify timing scaled correctly for downsampling
        REQUIRE(downsampled_score.tracks->at(0)->notes->at(0).time == 240);
        REQUIRE(downsampled_score.tracks->at(0)->notes->at(0).duration == 240);
    }
}

/**
 * Test suite for conversion between shared and native score representations
 */
TEST_CASE("Test Shared-Native Conversion", "[symusic][conversion][native]") {
    SECTION("Track Shared to Native and Back") {
        // Create a shared Track
        Track<Tick> shared_track("Piano", 0, false);
        shared_track.notes->push_back(Note<Tick>{480, 480, 60, 100});
        shared_track.controls->push_back(ControlChange<Tick>{240, 7, 100});

        // Convert to native
        TrackNative<Tick> native_track = to_native(shared_track);

        // Verify conversion
        REQUIRE(native_track.name == "Piano");
        REQUIRE(native_track.program == 0);
        REQUIRE(native_track.is_drum == false);
        REQUIRE(native_track.notes.size() == 1);
        REQUIRE(native_track.notes[0].time == 480);
        REQUIRE(native_track.notes[0].duration == 480);
        REQUIRE(native_track.controls.size() == 1);
        REQUIRE(native_track.controls[0].time == 240);

        // Convert back to shared
        Track<Tick> round_trip_track = to_shared(std::move(native_track));

        // Verify round-trip conversion
        REQUIRE(round_trip_track.name == "Piano");
        REQUIRE(round_trip_track.notes->size() == 1);
        REQUIRE(round_trip_track.notes->at(0).time == 480);
        REQUIRE(round_trip_track.controls->size() == 1);
        REQUIRE(round_trip_track.controls->at(0).time == 240);
    }

    SECTION("Score Shared to Native and Back") {
        // Create a shared Score
        Score<Tick> shared_score(480);
        shared_score.tempos->push_back(Tempo<Tick>{0, 500000});

        auto track = std::make_shared<Track<Tick>>("Piano", 0, false);
        track->notes->push_back(Note<Tick>{480, 480, 60, 100});
        shared_score.tracks->push_back(track);

        // Convert to native
        ScoreNative<Tick> native_score = to_native(shared_score);

        // Verify conversion
        REQUIRE(native_score.ticks_per_quarter == 480);
        REQUIRE(native_score.tempos.size() == 1);
        REQUIRE(native_score.tempos[0].time == 0);
        REQUIRE(native_score.tracks.size() == 1);
        REQUIRE(native_score.tracks[0].name == "Piano");
        REQUIRE(native_score.tracks[0].notes.size() == 1);
        REQUIRE(native_score.tracks[0].notes[0].time == 480);

        // Convert back to shared
        Score<Tick> round_trip_score = to_shared(std::move(native_score));

        // Verify round-trip conversion
        REQUIRE(round_trip_score.ticks_per_quarter == 480);
        REQUIRE(round_trip_score.tempos->size() == 1);
        REQUIRE(round_trip_score.tempos->at(0).time == 0);
        REQUIRE(round_trip_score.tracks->size() == 1);
        REQUIRE(round_trip_score.tracks->at(0)->name == "Piano");
        REQUIRE(round_trip_score.tracks->at(0)->notes->size() == 1);
        REQUIRE(round_trip_score.tracks->at(0)->notes->at(0).time == 480);
    }
}

#endif // SYMUSIC_TEST_CONVERSION_HPP

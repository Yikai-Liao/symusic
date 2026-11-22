#include <filesystem>
#include <vector>
#include <memory>

#include "catch2/catch_test_macros.hpp"
#include "fmt/core.h"

#include "symusic/score.h"
#include "symusic/event.h"

using namespace symusic;

static_assert(requires(const Score<Tick>& score, std::span<const u8> bytes) {
    Score<Tick>::from_midi(bytes);
    Score<Tick>::deserialize(bytes);
    score.to_midi();
    score.serialize();
});

TEST_CASE("Test Score class functionality", "[score]") {
    // Create a Score for testing
    Score<Tick> score(480);

    // Add tracks
    auto track1 = Track<Tick>("Piano", 0, false);
    track1.notes.push_back(detail::make_event<Note<Tick>>(0, 100, 60, 100));     // C4
    track1.notes.push_back(detail::make_event<Note<Tick>>(240, 240, 62, 100));   // D4

    auto track2 = Track<Tick>("Guitar", 1, false);
    track2.notes.push_back(detail::make_event<Note<Tick>>(0, 480, 64, 100));   // E4, long note

    score.tracks.push_back(track1);
    score.tracks.push_back(track2);

    // Add time signatures
    score.time_signatures.push_back(detail::make_event<TimeSignature<Tick>>(0, 4, 4));
    score.time_signatures.push_back(detail::make_event<TimeSignature<Tick>>(240, 3, 4));

    // Add key signatures
    score.key_signatures.push_back(detail::make_event<KeySignature<Tick>>(0, 0, 0));   // C major

    // Add tempos
    score.tempos.push_back(detail::make_event<Tempo<Tick>>(0, 500000));   // 120 bpm

    SECTION("Default constructor and initial values") {
        Score<Tick> default_score;
        REQUIRE(default_score.ticks_per_quarter == 960);
        REQUIRE(default_score.tracks.empty());
        REQUIRE(default_score.time_signatures.empty());
        REQUIRE(default_score.key_signatures.empty());
        REQUIRE(default_score.tempos.empty());
        REQUIRE(default_score.markers.empty());
        REQUIRE(default_score.empty());
    }

    SECTION("Explicit constructor with TPQ") {
        Score<Tick> tpq_score(960);
        REQUIRE(tpq_score.ticks_per_quarter == 960);
        REQUIRE(tpq_score.empty());
    }

    SECTION("Copy constructor") {
        Score<Tick> copied_score(score);
        REQUIRE(copied_score.ticks_per_quarter == score.ticks_per_quarter);
        REQUIRE(copied_score.tracks.size() == score.tracks.size());
        REQUIRE(copied_score.time_signatures.size() == score.time_signatures.size());
    }

    SECTION("Move constructor") {
        Score<Tick> original = score.copy();
        Score<Tick> moved_score(std::move(original));
        REQUIRE(moved_score.ticks_per_quarter == 480);
        REQUIRE(moved_score.tracks.size() == 2);
        // Don't check original after move, it's in an unspecified state
    }

    SECTION("empty() method") {
        REQUIRE_FALSE(score.empty());

        Score<Tick> empty_score(480);
        REQUIRE(empty_score.empty());
    }

    SECTION("sort() method") {
        // Create a new score with unsorted events
        Score<Tick> sortable(480);
        auto        track = Track<Tick>("Test", 0, false);

        // Add notes in unsorted order
        track.notes.push_back(detail::make_event<Note<Tick>>(240, 100, 60, 100));
        track.notes.push_back(detail::make_event<Note<Tick>>(0, 100, 62, 100));

        sortable.tracks.push_back(track);

        // Sort the score and store the result
        Score<Tick> sorted = sortable;
        sorted.sort();

        // Check if notes in the sorted score are in correct order
        REQUIRE(sorted.tracks.at(0).notes.at(0).time == 0);
        REQUIRE(sorted.tracks.at(0).notes.at(1).time == 240);

        // Also test inplace sorting
        sortable.sort();
        REQUIRE(sortable.tracks.at(0).notes.at(0).time == 0);
        REQUIRE(sortable.tracks.at(0).notes.at(1).time == 240);
    }

    SECTION("clip() methods") {
        // Test non-inplace clip
        Score<Tick> clipped = score.clip(200, 400, false);
        REQUIRE(clipped.tracks.at(0).notes.size() == 1);   // Only one note left (240-480)
        REQUIRE(
            clipped.tracks.at(1).notes.size() == 0
        );   // One note (0-480) clipped and removed

        // Test inplace clip
        score = score.clip(200, 400, false);
        REQUIRE(score.tracks.at(0).notes.size() == 1);
        REQUIRE(score.tracks.at(1).notes.size() == 0);
    }

    SECTION("shift_time() methods") {
        // Test non-inplace time shift
        Score<Tick> shifted = score;
        shifted.shift_time(100);
        REQUIRE(shifted.tracks.at(0).notes.at(0).time == 100);   // 0 -> 100
        REQUIRE(shifted.tracks.at(0).notes.at(1).time == 340);   // 240 -> 340

        // Test inplace time shift
        score.shift_time(100);
        REQUIRE(score.tracks.at(0).notes.at(0).time == 100);
        REQUIRE(score.tracks.at(0).notes.at(1).time == 340);
    }

    SECTION("shift_pitch() methods") {
        // Test non-inplace pitch shift
        Score<Tick> pitched = score;
        pitched.shift_pitch(12);                // Up one octave
        REQUIRE(pitched.tracks.at(0).notes.at(0).pitch == 72);   // C4 -> C5
        REQUIRE(pitched.tracks.at(0).notes.at(1).pitch == 74);   // D4 -> D5

        // Test inplace pitch shift
        score.shift_pitch(12);
        REQUIRE(score.tracks.at(0).notes.at(0).pitch == 72);
        REQUIRE(score.tracks.at(0).notes.at(1).pitch == 74);
    }

    SECTION("shift_velocity() methods") {
        // Test non-inplace velocity shift
        Score<Tick> velocitied = score;
        velocitied.shift_velocity(10);   // Increase velocity
        REQUIRE(velocitied.tracks.at(0).notes.at(0).velocity == 110);
        REQUIRE(velocitied.tracks.at(0).notes.at(1).velocity == 110);

        // Test inplace velocity shift
        score.shift_velocity(10);
        REQUIRE(score.tracks.at(0).notes.at(0).velocity == 110);
        REQUIRE(score.tracks.at(0).notes.at(1).velocity == 110);
    }

    SECTION("to_string() and summary() methods") {
        std::string str = score.to_string();
        REQUIRE_FALSE(str.empty());

        std::string summary = score.summary();
        REQUIRE_FALSE(summary.empty());
    }
}

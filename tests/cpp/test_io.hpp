#pragma once
#ifndef SYMUSIC_TEST_IO_HPP
#define SYMUSIC_TEST_IO_HPP

#include <filesystem>
#include <fstream>
#include "symusic.h"
#include "catch2/catch_test_macros.hpp"
using namespace symusic;
namespace fs = std::filesystem;

/**
 * Test suite for file I/O operations
 * These tests verify that reading and writing MIDI files works correctly
 */
TEST_CASE("Test MIDI File I/O", "[symusic][io][midi]") {
    // Create a temporary directory for test files
    fs::path temp_dir = fs::temp_directory_path() / "symusic_test";
    if (!fs::exists(temp_dir)) {
        fs::create_directory(temp_dir);
    }

    SECTION("MIDI Read/Write Roundtrip") {
        // Create a simple score with one track and a few notes
        Score<Tick> score(480);  // 480 ticks per quarter

        // Add a track with some notes
        auto track = std::make_shared<Track<Tick>>("Piano", 0, false);
        track->notes->push_back(Note<Tick>{480, 240, 60, 80});
        track->notes->push_back(Note<Tick>{720, 240, 64, 80});
        track->notes->push_back(Note<Tick>{960, 240, 67, 80});
        score.tracks->push_back(track);

        // Add tempo and time signature
        score.tempos->push_back(Tempo<Tick>{0, 500000});  // 120 BPM
        score.time_signatures->push_back(TimeSignature<Tick>{0, 4, 4});

        // Write the score to a MIDI file
        fs::path midi_path = temp_dir / "test_midi.mid";
        std::vector<uint8_t> midi_data = score.dumps<DataFormat::MIDI>();

        std::ofstream file(midi_path, std::ios::binary);
        file.write(reinterpret_cast<const char*>(midi_data.data()), midi_data.size());
        file.close();

        // Read the MIDI file back
        std::ifstream in_file(midi_path, std::ios::binary);
        std::vector<uint8_t> read_data((std::istreambuf_iterator<char>(in_file)), std::istreambuf_iterator<char>());
        in_file.close();

        Score<Tick> read_score = Score<Tick>::parse<DataFormat::MIDI>(std::span<const uint8_t>(read_data));

        // Verify the read score matches the original
        REQUIRE(read_score.ticks_per_quarter == 480);
        REQUIRE(read_score.tracks->size() == 1);
        REQUIRE(read_score.tracks->at(0)->name == "Piano");
        REQUIRE(read_score.tracks->at(0)->notes->size() == 3);
        REQUIRE(read_score.tempos->size() == 1);
        REQUIRE(read_score.tempos->at(0).mspq == 500000);
        REQUIRE(read_score.time_signatures->size() == 1);
        REQUIRE(read_score.time_signatures->at(0).numerator == 4);
        REQUIRE(read_score.time_signatures->at(0).denominator == 4);
    }

    SECTION("MIDI File with Multiple Tracks") {
        // Create a score with multiple tracks
        Score<Tick> score(480);

        // Track 1: Piano
        auto piano_track = std::make_shared<Track<Tick>>("Piano", 0, false);
        piano_track->notes->push_back(Note<Tick>{480, 240, 60, 80});
        piano_track->notes->push_back(Note<Tick>{720, 240, 64, 80});
        score.tracks->push_back(piano_track);

        // Track 2: Bass
        auto bass_track = std::make_shared<Track<Tick>>("Bass", 32, false);
        bass_track->notes->push_back(Note<Tick>{480, 480, 36, 100});
        bass_track->notes->push_back(Note<Tick>{960, 480, 43, 100});
        score.tracks->push_back(bass_track);

        // Track 3: Drums
        auto drum_track = std::make_shared<Track<Tick>>("Drums", 0, true);
        drum_track->notes->push_back(Note<Tick>{480, 120, 36, 100});  // Bass drum
        drum_track->notes->push_back(Note<Tick>{600, 120, 38, 80});   // Snare
        drum_track->notes->push_back(Note<Tick>{720, 120, 42, 60});   // Hi-hat
        score.tracks->push_back(drum_track);

        // Add tempo and time signature
        score.tempos->push_back(Tempo<Tick>{0, 500000});
        score.time_signatures->push_back(TimeSignature<Tick>{0, 4, 4});

        // Write the score to a MIDI file
        fs::path midi_path = temp_dir / "test_multitrack.mid";
        std::vector<uint8_t> midi_data = score.dumps<DataFormat::MIDI>();

        std::ofstream file(midi_path, std::ios::binary);
        file.write(reinterpret_cast<const char*>(midi_data.data()), midi_data.size());
        file.close();

        // Read the MIDI file back
        std::ifstream in_file(midi_path, std::ios::binary);
        std::vector<uint8_t> read_data((std::istreambuf_iterator<char>(in_file)), std::istreambuf_iterator<char>());
        in_file.close();

        Score<Tick> read_score = Score<Tick>::parse<DataFormat::MIDI>(std::span<const uint8_t>(read_data));

        // Verify the read score matches the original
        REQUIRE(read_score.tracks->size() == 3);

        // Check first track (Piano)
        REQUIRE(read_score.tracks->at(0)->program == 0);
        REQUIRE(read_score.tracks->at(0)->is_drum == false);
        REQUIRE(read_score.tracks->at(0)->notes->size() == 2);

        // Check second track (Bass)
        REQUIRE(read_score.tracks->at(1)->program == 32);
        REQUIRE(read_score.tracks->at(1)->is_drum == false);
        REQUIRE(read_score.tracks->at(1)->notes->size() == 2);

        // Check third track (Drums)
        REQUIRE(read_score.tracks->at(2)->is_drum == true);
        REQUIRE(read_score.tracks->at(2)->notes->size() == 3);
    }

    SECTION("MIDI File with Controller Events") {
        // Create a score with controller events
        Score<Tick> score(480);

        // Add a track with notes and controller events
        auto track = std::make_shared<Track<Tick>>("Piano", 0, false);
        track->notes->push_back(Note<Tick>{480, 240, 60, 80});

        // Add volume controller (CC 7)
        track->controls->push_back(ControlChange<Tick>{0, 7, 100});    // Initial volume
        track->controls->push_back(ControlChange<Tick>{480, 7, 80});   // Reduce volume
        track->controls->push_back(ControlChange<Tick>{720, 7, 120});  // Increase volume

        // Add pan controller (CC 10)
        track->controls->push_back(ControlChange<Tick>{0, 10, 64});     // Center pan
        track->controls->push_back(ControlChange<Tick>{480, 10, 100});  // Pan right
        track->controls->push_back(ControlChange<Tick>{720, 10, 30});   // Pan left

        score.tracks->push_back(track);

        // Write the score to a MIDI file
        fs::path midi_path = temp_dir / "test_controllers.mid";
        std::vector<uint8_t> midi_data = score.dumps<DataFormat::MIDI>();

        std::ofstream file(midi_path, std::ios::binary);
        file.write(reinterpret_cast<const char*>(midi_data.data()), midi_data.size());
        file.close();

        // Read the MIDI file back
        std::ifstream in_file(midi_path, std::ios::binary);
        std::vector<uint8_t> read_data((std::istreambuf_iterator<char>(in_file)), std::istreambuf_iterator<char>());
        in_file.close();

        Score<Tick> read_score = Score<Tick>::parse<DataFormat::MIDI>(std::span<const uint8_t>(read_data));

        // Verify the controller events were preserved
        REQUIRE(read_score.tracks->at(0)->controls->size() == 6);

        // Check for volume controllers
        int volume_controllers = 0;
        for (const auto& cc : *read_score.tracks->at(0)->controls) {
            if (cc.number == 7) volume_controllers++;
        }
        REQUIRE(volume_controllers == 3);

        // Check for pan controllers
        int pan_controllers = 0;
        for (const auto& cc : *read_score.tracks->at(0)->controls) {
            if (cc.number == 10) pan_controllers++;
        }
        REQUIRE(pan_controllers == 3);
    }

    // Clean up the temporary directory at the end of the test
    fs::remove_all(temp_dir);
}

#endif // SYMUSIC_TEST_IO_HPP

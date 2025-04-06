#include <memory>
#include <string>
#include <vector>

#include "catch2/catch_test_macros.hpp"
#include "symusic/track.h"
#include "symusic/event.h"

using namespace symusic;

TEST_CASE("Test Track class basic functionality", "[track]") {
    // Test default constructor
    Track<Tick> track;

    SECTION("Default constructor and initial values") {
        REQUIRE(track.name == "");
        REQUIRE(track.program == 0);
        REQUIRE(track.is_drum == false);
        REQUIRE(track.notes->empty());
        REQUIRE(track.controls->empty());
        REQUIRE(track.pitch_bends->empty());
        REQUIRE(track.pedals->empty());
        REQUIRE(track.lyrics->empty());
    }

    SECTION("Constructor with name, program, and is_drum") {
        Track<Tick> named_track("Piano", 1, false);
        REQUIRE(named_track.name == "Piano");
        REQUIRE(named_track.program == 1);
        REQUIRE(named_track.is_drum == false);
        REQUIRE(named_track.notes->empty());
    }

    SECTION("Copy constructor") {
        // Add some data
        track.name = "Test Track";
        track.program = 10;
        track.is_drum = true;
        track.notes->push_back(Note<Tick>(0, 100, 60, 100));

        // Test copy
        Track<Tick> copied_track(track);
        REQUIRE(copied_track.name == track.name);
        REQUIRE(copied_track.program == track.program);
        REQUIRE(copied_track.is_drum == track.is_drum);
        REQUIRE(copied_track.notes->size() == track.notes->size());
        REQUIRE(copied_track == track);
    }

    SECTION("Move constructor") {
        // Prepare data to be moved
        track.name = "Move Test";
        track.program = 20;
        track.notes->push_back(Note<Tick>(0, 100, 60, 100));

        // Record original data
        std::string orig_name = track.name;
        int orig_note_count = track.notes->size();

        // Test move constructor
        Track<Tick> moved_track(std::move(track));
        REQUIRE(moved_track.name == orig_name);
        REQUIRE(moved_track.program == 20);
        REQUIRE(moved_track.notes->size() == orig_note_count);
    }

    SECTION("Constructor with vectors of events") {
        // Create event vectors
        pyvec<Note<Tick>> notes;
        notes.push_back(Note<Tick>(0, 100, 60, 100));
        notes.push_back(Note<Tick>(100, 100, 62, 100));

        pyvec<ControlChange<Tick>> controls;
        controls.push_back(ControlChange<Tick>(0, 7, 100)); // Volume control

        pyvec<PitchBend<Tick>> pitch_bends;
        pitch_bends.push_back(PitchBend<Tick>(50, 8192)); // Center value pitch bend

        pyvec<Pedal<Tick>> pedals;
        pedals.push_back(Pedal<Tick>(0, 200)); // Hold pedal for 200 ticks

        pyvec<TextMeta<Tick>> lyrics;
        lyrics.push_back(TextMeta<Tick>(0, "La"));

        // Construct Track with vectors
        Track<Tick> event_track(
            "Event Track",
            5,
            false,
            std::move(notes),
            std::move(controls),
            std::move(pitch_bends),
            std::move(pedals),
            std::move(lyrics)
        );

        REQUIRE(event_track.name == "Event Track");
        REQUIRE(event_track.program == 5);
        REQUIRE(event_track.notes->size() == 2);
        REQUIRE(event_track.controls->size() == 1);
        REQUIRE(event_track.pitch_bends->size() == 1);
        REQUIRE(event_track.pedals->size() == 1);
        REQUIRE(event_track.lyrics->size() == 1);
    }

    SECTION("Assignment operator") {
        // Prepare data
        track.name = "Original";
        track.program = 30;
        track.notes->push_back(Note<Tick>(0, 100, 60, 100));

        // Create new Track and use assignment
        Track<Tick> assigned;
        assigned = track;

        REQUIRE(assigned.name == track.name);
        REQUIRE(assigned.program == track.program);
        REQUIRE(assigned.notes->size() == track.notes->size());
        REQUIRE(assigned == track);
    }

    SECTION("Move assignment operator") {
        // Prepare data
        track.name = "Move Assign";
        track.program = 40;
        track.notes->push_back(Note<Tick>(0, 100, 60, 100));

        // Record original values
        std::string orig_name = track.name;
        int orig_program = track.program;

        // Test move assignment
        Track<Tick> move_assigned;
        move_assigned = std::move(track);

        REQUIRE(move_assigned.name == orig_name);
        REQUIRE(move_assigned.program == orig_program);
        REQUIRE(move_assigned.notes->size() == 1);
    }
}

TEST_CASE("Test Track manipulation methods", "[track]") {
    // Create Track with data
    Track<Tick> track("Test Track", 0, false);

    // Add notes
    track.notes->push_back(Note<Tick>(0, 100, 60, 100));    // C4
    track.notes->push_back(Note<Tick>(100, 100, 62, 100));  // D4
    track.notes->push_back(Note<Tick>(200, 100, 64, 100));  // E4

    // Add controllers
    track.controls->push_back(ControlChange<Tick>(0, 7, 100));    // Volume
    track.controls->push_back(ControlChange<Tick>(100, 10, 64));  // Pan

    // Add pitch bends
    track.pitch_bends->push_back(PitchBend<Tick>(150, 8192 + 4096));  // 1/3 up bend

    // Add pedals
    track.pedals->push_back(Pedal<Tick>(0, 300));  // Pedal pressed for 300 ticks

    // Add lyrics
    track.lyrics->push_back(TextMeta<Tick>(0, "Do"));
    track.lyrics->push_back(TextMeta<Tick>(100, "Re"));
    track.lyrics->push_back(TextMeta<Tick>(200, "Mi"));

    SECTION("default_key() method") {
        auto key = track.default_key();
        REQUIRE(std::get<0>(key) == track.is_drum);
        REQUIRE(std::get<1>(key) == track.program);
        REQUIRE(std::get<2>(key) == track.name);
        REQUIRE(std::get<3>(key) == track.notes->size());
    }

    SECTION("copy() and deepcopy() methods") {
        // Test shallow copy
        Track<Tick> copied = track.copy();
        REQUIRE(copied == track);
        REQUIRE(copied.notes.get() == track.notes.get());  // Same pointer

        // Test deep copy
        Track<Tick> deep_copied = track.deepcopy();
        REQUIRE(deep_copied == track);
        REQUIRE(deep_copied.notes.get() != track.notes.get());  // Different pointer
        REQUIRE(*deep_copied.notes == *track.notes);  // Same content
    }

    SECTION("equality operators") {
        // Tracks with same content should be equal
        Track<Tick> same_track = track.copy();
        REQUIRE(same_track == track);

        // Tracks with different content should not be equal
        Track<Tick> different_track = track.copy();
        different_track.name = "Different";
        REQUIRE(different_track != track);
    }

    SECTION("start() and end() methods") {
        REQUIRE(track.start() == 0);  // First event starts at 0
        REQUIRE(track.end() == 300);  // Last ending event is pedal (0+300)
    }

    SECTION("note_num() method") {
        REQUIRE(track.note_num() == 3);  // 3 notes
    }

    SECTION("empty() method") {
        REQUIRE_FALSE(track.empty());

        Track<Tick> empty_track;
        REQUIRE(empty_track.empty());
    }

    SECTION("to_string() and summary() methods") {
        std::string str = track.to_string();
        REQUIRE_FALSE(str.empty());

        std::string summary = track.summary();
        REQUIRE_FALSE(summary.empty());
    }

    SECTION("sort() and sort_inplace() methods") {
        // Create a Track that needs sorting
        Track<Tick> unsorted("Unsorted", 0, false);
        unsorted.notes->push_back(Note<Tick>(200, 100, 60, 100));
        unsorted.notes->push_back(Note<Tick>(0, 100, 62, 100));
        unsorted.notes->push_back(Note<Tick>(100, 100, 64, 100));

        // Test non-inplace sort
        Track<Tick> sorted = unsorted.sort();
        REQUIRE(sorted.notes->at(0).time == 0);
        REQUIRE(sorted.notes->at(1).time == 100);
        REQUIRE(sorted.notes->at(2).time == 200);

        // Test inplace sort
        unsorted.sort_inplace();
        REQUIRE(unsorted.notes->at(0).time == 0);
        REQUIRE(unsorted.notes->at(1).time == 100);
        REQUIRE(unsorted.notes->at(2).time == 200);
    }

    SECTION("clip() and clip_inplace() methods") {
        // Test non-inplace clip
        Track<Tick> clipped = track.clip(50, 150, false);
        REQUIRE(clipped.notes->size() == 1);  // Only one note in range (100-200)
        REQUIRE(clipped.controls->size() == 1);  // Only one control in range (100)

        // Test inplace clip
        track.clip_inplace(50, 150, false);
        REQUIRE(track.notes->size() == 1);
        REQUIRE(track.controls->size() == 1);
    }

    SECTION("shift_time() and shift_time_inplace() methods") {
        // Test non-inplace time shift
        Track<Tick> shifted = track.shift_time(50);
        REQUIRE(shifted.notes->at(0).time == 50);  // 0 -> 50
        REQUIRE(shifted.notes->at(1).time == 150);  // 100 -> 150

        // Test inplace time shift
        track.shift_time_inplace(50);
        REQUIRE(track.notes->at(0).time == 50);
        REQUIRE(track.notes->at(1).time == 150);
    }

    SECTION("shift_pitch() and shift_pitch_inplace() methods") {
        // Test non-inplace pitch shift
        Track<Tick> pitched = track.shift_pitch(12);  // Up one octave
        REQUIRE(pitched.notes->at(0).pitch == 72);  // C4 -> C5
        REQUIRE(pitched.notes->at(1).pitch == 74);  // D4 -> D5

        // Test inplace pitch shift
        track.shift_pitch_inplace(12);
        REQUIRE(track.notes->at(0).pitch == 72);
        REQUIRE(track.notes->at(1).pitch == 74);
    }

    SECTION("shift_velocity() and shift_velocity_inplace() methods") {
        // Test non-inplace velocity shift
        Track<Tick> velocitied = track.shift_velocity(10);
        REQUIRE(velocitied.notes->at(0).velocity == 110);
        REQUIRE(velocitied.notes->at(1).velocity == 110);

        // Test inplace velocity shift
        track.shift_velocity_inplace(10);
        REQUIRE(track.notes->at(0).velocity == 110);
        REQUIRE(track.notes->at(1).velocity == 110);
    }
}

TEST_CASE("Test TrackNative class functionality", "[track][native]") {
    // Create TrackNative object
    TrackNative<Tick> track;

    SECTION("Default constructor") {
        REQUIRE(track.name.empty());
        REQUIRE(track.program == 0);
        REQUIRE(track.is_drum == false);
        REQUIRE(track.empty());
    }

    SECTION("Constructor with name, program, and is_drum") {
        TrackNative<Tick> named_track("Piano", 1, false);
        REQUIRE(named_track.name == "Piano");
        REQUIRE(named_track.program == 1);
        REQUIRE(named_track.is_drum == false);
    }

    SECTION("empty() method") {
        REQUIRE(track.empty());

        // Add data to make Track non-empty
        track.notes.push_back(Note<Tick>(0, 100, 60, 100));
        REQUIRE_FALSE(track.empty());

        // Make Track non-empty through other events
        TrackNative<Tick> track2;
        track2.controls.push_back(ControlChange<Tick>(0, 7, 100));
        REQUIRE_FALSE(track2.empty());

        TrackNative<Tick> track3;
        track3.pitch_bends.push_back(PitchBend<Tick>(0, 8192));
        REQUIRE_FALSE(track3.empty());

        TrackNative<Tick> track4;
        track4.pedals.push_back(Pedal<Tick>(0, 100));
        REQUIRE_FALSE(track4.empty());

        TrackNative<Tick> track5;
        track5.lyrics.push_back(TextMeta<Tick>(0, "Test"));
        REQUIRE_FALSE(track5.empty());
    }
}

#pragma once
#ifndef SYMUSIC_TEST_ZPP_HPP
#define SYMUSIC_TEST_ZPP_HPP

#include <cstring>
#include <new>
#include <span>
#include <string>
#include "symusic.h"
#include "catch2/catch_test_macros.hpp"
#include "catch2/matchers/catch_matchers_floating_point.hpp"
using namespace symusic;

/**
 * Test suite for ZPP serialization and deserialization
 * These tests verify that serializing and deserializing with ZPP works correctly
 */
TEST_CASE("Test ZPP Serialization", "[symusic][io][zpp]") {

    SECTION("Simple Event Serialization") {
        // Create a note event
        Note<Tick> note(480, 240, 60, 80);

        // Serialize the note
        std::vector<uint8_t> serialized = note.dumps<DataFormat::ZPP>();

        // Deserialize the note
        Note<Tick> deserialized = Note<Tick>::parse<DataFormat::ZPP>(serialized);

        // Verify the deserialized note matches the original
        REQUIRE(deserialized.time == note.time);
        REQUIRE(deserialized.duration == note.duration);
        REQUIRE(deserialized.pitch == note.pitch);
        REQUIRE(deserialized.velocity == note.velocity);
    }

    SECTION("Control Change Serialization") {
        // Create a control change event
        ControlChange<Tick> cc(240, 7, 100);   // CC 7 = volume

        // Serialize
        std::vector<uint8_t> serialized = cc.dumps<DataFormat::ZPP>();

        // Deserialize
        ControlChange<Tick> deserialized = ControlChange<Tick>::parse<DataFormat::ZPP>(serialized);

        // Verify
        REQUIRE(deserialized.time == cc.time);
        REQUIRE(deserialized.number == cc.number);
        REQUIRE(deserialized.value == cc.value);
    }

    SECTION("TextMeta Serialization") {
        // Create a text meta event
        TextMeta<Tick> text(300, "Test Text");

        // Serialize
        std::vector<uint8_t> serialized = text.dumps<DataFormat::ZPP>();

        // Deserialize
        TextMeta<Tick> deserialized = TextMeta<Tick>::parse<DataFormat::ZPP>(serialized);

        // Verify
        REQUIRE(deserialized.time == text.time);
        REQUIRE(deserialized.text == text.text);
    }

    SECTION("Track Serialization") {
        // Create a track
        TrackNative<Tick> track;
        track.name    = "Test Track";
        track.program = 0;
        track.is_drum = false;
        track.notes.push_back(Note<Tick>(480, 240, 60, 80));
        track.notes.push_back(Note<Tick>(720, 240, 64, 80));
        track.controls.push_back(ControlChange<Tick>(240, 7, 100));

        // Serialize
        std::vector<uint8_t> serialized = dumps<DataFormat::ZPP>(track);

        // Deserialize
        TrackNative<Tick> deserialized = parse<DataFormat::ZPP, TrackNative<Tick>>(serialized);

        // Verify
        REQUIRE(deserialized.name == track.name);
        REQUIRE(deserialized.program == track.program);
        REQUIRE(deserialized.is_drum == track.is_drum);
        REQUIRE(deserialized.notes.size() == track.notes.size());
        REQUIRE(deserialized.notes[0].time == track.notes[0].time);
        REQUIRE(deserialized.notes[1].time == track.notes[1].time);
        REQUIRE(deserialized.controls.size() == track.controls.size());
        REQUIRE(deserialized.controls[0].time == track.controls[0].time);
    }

    SECTION("Score Serialization") {
        // Create a score
        ScoreNative<Tick> score;
        score.ticks_per_quarter = 480;

        // Add a track
        TrackNative<Tick> track;
        track.name    = "Test Track";
        track.program = 0;
        track.is_drum = false;
        track.notes.push_back(Note<Tick>(480, 240, 60, 80));
        score.tracks.push_back(track);

        // Add tempo and time signature
        score.tempos.push_back(Tempo<Tick>(0, 500000));
        score.time_signatures.push_back(TimeSignature<Tick>(0, 4, 4));

        // Serialize
        std::vector<uint8_t> serialized = dumps<DataFormat::ZPP>(score);

        // Deserialize
        ScoreNative<Tick> deserialized = parse<DataFormat::ZPP, ScoreNative<Tick>>(serialized);

        // Verify
        REQUIRE(deserialized.ticks_per_quarter == score.ticks_per_quarter);
        REQUIRE(deserialized.tracks.size() == score.tracks.size());
        REQUIRE(deserialized.tracks[0].name == score.tracks[0].name);
        REQUIRE(deserialized.tracks[0].notes.size() == score.tracks[0].notes.size());
        REQUIRE(deserialized.tempos.size() == score.tempos.size());
        REQUIRE(deserialized.tempos[0].time == score.tempos[0].time);
        REQUIRE(deserialized.time_signatures.size() == score.time_signatures.size());
        REQUIRE(deserialized.time_signatures[0].time == score.time_signatures[0].time);
    }

    SECTION("Shared Score Serialization") {
        // Create a shared score
        Score<Tick> score(480);

        // Add a track with notes
        auto track = std::make_shared<Track<Tick>>("Test Track", 0, false);
        track->notes->push_back(Note<Tick>(480, 240, 60, 80));
        score.tracks->push_back(track);

        // Add tempo and time signature
        score.tempos->push_back(Tempo<Tick>(0, 500000));
        score.time_signatures->push_back(TimeSignature<Tick>(0, 4, 4));

        // Serialize
        std::vector<uint8_t> serialized = score.dumps<DataFormat::ZPP>();

        // Deserialize
        Score<Tick> deserialized = Score<Tick>::parse<DataFormat::ZPP>(serialized);

        // Verify
        REQUIRE(deserialized.ticks_per_quarter == score.ticks_per_quarter);
        REQUIRE(deserialized.tracks->size() == score.tracks->size());
        REQUIRE(deserialized.tracks->at(0)->name == score.tracks->at(0)->name);
        REQUIRE(deserialized.tracks->at(0)->notes->size() == score.tracks->at(0)->notes->size());
        REQUIRE(
            deserialized.tracks->at(0)->notes->at(0).time == score.tracks->at(0)->notes->at(0).time
        );
        REQUIRE(deserialized.tempos->size() == score.tempos->size());
        REQUIRE(deserialized.tempos->at(0).time == score.tempos->at(0).time);
        REQUIRE(deserialized.time_signatures->size() == score.time_signatures->size());
        REQUIRE(deserialized.time_signatures->at(0).time == score.time_signatures->at(0).time);
    }
}

namespace {

// Builds `make()` in storage and then overwrites the trailing padding with `fill`.
// Compilers often zero padding while initializing small members, so the garbage
// is written explicitly. All padded events here have 4-byte time and 1-byte
// members, so padding is the tail after the last member (`payload` bytes).
template<class E, class F>
std::vector<uint8_t> dumps_in_filled_storage(unsigned char fill, F make, size_t payload) {
    alignas(E) unsigned char storage[sizeof(E)];
    std::memset(storage, fill, sizeof(storage));
    E* event = new (storage) E(make());
    std::memset(storage + payload, fill, sizeof(E) - payload);
    auto bytes = event->template dumps<DataFormat::ZPP>();
    event->~E();
    return bytes;
}

template<class E, class F>
std::vector<uint8_t> dumps_vec_in_filled_storage(unsigned char fill, F make, size_t payload) {
    std::vector<E> events;
    events.reserve(4);
    for (int i = 0; i < 3; ++i) {
        events.push_back(make(i));
        auto* raw = reinterpret_cast<unsigned char*>(&events.back());
        std::memset(raw + payload, fill, sizeof(E) - payload);
    }
    return dumps<DataFormat::ZPP>(events);
}

}   // namespace

TEST_CASE("ZPP output does not depend on struct padding", "[symusic][io][zpp]") {
    SECTION("Padded events") {
        const auto check = [](auto make, size_t payload) {
            using E     = decltype(make());
            auto first  = dumps_in_filled_storage<E>(0xAA, make, payload);
            auto second = dumps_in_filled_storage<E>(0x55, make, payload);
            REQUIRE(first == second);
            REQUIRE(first.size() == sizeof(E));
            REQUIRE(E::template parse<DataFormat::ZPP>(first) == make());
        };
        check([] { return Note<Tick>(480, 240, 60, 80); }, 10);
        check([] { return Note<Quarter>(1.5f, 0.5f, 60, 80); }, 10);
        check([] { return Note<Second>(1.5f, 0.5f, 60, 80); }, 10);
        check([] { return ControlChange<Tick>(240, 7, 100); }, 6);
        check([] { return ControlChange<Second>(0.5f, 7, 100); }, 6);
        check([] { return TimeSignature<Tick>(0, 3, 4); }, 6);
        check([] { return TimeSignature<Quarter>(0.f, 3, 4); }, 6);
        check([] { return KeySignature<Tick>(0, -2, 1); }, 6);
        check([] { return KeySignature<Second>(0.f, -2, 1); }, 6);
    }

    SECTION("Padded events in a vector") {
        const auto make = [](int i) { return Note<Tick>(i * 10, 5, 60 + i, 80); };
        REQUIRE(
            dumps_vec_in_filled_storage<Note<Tick>>(0xAA, make, 10)
            == dumps_vec_in_filled_storage<Note<Tick>>(0x55, make, 10)
        );
    }

    SECTION("Padding bytes are zero and the layout is unchanged") {
        const auto bytes
            = dumps_in_filled_storage<Note<Tick>>(0xAA, [] { return Note<Tick>(1, 2, 3, 4); }, 10);
        REQUIRE(bytes.size() == 12);
        REQUIRE(bytes[8] == 3);
        REQUIRE(bytes[9] == 4);
        REQUIRE(bytes[10] == 0);
        REQUIRE(bytes[11] == 0);
    }

    SECTION("Images with nonzero padding still load") {
        // Pickles written before padding was zeroed carry arbitrary padding bytes.
        auto bytes = Note<Tick>(1, 2, 3, 4).dumps<DataFormat::ZPP>();
        bytes[10]  = 0xAA;
        bytes[11]  = 0x55;
        REQUIRE(Note<Tick>::parse<DataFormat::ZPP>(bytes) == Note<Tick>(1, 2, 3, 4));
    }
}

#endif   // SYMUSIC_TEST_ZPP_HPP

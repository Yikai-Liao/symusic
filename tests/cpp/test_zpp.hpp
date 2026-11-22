#pragma once
#ifndef SYMUSIC_TEST_ZPP_HPP
#define SYMUSIC_TEST_ZPP_HPP

#include <cstring>
#include <iterator>
#include <new>
#include <span>
#include <string>
#include <string_view>
#include "fmt/format.h"
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
        Note<Tick> note = detail::make_event<Note<Tick>>(480, 240, 60, 80);

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
        ControlChange<Tick> cc
            = detail::make_event<ControlChange<Tick>>(240, 7, 100);   // CC 7 = volume

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
        TextMeta<Tick> text = detail::make_event<TextMeta<Tick>>(300, "Test Text");

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
        Track<Tick> track;
        track.name    = "Test Track";
        track.program = 0;
        track.is_drum = false;
        track.notes.push_back(detail::make_event<Note<Tick>>(480, 240, 60, 80));
        track.notes.push_back(detail::make_event<Note<Tick>>(720, 240, 64, 80));
        track.controls.push_back(detail::make_event<ControlChange<Tick>>(240, 7, 100));

        // Serialize
        std::vector<uint8_t> serialized = dumps<DataFormat::ZPP>(track);

        // Deserialize
        Track<Tick> deserialized = parse<DataFormat::ZPP, Track<Tick>>(serialized);

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
        Score<Tick> score;
        score.ticks_per_quarter = 480;

        // Add a track
        Track<Tick> track;
        track.name    = "Test Track";
        track.program = 0;
        track.is_drum = false;
        track.notes.push_back(detail::make_event<Note<Tick>>(480, 240, 60, 80));
        score.tracks.push_back(track);

        // Add tempo and time signature
        score.tempos.push_back(detail::make_event<Tempo<Tick>>(0, 500000));
        score.time_signatures.push_back(detail::make_event<TimeSignature<Tick>>(0, 4, 4));

        // Serialize
        std::vector<uint8_t> serialized = dumps<DataFormat::ZPP>(score);

        // Deserialize
        Score<Tick> deserialized = parse<DataFormat::ZPP, Score<Tick>>(serialized);

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
        auto track = Track<Tick>("Test Track", 0, false);
        track.notes.push_back(detail::make_event<Note<Tick>>(480, 240, 60, 80));
        score.tracks.push_back(track);

        // Add tempo and time signature
        score.tempos.push_back(detail::make_event<Tempo<Tick>>(0, 500000));
        score.time_signatures.push_back(detail::make_event<TimeSignature<Tick>>(0, 4, 4));

        // Serialize
        std::vector<uint8_t> serialized = score.dumps<DataFormat::ZPP>();

        // Deserialize
        Score<Tick> deserialized = Score<Tick>::parse<DataFormat::ZPP>(serialized);

        // Verify
        REQUIRE(deserialized.ticks_per_quarter == score.ticks_per_quarter);
        REQUIRE(deserialized.tracks.size() == score.tracks.size());
        REQUIRE(deserialized.tracks.at(0).name == score.tracks.at(0).name);
        REQUIRE(deserialized.tracks.at(0).notes.size() == score.tracks.at(0).notes.size());
        REQUIRE(deserialized.tracks.at(0).notes.at(0).time == score.tracks.at(0).notes.at(0).time);
        REQUIRE(deserialized.tempos.size() == score.tempos.size());
        REQUIRE(deserialized.tempos.at(0).time == score.tempos.at(0).time);
        REQUIRE(deserialized.time_signatures.size() == score.time_signatures.size());
        REQUIRE(deserialized.time_signatures.at(0).time == score.time_signatures.at(0).time);
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
            REQUIRE(first.size() == 5 + sizeof(E));
            REQUIRE(E::template parse<DataFormat::ZPP>(first) == make());
        };
        check([] { return detail::make_event<Note<Tick>>(480, 240, 60, 80); }, 10);
        check([] { return detail::make_event<Note<Quarter>>(1.5f, 0.5f, 60, 80); }, 10);
        check([] { return detail::make_event<Note<Second>>(1.5f, 0.5f, 60, 80); }, 10);
        check([] { return detail::make_event<ControlChange<Tick>>(240, 7, 100); }, 6);
        check([] { return detail::make_event<ControlChange<Second>>(0.5f, 7, 100); }, 6);
        check([] { return detail::make_event<TimeSignature<Tick>>(0, 3, 4); }, 6);
        check([] { return detail::make_event<TimeSignature<Quarter>>(0.f, 3, 4); }, 6);
        check([] { return detail::make_event<KeySignature<Tick>>(0, -2, 1); }, 6);
        check([] { return detail::make_event<KeySignature<Second>>(0.f, -2, 1); }, 6);
    }

    SECTION("Padded events in a vector") {
        const auto make
            = [](int i) { return detail::make_event<Note<Tick>>(i * 10, 5, 60 + i, 80); };
        REQUIRE(
            dumps_vec_in_filled_storage<Note<Tick>>(0xAA, make, 10)
            == dumps_vec_in_filled_storage<Note<Tick>>(0x55, make, 10)
        );
    }

    SECTION("Padded events inside a track and score") {
        const auto build = [](unsigned char fill) {
            Score<Tick> score;
            score.ticks_per_quarter = 480;
            score.time_signatures.push_back(detail::make_event<TimeSignature<Tick>>(0, 3, 4));
            score.key_signatures.push_back(detail::make_event<KeySignature<Tick>>(0, -2, 1));
            auto track = Track<Tick>();
            for (int i = 0; i < 3; ++i)
                track.notes.push_back(detail::make_event<Note<Tick>>(i * 10, 5, 60 + i, 80));
            track.controls.push_back(detail::make_event<ControlChange<Tick>>(0, 7, 100));
            const auto dirty = [fill](auto& events, size_t payload) {
                for (auto& e : events) {
                    auto* raw = reinterpret_cast<unsigned char*>(&e);
                    std::memset(raw + payload, fill, sizeof(e) - payload);
                }
            };
            dirty(track.notes, 10);
            dirty(track.controls, 6);
            dirty(score.time_signatures, 6);
            dirty(score.key_signatures, 6);
            score.tracks.push_back(track);
            return score;
        };
        const auto first  = build(0xAA).dumps<DataFormat::ZPP>();
        const auto second = build(0x55).dumps<DataFormat::ZPP>();
        REQUIRE(first == second);
        REQUIRE(Score<Tick>::parse<DataFormat::ZPP>(first) == build(0x00));
    }

    SECTION("Padding bytes are zero and the layout is unchanged") {
        const auto bytes = dumps_in_filled_storage<Note<Tick>>(
            0xAA, [] { return detail::make_event<Note<Tick>>(1, 2, 3, 4); }, 10
        );
        const auto image = std::span(bytes).last(sizeof(Note<Tick>));
        REQUIRE(image[8] == 3);
        REQUIRE(image[9] == 4);
        REQUIRE(image[10] == 0);
        REQUIRE(image[11] == 0);
    }

    SECTION("Images with nonzero padding still load") {
        auto bytes = detail::make_event<Note<Tick>>(1, 2, 3, 4).dumps<DataFormat::ZPP>();
        auto image = std::span(bytes).last(sizeof(Note<Tick>));
        image[10]  = 0xAA;
        image[11]  = 0x55;
        REQUIRE(
            Note<Tick>::parse<DataFormat::ZPP>(bytes) == detail::make_event<Note<Tick>>(1, 2, 3, 4)
        );
    }
}

TEST_CASE("ZPP score bytes match the versioned layout", "[symusic][io][zpp]") {
    // A layout change must bump kZppVersion and regenerate the golden bytes.
    const auto check = []<TType T>(typename T::unit step, std::string_view golden) {
        constexpr u8 kExpectedZppVersion = 1;
        Score<T>     score(480);
        auto         track = std::make_shared<Track<T>>("Kit", 9, true);
        track->notes->emplace_back(step, 2 * step, 61, 83);
        track->controls->emplace_back(3 * step, 7, 101);
        track->pitch_bends->emplace_back(4 * step, -1234);
        track->pedals->emplace_back(5 * step, 3 * step);
        track->lyrics->emplace_back(6 * step, "la");
        score.tracks->push_back(track);
        score.time_signatures->emplace_back(7 * step, 3, 8);
        score.key_signatures->emplace_back(8 * step, -2, 1);
        score.tempos->emplace_back(9 * step, 500000);
        score.markers->emplace_back(10 * step, "A");

        const auto bytes = score.template dumps<DataFormat::ZPP>();
        REQUIRE(bytes.at(4) == kExpectedZppVersion);
        std::string hex;
        for (const auto byte : bytes) fmt::format_to(std::back_inserter(hex), "{:02x}", byte);
        REQUIRE(hex == golden);
    };

    SECTION("Tick") {
        check.template operator()<Tick>(
            120,
            "53594d5a01e001000001000000030000004b697409010100000078000000f00000003d53"
            "000001000000680100000765000001000000e00100002efbffff01000000580200006801"
            "000001000000d0020000020000006c6101000000480300000308000001000000c0030000"
            "fe010000010000003804000020a1070001000000b00400000100000041"
        );
    }
    SECTION("Quarter") {
        check.template operator()<Quarter>(
            0.25f,
            "53594d5a01e001000001000000030000004b69740901010000000000803e0000003f3d53"
            "0000010000000000403f07650000010000000000803f2efbffff010000000000a03f0000"
            "403f010000000000c03f020000006c61010000000000e03f030800000100000000000040"
            "fe010000010000000000104020a1070001000000000020400100000041"
        );
    }
    SECTION("Second") {
        check.template operator()<Second>(
            0.125f,
            "53594d5a01e001000001000000030000004b69740901010000000000003e0000803e3d53"
            "0000010000000000c03e07650000010000000000003f2efbffff010000000000203f0000"
            "c03e010000000000403f020000006c61010000000000603f03080000010000000000803f"
            "fe010000010000000000903f20a10700010000000000a03f0100000041"
        );
    }
}

#endif   // SYMUSIC_TEST_ZPP_HPP

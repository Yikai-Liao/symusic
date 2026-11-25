#pragma once
#ifndef SYMUSIC_TEST_CHECKED_TIME_HPP
#define SYMUSIC_TEST_CHECKED_TIME_HPP

#include <cmath>
#include <limits>
#include <stdexcept>

#include "symusic.h"
#include "symusic/ops.h"
#include "symusic/utils.h"
#include "catch2/catch_test_macros.hpp"
#include "catch2/catch_approx.hpp"

using namespace symusic;

TEST_CASE("Checked time helpers", "[symusic][checked_time]") {
    constexpr i32 kMax = std::numeric_limits<i32>::max();
    constexpr i32 kMin = std::numeric_limits<i32>::min();
    constexpr f64 kNan = std::numeric_limits<f64>::quiet_NaN();
    constexpr f64 kInf = std::numeric_limits<f64>::infinity();

    SECTION("narrow_i32 accepts both bounds") {
        REQUIRE(details::narrow_i32(kMax) == kMax);
        REQUIRE(details::narrow_i32(kMin) == kMin);
        REQUIRE_THROWS_AS(details::narrow_i32(static_cast<i64>(kMax) + 1), std::overflow_error);
        REQUIRE_THROWS_AS(details::narrow_i32(static_cast<i64>(kMin) - 1), std::overflow_error);
    }

    SECTION("add_time and sub_time check integer units only") {
        REQUIRE(details::add_time<i32>(kMax - 1, 1) == kMax);
        REQUIRE(details::add_time<i32>(kMin + 1, -1) == kMin);
        REQUIRE_THROWS_AS(details::add_time<i32>(kMax, 1), std::overflow_error);
        REQUIRE_THROWS_AS(details::add_time<i32>(kMin, -1), std::overflow_error);
        REQUIRE(details::sub_time<i32>(0, kMax) == -kMax);
        REQUIRE_THROWS_AS(details::sub_time<i32>(-2, kMax), std::overflow_error);
        REQUIRE_THROWS_AS(details::sub_time<i32>(kMax, -1), std::overflow_error);
        REQUIRE(details::add_time<f32>(3.0e38f, 3.0e38f) == std::numeric_limits<f32>::infinity());
        REQUIRE(details::sub_time<f32>(1.5f, 0.5f) == 1.0f);
    }

    SECTION("round_to_i32 checks finiteness and both bounds") {
        REQUIRE(details::round_to_i32(1.5) == 2);
        REQUIRE(details::round_to_i32(-1.5) == -2);
        REQUIRE(details::round_to_i32(2147483647.4) == kMax);
        REQUIRE(details::round_to_i32(-2147483648.4) == kMin);
        REQUIRE_THROWS_AS(details::round_to_i32(2147483647.6), std::overflow_error);
        REQUIRE_THROWS_AS(details::round_to_i32(-2147483648.6), std::overflow_error);
        REQUIRE_THROWS_AS(details::round_to_i32(kInf), std::overflow_error);
        REQUIRE_THROWS_AS(details::round_to_i32(-kInf), std::overflow_error);
        REQUIRE_THROWS_AS(details::round_to_i32(kNan), std::invalid_argument);
    }

    SECTION("round_to_i32 on f32") {
        REQUIRE(details::round_to_i32(1.5f) == 2);
        REQUIRE(details::round_to_i32(-2147483648.0f) == kMin);
        REQUIRE(details::round_to_i32(2147483520.0f) == 2147483520);
        REQUIRE_THROWS_AS(details::round_to_i32(2147483648.0f), std::overflow_error);
        REQUIRE_THROWS_AS(
            details::round_to_i32(std::numeric_limits<f32>::infinity()), std::overflow_error
        );
        REQUIRE_THROWS_AS(
            details::round_to_i32(std::numeric_limits<f32>::quiet_NaN()), std::invalid_argument
        );
    }

    SECTION("f64_to_i32 truncates") {
        REQUIRE(details::f64_to_i32(1.9) == 1);
        REQUIRE(details::f64_to_i32(-1.9) == -1);
        REQUIRE_THROWS_AS(details::f64_to_i32(kNan), std::invalid_argument);
        REQUIRE_THROWS_AS(details::f64_to_i32(4294967296.0), std::overflow_error);
    }
}

TEST_CASE("Tick arithmetic overflow", "[symusic][checked_time]") {
    constexpr i32 kMax = std::numeric_limits<i32>::max();
    constexpr i32 kMin = std::numeric_limits<i32>::min();

    SECTION("end() of events with duration") {
        REQUIRE(Note<Tick>(kMax - 5, 5, 60, 80).end() == kMax);
        REQUIRE_THROWS_AS(Note<Tick>(kMax - 5, 6, 60, 80).end(), std::overflow_error);
        REQUIRE_THROWS_AS(Pedal<Tick>(kMax, 1).end(), std::overflow_error);
        REQUIRE_THROWS_AS(Pedal<Tick>(kMin, -1).end(), std::overflow_error);
        // Float units do not overflow
        REQUIRE(
            Note<Quarter>(3.0e38f, 3.0e38f, 60, 80).end() == std::numeric_limits<f32>::infinity()
        );
    }

    SECTION("shift_time") {
        REQUIRE(Note<Tick>(kMax - 1, 1, 60, 80).shift_time(1).time == kMax);
        REQUIRE_THROWS_AS(Note<Tick>(kMax, 1, 60, 80).shift_time(1), std::overflow_error);
        REQUIRE_THROWS_AS(Pedal<Tick>(kMin, 1).shift_time(-1), std::overflow_error);
        REQUIRE_THROWS_AS(Tempo<Tick>(kMax, 500000).shift_time(kMax), std::overflow_error);
        Note<Tick> note(kMax, 1, 60, 80);
        REQUIRE_THROWS_AS(note.shift_time_inplace(1), std::overflow_error);
        REQUIRE(note.time == kMax);
    }

    SECTION("Track and Score end() propagate the error") {
        Track<Tick> track("t", 0, false);
        track.notes->emplace_back(kMax, 1, 60, 80);
        REQUIRE_THROWS_AS(track.end(), std::overflow_error);
        Score<Tick> score(480);
        score.tracks->push_back(std::make_shared<Track<Tick>>(track));
        REQUIRE_THROWS_AS(score.end(), std::overflow_error);
        REQUIRE_THROWS_AS(score.shift_time(1), std::overflow_error);
    }

    SECTION("clip with clip_end compares without overflowing") {
        pyvec<Note<Tick>> notes;
        notes.emplace_back(kMax - 5, 100, 60, 80);
        notes.emplace_back(kMax - 5, 5, 60, 80);
        notes.emplace_back(0, 10, 60, 80);
        ops::clip_inplace(notes, 0, kMax, true);
        auto& clipped = notes;
        REQUIRE(clipped.size() == 2);
        REQUIRE(clipped[0].time == kMax - 5);
        REQUIRE(clipped[0].duration == 5);
    }

    SECTION("trim with extreme values") {
        pyvec<Note<Tick>> notes;
        notes.emplace_back(kMax - 10, 100, 60, 80);
        ops::trim_inplace(notes, 0, kMax, 0, "retain", "truncate");
        REQUIRE(notes.size() == 1);
        REQUIRE(notes[0].duration == 10);

        pyvec<Note<Tick>> removed;
        removed.emplace_back(kMax - 10, 100, 60, 80);
        ops::trim_inplace(removed, 0, kMax);
        REQUIRE(removed.empty());

        pyvec<Note<Tick>> head;
        head.emplace_back(kMin + 10, 100, 60, 80);
        ops::trim_inplace(head, kMin + 20, kMax, 0, "truncate", "remove");
        REQUIRE(head.size() == 1);
        REQUIRE(head[0].time == kMin + 20);
        REQUIRE(head[0].duration == 90);
    }

    SECTION("adjust_time rejects overflowing results") {
        Track<Tick> track("t", 0, false);
        track.notes->emplace_back(0, 1, 60, 80);
        REQUIRE_THROWS_AS(
            ops::adjust_time_inplace(track, vec<i32>{0, 1}, vec<i32>{kMin, kMax}),
            std::overflow_error
        );

        // Events ending past the last original time are dropped, not wrapped
        Track<Tick> tail("t", 0, false);
        tail.notes->emplace_back(kMax - 1, 5, 60, 80);
        ops::adjust_time_inplace(tail, vec<i32>{0, kMax}, vec<i32>{0, kMax});
        REQUIRE(tail.notes->empty());

        // Equal original times give an infinite scale factor
        Track<Tick> flat("t", 0, false);
        flat.notes->emplace_back(0, 0, 60, 80);
        REQUIRE_THROWS_AS(
            ops::adjust_time_inplace(flat, vec<i32>{0, 0}, vec<i32>{0, 10}), std::invalid_argument
        );
    }
}

TEST_CASE("Conversion with extreme ticks", "[symusic][checked_time]") {
    constexpr i32 kMax = std::numeric_limits<i32>::max();

    SECTION("Tick to Second rejects an overflowing note end") {
        Score<Tick> score(480);
        auto        track = std::make_shared<Track<Tick>>("t", 0, false);
        track->notes->emplace_back(kMax, 1, 60, 80);
        score.tracks->push_back(track);
        REQUIRE_THROWS_AS(convert<Second>(score), std::overflow_error);
    }

    SECTION("Negative ticks use the default tempo before the first change") {
        Score<Tick> score(480);
        score.tempos->emplace_back(0, 1000000);
        auto track = std::make_shared<Track<Tick>>("t", 0, false);
        track->notes->emplace_back(-480, 480, 60, 80);
        score.tracks->push_back(track);
        const auto seconds = convert<Second>(score);
        REQUIRE(seconds.tracks->at(0)->notes->at(0).time == Catch::Approx(-0.5f));
        REQUIRE(seconds.tracks->at(0)->notes->at(0).duration == Catch::Approx(0.5f));
    }
}

TEST_CASE("Float to tick narrowing", "[symusic][checked_time]") {
    constexpr f64 kNan  = std::numeric_limits<f64>::quiet_NaN();
    constexpr f64 kInf  = std::numeric_limits<f64>::infinity();
    constexpr f32 kInff = std::numeric_limits<f32>::infinity();

    SECTION("qpm2mspq") {
        REQUIRE(Tempo<Tick>::qpm2mspq(120.0) == 500000);
        REQUIRE(Tempo<Tick>::qpm2mspq(60000000.0) == 1);
        for (const f64 qpm : {0.0, -1.0, -kInf, kInf, kNan, 1.0e300, 1.0e9}) {
            REQUIRE_THROWS_AS(Tempo<Tick>::qpm2mspq(qpm), std::invalid_argument);
        }
        for (const f64 qpm : {1.0e-3, 1.0e-300, 5.0e-324}) {
            REQUIRE_THROWS_AS(Tempo<Tick>::qpm2mspq(qpm), std::overflow_error);
        }
        Tempo<Second> tempo(0.0f, 500000);
        REQUIRE_THROWS_AS(tempo.set_qpm(kNan), std::invalid_argument);
        REQUIRE(tempo.mspq == 500000);
    }

    SECTION("Quarter to Tick") {
        Score<Quarter> score(480);
        auto           track = std::make_shared<Track<Quarter>>("t", 0, false);
        track->notes->emplace_back(1.0f, 1.0f, 60, 80);
        score.tracks->push_back(track);
        REQUIRE(convert<Tick>(score).tracks->at(0)->notes->at(0).duration == 480);
        for (const f32 bad : {1.0e10f, -1.0e10f, kInff, -kInff}) {
            track->notes->at(0).time = bad;
            REQUIRE_THROWS_AS(convert<Tick>(score), std::overflow_error);
        }
        track->notes->at(0).time = std::numeric_limits<f32>::quiet_NaN();
        REQUIRE_THROWS_AS(convert<Tick>(score), std::invalid_argument);
    }

    SECTION("Second to Tick") {
        Score<Second> score(480);
        auto          track = std::make_shared<Track<Second>>("t", 0, false);
        track->notes->emplace_back(1.0f, 1.0f, 60, 80);
        score.tracks->push_back(track);
        REQUIRE(convert<Tick>(score).tracks->at(0)->notes->at(0).time == 960);
        for (const f32 bad : {1.0e10f, -1.0e10f, kInff, -kInff}) {
            track->notes->at(0).time = bad;
            REQUIRE_THROWS_AS(convert<Tick>(score), std::overflow_error);
        }
        track->notes->at(0).time = std::numeric_limits<f32>::quiet_NaN();
        REQUIRE_THROWS_AS(convert<Tick>(score), std::invalid_argument);
    }

    SECTION("Second to Tick with a zero mspq tempo") {
        Score<Second> score(480);
        score.tempos->emplace_back(0.0f, 0);
        auto track = std::make_shared<Track<Second>>("t", 0, false);
        track->notes->emplace_back(1.0f, 1.0f, 60, 80);
        score.tracks->push_back(track);
        REQUIRE_THROWS(convert<Tick>(score));
    }

    SECTION("resample") {
        Score<Tick> score(480);
        auto        track = std::make_shared<Track<Tick>>("t", 0, false);
        track->notes->emplace_back(2, 480, 60, 80);
        score.tracks->push_back(track);
        REQUIRE(resample<Tick>(score, 960).tracks->at(0)->notes->at(0).time == 4);
        // Upscaling beyond the tick range
        track->notes->at(0).time = 1000000;
        REQUIRE_THROWS_AS(
            resample<Tick>(score, std::numeric_limits<i32>::max()), std::overflow_error
        );
        // Lower bound
        track->notes->at(0).time = -1000000;
        REQUIRE_THROWS_AS(
            resample<Tick>(score, std::numeric_limits<i32>::max()), std::overflow_error
        );
    }
}

#endif   // SYMUSIC_TEST_CHECKED_TIME_HPP

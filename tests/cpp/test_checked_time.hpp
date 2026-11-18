#pragma once
#ifndef SYMUSIC_TEST_CHECKED_TIME_HPP
#define SYMUSIC_TEST_CHECKED_TIME_HPP

#include <cmath>
#include <limits>
#include <stdexcept>

#include "symusic/utils.h"
#include "catch2/catch_test_macros.hpp"

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

    SECTION("f64_to_i32 truncates") {
        REQUIRE(details::f64_to_i32(1.9) == 1);
        REQUIRE(details::f64_to_i32(-1.9) == -1);
        REQUIRE_THROWS_AS(details::f64_to_i32(kNan), std::invalid_argument);
        REQUIRE_THROWS_AS(details::f64_to_i32(4294967296.0), std::overflow_error);
    }
}

#endif   // SYMUSIC_TEST_CHECKED_TIME_HPP

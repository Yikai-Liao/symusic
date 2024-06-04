#pragma once
#ifndef SYMUSIC_TEST_TIME_EVENTS_HPP
#define SYMUSIC_TEST_TIME_EVENTS_HPP

#include "symusic.h"
#include "catch2/catch_test_macros.hpp"
using namespace symusic;

TEST_CASE("Test Time Event", "[symusic]") {
    SECTION("Note") {
        // default constructor
        REQUIRE(Note<Tick>{} == Note<Tick>{0, 0 ,0, 0});
        REQUIRE(Note<Quarter>{} == Note<Quarter>{0, 0 ,0, 0});
        REQUIRE(Note<Second>{} == Note<Second>{0, 0 ,0, 0});
    }
}

#endif // SYMUSIC_TEST_TIME_EVENTS_HPP

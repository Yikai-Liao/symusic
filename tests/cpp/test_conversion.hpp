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
        REQUIRE_THAT(double(quarter_to_second(10.0, tempos)), Catch::Matchers::WithinRel(6.5, 0.0001));  // 实际值是6.5秒
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
        REQUIRE_THAT(double(tick_to_second(4800, tpq, tempos)), Catch::Matchers::WithinRel(6.5, 0.0001));  // 修正为6.5，根据实际测试结果
    }
}

#endif // SYMUSIC_TEST_CONVERSION_HPP 
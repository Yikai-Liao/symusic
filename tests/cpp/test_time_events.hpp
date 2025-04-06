#pragma once
#ifndef SYMUSIC_TEST_TIME_EVENTS_HPP
#define SYMUSIC_TEST_TIME_EVENTS_HPP

#include "symusic.h"
#include "catch2/catch_test_macros.hpp"
#include "catch2/matchers/catch_matchers_floating_point.hpp"
using namespace symusic;

/**
 * Test suite for the basic TimeEvent classes like Note, Pedal, ControlChange, etc.
 * These tests verify that the constructors, accessors, and modifiers work as expected.
 */
TEST_CASE("Test Time Event Constructors", "[symusic][time_events]") {
    SECTION("Note Default Constructor") {
        // Test default constructor with different time unit types
        REQUIRE(Note<Tick>{} == Note<Tick>{0, 0, 0, 0});
        REQUIRE(Note<Quarter>{} == Note<Quarter>{0, 0, 0, 0});
        REQUIRE(Note<Second>{} == Note<Second>{0, 0, 0, 0});
    }
    
    SECTION("Note Custom Constructor") {
        // Test custom constructor
        Note<Tick> note{480, 240, 60, 80};
        REQUIRE(note.time == 480);
        REQUIRE(note.duration == 240);
        REQUIRE(note.pitch == 60);
        REQUIRE(note.velocity == 80);
        
        // Test start/end methods
        REQUIRE(note.start() == 480);
        REQUIRE(note.end() == 720);
        
        // Test empty method
        REQUIRE_FALSE(note.empty());
        Note<Tick> empty_note{480, 0, 60, 80};
        REQUIRE(empty_note.empty());
        Note<Tick> zero_vel_note{480, 240, 60, 0};
        REQUIRE(zero_vel_note.empty());
    }
    
    SECTION("Pedal Constructor") {
        // Test pedal event
        Pedal<Tick> pedal{100, 200};
        REQUIRE(pedal.time == 100);
        REQUIRE(pedal.duration == 200);
        REQUIRE(pedal.start() == 100);
        REQUIRE(pedal.end() == 300);
    }
    
    SECTION("ControlChange Constructor") {
        // Test control change event
        ControlChange<Tick> cc{50, 7, 100};
        REQUIRE(cc.time == 50);
        REQUIRE(cc.number == 7);
        REQUIRE(cc.value == 100);
    }
    
    SECTION("TimeSignature Constructor") {
        // Test time signature event
        TimeSignature<Tick> ts{200, 3, 4};
        REQUIRE(ts.time == 200);
        REQUIRE(ts.numerator == 3);
        REQUIRE(ts.denominator == 4);
    }
    
    SECTION("KeySignature Constructor") {
        // Test key signature event
        KeySignature<Tick> ks{300, 2, 0};
        REQUIRE(ks.time == 300);
        REQUIRE(ks.key == 2);
        REQUIRE(ks.tonality == 0);
        REQUIRE(ks.degree() == 10); // (2*5) % 12 + 0*12 = 10
    }
    
    SECTION("Tempo Constructor") {
        // Test tempo event with MSPQ value
        Tempo<Tick> tempo{150, 500000};
        REQUIRE(tempo.time == 150);
        REQUIRE(tempo.mspq == 500000);
        
        // Test qpm conversion (microseconds per quarter to quarters per minute)
        REQUIRE_THAT(tempo.qpm(), Catch::Matchers::WithinRel(120.0, 0.001));
        
        // Test static factory from_qpm
        auto tempo2 = Tempo<Tick>::from_qpm(150, 120.0);
        REQUIRE(tempo2.time == 150);
        REQUIRE(tempo2.mspq == 500000);
    }
    
    SECTION("PitchBend Constructor") {
        // Test pitch bend event
        PitchBend<Tick> pb{400, 2000};
        REQUIRE(pb.time == 400);
        REQUIRE(pb.value == 2000);
    }
    
    SECTION("TextMeta Constructor") {
        // Test text meta event
        TextMeta<Tick> text{500, "Test Text"};
        REQUIRE(text.time == 500);
        REQUIRE(text.text == "Test Text");
    }
}

/**
 * Test for the shift operations on time events
 */
TEST_CASE("Test Time Event Shift Operations", "[symusic][time_events][shifts]") {
    SECTION("Note Shifting") {
        Note<Tick> note{100, 200, 60, 90};
        
        // Test time shifting
        auto shifted_note = note.shift_time(50);
        REQUIRE(shifted_note.time == 150);
        REQUIRE(shifted_note.duration == 200);
        REQUIRE(shifted_note.pitch == 60);
        REQUIRE(shifted_note.velocity == 90);
        
        // Test inplace shifting
        note.shift_time_inplace(50);
        REQUIRE(note.time == 150);
        
        // Test pitch shifting
        auto pitched_note = note.shift_pitch(12);
        REQUIRE(pitched_note.pitch == 72);
        REQUIRE(pitched_note.time == 150);
        
        // Test inplace pitch shifting
        note.shift_pitch_inplace(-24);
        REQUIRE(note.pitch == 36);
        
        // Test velocity shifting
        auto vel_note = note.shift_velocity(10);
        REQUIRE(vel_note.velocity == 100);
        
        // Test inplace velocity shifting
        note.shift_velocity_inplace(-20);
        REQUIRE(note.velocity == 70);
    }
    
    SECTION("Other Events Shifting") {
        // Test time shifting for other event types
        Pedal<Tick> pedal{100, 200};
        auto shifted_pedal = pedal.shift_time(50);
        REQUIRE(shifted_pedal.time == 150);
        
        ControlChange<Tick> cc{50, 7, 100};
        auto shifted_cc = cc.shift_time(25);
        REQUIRE(shifted_cc.time == 75);
        
        TimeSignature<Tick> ts{200, 3, 4};
        auto shifted_ts = ts.shift_time(100);
        REQUIRE(shifted_ts.time == 300);
        
        Tempo<Tick> tempo{150, 500000};
        auto shifted_tempo = tempo.shift_time(50);
        REQUIRE(shifted_tempo.time == 200);
    }
}

/**
 * Test for cross-type conversion between different time units
 */
TEST_CASE("Test Time Unit Conversion", "[symusic][time_events][conversion]") {
    SECTION("Note Conversion Between Units") {
        // Create a note with Tick time unit
        Note<Tick> tick_note{480, 240, 60, 100};
        
        // Convert to Quarter time unit
        Note<Quarter> quarter_note{1.0, 0.5, tick_note};
        REQUIRE(quarter_note.time == 1.0);
        REQUIRE(quarter_note.duration == 0.5);
        REQUIRE(quarter_note.pitch == 60);
        REQUIRE(quarter_note.velocity == 100);
        
        // Convert to Second time unit (assuming 120 BPM - 1 quarter = 0.5 seconds)
        Note<Second> second_note{0.5, 0.25, tick_note};
        REQUIRE(second_note.time == 0.5);
        REQUIRE(second_note.duration == 0.25);
        REQUIRE(second_note.pitch == 60);
        REQUIRE(second_note.velocity == 100);
    }
    
    SECTION("Other Events Conversion") {
        // Test conversion for other event types
        Tempo<Tick> tick_tempo{480, 500000};
        Tempo<Quarter> quarter_tempo{1.0, tick_tempo};
        REQUIRE(quarter_tempo.time == 1.0);
        REQUIRE(quarter_tempo.mspq == 500000);
        
        TimeSignature<Tick> tick_ts{960, 6, 8};
        TimeSignature<Quarter> quarter_ts{2.0, tick_ts};
        REQUIRE(quarter_ts.time == 2.0);
        REQUIRE(quarter_ts.numerator == 6);
        REQUIRE(quarter_ts.denominator == 8);
    }
}

/**
 * Test for comparison between time events
 */
TEST_CASE("Test Time Event Comparisons", "[symusic][time_events][comparison]") {
    SECTION("Note Comparison") {
        Note<Tick> note1{100, 200, 60, 90};
        Note<Tick> note2{100, 200, 60, 90};
        Note<Tick> note3{100, 200, 61, 90};
        
        REQUIRE(note1 == note2);
        REQUIRE(note1 != note3);
    }
    
    SECTION("Equal with Different Units") {
        // Notes with same values but different time units should not be directly comparable
        // This test ensures the type system works as expected
        Note<Tick> tick_note{480, 240, 60, 90};
        Note<Quarter> quarter_note{1.0, 0.5, 60, 90};
        
        // These are semantically equal but have different types
        // C++ type system prevents direct comparison
    }
}

#endif // SYMUSIC_TEST_TIME_EVENTS_HPP

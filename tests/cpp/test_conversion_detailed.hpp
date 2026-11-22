#include <memory>
#include <string>

#include "catch2/catch_test_macros.hpp"
#include "catch2/matchers/catch_matchers_all.hpp"

#include "symusic/conversion.h"
#include "symusic/track.h"
#include "symusic/score.h"
#include "symusic/event.h"

using namespace symusic;

TEST_CASE("Test conversion between different time types", "[conversion][time_unit]") {
    // Create a Score with Tick time type
    Score<Tick> tick_score(480);   // 480 ticks per quarter

    // Add a track
    auto track = Track<Tick>("Test", 0, false);

    // Add notes and other events
    track.notes.push_back(detail::make_event<Note<Tick>>(0, 480, 60, 100));     // One beat
    track.notes.push_back(detail::make_event<Note<Tick>>(480, 240, 62, 100));   // Half beat
    track.notes.push_back(detail::make_event<Note<Tick>>(720, 120, 64, 100));   // Quarter beat

    track.controls.push_back(detail::make_event<ControlChange<Tick>>(0, 7, 100));   // Volume
    track.pedals.push_back(detail::make_event<Pedal<Tick>>(0, 960));                // Two beats pedal

    tick_score.tracks.push_back(track);

    // Add global events
    tick_score.tempos.push_back(detail::make_event<Tempo<Tick>>(0, 500000));   // 120 BPM
    tick_score.time_signatures.push_back(detail::make_event<TimeSignature<Tick>>(0, 4, 4));

    SECTION("Test conversion from Tick to Quarter") {
        // Convert to Quarter time unit
        Score<Quarter> quarter_score = convert<Quarter>(tick_score);

        REQUIRE(quarter_score.tracks.size() == 1);

        // Verify time units are correctly converted
        // 480 ticks = 1 quarter
        REQUIRE(quarter_score.tracks.at(0).notes.at(0).time == 0.0f);
        REQUIRE(quarter_score.tracks.at(0).notes.at(0).duration == 1.0f);   // One beat

        REQUIRE(quarter_score.tracks.at(0).notes.at(1).time == 1.0f);
        REQUIRE(quarter_score.tracks.at(0).notes.at(1).duration == 0.5f);   // Half beat

        REQUIRE(quarter_score.tracks.at(0).notes.at(2).time == 1.5f);
        REQUIRE(quarter_score.tracks.at(0).notes.at(2).duration == 0.25f);   // Quarter beat

        // Verify other events conversion
        REQUIRE(quarter_score.tracks.at(0).pedals.at(0).time == 0.0f);
        REQUIRE(quarter_score.tracks.at(0).pedals.at(0).duration == 2.0f);   // Two beats

        // Verify global events conversion
        REQUIRE(quarter_score.tempos.at(0).time == 0.0f);
        REQUIRE(quarter_score.time_signatures.at(0).time == 0.0f);
    }

    SECTION("Test conversion from Tick to Second") {
        // Convert to Second time unit (assuming tempo is 120 BPM, each beat is 0.5 seconds)
        Score<Second> second_score = convert<Second>(tick_score);

        REQUIRE(second_score.tracks.size() == 1);

        // Verify time units are correctly converted
        // 480 ticks = 1 quarter = 0.5 seconds @120 BPM
        REQUIRE(second_score.tracks.at(0).notes.at(0).time == Catch::Approx(0.0f));
        REQUIRE(
            second_score.tracks.at(0).notes.at(0).duration == Catch::Approx(0.5f)
        );   // 0.5 seconds (one beat)

        REQUIRE(second_score.tracks.at(0).notes.at(1).time == Catch::Approx(0.5f));
        REQUIRE(
            second_score.tracks.at(0).notes.at(1).duration == Catch::Approx(0.25f)
        );   // 0.25 seconds (half beat)

        REQUIRE(second_score.tracks.at(0).notes.at(2).time == Catch::Approx(0.75f));
        REQUIRE(
            second_score.tracks.at(0).notes.at(2).duration == Catch::Approx(0.125f)
        );   // 0.125 seconds (quarter beat)

        // Verify other events conversion
        REQUIRE(second_score.tracks.at(0).pedals.at(0).time == Catch::Approx(0.0f));
        REQUIRE(
            second_score.tracks.at(0).pedals.at(0).duration == Catch::Approx(1.0f)
        );   // 1 second (two beats)

        // Verify global events conversion
        REQUIRE(second_score.tempos.at(0).time == Catch::Approx(0.0f));
        REQUIRE(second_score.time_signatures.at(0).time == Catch::Approx(0.0f));
    }

    SECTION("Test conversion from Quarter to Tick") {
        // First create a Score with Quarter time unit
        Score<Quarter> quarter_score(120);   // 120 is sampling rate, not meaningful for Quarter

        auto q_track = Track<Quarter>("Quarter Track", 0, false);
        q_track.notes.push_back(detail::make_event<Note<Quarter>>(0.0f, 1.0f, 60, 100));    // One beat
        q_track.notes.push_back(detail::make_event<Note<Quarter>>(1.0f, 0.5f, 62, 100));    // Half beat
        q_track.notes.push_back(detail::make_event<Note<Quarter>>(1.5f, 0.25f, 64, 100));   // Quarter beat

        quarter_score.tracks.push_back(q_track);
        quarter_score.tempos.push_back(Tempo<Quarter>::from_qpm(0.0f, 120.0f));   // 120 BPM

        // Convert to Tick time unit, specifying TPQ as 480
        Score<Tick> new_tick_score = resample(quarter_score, 480);

        REQUIRE(new_tick_score.ticks_per_quarter == 480);
        REQUIRE(new_tick_score.tracks.size() == 1);

        // Verify time units are correctly converted
        REQUIRE(new_tick_score.tracks.at(0).notes.at(0).time == 0);
        REQUIRE(
            new_tick_score.tracks.at(0).notes.at(0).duration == 480
        );   // One beat = 480 ticks

        REQUIRE(new_tick_score.tracks.at(0).notes.at(1).time == 480);
        REQUIRE(
            new_tick_score.tracks.at(0).notes.at(1).duration == 240
        );   // Half beat = 240 ticks

        REQUIRE(new_tick_score.tracks.at(0).notes.at(2).time == 720);
        REQUIRE(
            new_tick_score.tracks.at(0).notes.at(2).duration == 120
        );   // Quarter beat = 120 ticks
    }

    SECTION("Test conversion with minimum duration") {
        // Test conversion with minimum duration setting
        Score<Quarter> quarter_score
            = convert<Quarter>(tick_score, 0.1f);   // Minimum duration of 0.1 beats

        // Verify that notes shorter than minimum duration are adjusted
        REQUIRE(
            quarter_score.tracks.at(0).notes.at(2).duration == 0.25f
        );   // Originally 0.25 beats, not adjusted

        // Create a Score with very short notes
        Score<Tick> very_short_note_score(480);
        auto        short_track = Track<Tick>("Short", 0, false);
        short_track.notes.push_back(detail::make_event<Note<Tick>>(0, 24, 60, 100));   // 1/20 beat (24 ticks)
        very_short_note_score.tracks.push_back(short_track);

        // Convert and set minimum duration
        Score<Quarter> adjusted_score = convert<Quarter>(very_short_note_score, 0.1f);

        // Verify that short notes are adjusted to minimum duration
        REQUIRE(adjusted_score.tracks.at(0).notes.at(0).time == 0.0f);
        REQUIRE(
            adjusted_score.tracks.at(0).notes.at(0).duration == 0.1f
        );   // Adjusted to 0.1 beats
    }
}

TEST_CASE("Test low-level time conversion helpers", "[conversion][helpers]") {
    Score<Tick> tick_score(480);
    tick_score.tempos.push_back(Tempo<Tick>::from_qpm(0, 120.0));
    tick_score.tempos.push_back(Tempo<Tick>::from_qpm(360, 60.0));
    tick_score.time_signatures.push_back(detail::make_event<TimeSignature<Tick>>(0, 4, 4));
    tick_score.time_signatures.push_back(detail::make_event<TimeSignature<Tick>>(960, 3, 4));

    SECTION("Scalar and vector time conversion reuse the shared tempo map") {
        REQUIRE(convert_time<Quarter>(480, tick_score) == Catch::Approx(1.0f));
        REQUIRE(convert_time<Second>(240, tick_score) == Catch::Approx(0.25f));
        REQUIRE(convert_time<Second>(480, tick_score) == Catch::Approx(0.625f));

        const vec<Tick::unit> tick_times{0, 240, 360, 480, 960};
        const auto            second_times = convert_times<Second>(
            std::span<const Tick::unit>(tick_times.data(), tick_times.size()), tick_score
        );

        REQUIRE(second_times.size() == tick_times.size());
        REQUIRE(second_times[0] == Catch::Approx(0.0f));
        REQUIRE(second_times[1] == Catch::Approx(0.25f));
        REQUIRE(second_times[2] == Catch::Approx(0.375f));
        REQUIRE(second_times[3] == Catch::Approx(0.625f));
        REQUIRE(second_times[4] == Catch::Approx(1.625f));
    }

    SECTION("Time event conversion preserves payloads while remapping timestamps") {
        const auto quarter_time_signatures
            = convert_time_events<Quarter, TimeSignature>(tick_score.time_signatures, tick_score);
        const auto second_tempos = convert_time_events<Second, Tempo>(tick_score.tempos, tick_score);

        REQUIRE(quarter_time_signatures.size() == 2);
        REQUIRE(quarter_time_signatures[0].time == Catch::Approx(0.0f));
        REQUIRE(quarter_time_signatures[1].time == Catch::Approx(2.0f));
        REQUIRE(quarter_time_signatures[1].numerator == 3);
        REQUIRE(quarter_time_signatures[1].denominator == 4);

        REQUIRE(second_tempos.size() == 2);
        REQUIRE(second_tempos[0].time == Catch::Approx(0.0f));
        REQUIRE(second_tempos[1].time == Catch::Approx(0.375f));
        REQUIRE(second_tempos[0].mspq == Tempo<Tick>::from_qpm(0, 120.0).mspq);
        REQUIRE(second_tempos[1].mspq == Tempo<Tick>::from_qpm(0, 60.0).mspq);
    }

    SECTION("Duration event conversion handles tempo crossings and minimum durations") {
        vec<Note<Tick>> notes;
        notes.push_back(detail::make_event<Note<Tick>>(240, 240, 60, 100));
        notes.push_back(detail::make_event<Note<Tick>>(720, 30, 62, 90));

        const auto second_notes = convert_duration_events<Second, Note>(notes, tick_score);
        REQUIRE(second_notes.size() == 2);
        REQUIRE(second_notes[0].time == Catch::Approx(0.25f));
        REQUIRE(second_notes[0].duration == Catch::Approx(0.375f));
        REQUIRE(second_notes[1].time == Catch::Approx(1.125f));
        REQUIRE(second_notes[1].duration == Catch::Approx(0.0625f));

        const auto quarter_notes = convert_duration_events<Quarter, Note>(notes, tick_score, 0.1f);
        REQUIRE(quarter_notes.size() == 2);
        REQUIRE(quarter_notes[0].time == Catch::Approx(0.5f));
        REQUIRE(quarter_notes[0].duration == Catch::Approx(0.5f));
        REQUIRE(quarter_notes[1].time == Catch::Approx(1.5f));
        REQUIRE(quarter_notes[1].duration == Catch::Approx(0.1f));
    }
}

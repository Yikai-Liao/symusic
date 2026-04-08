#include <memory>
#include <string>

#include "catch2/catch_test_macros.hpp"
#include "catch2/matchers/catch_matchers_all.hpp"

#include "symusic/conversion.h"
#include "symusic/track.h"
#include "symusic/score.h"
#include "symusic/event.h"

using namespace symusic;

TEST_CASE("Test Track conversion between native and shared", "[conversion][track]") {
    // Create a complete TrackNative object
    TrackNative<Tick> native_track("Test Track", 10, false);

    // Add various events
    native_track.notes.push_back(Note<Tick>(0, 100, 60, 100));
    native_track.notes.push_back(Note<Tick>(100, 100, 62, 100));

    native_track.controls.push_back(ControlChange<Tick>(0, 7, 100));
    native_track.controls.push_back(ControlChange<Tick>(50, 10, 64));

    native_track.pitch_bends.push_back(PitchBend<Tick>(25, 8192));
    native_track.pitch_bends.push_back(PitchBend<Tick>(75, 12288));

    native_track.pedals.push_back(Pedal<Tick>(0, 200));

    native_track.lyrics.push_back(TextMeta<Tick>(0, "Do"));
    native_track.lyrics.push_back(TextMeta<Tick>(100, "Re"));

    SECTION("Test TrackNative to Track conversion") {
        // Convert to shared version
        Track<Tick> shared_track = to_shared(std::move(native_track));

        REQUIRE(shared_track.name == "Test Track");
        REQUIRE(shared_track.program == 10);
        REQUIRE(shared_track.is_drum == false);

        // Verify that all events are correctly converted
        REQUIRE(shared_track.notes->size() == 2);
        REQUIRE(shared_track.controls->size() == 2);
        REQUIRE(shared_track.pitch_bends->size() == 2);
        REQUIRE(shared_track.pedals->size() == 1);
        REQUIRE(shared_track.lyrics->size() == 2);

        // Verify specific event content
        REQUIRE(shared_track.notes->at(0).time == 0);
        REQUIRE(shared_track.notes->at(0).duration == 100);
        REQUIRE(shared_track.notes->at(0).pitch == 60);

        REQUIRE(shared_track.controls->at(1).time == 50);
        REQUIRE(shared_track.controls->at(1).number == 10);

        REQUIRE(shared_track.pitch_bends->at(0).time == 25);
        REQUIRE(shared_track.pitch_bends->at(0).value == 8192);

        REQUIRE(shared_track.pedals->at(0).time == 0);
        REQUIRE(shared_track.pedals->at(0).duration == 200);

        REQUIRE(shared_track.lyrics->at(0).time == 0);
        REQUIRE(shared_track.lyrics->at(0).text == "Do");
        REQUIRE(shared_track.lyrics->at(1).text == "Re");
    }

    SECTION("Test Track to TrackNative conversion") {
        // First create a Track object
        Track<Tick> shared_track("Shared Track", 20, true);

        shared_track.notes->push_back(Note<Tick>(0, 100, 60, 100));
        shared_track.controls->push_back(ControlChange<Tick>(50, 7, 120));
        shared_track.pitch_bends->push_back(PitchBend<Tick>(75, 4096));
        shared_track.pedals->push_back(Pedal<Tick>(25, 150));
        shared_track.lyrics->push_back(TextMeta<Tick>(10, "Mi"));

        // Convert to native version
        TrackNative<Tick> native = to_native(shared_track);

        REQUIRE(native.name == "Shared Track");
        REQUIRE(native.program == 20);
        REQUIRE(native.is_drum == true);

        // Verify that all events are correctly converted
        REQUIRE(native.notes.size() == 1);
        REQUIRE(native.controls.size() == 1);
        REQUIRE(native.pitch_bends.size() == 1);
        REQUIRE(native.pedals.size() == 1);
        REQUIRE(native.lyrics.size() == 1);

        // Verify specific event content
        REQUIRE(native.notes[0].time == 0);
        REQUIRE(native.notes[0].duration == 100);
        REQUIRE(native.notes[0].pitch == 60);

        REQUIRE(native.controls[0].time == 50);
        REQUIRE(native.controls[0].number == 7);
        REQUIRE(native.controls[0].value == 120);

        REQUIRE(native.pitch_bends[0].time == 75);
        REQUIRE(native.pitch_bends[0].value == 4096);

        REQUIRE(native.pedals[0].time == 25);
        REQUIRE(native.pedals[0].duration == 150);

        REQUIRE(native.lyrics[0].time == 10);
        REQUIRE(native.lyrics[0].text == "Mi");
    }
}

TEST_CASE("Test Score conversion between native and shared", "[conversion][score]") {
    // Create a complete ScoreNative object
    ScoreNative<Tick> native_score(480);

    // Add tracks
    TrackNative<Tick> track1("Piano", 0, false);
    track1.notes.push_back(Note<Tick>(0, 240, 60, 100));
    track1.notes.push_back(Note<Tick>(240, 240, 62, 100));

    TrackNative<Tick> track2("Drums", 9, true);
    track2.notes.push_back(Note<Tick>(0, 120, 36, 100));  // Bass drum
    track2.notes.push_back(Note<Tick>(120, 120, 42, 80)); // Closed hi-hat

    native_score.tracks.push_back(std::move(track1));
    native_score.tracks.push_back(std::move(track2));

    // Add other global events
    native_score.tempos.push_back(Tempo<Tick>(0, 500000));  // 120 BPM
    native_score.time_signatures.push_back(TimeSignature<Tick>(0, 4, 4));
    native_score.key_signatures.push_back(KeySignature<Tick>(0, 0, 0));  // C major
    native_score.markers.push_back(TextMeta<Tick>(0, "Start"));

    SECTION("Test ScoreNative to Score conversion") {
        // Convert to shared version
        Score<Tick> shared_score = to_shared(std::move(native_score));

        REQUIRE(shared_score.ticks_per_quarter == 480);

        // Verify tracks
        REQUIRE(shared_score.tracks->size() == 2);
        REQUIRE(shared_score.tracks->at(0)->name == "Piano");
        REQUIRE(shared_score.tracks->at(0)->program == 0);
        REQUIRE(shared_score.tracks->at(0)->is_drum == false);
        REQUIRE(shared_score.tracks->at(0)->notes->size() == 2);

        REQUIRE(shared_score.tracks->at(1)->name == "Drums");
        REQUIRE(shared_score.tracks->at(1)->program == 9);
        REQUIRE(shared_score.tracks->at(1)->is_drum == true);
        REQUIRE(shared_score.tracks->at(1)->notes->size() == 2);

        // Verify global events
        REQUIRE(shared_score.tempos->size() == 1);
        REQUIRE(shared_score.tempos->at(0).time == 0);
        REQUIRE(shared_score.tempos->at(0).mspq == 500000);

        REQUIRE(shared_score.time_signatures->size() == 1);
        REQUIRE(shared_score.time_signatures->at(0).time == 0);
        REQUIRE(shared_score.time_signatures->at(0).numerator == 4);
        REQUIRE(shared_score.time_signatures->at(0).denominator == 4);

        REQUIRE(shared_score.key_signatures->size() == 1);
        REQUIRE(shared_score.key_signatures->at(0).time == 0);
        REQUIRE(shared_score.key_signatures->at(0).key == 0);
        REQUIRE(shared_score.key_signatures->at(0).tonality == 0);

        REQUIRE(shared_score.markers->size() == 1);
        REQUIRE(shared_score.markers->at(0).time == 0);
        REQUIRE(shared_score.markers->at(0).text == "Start");
    }

    SECTION("Test Score to ScoreNative conversion") {
        // First create a complete Score object
        Score<Tick> shared_score(960);

        // Add track
        auto track = std::make_shared<Track<Tick>>("Guitar", 25, false);
        track->notes->push_back(Note<Tick>(0, 480, 64, 100));    // E4
        track->notes->push_back(Note<Tick>(480, 480, 67, 100));  // G4
        shared_score.tracks->push_back(track);

        // Add other global events
        shared_score.tempos->push_back(Tempo<Tick>(0, 400000));  // 150 BPM
        shared_score.time_signatures->push_back(TimeSignature<Tick>(0, 3, 4));  // 3/4 time
        shared_score.key_signatures->push_back(KeySignature<Tick>(0, 1, 0));    // G major
        shared_score.markers->push_back(TextMeta<Tick>(0, "Intro"));

        // Convert to native version
        ScoreNative<Tick> native = to_native(shared_score);

        REQUIRE(native.ticks_per_quarter == 960);

        // Verify tracks
        REQUIRE(native.tracks.size() == 1);
        REQUIRE(native.tracks[0].name == "Guitar");
        REQUIRE(native.tracks[0].program == 25);
        REQUIRE(native.tracks[0].is_drum == false);
        REQUIRE(native.tracks[0].notes.size() == 2);
        REQUIRE(native.tracks[0].notes[0].time == 0);
        REQUIRE(native.tracks[0].notes[0].duration == 480);
        REQUIRE(native.tracks[0].notes[0].pitch == 64);

        // Verify global events
        REQUIRE(native.tempos.size() == 1);
        REQUIRE(native.tempos[0].time == 0);
        REQUIRE(native.tempos[0].mspq == 400000);

        REQUIRE(native.time_signatures.size() == 1);
        REQUIRE(native.time_signatures[0].time == 0);
        REQUIRE(native.time_signatures[0].numerator == 3);
        REQUIRE(native.time_signatures[0].denominator == 4);

        REQUIRE(native.key_signatures.size() == 1);
        REQUIRE(native.key_signatures[0].time == 0);
        REQUIRE(native.key_signatures[0].key == 1);

        REQUIRE(native.markers.size() == 1);
        REQUIRE(native.markers[0].time == 0);
        REQUIRE(native.markers[0].text == "Intro");
    }
}

TEST_CASE("Test conversion between different time types", "[conversion][time_unit]") {
    // Create a Score with Tick time type
    Score<Tick> tick_score(480);  // 480 ticks per quarter

    // Add a track
    auto track = std::make_shared<Track<Tick>>("Test", 0, false);

    // Add notes and other events
    track->notes->push_back(Note<Tick>(0, 480, 60, 100));      // One beat
    track->notes->push_back(Note<Tick>(480, 240, 62, 100));    // Half beat
    track->notes->push_back(Note<Tick>(720, 120, 64, 100));    // Quarter beat

    track->controls->push_back(ControlChange<Tick>(0, 7, 100));  // Volume
    track->pedals->push_back(Pedal<Tick>(0, 960));  // Two beats pedal

    tick_score.tracks->push_back(track);

    // Add global events
    tick_score.tempos->push_back(Tempo<Tick>(0, 500000));  // 120 BPM
    tick_score.time_signatures->push_back(TimeSignature<Tick>(0, 4, 4));

    SECTION("Test conversion from Tick to Quarter") {
        // Convert to Quarter time unit
        Score<Quarter> quarter_score = convert<Quarter>(tick_score);

        REQUIRE(quarter_score.tracks->size() == 1);

        // Verify time units are correctly converted
        // 480 ticks = 1 quarter
        REQUIRE(quarter_score.tracks->at(0)->notes->at(0).time == 0.0f);
        REQUIRE(quarter_score.tracks->at(0)->notes->at(0).duration == 1.0f);  // One beat

        REQUIRE(quarter_score.tracks->at(0)->notes->at(1).time == 1.0f);
        REQUIRE(quarter_score.tracks->at(0)->notes->at(1).duration == 0.5f);  // Half beat

        REQUIRE(quarter_score.tracks->at(0)->notes->at(2).time == 1.5f);
        REQUIRE(quarter_score.tracks->at(0)->notes->at(2).duration == 0.25f);  // Quarter beat

        // Verify other events conversion
        REQUIRE(quarter_score.tracks->at(0)->pedals->at(0).time == 0.0f);
        REQUIRE(quarter_score.tracks->at(0)->pedals->at(0).duration == 2.0f);  // Two beats

        // Verify global events conversion
        REQUIRE(quarter_score.tempos->at(0).time == 0.0f);
        REQUIRE(quarter_score.time_signatures->at(0).time == 0.0f);
    }

    SECTION("Test conversion from Tick to Second") {
        // Convert to Second time unit (assuming tempo is 120 BPM, each beat is 0.5 seconds)
        Score<Second> second_score = convert<Second>(tick_score);

        REQUIRE(second_score.tracks->size() == 1);

        // Verify time units are correctly converted
        // 480 ticks = 1 quarter = 0.5 seconds @120 BPM
        REQUIRE(second_score.tracks->at(0)->notes->at(0).time == Catch::Approx(0.0f));
        REQUIRE(second_score.tracks->at(0)->notes->at(0).duration == Catch::Approx(0.5f));  // 0.5 seconds (one beat)

        REQUIRE(second_score.tracks->at(0)->notes->at(1).time == Catch::Approx(0.5f));
        REQUIRE(second_score.tracks->at(0)->notes->at(1).duration == Catch::Approx(0.25f));  // 0.25 seconds (half beat)

        REQUIRE(second_score.tracks->at(0)->notes->at(2).time == Catch::Approx(0.75f));
        REQUIRE(second_score.tracks->at(0)->notes->at(2).duration == Catch::Approx(0.125f));  // 0.125 seconds (quarter beat)

        // Verify other events conversion
        REQUIRE(second_score.tracks->at(0)->pedals->at(0).time == Catch::Approx(0.0f));
        REQUIRE(second_score.tracks->at(0)->pedals->at(0).duration == Catch::Approx(1.0f));  // 1 second (two beats)

        // Verify global events conversion
        REQUIRE(second_score.tempos->at(0).time == Catch::Approx(0.0f));
        REQUIRE(second_score.time_signatures->at(0).time == Catch::Approx(0.0f));
    }

    SECTION("Test conversion from Quarter to Tick") {
        // First create a Score with Quarter time unit
        Score<Quarter> quarter_score(120);  // 120 is sampling rate, not meaningful for Quarter

        auto q_track = std::make_shared<Track<Quarter>>("Quarter Track", 0, false);
        q_track->notes->push_back(Note<Quarter>(0.0f, 1.0f, 60, 100));      // One beat
        q_track->notes->push_back(Note<Quarter>(1.0f, 0.5f, 62, 100));      // Half beat
        q_track->notes->push_back(Note<Quarter>(1.5f, 0.25f, 64, 100));     // Quarter beat

        quarter_score.tracks->push_back(q_track);
        quarter_score.tempos->push_back(Tempo<Quarter>::from_qpm(0.0f, 120.0f));  // 120 BPM

        // Convert to Tick time unit, specifying TPQ as 480
        Score<Tick> new_tick_score = resample(quarter_score, 480);

        REQUIRE(new_tick_score.ticks_per_quarter == 480);
        REQUIRE(new_tick_score.tracks->size() == 1);

        // Verify time units are correctly converted
        REQUIRE(new_tick_score.tracks->at(0)->notes->at(0).time == 0);
        REQUIRE(new_tick_score.tracks->at(0)->notes->at(0).duration == 480);  // One beat = 480 ticks

        REQUIRE(new_tick_score.tracks->at(0)->notes->at(1).time == 480);
        REQUIRE(new_tick_score.tracks->at(0)->notes->at(1).duration == 240);  // Half beat = 240 ticks

        REQUIRE(new_tick_score.tracks->at(0)->notes->at(2).time == 720);
        REQUIRE(new_tick_score.tracks->at(0)->notes->at(2).duration == 120);  // Quarter beat = 120 ticks
    }

    SECTION("Test conversion with minimum duration") {
        // Test conversion with minimum duration setting
        Score<Quarter> quarter_score = convert<Quarter>(tick_score, 0.1f);  // Minimum duration of 0.1 beats

        // Verify that notes shorter than minimum duration are adjusted
        REQUIRE(quarter_score.tracks->at(0)->notes->at(2).duration == 0.25f);  // Originally 0.25 beats, not adjusted

        // Create a Score with very short notes
        Score<Tick> very_short_note_score(480);
        auto short_track = std::make_shared<Track<Tick>>("Short", 0, false);
        short_track->notes->push_back(Note<Tick>(0, 24, 60, 100));  // 1/20 beat (24 ticks)
        very_short_note_score.tracks->push_back(short_track);

        // Convert and set minimum duration
        Score<Quarter> adjusted_score = convert<Quarter>(very_short_note_score, 0.1f);

        // Verify that short notes are adjusted to minimum duration
        REQUIRE(adjusted_score.tracks->at(0)->notes->at(0).time == 0.0f);
        REQUIRE(adjusted_score.tracks->at(0)->notes->at(0).duration == 0.1f);  // Adjusted to 0.1 beats
    }
}

TEST_CASE("Test low-level time conversion helpers", "[conversion][helpers]") {
    Score<Tick> tick_score(480);
    tick_score.tempos->push_back(Tempo<Tick>::from_qpm(0, 120.0));
    tick_score.tempos->push_back(Tempo<Tick>::from_qpm(360, 60.0));
    tick_score.time_signatures->push_back(TimeSignature<Tick>(0, 4, 4));
    tick_score.time_signatures->push_back(TimeSignature<Tick>(960, 3, 4));

    SECTION("Scalar and vector time conversion reuse the shared tempo map") {
        REQUIRE(convert_time<Quarter>(480, tick_score) == Catch::Approx(1.0f));
        REQUIRE(convert_time<Second>(240, tick_score) == Catch::Approx(0.25f));
        REQUIRE(convert_time<Second>(480, tick_score) == Catch::Approx(0.625f));

        const vec<Tick::unit> tick_times{0, 240, 360, 480, 960};
        const auto second_times = convert_times<Second>(
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
        const auto quarter_time_signatures = convert_time_events<Quarter>(
            *tick_score.time_signatures, tick_score
        );
        const auto second_tempos = convert_time_events<Second>(*tick_score.tempos, tick_score);

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
        pyvec<Note<Tick>> notes;
        notes.push_back(Note<Tick>(240, 240, 60, 100));
        notes.push_back(Note<Tick>(720, 30, 62, 90));

        const auto second_notes = convert_duration_events<Second>(notes, tick_score);
        REQUIRE(second_notes.size() == 2);
        REQUIRE(second_notes[0].time == Catch::Approx(0.25f));
        REQUIRE(second_notes[0].duration == Catch::Approx(0.375f));
        REQUIRE(second_notes[1].time == Catch::Approx(1.125f));
        REQUIRE(second_notes[1].duration == Catch::Approx(0.0625f));

        const auto quarter_notes = convert_duration_events<Quarter>(notes, tick_score, 0.1f);
        REQUIRE(quarter_notes.size() == 2);
        REQUIRE(quarter_notes[0].time == Catch::Approx(0.5f));
        REQUIRE(quarter_notes[0].duration == Catch::Approx(0.5f));
        REQUIRE(quarter_notes[1].time == Catch::Approx(1.5f));
        REQUIRE(quarter_notes[1].duration == Catch::Approx(0.1f));
    }
}

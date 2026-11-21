#include <algorithm>
#include <array>

#include <fmt/format.h>
#include <minimidi/MiniMidi.hpp>

#include "abc_test.h"
#include "../../src/io/abc/midi_output.h"

namespace symusic::abc_test {

TEST_CASE("test_chord_channel_ownership", "[abc][directives]") {
    abc::detail::Diagnostics       diagnostics("channels.abc");
    abc::detail::ProgramNumbers    programs;
    abc::detail::VoiceOutput       output("", programs, diagnostics);
    abc::detail::MidiTrackChannels plan{1, {10, 2}};
    output.add_lane(plan, {});
    plan.extras.front() = 2;

    output.allocate_channels(0, 1, {});
    CHECK(output.chord_route(0, 1).is_drum());
    CHECK_FALSE(output.chord_route(0, 0).is_drum());
    output.allocate_channels(0, 1, {});
    CHECK_FALSE(output.chord_route(0, 1).is_drum());
    CHECK_THROWS_AS(output.allocate_channels(0, 1, {}), std::invalid_argument);
}

TEST_CASE("test_bend_tied_release", "[abc][directives]") {
    const auto [score, warnings] = abc::read(
        "X:1\nM:4/4\nL:1/4\nK:C\n%%MIDI bendvelocity 2147483647 -2147483648 3\n"
        "!bend!C-C !bend!D E|"
    );
    const auto& track = *score.tracks->front();
    const auto& bends = *track.pitch_bends;
    REQUIRE(warnings.size() == 2);
    REQUIRE(bends.size() == 8);
    CHECK(bends[0] == PitchBend<Tick>{1, 8191});
    CHECK(bends[1] == PitchBend<Tick>{320, 8190});
    CHECK(bends[2] == PitchBend<Tick>{639, -8192});
    CHECK(bends[3] == PitchBend<Tick>{960, 0});
    CHECK(bends[4] == PitchBend<Tick>{961, 8191});
    CHECK(track.notes->front().duration == 959);
}

TEST_CASE("test_shape_command_order", "[abc][directives]") {
    const auto [score, warnings] = abc::read(
        "X:1\nM:4/4\nL:1/4\nK:C\n%%MIDI controlstring 11 50 60\n"
        "%%MIDI bendstring 300 -100\n!shape!C [I:MIDI control 11 80][I:MIDI pitchbend 127 127] D"
    );
    REQUIRE(warnings.empty());
    const auto& track = *score.tracks->front();
    const auto& cc    = *track.controls;
    const auto& bends = *track.pitch_bends;
    REQUIRE(cc.size() == 4);
    REQUIRE(bends.size() == 4);
    CHECK(cc[2] == ControlChange<Tick>{480, 11, 127});
    CHECK(cc[3] == ControlChange<Tick>{480, 11, 80});
    CHECK(bends[2] == PitchBend<Tick>{480, 0});
    CHECK(bends[3] == PitchBend<Tick>{480, 8191});
    CHECK(track.notes->at(1).time == 481);
}

TEST_CASE("test_comma53_quantization", "[abc][directives]") {
    const auto [score, warnings]
        = abc::read(kHeader + "CDEF | [I:MIDI tuningsystem comma53] CDEF | ^/C _/D ^3/4C _3/4D |]");
    REQUIRE(warnings.empty());
    CHECK(pitches(score) == vec<i32>{60, 62, 64, 65, 59, 62, 64, 64, 60, 61, 60, 61});
    const auto& bends = *score.tracks->front()->pitch_bends;
    CHECK(
        vec<PitchBend<Tick>>(bends.begin(), bends.end())
        == vec<PitchBend<Tick>>{
            {1921, 4095},
            {2401, 154},
            {2881, 309},
            {3361, 4018},
            {3841, 1854},
            {4321, 2395},
            {4801, 2782},
            {5281, 1468}
        }
    );
}

TEST_CASE("test_comma53_written_order", "[abc][directives]") {
    const auto [score, warnings] = abc::read(
        kHeader
        + "V:1\n|: C4 | [I:MIDI tuningsystem comma53] C4 :| "
          "[I:MIDI C 61] C4 | [I:MIDI temperamentnormal] C4 |]\n"
          "V:2\nC4 |]"
    );
    REQUIRE(warnings.empty());
    REQUIRE(score.tracks->size() == 2);
    CHECK(pitches(*score.tracks->front()) == vec<i32>{60, 59, 60, 59, 60, 59});
    const auto& bass = *score.tracks->at(1);
    REQUIRE(bass.notes->size() == 1);
    CHECK(bass.notes->front().pitch == 59);
    REQUIRE(bass.pitch_bends->size() == 1);
    CHECK(bass.pitch_bends->front() == PitchBend<Tick>{1, 4095});
}

TEST_CASE("test_overlay_percussion_map", "[abc][directives]") {
    const auto [score, warnings] = abc::read(
        "%abc-2.1\n%%MIDI drummap C 35\nX:1\nT:Overlay percussion\nM:2/4\nL:1/4\nK:C\n"
        "%%MIDI channel 10\n"
        "C2 | C2 | && [I:MIDI drummap C 38] C2 | C2 | C2 |"
    );
    REQUIRE(warnings.empty());
    REQUIRE(score.tracks->size() == 2);
    const auto& primary = *score.tracks->front();
    const auto& overlay = *score.tracks->back();
    REQUIRE(primary.is_drum);
    REQUIRE(overlay.is_drum);
    CHECK(onsets(primary) == Onsets{{1, 35}, {961, 35}, {1921, 35}});
    CHECK(onsets(overlay) == Onsets{{1, 38}, {961, 38}});
}

TEST_CASE("test_continued_midi_aliases", "[abc][directives]") {
    const auto actual
        = abc::read(kStrictHeader + "[I:MIDIdef soft vol 64] C [E\\\n%%MIDIx soft\nG] D |]");
    const auto expected = abc::read(kStrictHeader + "C [I:MIDI vol 64][EG] D |]");
    REQUIRE(actual.warnings.empty());
    CHECK(actual.value == expected.value);
}

TEST_CASE("test_numeric_voice_target", "[abc][directives]") {
    const auto [score, warnings] = abc::read(
        "%abc-2.1\nX:1\nT:\nM:4/4\nL:1/4\n"
        "%%MIDI voice 02 instrument=59\n"
        "V:0001\nV:2\nK:C\n[V:1] CDEF |]\n[V:0002] GABc |]"
    );
    REQUIRE(warnings.empty());
    REQUIRE(score.tracks->size() == 2);
    CHECK(score.tracks->at(0)->program == 0);
    CHECK(score.tracks->at(1)->program == 58);
    CHECK(score.tracks->at(0)->notes->size() == 4);
    CHECK(score.tracks->at(1)->notes->size() == 4);
}

TEST_CASE("test_tuning_sysex_chord", "[abc][directives]") {
    const auto [score, warnings]
        = abc::read(kHeader + "%%MIDI snt 60 60.25\n[CE] D F G |]", "single-note.abc");
    REQUIRE(warnings.empty());
    CHECK(score == abc::read(kHeader + "[CE] D F G |]").value);
    REQUIRE(score.tracks->size() == 1);
    const auto& track = *score.tracks->front();
    REQUIRE(track.notes->size() == 5);
    CHECK(track.notes->at(0).pitch == 60);
    CHECK(track.notes->at(1).pitch == 64);
    CHECK(track.pitch_bends->empty());
}

TEST_CASE("test_repeat_channel_restore", "[abc][directives]") {
    const auto [score, warnings] = abc::read(
        kHeader
        + "%%MIDI transpose 5\nA A A [I:MIDI channel 10] "
          "[I:MIDI makechordchannels 1] [CE] |: [I:MIDI control 7 42] "
          "D [I:MIDI channel 3] E F G :|"
    );
    REQUIRE(warnings.empty());
    REQUIRE(score.tracks->size() == 3);
    const auto extra = std::ranges::find_if(*score.tracks, [](const auto& track) {
        return !track->controls->empty();
    });
    REQUIRE(extra != score.tracks->end());
    const auto& track = **extra;
    CHECK_FALSE(track.is_drum);
    REQUIRE(track.notes->size() == 1);
    CHECK(track.notes->front().pitch == 69);
    CHECK(*track.controls == pyvec<ControlChange<Tick>>{{1920, 7, 42}, {3840, 7, 42}});
}

TEST_CASE("test_chord_channel_allocation", "[abc][directives]") {
    const auto [score, warnings] = abc::read(
        kHeader
        + "%%MIDI program 17\n%%MIDI makechordchannels 1\n"
          "[CE] [I:MIDI makechordchannels 1] [FA] B c |]"
    );
    REQUIRE(warnings.empty());
    REQUIRE(score.tracks->size() == 3);
    // abc2midi gives a new channel the last explicitly set program of the channel allocating it.
    // The first allocation copies 17. The second starts from a channel with no explicit program.
    const std::array expected
        = {std::pair{17, vec<i32>{60}},
           std::pair{17, vec<i32>{64, 65, 71, 72}},
           std::pair{0, vec<i32>{69}}};
    for (size_t index = 0; index < expected.size(); ++index) {
        CAPTURE(index);
        const auto& [program, notes] = expected[index];
        const auto& track            = *score.tracks->at(index);
        CHECK(track.program == program);
        CHECK(pitches(track) == notes);
    }
}

TEST_CASE("test_explicit_chord_channel", "[abc][directives]") {
    const auto [score, warnings] = abc::read(
        kHeader
        + "%%MIDI makechordchannels 1\n[CE] [I:MIDI channel 2] "
          "[I:MIDI control 7 42] [I:MIDI program 2 17] [FA] B c |]"
    );
    REQUIRE(warnings.empty());
    REQUIRE(score.tracks->size() == 3);
    const auto& original = *score.tracks->at(1);
    CHECK(original.program == 0);
    REQUIRE(original.notes->size() == 1);
    CHECK(original.notes->front().pitch == 64);
    CHECK(*original.controls == pyvec<ControlChange<Tick>>{{480, 7, 42}});
    const auto& changed = *score.tracks->at(2);
    CHECK(changed.program == 17);
    REQUIRE(changed.notes->size() == 1);
    CHECK(changed.notes->front().pitch == 69);
    CHECK(changed.notes->front().time == 491);
}

TEST_CASE("test_chord_channel_limits", "[abc][directives]") {
    const auto [score, warnings] = abc::read(
        kHeader
        + "%%MIDI makechordchannels 10\n%%MIDI makechordchannels 5\n"
          "[I:MIDI makechordchannels 0] [CE] D E F |]"
    );
    REQUIRE(warnings.empty());
    REQUIRE(score.tracks->size() == 2);
    REQUIRE(score.tracks->back()->notes->size() == 1);
    CHECK_FALSE(score.tracks->back()->is_drum);
    CHECK(score.tracks->back()->notes->front().pitch == 64);
    CHECK_THROWS_MATCHES(
        abc::read(
            kHeader + "%%MIDI makechordchannels 9\n%%MIDI makechordchannels 6\n[CE] D E F |]"
        ),
        std::invalid_argument,
        Catch::Matchers::MessageMatches(
            Catch::Matchers::ContainsSubstring("[midi-channels-exhausted]")
        )
    );
}

TEST_CASE("test_cross_voice_channels", "[abc][directives]") {
    const auto opening = kHeader + "[V:a] \"C\"[I:MIDI makechordchannels 1] [CE] D E F |]\n";
    const auto [score, warnings] = abc::read(
        opening
        + "[V:b] [I:MIDI makechordchannels 1] [CG] [I:MIDI program 2 17] "
          "[CF] [I:MIDI program 7 19] [DA] B |]"
    );
    REQUIRE(warnings.empty());
    REQUIRE(score.tracks->size() == 7);
    const auto& extra = *score.tracks->at(5);
    CHECK(extra.program == 0);
    REQUIRE(extra.notes->size() == 2);
    CHECK(extra.notes->at(0).pitch == 67);
    CHECK(extra.notes->at(1).pitch == 65);
    const auto& changed = *score.tracks->at(6);
    CHECK(changed.program == 19);
    REQUIRE(changed.notes->size() == 1);
    CHECK(changed.notes->front().pitch == 69);
    CHECK(changed.notes->front().time == 971);
}

TEST_CASE("test_overlay_channel_order", "[abc][directives]") {
    const auto [score, warnings] = abc::read(
        kHeader
        + "C4 & [I:MIDI makechordchannels 1] [CE] [I:MIDI program 4 19] [DF] G A | "
          "[I:MIDI makechordchannels 1] [CG] [I:MIDI program 2 17] [DA] B c |]"
    );
    REQUIRE(warnings.empty());
    REQUIRE(score.tracks->size() == 6);
    const auto& primary = *score.tracks->at(2);
    CHECK(primary.program == 17);
    REQUIRE(primary.notes->size() == 1);
    CHECK(primary.notes->front().pitch == 69);
    CHECK(primary.notes->front().time == 2411);
    const auto& secondary = *score.tracks->at(5);
    CHECK(secondary.program == 19);
    REQUIRE(secondary.notes->size() == 1);
    CHECK(secondary.notes->front().pitch == 65);
    CHECK(secondary.notes->front().time == 491);
}

TEST_CASE("test_zero_beat_spacing", "[abc][directives]") {
    for (const auto& [meter, phrase, expected] : {
             std::tuple{"4/4", "CDEFGABc |", vec<i32>{98, 61, 61, 61, 80, 61, 61, 61}},
             {"6/4", "CDEFGABc |", {98, 61, 61, 61, 61, 61, 80, 61}},
         }) {
        CAPTURE(meter);
        const auto [score, warnings] = abc::read(
            fmt::format("X:1\nM:{}\nL:1/4\nK:C\n%%MIDI beat 98 80 61 0\n{}", meter, phrase)
        );
        CHECK(warnings.empty());
        CHECK(velocities(score) == expected);
    }
}

TEST_CASE("test_meter_beat_spacing", "[abc][directives]") {
    const auto explicit_beat     = kHeader + "%%MIDI beat 98 80 61 1\n";
    const auto [score, warnings] = abc::read(explicit_beat + "CDEF | [M:4/4] GABc |]");
    CHECK(warnings.empty());
    CHECK(velocities(score) == vec<i32>{98, 80, 80, 80, 98, 61, 80, 61});

    const auto changed = abc::read(kHeader + "!f!CDEF | [M:3/4] GAB |]");
    CHECK(changed.warnings.empty());
    CHECK(velocities(changed.value) == vec<i32>{105, 95, 95, 95, 105, 80, 80});

    const auto pattern = abc::read(explicit_beat + "%%MIDI beatstring pm\nCDEF | [M:2/4] GA |]");
    CHECK(pattern.warnings.empty());
    CHECK(velocities(pattern.value) == vec<i32>{61, 61, 80, 61, 61, 80});
}

TEST_CASE("test_repeat_beat_spacing", "[abc][directives]") {
    const auto [score, warnings] = abc::read(
        kHeader + "%%MIDI beat 98 80 61 1\n|: CDEF | [M:3/4] [I:MIDI beat 99 81 62 1] GAB :|"
    );
    CHECK(warnings.empty());
    CHECK(velocities(score) == vec<i32>{98, 80, 80, 80, 99, 81, 81, 99, 81, 81, 81, 99, 81, 81});
}

TEST_CASE("test_grace_beat_spacing", "[abc][directives]") {
    const auto [score, warnings] = abc::read(kHeader + "%%MIDI beat 98 80 61 1\nC{a}[M:3/4]D E |]");
    CHECK(warnings.empty());
    CHECK(velocities(score) == vec<i32>{98, 80, 61, 61});
}

TEST_CASE("test_voice_instruments", "[abc][directives]") {
    // ABC 2.1 section 11.2 uses one-based instruments independently of programbase.
    const auto [score, warnings] = abc::read(
        "%abc-2.1\nX:1\nT:\nM:4/4\nL:1/4\n"
        "%%MIDI voice Bass instrument=59\n"
        "V:Lead\nV:Bass\nK:C\n"
        "%%MIDI programbase 0\n"
        "V:Lead\n%%MIDI voice instrument=74\nC\n"
        "[I:MIDI voice Bass instrument=128] D\n"
        "V:Bass\nE\n"
        "[I:MIDI voice Lead instrument=1] F\n"
        "V:Lead\nG\n"
    );
    REQUIRE(warnings.empty());
    REQUIRE(score.tracks->size() == 3);
    const auto& new_lead = *score.tracks->at(0);
    const auto& lead     = *score.tracks->at(1);
    const auto& bass     = *score.tracks->at(2);
    CHECK(lead.program == 73);
    REQUIRE(lead.notes->size() == 2);
    CHECK(lead.notes->at(0).pitch == 60);
    CHECK(lead.notes->at(1).pitch == 62);
    CHECK(new_lead.program == 0);
    REQUIRE(new_lead.notes->size() == 1);
    CHECK(new_lead.notes->front().pitch == 67);
    CHECK(new_lead.notes->front().time == 961);
    CHECK(bass.program == 127);
    REQUIRE(bass.notes->size() == 2);
    CHECK(bass.notes->at(0).pitch == 64);
    CHECK(bass.notes->at(0).time == 1);
    CHECK(bass.notes->at(1).pitch == 65);
}

TEST_CASE("test_chord_program_numbers", "[abc][directives]") {
    // ABC 2.1 section 11.2 gives chordprog 20 for Church Organ.
    const auto chord_program = [](const std::string& text) {
        const auto [score, warnings] = abc::read(text + "\"C\"CDEF |]");
        REQUIRE(warnings.empty());
        REQUIRE(score.tracks->size() == 3);
        REQUIRE(score.tracks->at(2)->notes->size() == 6);
        return score.tracks->at(2)->program;
    };
    for (const auto& version : kVersionLines) {
        CAPTURE(version);
        CHECK(chord_program(version + kHeader + "%%MIDI chordprog 20\n") == 19);
        CHECK(chord_program(version + kHeader + "[I:MIDI chordprog 128]") == 127);
        CHECK(
            chord_program(version + kHeader + "%%MIDI chordprog 20\n%%MIDI programbase 0\n") == 20
        );
        CHECK_THROWS_AS(
            abc::read(version + kHeader + "%%MIDI chordprog 0\n\"C\"CDEF |]"), std::invalid_argument
        );
    }
}

TEST_CASE("test_forward_voice_target", "[abc][directives]") {
    const auto [score, warnings] = abc::read(
        "%abc-2.1\nX:1\nT:\nM:4/4\nL:1/4\n"
        "%%MIDI voice 12345678901234567890bass instrument=59\nK:C\n"
        "V:Lead\nC\nV:12345678901234567890other\nE\n"
    );
    REQUIRE(warnings.empty());
    REQUIRE(score.tracks->size() == 2);
    CHECK(score.tracks->at(0)->program == 0);
    CHECK(score.tracks->at(1)->program == 58);
}

TEST_CASE("test_voice_bank_order", "[abc][directives]") {
    const auto [score, warnings] = abc::read(
        kStrictHeader
        + "[I:MIDI voice instrument=53 bank=131] C "
          "[I:MIDI voice instrument=53 bank=2] D [I:MIDI voice instrument=53] E"
    );
    REQUIRE(warnings.empty());
    REQUIRE(score.tracks->size() == 3);
    const auto check = [](const Score<Tick>& value) {
        REQUIRE(value.tracks->size() == 3);
        const auto& gm   = *value.tracks->at(0);
        const auto& low  = *value.tracks->at(1);
        const auto& high = *value.tracks->at(2);
        for (const auto* track : {&gm, &low, &high}) {
            CHECK(track->program == 52);
            REQUIRE(track->notes->size() == 1);
        }
        CHECK(gm.controls->empty());
        CHECK(gm.notes->front().time == 961);
        REQUIRE(low.controls->size() == 2);
        CHECK(low.controls->at(0) == ControlChange<Tick>{0, 0, 0});
        CHECK(low.controls->at(1) == ControlChange<Tick>{0, 32, 1});
        CHECK(low.notes->front().time == 481);
        REQUIRE(high.controls->size() == 2);
        CHECK(high.controls->at(0) == ControlChange<Tick>{0, 0, 1});
        CHECK(high.controls->at(1) == ControlChange<Tick>{0, 32, 2});
        CHECK(high.notes->front().time == 1);
    };
    check(score);
    const auto exported                     = abc::write(score);
    const auto [reparsed, ignored_warnings] = abc::read(exported.value);
    check(reparsed);

    const auto                     bytes = reparsed.dumps<DataFormat::MIDI>();
    const minimidi::MidiFileView<> midi(bytes);
    size_t                         banked_tracks = 0;
    for (const auto& track : midi) {
        size_t bank_controls = 0;
        bool   program_sent  = false;
        for (const auto& message : track) {
            if (message.type() == minimidi::MessageType::ProgramChange) {
                program_sent = true;
            } else if (message.type() == minimidi::MessageType::ControlChange) {
                const auto& control = message.cast<minimidi::ControlChange>();
                if (control.control_number() == 0 || control.control_number() == 32) {
                    CHECK_FALSE(program_sent);
                    ++bank_controls;
                }
            }
        }
        if (bank_controls != 0) {
            CHECK(bank_controls == 2);
            CHECK(program_sent);
            ++banked_tracks;
        }
    }
    CHECK(banked_tracks == 2);
}

TEST_CASE("test_muted_voice_lyrics", "[abc][directives]") {
    const auto [score, warnings] = abc::read(
        kStrictHeader
        + "V:Muted\nC\n%%MIDI voice mute\nD E & G A B |\nw: a b c d e f\n"
          "V:Audible\nF2\n"
    );
    REQUIRE(warnings.empty());
    size_t              audible = 0;
    vec<TextMeta<Tick>> lyrics;
    for (const auto& track : *score.tracks) {
        for (const auto& note : *track->notes) {
            CHECK(note.time == 1);
            CHECK((note.pitch == 60 || note.pitch == 65));
            ++audible;
        }
        lyrics.insert(lyrics.end(), track->lyrics->begin(), track->lyrics->end());
    }
    CHECK(audible == 2);
    REQUIRE(lyrics.size() == 6);
    CHECK(lyrics.at(1) == TextMeta<Tick>{481, "b"});
    CHECK(lyrics.at(5) == TextMeta<Tick>{961, "f"});
}

TEST_CASE("test_part_voice_target", "[abc][directives]") {
    const auto [score, warnings] = abc::read(
        "X:1\nT:Parts\nP:BA\nM:4/4\nL:1/4\nV:Lead name=\"Lead\"\nV:Bass name=\"Bass\"\nK:C\n"
        "P:A\n[V:Lead] C4 | [V:Bass] D4 |\nP:B\n"
        "[V:Bass][V:Lead][I:MIDI voice Bass mute] E4 | [V:Bass] F4 |]"
    );
    REQUIRE(warnings.empty());
    REQUIRE(score.tracks->size() == 1);
    const auto& lead = *score.tracks->front();
    REQUIRE(lead.name == "Lead");
    REQUIRE(lead.notes->size() == 2);
    CHECK(lead.notes->at(0).pitch == 64);
    CHECK(lead.notes->at(1).pitch == 60);
}

TEST_CASE("test_performed_transposition", "[abc][directives]") {
    const std::string voices = "[V:one] CDEF |]\n[V:two] GABc |]";
    const auto        check  = [](const std::string& text, const vec<vec<i32>>& expected) {
        const auto [score, warnings] = abc::read("%abc-2.1\n" + text);
        REQUIRE(warnings.empty());
        REQUIRE(score.tracks->size() == expected.size());
        for (size_t i = 0; i < expected.size(); ++i) {
            CHECK(pitches(*score.tracks->at(i)) == expected[i]);
        }
    };
    check(
        kHeader + "%%MIDI transpose 5\nI:MIDI rtranspose -2\n" + voices,
        {{63, 65, 67, 68}, {70, 72, 74, 75}}
    );
    check(kHeader + "%%MIDI rtranspose 3\n" + voices, {{66, 68, 70, 71}, {73, 75, 77, 78}});
    check(
        "X:1\nT:\nM:4/4\nL:1/4\nV:one\nV:two\nK:C\n%%MIDI transpose 5\n" + voices,
        {{60, 62, 64, 65}, {72, 74, 76, 77}}
    );
    check(
        kHeader + "[V:one] C [I:MIDI transpose 5] DEF |]\n[V:two] GABc |]",
        {{65, 67, 69, 70}, {72, 74, 76, 77}}
    );
    check(kHeader + "%%MIDI rtranspose 3\nC4 & D4 |]", {{66}, {68}});
    check(kHeader + "|: C [I:MIDI rtranspose 1] D :| E F |]", {{60, 63, 61, 64, 66, 67}});
    check(
        kHeader + "\"C\" C [I:MIDI rtranspose 2] D E F |]",
        {{62, 66, 68, 69}, {40, 42}, {54, 58, 61, 54, 58, 61}}
    );
    check(
        kHeader + "%%MIDI rtranspose 2\n%%MIDI droneon\nCDEF |]\n%%MIDI droneoff",
        {{64, 66, 68, 69}, {51, 39}}
    );
    // The MIDI replay is independent of the ABC 2.1 K: transposition.
    check(
        "X:1\nT:\nM:4/4\nL:1/4\nK:C transpose=3\n%%MIDI transpose 5\n"
        "[V:one] C [K:transpose=0] DEF |]\n[V:two] GABc |]",
        {{68, 67, 69, 70}, {75, 77, 79, 80}}
    );
}

TEST_CASE("test_single_note_tuning", "[abc][directives]") {
    const auto [score, warnings] = abc::read(
        "%abc-2.1\nX:1\nT:Tuning\nM:4/4\nL:1/4\n%%MIDI snt 60 61.5\nK:C\n"
        "[V:1] |: C [I:MIDI snt 60 62.125] C z2 :|\n"
        "[V:2] z [I:MIDI snt 61 60.001] [I:MIDI snt 61 63.25] E z2 |]"
    );
    for (const auto& warning : warnings) UNSCOPED_INFO(warning);
    REQUIRE(warnings.empty());
    const auto plain = abc::read(
        "%abc-2.1\nX:1\nT:Tuning\nM:4/4\nL:1/4\nK:C\n"
        "[V:1] |: C C z2 :|\n[V:2] z E z2 |]"
    );
    REQUIRE(plain.warnings.empty());
    CHECK(score == plain.value);
}

}   // namespace symusic::abc_test

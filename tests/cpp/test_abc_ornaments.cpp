#include <algorithm>
#include <array>

#include <fmt/format.h>

#include "abc_test.h"

namespace symusic::abc_test {

TEST_CASE("test_roll_key_neighbors", "[abc][ornaments]") {
    const auto [score, warnings] = abc::read("X:1\nM:2/4\nL:1/8\nK:D\n^g~f");
    REQUIRE(warnings.empty());
    const auto& notes = *score.tracks->front()->notes;
    REQUIRE(notes.size() == 3);
    CHECK(notes[0] == Note<Tick>{1, 239, 80, 90});
    CHECK(notes[1] == Note<Tick>{241, 59, 79, 65});
    CHECK(notes[2] == Note<Tick>{301, 179, 78, 65});
}

TEST_CASE("test_ornament_attacks", "[abc][ornaments]") {
    const Passage cases[] = {
        {".F G", {{1, 239, 65}, {481, 479, 67}}},
        {"~F G", {{1, 119, 67}, {121, 359, 65}, {481, 479, 67}}},
        {"RF", {{1, 95, 65}, {97, 95, 67}, {193, 95, 65}, {289, 95, 64}, {385, 95, 65}}},
        {"[I:MIDI harpmode 1] ~F3 G",
         {{1, 239, 65}, {241, 239, 65}, {481, 959, 65}, {1441, 479, 67}}},
    };
    for (const auto& [music, notes] : cases) {
        CAPTURE(music);
        const auto [score, warnings] = abc::read(kHeader + music);
        CHECK(warnings.empty());
        CHECK(melody(score) == notes);
    }
}

TEST_CASE("test_roll_shorthand", "[abc][ornaments]") {
    for (const std::string_view mode : {"", "[I:MIDI harpmode 1] "}) {
        CAPTURE(mode);
        const auto opening                         = kStrictHeader + std::string(mode);
        const auto [shorthand, shorthand_warnings] = abc::read(opening + "~F3 G");
        const auto [named, named_warnings]         = abc::read(opening + "!roll!F3 G");
        const auto [alias, alias_warnings]         = abc::read(opening + "[U:h=!roll!] hF3 G");
        CHECK(shorthand_warnings.empty());
        CHECK(named_warnings.empty());
        CHECK(alias_warnings.empty());
        CHECK(shorthand.note_num() > 2);
        CHECK(named == shorthand);
        CHECK(alias == shorthand);
    }
}

TEST_CASE("test_unperformed_marks", "[abc][ornaments]") {
    const auto             plain = abc::read(kStrictHeader + "CDEF | GABc |]").value;
    const std::string_view marks[]
        = {"trill(", "trill)", "lowermordent", "uppermordent",  "mordent",   "pralltriller",
           "turn",   "turnx",  "invertedturn", "invertedturnx", "+",         "plus",
           "snap",   "slide",  "wedge",        "upbow",         "downbow",   "open",
           "thumb",  "segno",  "coda",         "D.S.",          "D.C.",      "dacoda",
           "dacapo", "fine",   "shortphrase",  "mediumphrase",  "longphrase"};
    for (const auto mark : marks) {
        CAPTURE(mark);
        const auto [score, warnings]
            = abc::read(kStrictHeader + fmt::format("C !{}!DEF | GABc |]", mark));
        CHECK(warnings.empty());
        CHECK(score == plain);
    }

    const auto [score, warnings]
        = abc::read(kStrictHeader + "MC !dacoda!D E !fine!F | O G S A B c !D.S.! |]");
    CHECK(warnings.empty());
    CHECK(score == plain);

    const auto unknown = abc::read(kStrictHeader + "C !unrecognized-mark!DEF | GABc |]");
    REQUIRE(unknown.warnings.size() == 1);
    CHECK_THAT(
        unknown.warnings.front(), Catch::Matchers::ContainsSubstring("[ignored-decoration]")
    );
    CHECK(unknown.value == plain);
}

TEST_CASE("test_shorthand_mark_attacks", "[abc][ornaments]") {
    const auto plain = abc::read(kStrictHeader + "CDEF |]").value;
    const auto bare  = abc::read(kStrictHeader + "PC SD uE vF |]");
    CHECK(bare.warnings.empty());
    CHECK(bare.value == plain);

    const auto named = abc::read(kStrictHeader + "!P!C !S!D !u!E !v!F |]");
    REQUIRE(named.warnings.size() == 4);
    for (const auto& warning : named.warnings)
        CHECK_THAT(warning, Catch::Matchers::ContainsSubstring("[ignored-decoration]"));
    CHECK(named.value == plain);
}

TEST_CASE("test_bar_decorations", "[abc][ornaments]") {
    for (const auto& opening : {kStrictHeader, kHeader}) {
        // Section 4.14 allows decorations on bars. Include a note attachment
        // as well because coda and mordent shorthands are valid on either.
        const auto [score, warnings] = abc::read(
            opening + "|: OC D E MF H| G A B c T :| c B A G O|| F E D C M[| E D C2 ~|]"
        );
        const auto plain
            = abc::read(opening + "|: C D E F | G A B c :| c B A G || F E D C [| E D C2 |]");
        CHECK(warnings.empty());
        CHECK(plain.warnings.empty());
        CHECK(score == plain.value);
    }
}

TEST_CASE("test_tied_ornament_attacks", "[abc][ornaments]") {
    // The abc2midi roll and trill patterns fill the joined note. Only pulses on a beat get beat
    // velocity.
    for (const auto& [mark, pattern, count] : {
             std::tuple{"R", vec<i8>{62, 64, 62, 60, 62}, 5},
             {"!trill!", vec<i8>{64, 62}, 16},
         }) {
        CAPTURE(mark);
        const auto [score, warnings] = abc::read(kHeader + "C " + mark + "D- D F |]");
        REQUIRE(warnings.empty());
        REQUIRE(score.tracks->size() == 1);
        const auto& notes = *score.tracks->front()->notes;
        REQUIRE(notes.size() == static_cast<size_t>(count) + 2);
        CHECK(notes.front() == Note<Tick>{1, 479, 60, 90});
        const i32 step = 960 / count;
        for (i32 i = 0; i < count; ++i) {
            CAPTURE(i);
            CHECK(
                notes[i + 1]
                == Note<Tick>{
                    481 + i * step,
                    step - 1,
                    pattern[i % pattern.size()],
                    i * step % 480 == 0 ? i8{80} : i8{65}
                }
            );
        }
        CHECK(notes.back() == Note<Tick>{1441, 479, 65, 80});
    }
}

TEST_CASE("test_tied_roll_program", "[abc][ornaments]") {
    const auto [score, warnings]
        = abc::read(kStrictHeader + "C3 RD- | [I:MIDI program 41] [FD] E F G |]");
    REQUIRE(warnings.empty());
    REQUIRE(score.tracks->size() == 2);
    const auto& opening = *score.tracks->front();
    const auto& changed = *score.tracks->back();
    CHECK(opening.program == 0);
    CHECK(changed.program == 41);
    const auto& roll = *opening.notes;
    REQUIRE(roll.size() == 6);
    const std::array<i8, 5> pitches{62, 64, 62, 60, 62};
    for (auto pulse = pitches.begin(); pulse != pitches.end(); ++pulse) {
        const auto index = pulse - pitches.begin();
        CHECK(roll.at(index + 1).time == 1441 + 192 * index);
        CHECK(roll.at(index + 1).duration == 191);
        CHECK(roll.at(index + 1).pitch == *pulse);
    }
    const auto& later = *changed.notes;
    REQUIRE(later.size() == 4);
    CHECK(later.front().time == 1921);
    CHECK(later.front().pitch == 65);
    CHECK(later.at(1).time == 2401);
    CHECK(later.at(1).pitch == 64);
}

TEST_CASE("test_trill_trim_and_slur", "[abc][ornaments]") {
    for (const auto [phrase, legato_attacks] : {
             std::pair{"C TD EF |]", 0},
             {"(C TD) EF |]", 6},
             {"(C TD E) F |]", 8},
         }) {
        CAPTURE(phrase);
        const auto [score, warnings] = abc::read(kHeader + "%%MIDI trim 1/16\n" + phrase);
        REQUIRE(warnings.empty());
        const auto& notes = *score.tracks->front()->notes;
        REQUIRE(notes.size() == 11);
        for (i32 i = 0; i < 8; ++i) {
            CHECK(notes[i + 1].time == 481 + 60 * i);
            CHECK(notes[i + 1].duration == (i < legato_attacks ? 59 : 29));
        }
        CHECK(notes[9].time == 961);
    }
}

TEST_CASE("test_expanded_ornament_release", "[abc][ornaments]") {
    const auto [score, warnings]
        = abc::read(kHeader + "%%MIDI chordattack 4\n%%MIDI expand 1/24\nTC [DF] E G | z8 |]");
    REQUIRE(warnings.empty());
    const auto& notes = *score.tracks->front()->notes;
    REQUIRE(notes.size() == 12);
    CHECK(notes.at(6).time == 361);
    CHECK(notes.at(6).end() == 440);
    CHECK(notes.at(7).time == 421);
    CHECK(notes.at(7).end() == 504);
    CHECK(notes.at(8).time == 481);
    CHECK(notes.at(9).time == 485);
    CHECK(notes.at(10).time == 961);
}

TEST_CASE("test_tied_roll_release_queue", "[abc][ornaments]") {
    const auto [score, warnings]
        = abc::read(kHeader + "%%MIDI chordattack 4\n%%MIDI expand 1/24\nRC-C [DF] E | z8 |]");
    REQUIRE(warnings.empty());
    const auto& notes = *score.tracks->front()->notes;
    REQUIRE(notes.size() == 8);
    CHECK(notes.at(3).time == 577);
    CHECK(notes.at(3).end() == 788);
    CHECK(notes.at(4).time == 769);
    CHECK(notes.at(4).end() == 984);
    CHECK(notes.at(5).time == 961);
    CHECK(notes.at(6).time == 965);
}

TEST_CASE("test_cut_articulation", "[abc][ornaments]") {
    for (const auto& [directive, lead, host, following] : {
             std::tuple{"expand 1/24", 139, 379, 499},
             {"trim 1/8", 119, 299, 419},
         }) {
        CAPTURE(directive);
        const auto opening           = kStrictHeader + fmt::format("%%MIDI {}\n", directive);
        const auto [score, warnings] = abc::read(opening + "C ~D EF | z4 |]");
        REQUIRE(warnings.empty());
        const auto& notes = *score.tracks->front()->notes;
        REQUIRE(notes.size() == 5);
        CHECK(notes[1] == Note<Tick>{481, lead, 64, 80});
        CHECK(notes[2] == Note<Tick>{601, host, 62, 65});
        CHECK(notes[3] == Note<Tick>{961, following, 64, 80});
    }
}

TEST_CASE("test_proportional_cut", "[abc][ornaments]") {
    const auto opening = kHeader + "%%MIDI grace 1/3\n";
    for (const auto& [music, lead, host, following] : {
             std::tuple{"C ~D2 E | z4 |]", 319, 639, 1441},
             {"C ~D>E F | z4 |]", 239, 479, 1201},
         }) {
        CAPTURE(music);
        const auto [score, warnings] = abc::read(opening + music);
        REQUIRE(warnings.empty());
        const auto& notes = *score.tracks->front()->notes;
        REQUIRE(notes.size() >= 4);
        CHECK(notes[1] == Note<Tick>{481, lead, 64, 80});
        CHECK(notes[2] == Note<Tick>{482 + lead, host, 62, 65});
        CHECK(notes[3].time == following);
    }
}

TEST_CASE("test_three_unit_roll", "[abc][ornaments]") {
    const std::string opening = "X:1\nT:\nM:4/4\nL:1/8\nK:C\n";
    for (const auto& [setup, music, written_out] : {
             std::tuple{"", "C ~D6/2 E2 F2", "C D {E}D {C}D E2 F2"},
             {"", "C ~D30000001/10000000 E2", "C {E}D30000001/10000000 E2"},
             {"", "C ~D3- D E2", "C D {E}D {C}D- D E2"},
             {"", "C [L:1/4]~D3/2 E F", "C [L:1/4]{E}D3/2 E F"},
             {"", "C (3:2:1~D3 E2 F2", "C {E}D2 E2 F2"},
             {"", "C H~D3 E2 F2", "C {E}D6 E2 F2"},
             {"", "C ~D2>E2 F2", "C {E}D3 E F2"},
             {"%%MIDI fermatafixed\n", "C H~D2 E2 F2", "C D {E}D {C}D E2 F2"},
             {"%%MIDI grace 1/3\n", "C {a}~D3 E2", "C {a}D {E}D {C}D E2"},
             {"%%MIDI grace 1/3\n",
              "C {a}[I:MIDI fermatafixed]H~D2 E2 F2",
              "C {a}[I:MIDI fermatafixed]D {E}D {C}D E2 F2"},
             {kStressProfile.data(), "C ~D3 E2 F2", "C D {E}D {C}D E2 F2"},
         }) {
        CAPTURE(setup, music);
        const auto performed = abc::read(opening + setup + music + " |]");
        const auto written   = abc::read(opening + setup + written_out + " |]");
        REQUIRE(performed.warnings.empty());
        REQUIRE(written.warnings.empty());
        CHECK(*performed.value.tracks->front()->notes == *written.value.tracks->front()->notes);
    }
}

TEST_CASE("test_broken_roll_duration", "[abc][ornaments]") {
    const std::string opening = "%abc-2.1\nX:1\nT:\nM:4/4\nL:1/8\nK:C\n";
    for (const auto& [music, written_out] : {
             std::pair{"C ~D3>E3 F", "C D3/2 {E}D3/2 {C}D3/2 E3/2 F"},
             {"C D3>~E3 F", "C D9/2 E/2 {F}E/2 {D}E/2 F"},
         }) {
        CAPTURE(music);
        const auto performed = abc::read(opening + music + " |]");
        const auto written   = abc::read(opening + written_out + " |]");
        REQUIRE(performed.warnings.empty());
        REQUIRE(written.warnings.empty());
        CHECK(*performed.value.tracks->front()->notes == *written.value.tracks->front()->notes);
    }
}

TEST_CASE("test_arpeggio_random_spacing", "[abc][ornaments]") {
    const auto opening = kHeader + "%%MIDI randomchordattack 10\n";
    const auto plain   = abc::read(opening + "[CEG] [DFA] G |]");
    const auto marked  = abc::read(opening + "!arpeggio![CEG] [DFA] G |]");
    const auto tripled = abc::read(
        kHeader + "%%MIDI randomchordattack 30\n[CEG] [I:MIDI randomchordattack 10] [DFA] G |]"
    );
    REQUIRE(plain.warnings.empty());
    REQUIRE(marked.warnings.empty());
    REQUIRE(tripled.warnings.empty());
    const auto& ordinary    = *plain.value.tracks->front()->notes;
    const auto& arpeggiated = *marked.value.tracks->front()->notes;
    REQUIRE(ordinary.size() == 7);
    REQUIRE(arpeggiated.size() == ordinary.size());
    CHECK(arpeggiated[0].time == ordinary[0].time);
    CHECK(arpeggiated == *tripled.value.tracks->front()->notes);
    CHECK(arpeggiated != ordinary);
    for (size_t i = 1; i < 3; ++i) {
        const auto gap = arpeggiated[i].time - arpeggiated[i - 1].time;
        CHECK(gap >= 0);
        CHECK(gap < 30);
        CHECK(arpeggiated[i].end() == ordinary[i].end());
    }
    CHECK(std::equal(arpeggiated.begin() + 3, arpeggiated.end(), ordinary.begin() + 3));
}

TEST_CASE("test_layout_mark_scope", "[abc][ornaments]") {
    const auto opening = kStrictHeader + "%%MIDI chordattack 4\n";
    for (const auto& [marked, plain] : {
             std::pair{"C> .y2 D E |]", "C>D E |]"},
             {"C !fermata! y D E |]", "C D E |]"},
             {"C !arpeggio! y [DFA] |]", "C [DFA] |]"},
             {"C !arpeggio! | [DFA] |]", "C | [DFA] |]"},
             {"!arpeggio! C [DFA] |]", "C [DFA] |]"},
             {"C !ff! y D E |]", "C !ff! D E |]"},
         }) {
        CAPTURE(marked);
        const auto actual   = abc::read(opening + marked);
        const auto expected = abc::read(opening + plain);
        REQUIRE(actual.warnings.empty());
        REQUIRE(expected.warnings.empty());
        CHECK(actual.value == expected.value);
    }

    const auto [marked, warnings] = abc::read(opening + "C !arpeggio![DFA] |]");
    REQUIRE(warnings.empty());
    const auto& notes = *marked.tracks->front()->notes;
    REQUIRE(notes.size() == 4);
    CHECK(notes[1].time == 481);
    CHECK(notes[2].time == 493);
    CHECK(notes[3].time == 505);
}

TEST_CASE("test_default_dynamic", "[abc][ornaments]") {
    // Section 4.14 makes mf the opening volume. abc2midi plays explicit mf at 90 on
    // the downbeat, 80 on a beat and 65 between beats in 4/4.
    for (const auto& version : kVersionLines) {
        CAPTURE(version);
        const auto opening     = version + kHeader;
        const auto implicit_mf = abc::read(opening + "CDEF & GABC |");
        const auto explicit_mf = abc::read(opening + "!mf!CDEF & !mf!GABC |");
        REQUIRE(implicit_mf.warnings.empty());
        CHECK(explicit_mf.warnings.empty());
        CHECK(implicit_mf.value == explicit_mf.value);
        CHECK(velocities(implicit_mf.value) == vec<i32>{90, 80, 80, 80});
        CHECK(velocities(implicit_mf.value, 1) == vec<i32>{90, 80, 80, 80});

        const auto beat = abc::read(opening + "%%MIDI beat 99 82 61 2\nCDEF |");
        CHECK(beat.warnings.empty());
        CHECK(velocities(beat.value) == vec<i32>{99, 61, 82, 61});
    }
}

TEST_CASE("test_extended_dynamics", "[abc][ornaments]") {
    const auto actual   = abc::read(kStrictHeader + "CD | EF |\ns: !pppp! * | !ffff! *");
    const auto expected = abc::read(kStrictHeader + "!ppp!CD | !fff!EF |");
    CHECK(actual.warnings.empty());
    CHECK(expected.warnings.empty());
    CHECK(actual.value == expected.value);
    CHECK(velocities(actual.value) == vec<i32>{30, 20, 127, 125});
}

TEST_CASE("test_accent_scope", "[abc][ornaments]") {
    for (const auto& version : kVersionLines) {
        CAPTURE(version);
        const auto opening = version + kHeader;
        for (const auto* mark : {"L", "!accent!", "!>!", "!emphasis!", "!sfz!"}) {
            CAPTURE(mark);
            const auto result = abc::read(opening + "C " + mark + "D E F |");
            CHECK(result.warnings.empty());
            CHECK(velocities(result.value) == vec<i32>{90, 95, 80, 80});
        }
        const auto chord = abc::read(opening + "%%MIDI chordattack 0\n!accent![CE] [F!>!A] G");
        CHECK(chord.warnings.empty());
        CHECK(velocities(chord.value) == vec<i32>{105, 105, 80, 95, 80});
        const auto detached = abc::read(opening + "C- !accent!C !accent!z E |");
        CHECK(detached.warnings.empty());
        CHECK(velocities(detached.value) == vec<i32>{90, 80});
        const auto grace = abc::read(opening + "C {g}!accent!D E |");
        CHECK(grace.warnings.empty());
        CHECK(velocities(grace.value) == vec<i32>{90, 80, 80, 80});
        const auto absolute = abc::read(opening + "!fff!C !accent!D [I:MIDI vol 44]!accent!E F |");
        CHECK(absolute.warnings.empty());
        CHECK(velocities(absolute.value) == vec<i32>{127, 127, 44, 125});
    }
}

TEST_CASE("test_overlapping_tied_roll", "[abc][ornaments]") {
    const auto opening           = kStrictHeader + "%%MIDI chordattack 0\n";
    const auto [score, warnings] = abc::read(opening + "RE- [CE3-] E C |]");
    REQUIRE(warnings.empty());
    const auto [reference, reference_warnings] = abc::read(opening + "RE4 |]");
    REQUIRE(reference_warnings.empty());
    const auto&     notes = *score.tracks->front()->notes;
    vec<Note<Tick>> roll;
    vec<i32>        other_onsets;
    for (const auto& note : notes) {
        if (note.pitch == 60)
            other_onsets.push_back(note.time);
        else
            roll.push_back(note);
    }
    CHECK(std::ranges::equal(roll, *reference.tracks->front()->notes));
    CHECK(other_onsets == vec<i32>{481, 1441});
}

TEST_CASE("test_tenuto_gate", "[abc][ornaments]") {
    // Section 4.14 defines tenuto as holding the full written duration.
    for (const auto& version : kVersionLines) {
        CAPTURE(version);
        for (const auto& [directive, unmarked_gate] : {
                 std::pair{"trim", 359},
                 {"expand", 599},
             }) {
            CAPTURE(directive);
            const auto body = fmt::format("%%MIDI {} 1/4\nC !tenuto!D E |]", directive);
            const auto [score, warnings] = abc::read(version + kHeader + body);
            REQUIRE(warnings.empty());
            const auto& notes = *score.tracks->front()->notes;
            REQUIRE(notes.size() == 3);
            CHECK(notes[0].time == 1);
            CHECK(notes[0].duration == unmarked_gate);
            CHECK(notes[1].time == 481);
            CHECK(notes[1].duration == 479);
            CHECK(notes[2].time == 961);
            CHECK(notes[2].duration == unmarked_gate);
        }
    }
}

TEST_CASE("test_inverted_fermata", "[abc][ornaments]") {
    for (const auto& [directive, held_gate] : {
             std::pair{"fermatafixed", 1439},
             {"fermataproportional", 1919},
         }) {
        CAPTURE(directive);
        const auto opening           = kStrictHeader + fmt::format("%%MIDI {}\n", directive);
        const auto [score, warnings] = abc::read(opening + "C !invertedfermata!D2 E |]");
        const auto ordinary          = abc::read(opening + "C !fermata!D2 E |]");
        REQUIRE(warnings.empty());
        REQUIRE(ordinary.warnings.empty());
        CHECK(score == ordinary.value);
        const auto& notes = *score.tracks->front()->notes;
        REQUIRE(notes.size() == 3);
        CHECK(notes[1].time == 481);
        CHECK(notes[1].duration == held_gate);
        CHECK(notes[2].time == 482 + held_gate);
    }
}

TEST_CASE("test_meter_group_accents", "[abc][ornaments]") {
    const auto check = [](std::string_view meter, std::string_view music, vec<i32> expected) {
        CAPTURE(meter, music);
        const auto [score, warnings]
            = abc::read(fmt::format("X:1\nM:{}\nL:1/8\nK:C\n{}", meter, music));
        REQUIRE(warnings.empty());
        CHECK(velocities(score) == expected);
        REQUIRE_FALSE(score.time_signatures->empty());
        CHECK(score.time_signatures->front() == TimeSignature<Tick>{0, 7, 8});
    };
    // Sections 3.1.6 and 4.14: additive groups retain their accents under the default mf.
    check("(2+3+2)/8", "CDEFGAB", {90, 65, 80, 65, 65, 80, 65});
    check("2+3+2/8", "!p!CDEFGAB", {60, 35, 50, 35, 35, 50, 35});
    check("7/8", "CDEFGAB", {90, 80, 80, 80, 80, 80, 80});
    check("2+3+2/8", "|: CDEFGAB | [M:3+2+2/8] CDEFGAB :|", {90, 65, 80, 65, 65, 80, 65, 90, 65, 65,
                                                             80, 65, 80, 65, 90, 65, 80, 65, 65, 80,
                                                             65, 90, 65, 65, 80, 65, 80, 65});
}

}   // namespace symusic::abc_test

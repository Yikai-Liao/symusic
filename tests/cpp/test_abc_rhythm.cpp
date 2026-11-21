#include <algorithm>

#include <fmt/format.h>

#include "abc_test.h"

namespace symusic::abc_test {

TEST_CASE("test_multimeasure_boundaries", "[abc][rhythm]") {
    for (const auto& [compact, expanded] : {
             std::pair{"C4 | Z2 & E4 | F4 |]", "C4 | z4 | z4 & E4 | F4 |]"},
             {"|: C4 | X2 | D4 :| E4 |]", "|: C4 | z4 | z4 | D4 :| E4 |]"},
             {"C4 | Z2 | D4 |]\nw: one | | | two\ns: * | | | !p!",
              "C4 | z4 | z4 | D4 |]\nw: one | | | two\ns: * | | | !p!"},
         }) {
        CAPTURE(compact);
        const auto actual   = abc::read(kStrictHeader + compact);
        const auto expected = abc::read(kStrictHeader + expanded);
        REQUIRE(expected.warnings.empty());
        CHECK(actual.warnings.empty());
        CHECK(actual.value == expected.value);
    }
}

TEST_CASE("test_dotted_slur_close", "[abc][rhythm]") {
    const auto [expected, plain_warnings] = abc::read(kStrictHeader + ".(C(DE)G)F |]");
    const auto [dotted, dotted_warnings]  = abc::read(kStrictHeader + ".(C(DE)G.)F |]");
    const auto [staccato, short_warnings] = abc::read(kStrictHeader + ".(C(DE)G.).F |]");
    CHECK(plain_warnings.empty());
    CHECK(dotted_warnings.empty());
    CHECK(short_warnings.empty());
    CHECK(dotted == expected);
    CHECK(melody(dotted).back() == std::tuple{1921, 479, 65});
    CHECK(melody(staccato).back() == std::tuple{1921, 239, 65});
}

TEST_CASE("test_nested_slurs", "[abc][rhythm]") {
    // Section 4.11 permits both nested slurs and slurs around a single note.
    const auto [score, warnings] = abc::read(kStrictHeader + "%%MIDI trim 1/8\nC ((D)EF) G |]");
    CHECK(warnings.empty());
    CHECK(
        melody(score)
        == Notes{
            {1, 419, 60},
            {481, 479, 62},
            {961, 479, 64},
            {1441, 419, 65},
            {1921, 419, 67},
        }
    );
}

TEST_CASE("test_deprecated_tempo_units", "[abc][rhythm]") {
    // ABC 2.1 section 10.1 recommends accepting both unit-relative forms.
    const auto [score, warnings] = abc::read(
        kHeader + R"(C [L:1/8][Q:120] D [L:1/16][Q:C=120] E [Q:"a ] label" 1/4=120] F)"
    );
    CHECK(warnings.empty());
    REQUIRE(score.tempos->size() == 4);
    CHECK(score.tempos->at(1).mspq == 1000000);
    CHECK(score.tempos->at(2).mspq == 2000000);
    CHECK(score.tempos->at(3).mspq == 500000);
}

TEST_CASE("test_grace_broken_weights", "[abc][rhythm]") {
    // ABC 2.1 sections 4.4 and 4.12: a grace pair uses the written dotted ratio.
    const auto [score, warnings] = abc::read(kStrictHeader + "{a>b}C D |]");
    CHECK(warnings.empty());
    CHECK(melody(score) == Notes{{1, 179, 81}, {181, 59, 83}, {241, 239, 60}, {481, 479, 62}});

    for (const auto& [marked, weighted] : {
             std::pair{"{/a>>b}C D |]", "{/a7/4b/4}C D |]"},
             std::pair{"{a2<<b2}C2 D |]", "{a/2b7/2}C2 D |]"},
             std::pair{"{a>[bg]}C D |]", "{a3/2[bg]/2}C D |]"},
             std::pair{"A<{a>b}A B |]", "A/2{a3/2b/2}A3/2 B |]"},
             std::pair{"A{a>b}<A B |]", "A/2{a3/2b/2}A3/2 B |]"},
         }) {
        CAPTURE(marked);
        const auto actual   = abc::read(kStrictHeader + marked);
        const auto expected = abc::read(kStrictHeader + weighted);
        CHECK(actual.warnings.empty());
        CHECK(expected.warnings.empty());
        CHECK(actual.value == expected.value);
    }
}

TEST_CASE("test_incomplete_grace_pair", "[abc][rhythm]") {
    const auto [score, warnings] = abc::read(kStrictHeader + "{>ab>}C {a>b2}D |]", "grace.abc");
    const auto expected          = abc::read(kStrictHeader + "{ab}C {ab2}D |]");
    REQUIRE(warnings.size() == 3);
    CHECK(warnings.at(0).starts_with("grace.abc:7:2: ABC [unexpected-broken-rhythm]"));
    CHECK(warnings.at(1).starts_with("grace.abc:7:5: ABC [unfinished-broken-rhythm]"));
    CHECK(warnings.at(2).starts_with("grace.abc:7:11: ABC [unequal-broken-rhythm]"));
    CHECK(expected.warnings.empty());
    CHECK(score == expected.value);
}

TEST_CASE("test_named_tempo", "[abc][rhythm]") {
    const auto [score, warnings] = abc::read(kStrictHeader + R"(C [Q:"Andante"] D)");
    CHECK(warnings.empty());
    REQUIRE(score.tempos->size() == 2);
    CHECK(score.tempos->back().mspq == 681818);
}

TEST_CASE("test_text_only_tempo", "[abc][rhythm]") {
    const std::string prefix = "%abc-2.1\nX:1\nT:Example\nM:4/4\nL:1/4\n";
    for (const auto& [tempo_fields, expected] :
         {std::pair{"Q:1/4=90\nQ:\"Slowly with feeling\"\n", 666666},
          {"Q:\"Andante\"\nQ:\"\"\n", 681818}}) {
        CAPTURE(tempo_fields);
        const auto [score, warnings]
            = abc::read(prefix + tempo_fields + "K:C\nC D E F |]", "tempo.abc");
        REQUIRE(warnings.size() == 1);
        CHECK(warnings.front().starts_with("tempo.abc:7:1: ABC [text-tempo]"));
        REQUIRE(score.tempos->size() == 1);
        CHECK(score.tempos->front() == Tempo<Tick>{0, expected});
    }
}

TEST_CASE("test_written_lengths", "[abc][rhythm]") {
    // Sections 4.3 and 4.17 multiply both length factors in [C/3E/3]2 to give 2/3 unit.
    const Passage cases[] = {
        {"C// z/ D3/2", {{1, 119, 60}, {361, 719, 62}}},
        {"C/3 z/5 D", {{1, 159, 60}, {257, 479, 62}}},
        {"[C/3E/3]2 F", {{1, 319, 60}, {11, 309, 64}, {321, 479, 65}}},
        {"[Cz/3] D", {{1, 479, 60}, {481, 479, 62}}},
        {"(3CDE F", {{1, 319, 60}, {321, 319, 62}, {641, 319, 64}, {961, 479, 65}}},
        {"(3:4:2 CD F", {{1, 639, 60}, {641, 639, 62}, {1281, 479, 65}}},
        {"C>!tenuto![K:G]F", {{1, 719, 60}, {721, 239, 66}}},
        {"[Cz2E]/2 X D", {{1, 239, 60}, {11, 229, 64}, {2161, 479, 62}}},
    };
    for (const auto& [music, notes] : cases) {
        CAPTURE(music);
        const auto [score, warnings] = abc::read(kHeader + music);
        CHECK(warnings.empty());
        CHECK(melody(score) == notes);
    }
}

TEST_CASE("test_adjacent_part_rhythm", "[abc][rhythm]") {
    const std::string prefix = "%abc-2.1\nX:1\nT:Part labels\nM:2/4\nL:1/4\nP:";

    SECTION("A grace group borrows from its host in the next part") {
        const auto [score, warnings] = abc::read(prefix + "AB\nK:C\n[P:A]C {^F}[P:B] F f |]");
        CHECK(warnings.empty());
        CHECK(melody(score) == Notes{{1, 479, 60}, {481, 119, 66}, {601, 359, 66}, {961, 479, 78}});
    }

    SECTION("Jumping past a grace group gives its full duration to the host") {
        const auto [score, warnings] = abc::read(prefix + "B\nK:C\n[P:A]C {^F}[P:B] F f |]");
        CHECK(warnings.empty());
        CHECK(melody(score) == Notes{{1, 479, 66}, {481, 479, 78}});
    }

    SECTION("A broken pair uses each part's own verse occurrence") {
        const auto [score, warnings] = abc::read(
            prefix + "BAB\nK:C\nP:A\nC>\nw: left\nP:B\nD E |]\nw: first end\nw: second end\n"
        );
        CHECK(warnings.empty());
        CHECK(
            melody(score)
            == Notes{{1, 239, 62}, {241, 479, 64}, {721, 719, 60}, {1441, 239, 62}, {1681, 479, 64}}
        );
        CHECK(
            sung(score)
            == Lyrics{{1, "first"}, {241, "end"}, {721, "left"}, {1441, "second"}, {1681, "end"}}
        );
    }

    SECTION("The rhythm mark can appear after the part label") {
        const auto [score, warnings] = abc::read(prefix + "AB\nK:C\n[P:A]C [P:B] >{g}D E |]");
        CHECK(warnings.empty());
        CHECK(melody(score) == Notes{{1, 719, 60}, {721, 119, 79}, {841, 119, 62}, {961, 479, 64}});
    }

    SECTION("An implicit repeat opening lets the first pass complete the broken pair") {
        const auto [score, warnings] = abc::read(prefix + "AB\nK:C\n[P:A]C >[P:B] D E :|");
        CHECK(warnings.empty());
        CHECK(
            melody(score)
            == Notes{{1, 719, 60}, {721, 239, 62}, {961, 479, 64}, {1441, 239, 62}, {1681, 479, 64}}
        );
    }
}

TEST_CASE("test_reordered_broken_pairs", "[abc][rhythm]") {
    // ABC 2.1 sections 3.1.9 and 4.4: the explicit lengths are equivalent on every visit.
    const std::string prefix = "%abc-2.1\nX:1\nT:Broken rhythm\nM:2/4\nL:1/4\nP:";
    for (const auto order : {"A", "B", "AB", "BA", "BAB"}) {
        const auto start = prefix + order + "\nK:C\n";
        for (const auto& [pair, expanded] : {
                 std::pair{"[P:A]C >[P:B] D E :|", "[P:A]C3/2 [P:B] D/2 E :|"},
                 {"[P:A]C [P:B] >D E :|", "[P:A]C3/2 [P:B] D/2 E :|"},
                 {"[P:A]C <<[P:B][L:1/8] D2 E |]", "[P:A]C/4 [P:B][L:1/8] D7/2 E |]"},
             }) {
            CAPTURE(order, pair);
            const auto actual   = abc::read(start + pair);
            const auto expected = abc::read(start + expanded);
            CHECK(actual.warnings.empty());
            CHECK(expected.warnings.empty());
            CHECK(actual.value == expected.value);
        }
    }
}

TEST_CASE("test_broken_pair_fields", "[abc][rhythm]") {
    const std::string prefix = "%abc-2.1\nX:1\nT:Broken rhythm\nM:4/4\nL:1/4\n";
    for (const auto order : {"", "P:AB\n"}) {
        const auto start = prefix + order + "K:C\n";
        for (const auto effect : {"!mf!", "!ped!", "[Q:1/4=90]", "[K:G]", "[L:1/4]"}) {
            CAPTURE(order, effect);
            const auto actual   = abc::read(start + "[P:A] C " + effect + " > [P:B] D E |]");
            const auto expanded = abc::read(start + "[P:A] C3/2 " + effect + " [P:B] D/2 E |]");
            CHECK(actual.warnings.empty());
            CHECK(expanded.warnings.empty());
            CHECK(actual.value == expanded.value);
        }
    }
}

TEST_CASE("test_cross_part_chained_pairs", "[abc][rhythm]") {
    const std::string prefix = "%abc-2.1\nX:1\nT:Broken rhythm\nM:4/4\nL:1/4\nP:";
    for (const auto& [order, pair, expanded] : {
             std::tuple{"ABC", "[P:A]C>[P:B]D>[P:C]E F", "[P:A]C3/2[P:B]D/2[P:C]E F"},
             {"AB", "[P:A]C>D>[P:B]E F", "[P:A]C3/2 D/2[P:B]E F"},
             {"B", "[P:A]C>D>[P:B]E F", "[P:A]C3/2 D/2[P:B]E F"},
         }) {
        CAPTURE(order, pair);
        const auto start  = prefix + order + "\nK:C\n";
        const auto actual = abc::read(start + pair);
        REQUIRE(actual.warnings.size() == 1);
        CHECK(actual.warnings.front().find("[unequal-broken-rhythm]") != std::string::npos);
        CHECK(actual.value == abc::read(start + expanded).value);
    }
}

TEST_CASE("test_shortened_pair_start", "[abc][rhythm]") {
    const std::string prefix = "%abc-2.1\nX:1\nT:Broken rhythm\nM:4/4\nL:1/4\nP:";
    for (const auto order : {"AB", "B"}) {
        CAPTURE(order);
        const auto start  = prefix + order + "\nK:C\n";
        const auto actual = abc::read(start + "[P:A]C>D>[P:B]E/2 F");
        CHECK(actual.warnings.empty());
        CHECK(actual.value == abc::read(start + "[P:A]C3/2 D3/4[P:B]E/4 F").value);
    }
}

TEST_CASE("test_reordered_tuplet_slots", "[abc][rhythm]") {
    // Sections 3.1.9 and 4.13: the ratio applies to the next written notes.
    const std::string prefix = "%abc-2.1\nX:1\nT:Tuplets\nM:2/4\nL:1/4\nP:";
    for (const auto order : {"A", "B", "AB", "BA"}) {
        CAPTURE(order);
        const auto start    = prefix + order + "\nK:C\n";
        const auto actual   = abc::read(start + "[P:A](3 C [P:B] D E F :|");
        const auto expanded = abc::read(start + "[P:A]C2/3 [P:B] D2/3 E2/3 F :|");
        CHECK(actual.warnings.empty());
        CHECK(expanded.warnings.empty());
        CHECK(actual.value == expanded.value);
    }

    const auto [score, warnings] = abc::read(prefix + "B\nK:C\n[P:A](3 C [P:B] {g}D E F |]");
    CHECK(warnings.empty());
    CHECK(melody(score) == Notes{{1, 79, 79}, {81, 239, 62}, {321, 319, 64}, {641, 479, 65}});
}

TEST_CASE("test_cross_part_harp_attacks", "[abc][rhythm]") {
    const std::string prefix = "%abc-2.1\nX:1\nT:Harp\nM:2/4\nL:1/4\nP:";
    const auto        check  = [&](std::string_view order, std::string_view music, Notes expected) {
        CAPTURE(order, music);
        const auto tune = fmt::format("{}{}\n%%MIDI harpmode 1\nK:C\n{}", prefix, order, music);
        const auto [score, warnings] = abc::read(tune);
        CHECK(warnings.empty());
        CHECK(melody(score) == expected);
    };
    check(
        "B", "[P:A]C >[P:B] ~D E |]", {{1, 59, 62}, {61, 59, 62}, {121, 119, 62}, {241, 479, 64}}
    );
    check(
        "AB",
        "[P:A]C [P:B] >~D E |]",
        {{1, 719, 60}, {721, 59, 62}, {781, 59, 62}, {841, 119, 62}, {961, 479, 64}}
    );
    check(
        "BA",
        "[P:A]~C2 >[P:B] D E |]",
        {{1, 239, 62}, {241, 479, 64}, {721, 239, 60}, {961, 239, 60}, {1201, 719, 60}}
    );
    check(
        "A",
        "[I:MIDI fermatafixed][P:A]H~C3 >[P:B] D2 E |]",
        {{1, 479, 60}, {481, 479, 60}, {961, 1439, 60}}
    );
}

TEST_CASE("test_fermata_grace_order", "[abc][rhythm]") {
    const std::string prefix = "%abc-2.1\nX:1\nT:Written timing\nM:2/4\nL:1/4\nP:B\nK:C\n";
    for (const auto& [command, host, expected] : {
             std::tuple{"fermatafixed", "H D2", Notes{{1, 1439, 62}, {1441, 479, 64}}},
             {"gracedivider 8", "{g}D2", Notes{{1, 59, 79}, {61, 899, 62}, {961, 479, 64}}},
             {"grace 1/2", "{g}D2", Notes{{1, 479, 79}, {481, 479, 62}, {961, 479, 64}}},
         }) {
        CAPTURE(command);
        const auto [score, warnings]
            = abc::read(prefix + fmt::format("[P:A][I:MIDI {}] C [P:B] {} E |]", command, host));
        CHECK(warnings.empty());
        CHECK(melody(score) == expected);
    }

    const auto [rewound, warnings] = abc::read(
        "%abc-2.1\nX:1\nT:Grace\nM:2/4\nL:1/4\nP:BA\nK:C\n"
        "[P:A]{g}[I:MIDI gracedivider 8] D2 [P:B] {a}E2 F |]"
    );
    CHECK(warnings.empty());
    CHECK(
        melody(rewound)
        == Notes{{1, 59, 81}, {61, 899, 64}, {961, 479, 65}, {1441, 119, 79}, {1561, 839, 62}}
    );

    const auto [voices, voice_warnings] = abc::read(
        kStrictHeader
        + "[V:one][I:MIDI fermatafixed][I:MIDI gracedivider 8] C "
          "[V:two] H D2 {a}E |]"
    );
    CHECK(voice_warnings.empty());
    REQUIRE(voices.tracks->size() == 2);
    const auto& notes = *voices.tracks->at(1)->notes;
    REQUIRE(notes.size() == 3);
    CHECK(notes[0].duration == 1439);
    CHECK(notes[1].time == 1441);
    CHECK(notes[1].duration == 59);
    CHECK(notes[2].time == 1501);
}

TEST_CASE("test_broken_pair_annotation", "[abc][rhythm]") {
    for (const char placement : std::string_view("^_<>@")) {
        CAPTURE(placement);
        const auto [score, warnings]
            = abc::read(kStrictHeader + fmt::format(R"(C "{}text" >D EF |])", placement));
        CHECK(warnings.empty());
        CHECK(
            melody(score) == Notes{{1, 719, 60}, {721, 239, 62}, {961, 479, 64}, {1441, 479, 65}}
        );
    }
}

TEST_CASE("test_broken_pair_chord_symbol", "[abc][rhythm]") {
    const auto prefix                      = kStrictHeader + "%%MIDI gchord fccc\n";
    const auto [expected, common_warnings] = abc::read(prefix + R"("C"C>"F"D EF |])");
    REQUIRE(common_warnings.empty());
    REQUIRE(expected.tracks->size() == 3);
    CHECK(expected.tracks->front()->notes->at(1).time == 721);

    const auto [score, warnings] = abc::read(prefix + R"("C"C"F">D EF |])");
    CHECK(warnings.empty());
    CHECK(score == expected);
}

TEST_CASE("test_staccato_grace_host", "[abc][rhythm]") {
    const auto [score, warnings]
        = abc::read(fmt::format("{}{}C {{a}}.D2 E | GABc |]", kHeader, kStressProfile));
    REQUIRE(warnings.empty());
    const auto& notes = *score.tracks->front()->notes;
    REQUIRE(notes.size() == 8);
    CHECK(notes[1] == Note<Tick>{673, 71, 81, 75});
    CHECK(notes[2] == Note<Tick>{745, 215, 62, 75});
    CHECK(notes[3] == Note<Tick>{1537, 383, 64, 65});
}

TEST_CASE("test_fermata_grace_fraction", "[abc][rhythm]") {
    const auto [score, warnings] = abc::read(
        fmt::format("{}{}%%MIDI grace 1/2\nC {{a}}HD E | F4 |]", kHeader, kStressProfile)
    );
    REQUIRE(warnings.empty());
    const auto& notes = *score.tracks->front()->notes;
    REQUIRE(notes.size() == 5);
    CHECK(notes[1] == Note<Tick>{673, 287, 81, 75});
    CHECK(notes[2] == Note<Tick>{961, 575, 62, 102});
    CHECK(notes[3] == Note<Tick>{1537, 383, 64, 65});
}

TEST_CASE("test_deferred_fermata_mode", "[abc][rhythm]") {
    const auto [score, warnings] = abc::read(
        fmt::format(
            "{}{}%%MIDI grace 3/4\n{{a}}[I:MIDI fermatafixed] HC3 | D4 |]", kHeader, kStressProfile
        )
    );
    REQUIRE(warnings.empty());
    const auto& notes = *score.tracks->front()->notes;
    REQUIRE(notes.size() == 3);
    CHECK(notes[0] == Note<Tick>{1, 1103, 81, 112});
    CHECK(notes[1] == Note<Tick>{1105, 815, 60, 102});
    CHECK(notes[2].time == 1921);
}

TEST_CASE("test_free_meter_pulse", "[abc][rhythm]") {
    const std::string prefix   = "%abc-2.1\nX:1\nT:Free\nL:1/4\n";
    const auto        original = abc::read(prefix + "M:none\nK:C\nC [M:3/4] D3 | [M:none] E F |]");
    REQUIRE(original.warnings.empty());
    const auto& score = original.value;
    CHECK(
        *score.time_signatures == pyvec<TimeSignature<Tick>>{{0, 4, 4}, {480, 3, 4}, {1920, 4, 4}}
    );

    const auto exported = abc::write(score);
    const auto reloaded = abc::read(exported.value);
    CHECK(reloaded.warnings.empty());
    CHECK(reloaded.value == score);
    const auto midi = Score<Tick>::parse<DataFormat::MIDI>(score.dumps<DataFormat::MIDI>());
    CHECK(*midi.time_signatures == *score.time_signatures);
}

TEST_CASE("test_additive_meter_signature", "[abc][rhythm]") {
    const auto original = abc::read(
        "%abc-2.1\nX:1\nT:Groups\nL:1/8\nM:( 2 + 3 + 2 ) / 8\nK:C\n"
        "C D E F G A B | [M:3+2+2/8] c B A G F E D |]"
    );
    REQUIRE(original.warnings.empty());
    const auto& score = original.value;
    CHECK(*score.time_signatures == pyvec<TimeSignature<Tick>>{{0, 7, 8}, {1680, 7, 8}});
    const auto exported = abc::write(score);
    CHECK_THAT(exported.value, Catch::Matchers::ContainsSubstring("\nM:7/8\n"));
    const auto reloaded = abc::read(exported.value);
    CHECK(reloaded.warnings.empty());
    CHECK(reloaded.value == score);
    CHECK(Score<Tick>::parse<DataFormat::ZPP>(score.dumps<DataFormat::ZPP>()) == score);
}

TEST_CASE("test_trailing_rests", "[abc][rhythm]") {
    const std::string prefix = "%abc-2.1\nX:1\nT:Rests\nM:3/4\nL:1/4\nK:C\n";
    for (const auto* music : {"C3 | Z2 |]", "C3 | X2 |]", "C3 | z3 |]", "C3 | x3 |]"}) {
        CAPTURE(music);
        const auto original = abc::read(prefix + music);
        REQUIRE(original.warnings.empty());
        CHECK(original.value.end() == original.value.tracks->front()->end());
        CHECK(original.value.end() == 1440);
    }
}

TEST_CASE("test_implicit_free_meter", "[abc][rhythm]") {
    // Sections 3.1.6–3.1.7 give omitted meter the free-meter pulse and an eighth-note unit.
    const std::string music         = "K:C\nCDEF||GABc|";
    const auto [implicit, warnings] = abc::read("X:1\nT:Free meter\n" + music);
    const auto explicit_m           = abc::read("X:1\nT:Free meter\nM:none\n" + music).value;
    REQUIRE(warnings.empty());
    const auto& notes = *implicit.tracks->front()->notes;
    CHECK(notes == *explicit_m.tracks->front()->notes);
    REQUIRE(notes.size() == 8);
    CHECK(notes[1].time == 241);
    CHECK(notes[4].velocity == 80);
}

TEST_CASE("test_grace_tie_release", "[abc][rhythm]") {
    // abc2midi leaves the tied C at its joined release and adds 60 ticks to untied notes.
    const Passage cases[] = {
        {"C-C D E | z4 |]", {{1, 959, 60}, {961, 539, 62}, {1441, 539, 64}}},
        {"C- {C}D E z |]", {{1, 599, 60}, {601, 419, 62}, {961, 539, 64}}},
        {"{C-C}D E z2 |]", {{1, 239, 60}, {241, 299, 62}, {481, 539, 64}}},
        {"C- {[C]}D E z |]", {{1, 599, 60}, {601, 419, 62}, {961, 539, 64}}},
        {"{C-}C E z2 |]", {{1, 479, 60}, {481, 539, 64}}},
    };
    for (const auto& version : kVersionLines) {
        const auto opening = version + kHeader + "%%MIDI expand 1/8\n";
        for (const auto& [music, notes] : cases) {
            CAPTURE(version, music);
            const auto [score, warnings] = abc::read(opening + music);
            REQUIRE(warnings.empty());
            CHECK(melody(score) == notes);
        }
    }
}

TEST_CASE("test_unequal_tuplet_atoms", "[abc][rhythm]") {
    const auto [score, warnings]
        = abc::read(kStrictHeader + "%%MIDI chordattack 0\n(3::3 [C2E]/ y z/ D2 F");
    REQUIRE(warnings.empty());
    const auto& notes = *score.tracks->front()->notes;
    REQUIRE(notes.size() == 4);
    CHECK(notes[0].duration == 319);
    CHECK(notes[1].duration == 159);
    CHECK(notes[2].time == 481);
    CHECK(notes[2].duration == 639);
    CHECK(notes[3].time == 1121);
}

TEST_CASE("test_implicit_tuplet_ratios", "[abc][rhythm]") {
    // Under section 4.13, five and seven notes take three units in compound meter and two
    // otherwise.
    struct Case {
        const char* opening;
        const char* music;
        i32         next_onset;
    };
    for (const auto& version : kVersionLines) {
        CAPTURE(version);
        for (const auto& [opening, music, next_onset] : {
                 Case{"3/4", "(5 CDEFG A", 961},
                 Case{"6/8", "(5 CDEFG A", 1441},
                 Case{"6/8", "C[M:4/4](5::1 D E", 673},
                 Case{"4/4", "C[M:9/8](7::1 D E", 686},
                 Case{"4/4", "C {A}[M:6/8](5::1 D E", 769},
                 Case{"6/8", "(5:2:1 C D", 193},
             }) {
            CAPTURE(opening, music);
            const auto [score, warnings]
                = abc::read(version + "X:1\nT:\nM:" + opening + "\nL:1/4\nK:C\n" + music);
            REQUIRE(warnings.empty());
            CHECK(score.tracks->front()->notes->back().time == next_onset);
        }
    }
}

TEST_CASE("test_hornpipe_meter_change", "[abc][rhythm]") {
    const auto [score, warnings]
        = abc::read("X:1\nM:4/4\nL:1/8\nR:hornpipe\nK:C\nC<D | [M:6/8]EF|");
    REQUIRE(warnings.empty());
    const auto& notes = *score.tracks->front()->notes;
    REQUIRE(notes.size() == 4);
    CHECK(notes[1].time == 161);
    CHECK(notes[2].time == 481);
    CHECK(notes[3].time == 801);
    CHECK(notes[2].duration == 319);
    CHECK(notes[3].duration == 159);

    const auto sixteenths = abc::read(
        "X:1\nT:\nM:2/4\nL:1/16\nR:hornpipe\nK:C\n"
        "CDEF GABC | [M:4/4] CDEF GABC CDEF GABC |]"
    );
    REQUIRE(sixteenths.warnings.empty());
    const auto& switched = *sixteenths.value.tracks->front()->notes;
    REQUIRE(switched.size() == 24);
    CHECK(switched[8].time == 961);
    CHECK(switched[9].time == 1121);
    CHECK(switched[9].duration == 79);
}

TEST_CASE("test_midpair_meter_change", "[abc][rhythm]") {
    const auto [score, warnings]
        = abc::read("%abc-2.1\nX:1\nT:\nM:4/4\nL:1/8\nR:hornpipe\nK:C\nC[M:6/8]DEFGA |]");
    REQUIRE(warnings.empty());
    const auto& notes = *score.tracks->front()->notes;
    REQUIRE(notes.size() == 6);
    CHECK(notes[0].duration == 319);
    CHECK(notes[1].time == 321);
    CHECK(notes[1].duration == 159);
    CHECK(notes[3].time == 801);
    REQUIRE(score.time_signatures->size() == 2);
    CHECK(score.time_signatures->back() == TimeSignature<Tick>{320, 6, 8});
}

TEST_CASE("test_free_meter_hornpipe", "[abc][rhythm]") {
    for (const auto meter : {"", "M:none\n"}) {
        CAPTURE(meter);
        const auto [score, warnings] = abc::read(
            fmt::format("%abc-2.1\nX:1\nT:\n{}L:1/8\nR:hornpipe\nK:C\nCDEF GABC |]", meter)
        );
        REQUIRE(warnings.empty());
        const auto& notes = *score.tracks->front()->notes;
        REQUIRE(notes.size() == 8);
        CHECK(notes[1].time == 321);
        CHECK(notes[1].duration == 159);
    }
}

TEST_CASE("test_cut_time_hornpipe", "[abc][rhythm]") {
    const auto [score, warnings]
        = abc::read("X:1\nT:\nM:C|\nL:1/16\nR:hornpipe\nK:C\nCDEF GABC [R:hornpipe] CDEF GABC |]");
    REQUIRE(warnings.size() == 1);
    CHECK(warnings.front().starts_with("<string>:5:1: ABC [hornpipe-meter]"));
    const auto& notes = *score.tracks->front()->notes;
    REQUIRE(notes.size() == 16);
    CHECK(notes[2].time == 241);
    CHECK(notes[9].time == 1081);
    CHECK(notes[10].time == 1241);
    CHECK(notes[10].duration == 79);
}

TEST_CASE("test_hornpipe_opening_meter", "[abc][rhythm]") {
    const auto [score, warnings]
        = abc::read("X:1\nT:\nM:6/8\nL:1/8\nK:C\n[R:hornpipe] C>DEFGA | [M:4/4] CDEF GABC |]");
    REQUIRE(warnings.empty());
    const auto& notes = *score.tracks->front()->notes;
    REQUIRE(notes.size() == 14);
    CHECK(notes[1].time == 321);   // The explicit pair still uses the 2:1 ratio.
    CHECK(notes[7].time == 1681);
    CHECK(notes[7].duration == 239);
}

TEST_CASE("test_midpair_rhythm_change", "[abc][rhythm]") {
    constexpr std::string_view start = "%abc-2.1\nX:1\nT:\nM:4/4\nL:1/8\nK:C\n";
    for (const auto body : {
             "CD E [R:hornpipe] F GABC |]",
             "CD E\nR:Hornpipe\nF GABC |]",
             "CD E [R:hornpipe] F [R:reel] GABC |]",
         }) {
        CAPTURE(body);
        const auto [score, warnings] = abc::read(fmt::format("{}{}", start, body));
        REQUIRE(warnings.empty());
        const auto& notes = *score.tracks->front()->notes;
        REQUIRE(notes.size() == 8);
        vec<i32> times;
        vec<i32> lengths;
        for (const auto& note : notes) {
            times.push_back(note.time);
            lengths.push_back(note.duration);
        }
        CHECK(times == vec<i32>{1, 241, 481, 801, 961, 1281, 1441, 1761});
        CHECK(lengths == vec<i32>{239, 239, 319, 159, 319, 159, 319, 159});
    }
}

TEST_CASE("test_cross_voice_rhythm_order", "[abc][rhythm]") {
    const auto [score, warnings] = abc::read(
        "%abc-2.1\nX:1\nT:\nM:4/4\nL:1/8\nK:C\n"
        "[V:lead] |: CDEF CDEF |\n"
        "[V:bass] [R:hornpipe] cdef cdef |]\n"
        "[V:lead] EFGA Bcde :|\n"
    );
    REQUIRE(warnings.empty());
    REQUIRE(score.tracks->size() == 2);
    const auto& lead = *score.tracks->at(0)->notes;
    const auto& bass = *score.tracks->at(1)->notes;
    REQUIRE(lead.size() == 32);
    REQUIRE(bass.size() == 8);
    CHECK(lead[1].time == 241);
    CHECK(lead[9].time == 2241);
    CHECK(lead[17].time == 4081);
    CHECK(lead[25].time == 6081);
    CHECK(bass[1].time == 321);
}

TEST_CASE("test_hornpipe_broken_ratio", "[abc][rhythm]") {
    constexpr std::string_view start = "X:1\nT:\nM:4/4\nL:1/8\n";
    for (const auto [music, second, fourth] : {
             std::tuple{"%%MIDI ratio 3 1\nR:hornpipe\nK:C\nC>DEF GABC |]", 321, 801},
             {"R:hornpipe\n%%MIDI ratio 3 1\nK:C\nC>DEF GABC |]", 361, 841},
             {"R:hornpipe\n%%MIDI ratio 3 1\nR:reel\nK:C\nC>DEF GABC |]", 361, 841},
             {"K:C\n[I:MIDI ratio 3 1] C>[R:hornpipe] DEF GABC |]", 321, 801},
             {"K:C\n[R:hornpipe] C>[I:MIDI ratio 3 1] D E[R:Hornpipe]F GABC |]", 361, 801},
         }) {
        CAPTURE(music);
        const auto [score, warnings] = abc::read(fmt::format("{}{}", start, music));
        REQUIRE(warnings.empty());
        const auto& notes = *score.tracks->front()->notes;
        REQUIRE(notes.size() == 8);
        CHECK(notes[1].time == second);
        CHECK(notes[3].time == fourth);
    }
}

TEST_CASE("test_reordered_broken_ratio", "[abc][rhythm]") {
    constexpr std::string_view start = "X:1\nT:\nM:4/4\nL:1/8\n";
    for (const auto [music, expected] : {
             std::pair{"K:C\n|: C>D z6| E>[I:MIDI ratio 2 1]F z6 :|", vec<i32>{360, 320, 360, 320}},
             {"P:BAB\nK:C\nP:A\nC>D z6|\n%%MIDI ratio 2 1\nP:B\nE>F z6|", {320, 360, 320}},
         }) {
        CAPTURE(music);
        const auto [score, warnings] = abc::read(fmt::format("{}{}", start, music));
        REQUIRE(warnings.empty());
        const auto& notes = *score.tracks->front()->notes;
        REQUIRE(notes.size() == 2 * expected.size());
        vec<i32> gaps;
        for (auto note = notes.begin(); note != notes.end(); note += 2)
            gaps.push_back((note + 1)->time - note->time);
        CHECK(gaps == expected);
    }
}

TEST_CASE("test_cross_unit_broken_pair", "[abc][rhythm]") {
    const std::string start            = "X:1\nT:\nM:4/4\nL:1/8\nK:C\n";
    const auto [equal, equal_warnings] = abc::read(start + "C>[L:1/16]D2 E2F2 G2A2B2c2 |]");
    REQUIRE(equal_warnings.empty());
    const auto& notes = *equal.tracks->front()->notes;
    REQUIRE(notes.size() == 8);
    CHECK(notes[0].duration == 359);
    CHECK(notes[1].time == 361);
    CHECK(notes[1].duration == 119);
    CHECK(notes[2].time == 481);

    const auto [unequal, warnings] = abc::read(start + "C>[L:1/16]D E2F2 |]", "unit.abc");
    REQUIRE(warnings.size() == 1);
    CHECK(warnings.front().starts_with("unit.abc:6:2: ABC [unequal-broken-rhythm]"));
    const auto& fallback = *unequal.tracks->front()->notes;
    REQUIRE(fallback.size() == 4);
    CHECK(fallback[0].duration == 239);
    CHECK(fallback[1].time == 241);
    CHECK(fallback[1].duration == 119);
}

TEST_CASE("test_adjusted_field_boundary", "[abc][rhythm]") {
    const auto [swing, swing_warnings]
        = abc::read("X:1\nT:\nM:4/4\nL:1/8\nR:hornpipe\nK:C\nC[L:1/8][Q:1/4=90]DEF GABC |]");
    REQUIRE(swing_warnings.empty());
    REQUIRE(swing.tracks->front()->notes->size() == 8);
    CHECK(swing.tracks->front()->notes->at(1).time == 321);
    REQUIRE(swing.tempos->size() == 2);
    CHECK(swing.tempos->back() == Tempo<Tick>{321, 666666});

    const auto [relative, warnings]
        = abc::read("X:1\nT:\nM:4/4\nL:1/8\nK:C\nC>[L:1/16][Q:C=90]D2 E2F2 |]");
    REQUIRE(warnings.empty());
    REQUIRE(relative.tempos->size() == 2);
    CHECK(relative.tempos->back() == Tempo<Tick>{361, 2666666});
}

TEST_CASE("test_grace_group_unit", "[abc][rhythm]") {
    constexpr std::string_view start = "X:1\nT:\nM:4/4\nL:1/8\nK:C\n";
    for (const auto [pair, grace_length] : {
             std::pair{"C>{G}[L:1/16]D2", 59},
             {"C>[L:1/16]{G}D2", 29},
         }) {
        CAPTURE(pair);
        const auto [score, warnings] = abc::read(fmt::format("{}{} E2F2 G2A2B2c2 |]", start, pair));
        REQUIRE(warnings.empty());
        const auto& notes = *score.tracks->front()->notes;
        REQUIRE(notes.size() == 9);
        CHECK(notes[1].pitch == 67);
        CHECK(notes[1].time == 361);
        CHECK(notes[1].duration == grace_length);
        CHECK(notes[2].time == 362 + grace_length);
        CHECK(notes[3].time == 481);
        CHECK(notes[3].duration == 239);
        CHECK(notes[4].time == 721);
    }
}

TEST_CASE("test_grace_tuplet_timing", "[abc][rhythm]") {
    struct Case {
        std::string_view music;
        i32              grace_start;
        i32              grace_gate;
        i32              following_start;
        i32              last_start;
    };
    for (const auto& [music, start, gate, following, last] : {
             Case{"(3C{d}E F G |]", 321, 79, 401, 961},
             Case{"{d}(3CEF G |]", 1, 119, 121, 961},
             Case{"(3CEF {d}G |]", 961, 119, 1081, 1081},
             Case{"%%MIDI gracedivider 6\n(3C{d}E F G |]", 321, 52, 374, 961},
             Case{"%%MIDI grace 1/4\n(3C{d}E F G |]", 321, 79, 401, 961},
             Case{"(2{d}CD E |]", 1, 179, 181, 1441},
         }) {
        CAPTURE(music);
        const auto [score, warnings] = abc::read(kHeader + std::string(music));
        CHECK(warnings.empty());
        const auto& notes = *score.tracks->front()->notes;
        const auto  grace = std::ranges::find(notes, 74, &Note<Tick>::pitch);
        REQUIRE(grace != notes.end());
        REQUIRE(std::next(grace) != notes.end());
        CHECK(grace->time == start);
        CHECK(grace->duration == gate);
        CHECK(std::next(grace)->time == following);
        // The grace steals from its host without consuming one of the tuplet's notes.
        CHECK(notes.back().time == last);
    }
}

TEST_CASE("test_grace_host_meter", "[abc][rhythm]") {
    const auto [score, warnings] = abc::read("X:1\nT:\nM:4/4\nL:1/4\nK:C\nD {F}[M:6/8]F Z G |]");
    REQUIRE(warnings.empty());
    const auto& notes = *score.tracks->front()->notes;
    REQUIRE(notes.size() == 4);
    CHECK(notes.back().time == 2401);
}

TEST_CASE("test_grace_chord_tuplet_slot", "[abc][rhythm]") {
    // Sections 4.12–4.13 place grace outside the counted notes.
    const auto [score, warnings] = abc::read(kHeader + "(3C{[de]}E F G |]");
    CHECK(warnings.empty());
    const auto& notes = *score.tracks->front()->notes;
    REQUIRE(notes.size() == 6);
    CHECK(notes[1] == Note<Tick>{321, 79, 74, 65});
    CHECK(notes[3] == Note<Tick>{401, 239, 64, 65});
    CHECK(notes[4] == Note<Tick>{641, 319, 65, 65});
    CHECK(notes[5].time == 961);
}

TEST_CASE("test_harp_unit_change", "[abc][rhythm]") {
    constexpr std::string_view start = "X:1\nT:\nM:4/4\nL:1/8\n%%MIDI harpmode 1\nK:C\n";
    for (const auto [music, expected] : {
             std::pair{"~C>[L:1/16]D EF |]", vec<i32>{1, 61, 121, 301, 361, 481}},
             {"C>[L:1/16]~D2 E2F2 |]", {1, 181, 211, 241, 301, 541}},
         }) {
        CAPTURE(music);
        const auto [score, warnings] = abc::read(fmt::format("{}{}", start, music));
        REQUIRE(warnings.empty());
        vec<i32> times;
        for (const auto& note : *score.tracks->front()->notes) times.push_back(note.time);
        CHECK(times == expected);
    }
}

TEST_CASE("test_fermata_rhythm_order", "[abc][rhythm]") {
    constexpr std::string_view start = "X:1\nT:\nM:4/4\nL:1/8\n%%MIDI fermatafixed\nK:C\n";
    for (const auto [shorthand, explicit_lengths] : {
             std::pair{"HC>[L:1/16]D2", "HC3/2[L:1/16]D"},
             {"C>[L:1/16]HD2", "C3/2[L:1/16]HD"},
         }) {
        CAPTURE(shorthand);
        const auto actual         = abc::read(fmt::format("{}{} E2F2 |]", start, shorthand));
        const auto explicit_score = abc::read(fmt::format("{}{} E2F2 |]", start, explicit_lengths));
        REQUIRE(actual.warnings.empty());
        REQUIRE(explicit_score.warnings.empty());
        CHECK(*actual.value.tracks->front()->notes == *explicit_score.value.tracks->front()->notes);
    }
}

TEST_CASE("test_deferred_host_fermata", "[abc][rhythm]") {
    const auto [score, warnings]
        = abc::read("X:1\nT:\nM:4/4\nL:1/8\n%%MIDI fermatafixed\nK:C\n{G3}[L:1/16]HD E2F2 |]");
    REQUIRE(warnings.empty());
    const auto& notes = *score.tracks->front()->notes;
    REQUIRE(notes.size() == 4);
    CHECK(notes[0].pitch == 67);
    CHECK(notes[0].duration == 179);
    CHECK(notes[1].time == 181);
    CHECK(notes[1].duration == 59);
    CHECK(notes[2].time == 241);
}

TEST_CASE("test_fractional_steps_after_long_rest", "[abc][rhythm]") {
    // One unit is one tick. The three rests put C exactly on an integer tick.
    const auto result = abc::read("X:1\nM:none\nL:1/1920\nK:C\nz2000000000 z/3 z/3 z/3 C3 |]\n");
    REQUIRE(result.value.tracks->size() == 1);
    CHECK(result.value.tracks->front()->notes->front().time == 2'000'000'002);
}

}   // namespace symusic::abc_test

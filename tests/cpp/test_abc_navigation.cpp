#include <fmt/format.h>

#include "abc_test.h"

namespace symusic::abc_test {

TEST_CASE("test_repeat_tied_accidental", "[abc][navigation]") {
    auto [score, warnings] = abc::read("X:1\nM:4/4\nL:1/4\nK:C\n|: ^F2- | F |1 C :|2 D | E");
    REQUIRE(score.ticks_per_quarter == 480);
    REQUIRE(score.tracks->size() == 1);
    const auto& notes = *score.tracks->front()->notes;
    REQUIRE(notes.size() == 5);
    CHECK(notes[0] == Note<Tick>{1, 1439, 66, 90});
    CHECK(notes[1].pitch == 60);
    CHECK(notes[2].pitch == 66);
    CHECK(notes[2].duration == 1439);
    CHECK(notes[3].pitch == 62);
    CHECK(notes[4].pitch == 64);
}

TEST_CASE("test_repeat_key_and_unit", "[abc][navigation]") {
    const auto [score, warnings] = abc::read("X:1\nM:4/4\nL:1/4\nK:C\n|: F [K:G][L:1/8] F2 :| F2");
    REQUIRE(warnings.empty());
    const auto& notes = *score.tracks->front()->notes;
    REQUIRE(notes.size() == 5);
    CHECK(notes[2] == Note<Tick>{961, 479, 65, 90});
    CHECK(notes[3].time == 1441);
    CHECK(notes[4].time == 1921);
}

TEST_CASE("test_invisible_overlay_bar", "[abc][navigation]") {
    const auto [score, warnings]
        = abc::read("X:1\nT:Overlay\nM:none\nL:1/4\nK:C\nC D & E F [|] G A");
    CHECK(warnings.empty());
    REQUIRE(score.tracks->size() == 2);
    const auto& primary = *score.tracks->at(0)->notes;
    const auto& overlay = *score.tracks->at(1)->notes;
    REQUIRE(primary.size() == 4);
    REQUIRE(overlay.size() == 2);
    CHECK(primary[2].pitch == 67);
    CHECK(primary[2].time == 961);
    CHECK(overlay[0].pitch == 64);
    CHECK(overlay[0].time == 1);
}

TEST_CASE("test_free_meter_overlay_origin", "[abc][navigation]") {
    for (const auto* bar : {"[|", "||", "|]"}) {
        CAPTURE(bar);
        const auto [score, warnings]
            = abc::read(std::string("X:1\nM:none\nL:1/4\nK:C\nC D ") + bar + " E F & G A |]");
        CHECK(warnings.empty());
        REQUIRE(score.tracks->size() == 2);
        const auto& overlay = *score.tracks->at(1)->notes;
        REQUIRE(overlay.size() == 2);
        CHECK(overlay.front().pitch == 67);
        CHECK(overlay.front().time == 961);
        CHECK(overlay.back().time == 1441);
    }
}

TEST_CASE("test_ending_routes", "[abc][navigation]") {
    // Section 4.8 uses the latest double bar as the opening of an implicit repeat.
    struct Route {
        const char* music;
        vec<i32>    pitches;
    };
    const Route cases[] = {
        {"C4 || D4 :| E4 |", {60, 62, 62, 64}},
        {"|: C [1 D :| [2 E :| [3 F |", {60, 62, 60, 64, 60, 65}},
        {"|: CD [\"1\"] EF :| [\"2\"] GA | Bc", {60, 62, 64, 65, 60, 62, 64, 65, 67, 69, 71, 72}},
    };
    for (const auto& [music, expected] : cases) {
        CAPTURE(music);
        const auto score = abc::read(kHeader + music).value;
        CHECK(pitches(score) == expected);
    }
}

TEST_CASE("test_implicit_repeat_opening", "[abc][navigation]") {
    struct Route {
        const char* music;
        vec<i32>    notes;
    };
    const Route cases[] = {
        {"C4 :| D4 :| E4 |]", {60, 60, 62, 62, 64}},
        {"C4 || D4 |] E4 [| F4 :| G4 |]", {60, 62, 64, 65, 65, 67}},
        {"C4 || D4 | E4 .| F4 [|] G4 :|", {60, 62, 64, 65, 67, 62, 64, 65, 67}},
        {"|: C4 || D4 :| E4 || F4 :|", {60, 62, 60, 62, 64, 65, 65}},
        {"C4 || D4 |1 E4 :|2 F4 |] G4", {60, 62, 64, 62, 65, 67}},
        {"C4 || D4 :||: E4 || F4 :| G4", {60, 62, 62, 64, 65, 64, 65, 67}},
    };
    for (const auto& [music, expected] : cases) {
        CAPTURE(music);
        const auto [score, warnings] = abc::read(kStrictHeader + music);
        CHECK(warnings.empty());
        CHECK(pitches(score) == expected);
    }
}

TEST_CASE("test_paired_repeat_count", "[abc][navigation]") {
    // ABC 2.1 section 4.8: the matched triple repeat plays three times.
    const auto [score, warnings] = abc::read(kStrictHeader + "|:: C4 D4 ::| E4 |]");
    CHECK(warnings.empty());
    CHECK(pitches(score) == vec{60, 62, 60, 62, 60, 62, 64});
}

TEST_CASE("test_implicit_repeat_endings", "[abc][navigation]") {
    const auto [score, warnings] = abc::read(kStrictHeader + "C D :| E F | [1 G A :| [2 B c |]");
    CHECK(warnings.empty());
    CHECK(pitches(score) == vec{60, 62, 60, 62, 64, 65, 67, 69, 64, 65, 71, 72});
}

TEST_CASE("test_ending_field_change", "[abc][navigation]") {
    // Section 13.1 puts M: between the close and [2 in Old Sir Simon the King.
    const auto [score, warnings]
        = abc::read(kStrictHeader + "|: C D | [1 E F :|\\\nM:3/4\n[2 G A B |]");
    CHECK(warnings.empty());
    CHECK(pitches(score) == vec{60, 62, 64, 65, 60, 62, 67, 69, 71});
    REQUIRE(score.time_signatures->size() == 2);
    const auto& meter = score.time_signatures->back();
    CHECK(meter.time == 2880);
    CHECK(meter.numerator == 3);
    CHECK(meter.denominator == 4);
}

TEST_CASE("test_intermediate_ending_fields", "[abc][navigation]") {
    const auto [score, warnings]
        = abc::read(kStrictHeader + "|:: C D | [1 E F ::| [M:3/4] [2 G A B ::| [3 c d e f |]");
    CHECK(warnings.empty());
    CHECK(pitches(score) == vec{60, 62, 64, 65, 60, 62, 67, 69, 71, 60, 62, 72, 74, 76, 77});
    CHECK(
        score.time_signatures->collect()
        == vec<TimeSignature<Tick>>{{0, 4, 4}, {2880, 3, 4}, {4320, 4, 4}}
    );
}

TEST_CASE("test_dotted_bar", "[abc][navigation]") {
    // Section 4.8 prefixes a bar line with a dot. A dot before a note is staccato.
    for (const std::string_view bar : {"|", "[|", "[|]", "||", "|]", ":|", "::", ":||:"}) {
        CAPTURE(bar);
        const auto [plain, plain_warnings]
            = abc::read(kStrictHeader + fmt::format("C4{}D4 |]", bar));
        const auto [dotted, warnings] = abc::read(kStrictHeader + fmt::format("C4.{}D4 |]", bar));
        REQUIRE(plain_warnings.empty());
        CHECK(warnings.empty());
        CHECK(dotted == plain);
    }
    const auto [staccato, warnings] = abc::read(kStrictHeader + "C4 | .D4 |]");
    CHECK(warnings.empty());
    CHECK(melody(staccato) == Notes{{1, 1919, 60}, {1921, 959, 62}});
}

TEST_CASE("test_thick_repeat_edge", "[abc][navigation]") {
    for (const std::string_view close : {":|]", ":[|", ":|[|"}) {
        CAPTURE(close);
        const auto [score, warnings]
            = abc::read(kStrictHeader + fmt::format("|[|:C4{}D4|]", close));
        CHECK(warnings.empty());
        CHECK(pitches(score) == vec{60, 60, 62});
    }
}

TEST_CASE("test_repeat_counts", "[abc][navigation]") {
    struct Route {
        const char* music;
        vec<i32>    pitches;
    };
    const Route cases[] = {
        {"|:: C :| D", {60, 60, 60, 62}},
        {"C::| D", {60, 60, 60, 62}},
        {"|:: C ::|::: D :::| E", {60, 60, 60, 62, 62, 62, 62, 64}},
        {"|:: C [1,2 D ::| [3 E | F", {60, 62, 60, 62, 60, 64, 65}},
    };
    for (const auto& [music, expected] : cases) {
        CAPTURE(music);
        const auto [score, warnings] = abc::read(kHeader + music);
        CHECK(warnings.empty());
        CHECK(pitches(score) == expected);
    }
    // Multiple repeat colons do not require a corresponding numbered ending.
    const auto [score, warnings] = abc::read(kHeader + "|" + std::string(65, ':') + " C :|");
    CHECK(warnings.empty());
    REQUIRE(score.tracks->size() == 1);
    CHECK(score.tracks->front()->notes->size() == 66);

    const auto bare = abc::read(kHeader + "C " + std::string(4096, ':') + " D", "case.abc");
    REQUIRE(bare.warnings.size() == 1);
    CHECK(bare.warnings.front().starts_with("case.abc:6:3: ABC [ambiguous-repeat]"));
    CHECK(melody(bare.value) == Notes{{1, 479, 60}, {481, 479, 60}, {961, 479, 62}});
}

TEST_CASE("test_skipped_ending_bars", "[abc][navigation]") {
    constexpr auto passes = 2000;
    auto           music  = kHeader + "|" + std::string(passes - 1, ':') + " C [1 D ";
    // Scanning these bars on every pass would exceed the navigation work budget.
    for (i32 i = 0; i < passes; ++i) music += "| ";
    const auto [score, warnings] = abc::read(music + ":| E");
    CHECK(warnings.empty());
    REQUIRE(score.tracks->size() == 1);
    const auto& notes = *score.tracks->front()->notes;
    REQUIRE(notes.size() == passes + 2);
    CHECK(notes[1].pitch == 62);
    CHECK(notes[passes].pitch == 60);
    CHECK(notes.back().pitch == 64);
}

TEST_CASE("test_trailing_endings", "[abc][navigation]") {
    const auto expected = Notes{
        {1, 479, 60},
        {481, 479, 62},
        {961, 479, 60},
        {1441, 479, 64},
        {1921, 479, 60},
        {2401, 479, 64},
        {2881, 479, 65},
    };
    for (const auto* music : {
             "|:: C [1 D ::| [2,3 E || F",
             "|:: C [1 D ::|2-3 E |] F",
             "|:: C [1 D ::|[2,3 E [| F",
         }) {
        CAPTURE(music);
        const auto [score, warnings]
            = abc::read(std::string("X:1\nT:Endings\nM:4/4\nL:1/4\nP:A\nK:C\nP:A\n") + music);
        CHECK(warnings.empty());
        CHECK(melody(score) == expected);
    }
}

TEST_CASE("test_part_ending_selection", "[abc][navigation]") {
    // ABC 2.1 section 4.10: the itinerary repeats the part, not its ending bars.
    const std::string prefix     = "%abc-2.1\nX:1\nT:Endings\nM:4/4\nL:1/4\nP:A3\nK:C\nP:A\n";
    const auto [score, warnings] = abc::read(prefix + "C4 | [1 D4 :| [2 E4 :| [3 F4 |]");
    CHECK(warnings.empty());
    CHECK(pitches(score) == vec{60, 62, 60, 64, 60, 65});

    const auto explicit_repeat = abc::read(prefix + "|: C4 | [1 D4 :| [2 E4 |]");
    CHECK(explicit_repeat.warnings.empty());
    CHECK(
        pitches(explicit_repeat.value)
        == vec{
            60,
            62,
            60,
            64,
            60,
            62,
            60,
            64,
            60,
            62,
            60,
            64,
        }
    );
}

TEST_CASE("test_ending_accidentals", "[abc][navigation]") {
    for (const auto* marker : {":|2", ":|[2", ":| [2"}) {
        CAPTURE(marker);
        const auto [score, warnings] = abc::read(
            std::string("X:1\nM:2/4\nL:1/4\nK:C\n|: C ^F |[1 D2 ") + marker + " F2 |] G2"
        );
        CHECK(warnings.empty());
        CHECK(
            melody(score)
            == Notes{
                {1, 479, 60},
                {481, 479, 66},
                {961, 959, 62},
                {1921, 479, 60},
                {2401, 479, 66},
                {2881, 959, 65},
                {3841, 959, 67},
            }
        );
    }
}

TEST_CASE("test_ending_beat_position", "[abc][navigation]") {
    const auto opening = kHeader + "%%MIDI beat 105 95 80 2\n";
    for (const auto* first : {"[1", "|1", "| [1"}) {
        for (const auto* second : {":|2", ":| [2"}) {
            CAPTURE(first, second);
            const auto [score, warnings]
                = abc::read(opening + "|: C ^F " + first + " D2 " + second + " F2 |] G2");
            CHECK(warnings.empty());
            REQUIRE(score.tracks->size() == 1);
            const auto& notes = *score.tracks->front()->notes;
            REQUIRE(notes.size() == 7);
            CHECK(notes[5].pitch == 65);
            CHECK(notes[5].velocity == (std::string_view(first).starts_with('|') ? 105 : 95));
        }
    }
}

TEST_CASE("test_repeat_overlay_close", "[abc][navigation]") {
    const std::string opening    = "%abc-2.1\nX:1\nT:Repeated overlay\nM:2/4\nL:1/4\nK:C\n";
    const auto [score, warnings] = abc::read(opening + "A2 | B |: CD | EF | && G2 | A2 :| B2 |]");
    REQUIRE(warnings.empty());
    REQUIRE(score.tracks->size() == 2);
    const auto& primary = *score.tracks->front()->notes;
    REQUIRE(primary.size() == 11);
    CHECK(primary.back().time == 5281);
    vec<i32> side;
    for (const auto& note : *score.tracks->back()->notes) side.push_back(note.time);
    CHECK(side == vec<i32>{1441, 2401, 3361, 4321});
}

TEST_CASE("test_overlay_ending_join", "[abc][navigation]") {
    const std::string opening = "%abc-2.1\nX:1\nT:Ending overlay\nM:2/4\nL:1/4\nK:C\n";
    const auto [score, warnings]
        = abc::read(opening + "|: C2 :| [2-3 D2 | E2 | && F2 | G2 || A2 |]");
    REQUIRE(warnings.empty());
    REQUIRE(score.tracks->size() == 2);
    const auto& primary = *score.tracks->front()->notes;
    REQUIRE(primary.size() == 8);
    CHECK(primary.back().time == 6721);
    vec<i32> side;
    for (const auto& note : *score.tracks->back()->notes) side.push_back(note.time);
    CHECK(side == vec<i32>{1921, 2881, 4801, 5761});
}

TEST_CASE("test_ending_lists", "[abc][navigation]") {
    const auto [score, warnings] = abc::read(kHeader + "|: C [7-8,1,3-33,3 D :| [2 E || F");
    CHECK(warnings.empty());
    const auto notes = melody(score);
    REQUIRE(notes.size() == 67);
    CHECK(notes[1] == std::tuple{481, 479, 62});
    CHECK(notes[3] == std::tuple{1441, 479, 64});
    CHECK(notes[5] == std::tuple{2401, 479, 62});
    CHECK(notes[65] == std::tuple{31201, 479, 62});
    CHECK(notes[66] == std::tuple{31681, 479, 65});

    rejects(kHeader + "|: C [0 D :|", "6:7: ABC [ending-range]");
    rejects(kHeader + "|: C [3-2 D :|", "6:7: ABC [invalid-ending]");
    rejects(kHeader + "|: C [1-2-3 D :|", "6:6: ABC [malformed-group]");
    rejects(kHeader + "|: C [1-2147483647 D :|", "6:4: ABC [expansion-limit]");
}

TEST_CASE("test_nested_part_order", "[abc][navigation]") {
    // ABC 2.1 section 3.1.9: nested groups can repeat and dots are ignored.
    const std::string prefix     = "%abc-2.1\nX:1\nT:Parts\nM:4/4\nL:1/4\nP:";
    const std::string parts      = "\nK:C\nP:A\nC\nP:B\nD";
    const auto [score, warnings] = abc::read(prefix + "(A.(BA)2)2" + parts);
    CHECK(warnings.empty());
    CHECK(pitches(score) == vec{60, 62, 60, 62, 60, 60, 62, 60, 62, 60});
    CHECK(abc::read(prefix + "A1.2" + parts).value.note_num() == 12);
}

TEST_CASE("test_reordered_part_notation", "[abc][navigation]") {
    const std::string prefix = "%abc-2.1\nX:1\nT:Parts\nM:2/4\nL:1/4\nP:";

    SECTION("Returning to an earlier part restores its notation") {
        const auto [score, warnings]
            = abc::read(prefix + "ABA\nK:C\nP:A\nF C |\nP:B\n[K:D][L:1/8] F2 C2 |]");
        CHECK(warnings.empty());
        CHECK(
            melody(score)
            == Notes{
                {1, 479, 65},
                {481, 479, 60},
                {961, 479, 66},
                {1441, 479, 61},
                {1921, 479, 65},
                {2401, 479, 60},
            }
        );
        REQUIRE(score.key_signatures->size() == 2);
        CHECK(score.key_signatures->back() == KeySignature<Tick>{960, 2, 0});
    }

    SECTION("Skipping a part keeps its written notation for the following part") {
        const auto [score, warnings]
            = abc::read(prefix + "B\nK:C\nP:A\nF [K:D][L:1/8] C2 |\nP:B\nF [Q:C=90] C3 |]");
        REQUIRE(warnings.size() == 1);
        CHECK(warnings.front().find("[deprecated-tempo]") != std::string::npos);
        CHECK(melody(score) == Notes{{1, 239, 66}, {241, 719, 61}});
        REQUIRE(score.key_signatures->size() == 1);
        CHECK(score.key_signatures->front() == KeySignature<Tick>{0, 0, 0});
        REQUIRE(score.tempos->size() == 2);
        CHECK(score.tempos->back().time == 241);
        CHECK(score.tempos->back().mspq == 1333333);
    }

    SECTION("Overlay-local changes do not become the next part's defaults") {
        const auto [score, warnings]
            = abc::read(prefix + "B\nK:C\nP:A\n(& F C & [K:D][L:1/8] A B &) |\nP:B\nF C |]");
        CHECK(warnings.empty());
        CHECK(melody(score) == Notes{{1, 479, 65}, {481, 479, 60}});
    }
}

TEST_CASE("test_skipped_ending_notation", "[abc][navigation]") {
    const auto [score, warnings] = abc::read(
        "%abc-2.1\nX:1\nT:Endings\nM:2/4\nL:1/4\nK:C\n"
        "|: F G |[1 [K:D][L:1/8] F2 G2 :|[2 F G3 |]"
    );
    CHECK(warnings.empty());
    CHECK(
        melody(score)
        == Notes{
            {1, 479, 65},
            {481, 479, 67},
            {961, 479, 66},
            {1441, 479, 67},
            {1921, 479, 65},
            {2401, 479, 67},
            {2881, 239, 66},
            {3121, 719, 67},
        }
    );
    REQUIRE(score.key_signatures->size() == 2);
    CHECK(score.key_signatures->back() == KeySignature<Tick>{960, 2, 0});
}

TEST_CASE("test_midbar_part_accidentals", "[abc][navigation]") {
    // ABC 2.1 sections 3.1.9 and 11.3: a change of part can fall inside a bar.
    const std::string prefix = "%abc-2.1\nX:1\nT:Accidentals\nM:2/4\nL:1/4\nP:";
    const auto check = [&](std::string_view order, std::string_view music, vec<i32> expected) {
        CAPTURE(order, music);
        const auto [score, warnings]
            = abc::read(fmt::format("{}{}\nK:C\n{}", prefix, order, music));
        CHECK(warnings.empty());
        CHECK(pitches(score) == expected);
    };
    check("B", "[P:A]^F G [P:B] F f |]", {66, 78});
    check("B", "[P:A][^F G] [P:B] F f |]", {66, 78});
    check("B", "[P:A]{^F}D [P:B] F f |]", {66, 78});
    check("B", "[P:A]^F G | [P:B] F f |]", {65, 77});
    check("B", "[P:A]^F & _F [P:B] F f |]", {66, 78});
    check("B", "[P:A](& ^F & _F &) [P:B] F f |]", {65, 77});
    check("B", "[P:A][K:D] =F D [P:B] F f |]", {65, 77});
    check("B", "[P:A]^F [K:C] D [P:B] F f |]", {65, 77});
    check("B", "[I:propagate-accidentals octave][P:A]^F G [P:B] F f |]", {66, 77});
    check("B", "[I:propagate-accidentals octave][P:A]^F [K:octave=1][P:B] F F, |]", {77, 66});
    check("B", "[I:propagate-accidentals not][P:A]^F G [P:B] F f |]", {65, 77});
    check("ABA", "[P:A]F G [P:B] _F E", {65, 67, 64, 64, 65, 67});
    check("B", "[P:A]^F [P:B] F G :|", {66, 67, 66, 67});
}

TEST_CASE("test_macro_part_accidentals", "[abc][navigation]") {
    // The macro's notes and part marker share a source location but not a written slot.
    const auto [score, warnings] = abc::read(
        "%abc-2.1\nX:1\nT:Macro parts\nM:2/4\nL:1/4\nP:ABA\nK:C\n"
        "m: ~C2 = ^F [P:B] F G\nP:A\n~C2 |]"
    );
    CHECK(warnings.empty());
    CHECK(melody(score) == Notes{{1, 479, 66}, {481, 479, 66}, {961, 479, 67}, {1441, 479, 66}});
}

TEST_CASE("test_macro_distinct_parts", "[abc][navigation]") {
    SECTION("Parts at one source location") {
        const std::string header = "%abc-2.1\nX:1\nT:Macro parts\nM:2/4\nL:1/4\nP:CBA\nK:C\n";
        const auto [expanded, warnings]
            = abc::read(header + "m: ~C2 = [P:A] C [P:B] D [P:C] E\n~C2 |]");
        const auto [written, direct_warnings] = abc::read(header + "[P:A] C [P:B] D [P:C] E |]");
        CHECK(warnings.empty());
        CHECK(direct_warnings.empty());
        CHECK(melody(expanded) == Notes{{1, 479, 64}, {481, 479, 62}, {961, 479, 60}});
        CHECK(expanded == written);
    }
    SECTION("Each voice receives each part") {
        const std::string header
            = "%abc-2.1\nX:1\nT:Macro parts\nM:2/4\nL:1/4\nP:BA\nV:one\nV:two\nK:C\n";
        const std::string music         = "[P:A][V:one] C [V:two] E [P:B][V:one] D [V:two] F |]";
        const auto [expanded, warnings] = abc::read(header + "m: ~C2 = " + music + "\n~C2");
        const auto [written, direct_warnings] = abc::read(header + music);
        CHECK(warnings.empty());
        CHECK(direct_warnings.empty());
        REQUIRE(expanded.tracks->size() == 2);
        CHECK(expanded.tracks->at(0)->notes->at(0).pitch == 62);
        CHECK(expanded.tracks->at(1)->notes->at(0).pitch == 65);
        CHECK(expanded == written);
    }
}

TEST_CASE("test_written_meter_restore", "[abc][navigation]") {
    // Sections 4.5 and 4.13 give Z and implicit tuplets their local written meter.
    const std::string prefix = "%abc-2.1\nX:1\nT:Meter changes\nM:2/4\nL:1/4\n";

    SECTION("A part inherits the meter set before its label") {
        const auto [score, warnings]
            = abc::read(prefix + "P:B\nK:C\nP:A\n[M:3/4] F3 |\nP:B\nZ | G3 |]");
        CHECK(warnings.empty());
        CHECK(melody(score) == Notes{{1441, 1439, 67}});
        REQUIRE(score.time_signatures->size() == 1);
        CHECK(score.time_signatures->front() == TimeSignature<Tick>{0, 2, 4});
    }

    SECTION("Revisiting a part restores its implicit tuplet ratio") {
        const auto [score, warnings]
            = abc::read(prefix + "P:ABA\nK:C\nP:A\n(5 C D E F G |\nP:B\n[M:6/8] G3 |]");
        CHECK(warnings.empty());
        const auto notes = melody(score);
        REQUIRE(notes.size() == 11);
        for (size_t i = 0; i < 5; ++i) {
            const auto [time, gate, pitch] = notes[i];
            CHECK(notes[i + 6] == std::tuple{time + 2400, gate, pitch});
        }
    }

    SECTION("The meter written in a skipped ending determines the next measure rest") {
        const auto [score, warnings]
            = abc::read(prefix + "K:C\n|: F2 |[1 [M:3/4] F3 :|[2 Z | G3 |]");
        CHECK(warnings.empty());
        CHECK(
            melody(score)
            == Notes{
                {1, 959, 65},
                {961, 1439, 65},
                {2401, 959, 65},
                {4801, 1439, 67},
            }
        );
    }
}

TEST_CASE("test_part_meter_backing", "[abc][navigation][accompaniment]") {
    // ABC 2.1 sections 3.1.9 and 4.5 set the part order and written measure rest.
    // The abc2midi backing convention uses the last performed meter on the jump.
    const auto [score, warnings] = abc::read(
        "%abc-2.1\nX:1\nT:Part meter and backing\nM:6/8\nL:1/8\nP:ABA\nK:C\n"
        "P:A\n\"G\"G6 | Z | G6 |\n"
        "P:B\n[M:4/4] \"C\"C8 |]"
    );
    REQUIRE(warnings.empty());
    REQUIRE(score.ticks_per_quarter == 480);
    REQUIRE(score.tracks->size() == 3);

    // A has three 1440-tick bars, including Z; B has one 1920-tick bar.
    CHECK(
        note_spans(*score.tracks->at(0))
        == Notes{
            {1, 1439, 67}, {2881, 1439, 67}, {4321, 1919, 60}, {6241, 1439, 67}, {9121, 1439, 67}
        }
    );
    REQUIRE(score.time_signatures->size() == 2);
    CHECK(score.time_signatures->at(0) == TimeSignature<Tick>{0, 6, 8});
    CHECK(score.time_signatures->at(1) == TimeSignature<Tick>{4320, 4, 4});

    // The first A uses the 6/8 pattern; the returning A keeps B's 4/4 pattern.
    CHECK(
        onsets(*score.tracks->at(1))
        == Onsets{
            {1, 43},
            {721, 43},
            {1441, 43},
            {2161, 43},
            {2881, 43},
            {3601, 43},
            {4321, 36},
            {5281, 36},
            {6241, 43},
            {7201, 43},
            {7681, 43},
            {8641, 43},
            {9121, 43},
            {10081, 43}
        }
    );
    vec<i32> chord_onsets;
    for (const auto& note : *score.tracks->at(2)->notes)
        if (chord_onsets.empty() || chord_onsets.back() != note.time)
            chord_onsets.push_back(note.time);
    CHECK(
        chord_onsets == vec<i32>{481, 1201, 1921, 2641, 3361, 4081, 4801, 5761, 6721, 8161, 9601}
    );
}

TEST_CASE("test_opening_rest", "[abc][navigation]") {
    const auto [score, warnings] = abc::read(kHeader + "|: C\nX:|\nD");
    const auto spaced            = abc::read(kHeader + "|: C\nX :|\nD");
    CHECK(warnings == spaced.warnings);
    CHECK(melody(score) == melody(spaced.value));
    CHECK(score.note_num() == 3);
}

TEST_CASE("test_extended_overlay_origin", "[abc][navigation]") {
    const auto [score, warnings] = abc::read(
        "X:1\nT:Extended overlay\nM:2/4\nL:1/4\nK:C\n"
        "GA|(&Bc|de|de&FG|FG|FG|&)|DE|\n"
        "GA|(&Bc|de|de&FG|FG|FG|&)|DE|"
    );
    REQUIRE(warnings.empty());
    REQUIRE(score.tracks->size() == 2);
    const auto& primary = *score.tracks->at(0)->notes;
    const auto& overlay = *score.tracks->at(1)->notes;
    REQUIRE(primary.size() == 20);
    REQUIRE(overlay.size() == 12);
    for (const size_t occurrence : {0, 1}) {
        for (size_t note = 0; note < 6; ++note) {
            CAPTURE(occurrence, note);
            const auto& main = primary[occurrence * 10 + 2 + note];
            const auto& side = overlay[occurrence * 6 + note];
            CHECK(side.pitch == (note % 2 == 0 ? 65 : 67));
            CHECK(side.time == main.time);
            CHECK(side.duration == main.duration);
            CHECK(side.velocity == main.velocity);
        }
    }
}

TEST_CASE("test_adjacent_overlays", "[abc][navigation]") {
    for (const std::string_view opening : {"CD", "CD & EF & GA"}) {
        CAPTURE(opening);
        const auto [score, warnings] = abc::read(
            fmt::format(
                "X:1\nT:Extended overlay\nM:2/4\nL:1/4\nK:C\n"
                "{} | (& cd | ef & ga | bc' | &) (& AB | cd & EF | GA | & ef | ga | &) |",
                opening
            )
        );
        REQUIRE(warnings.empty());
        REQUIRE(score.tracks->size() == 3);
        const auto& primary = *score.tracks->at(0)->notes;
        REQUIRE(primary.size() == 10);
        for (const size_t lane : {1, 2}) {
            const auto& notes = *score.tracks->at(lane)->notes;
            REQUIRE(notes.size() >= 4);
            auto main = primary.end() - 4;
            for (auto side = notes.end() - 4; side != notes.end(); ++side, ++main) {
                CHECK(side->time == main->time);
                CHECK(side->velocity == main->velocity);
            }
        }
    }
}

TEST_CASE("test_idle_overlay_ties", "[abc][navigation]") {
    const auto [score, warnings] = abc::read(
        "X:1\nT:Extended overlay\nM:2/4\nL:1/4\nK:C\n"
        "CD | (& EF & GA & B c- &) (& de | fg & ab | c' d' &) (& AB & cd & c B &) |"
    );
    REQUIRE(warnings.size() == 1);
    CHECK(warnings.front().find("[unmatched-tie]") != std::string::npos);
    REQUIRE(score.tracks->size() == 3);
    const auto& primary = *score.tracks->at(0)->notes;
    const auto& overlay = *score.tracks->at(2)->notes;
    REQUIRE(primary.size() == 10);
    REQUIRE(overlay.size() == 4);
    CHECK(overlay[1].pitch == overlay[2].pitch);
    CHECK(overlay[1].time == primary[3].time);
    CHECK(overlay[2].time == primary[8].time);
    CHECK(overlay[1].duration == primary[3].duration);
}

TEST_CASE("test_repeat_tuplet_meter", "[abc][navigation]") {
    const auto [score, warnings] = abc::read(kHeader + "|: (5ABCDE [M:6/8] z :| F");
    REQUIRE(warnings.empty());
    const auto& notes = *score.tracks->front()->notes;
    REQUIRE(notes.size() == 11);
    CHECK(notes[0].duration == 191);
    CHECK(notes[5].time == 1441);
    CHECK(notes[5].duration == 191);
    CHECK(notes.back().time == 2881);
    REQUIRE(score.time_signatures->size() == 4);
    CHECK(score.time_signatures->at(2) == TimeSignature<Tick>{1440, 4, 4});
}

TEST_CASE("test_simultaneous_repeat_meter", "[abc][navigation]") {
    const auto [score, warnings] = abc::read(kHeader + "|: [M:6/8] C [M:3/4] D :| E");
    CHECK(warnings.empty());
    REQUIRE(score.time_signatures->size() == 4);
    CHECK(score.time_signatures->at(2) == TimeSignature<Tick>{960, 6, 8});
}

TEST_CASE("test_repeat_pickup", "[abc][navigation]") {
    const auto opening = kHeader + "%%MIDI beat 105 95 80 2\n";
    const auto first   = abc::read(opening + "|: \"C\"C D :| E F G A |]");
    REQUIRE(first.warnings.empty());
    CHECK(velocities(first.value) == vec<i32>{105, 80, 105, 80, 105, 80, 95, 80});
    REQUIRE(first.value.tracks->size() == 3);
    vec<i32> bass_attacks;
    for (const auto& note : *first.value.tracks->at(1)->notes) bass_attacks.push_back(note.time);
    CHECK(bass_attacks == vec<i32>{1, 961, 1921, 2881});

    const auto later = abc::read(opening + "CDEF |: G A :| B c |]");
    REQUIRE(later.warnings.empty());
    CHECK(velocities(later.value) == vec<i32>{105, 80, 95, 80, 105, 80, 95, 80, 105, 80});
}

}   // namespace symusic::abc_test

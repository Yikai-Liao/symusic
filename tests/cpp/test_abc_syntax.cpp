#include <algorithm>
#include <filesystem>
#include <fstream>
#include <random>

#include <fmt/format.h>

#include "abc_test.h"

namespace symusic::abc_test {

TEST_CASE("test_header_include_order", "[abc][syntax]") {
    const auto [score, warnings] = abc::read_file("testcases/abc_headers/tune.abc");
    CAPTURE(warnings);
    const auto [expected, direct_warnings]
        = abc::read("%abc-2.1\nX:1\nT:Header définitions\nM:3/4\nL:1/4\nK:C\n!p!G3 A |]");
    CHECK(warnings.empty());
    CHECK(direct_warnings.empty());
    CHECK(score == expected);
    CHECK(pitches(score) == vec<i32>{67, 69});
}

TEST_CASE("test_nested_include_cycle", "[abc][syntax]") {
    const auto [score, warnings] = abc::read_file("testcases/abc_headers/nested.abc");
    const auto expected
        = abc::read("%abc-2.1\nX:1\nT:Nested headers\nM:3/4\nL:1/8\nK:C\n!f!C2 D E |]");
    CHECK(score == expected.value);
    REQUIRE(warnings.size() == 5);
    CHECK(warnings[0].find("nested-inner.abh:3:1: ABC [cyclic-include]") != std::string::npos);
    CHECK(warnings[0].ends_with("\n\"I:abc-include ./nested-outer.abh\""));
    CHECK(warnings[1].find("nested-inner.abh:4:1: ABC [cyclic-include]") != std::string::npos);
    CHECK(
        warnings[2].find("nested-outer.abh:4:1: ABC [invalid-include-content]") != std::string::npos
    );
    CHECK(warnings[3].find("nested-inner.abh:3:1: ABC [cyclic-include]") != std::string::npos);
    CHECK(warnings[4].find("nested-inner.abh:4:1: ABC [cyclic-include]") != std::string::npos);
}

TEST_CASE("test_include_cycle_recovery", "[abc][syntax]") {
    const auto directory = std::filesystem::temp_directory_path()
                           / fmt::format("symusic-abc-includes-{}", std::random_device{}());
    REQUIRE(std::filesystem::create_directory(directory));
    const struct Cleanup {
        const std::filesystem::path& path;
        ~Cleanup() {
            std::error_code error;
            std::filesystem::remove_all(path, error);
        }
    } cleanup{directory};
    const auto put = [&](const char* name, std::string_view text) {
        std::ofstream output(directory / name, std::ios::binary);
        output << text;
        output.close();
        REQUIRE(output.good());
    };
    put("root.abc", "%abc-2.1\nI:abc-include outer.abh\nI:abc-include last.abh\nX:1\nT:\nK:C\nC");
    put("outer.abh", "I:abc-include inner.abh\n%" + std::string(2 * 1024 * 1024 - 64, ' '));
    put("inner.abh", "I:abc-include ./outer.abh\n");
    put("last.abh", "L:1/4\n%" + std::string(256, ' '));

    const auto [score, warnings] = abc::read_file(directory / "root.abc");
    CHECK(melody(score) == Notes{{1, 479, 60}});
    REQUIRE(warnings.size() == 1);
    CHECK(warnings[0].find("inner.abh:1:1: ABC [cyclic-include]") != std::string::npos);
}

TEST_CASE("test_tunebook_preamble", "[abc][syntax]") {
    const auto [score, warnings] = abc::read(
        "%abc-2.1\nL:1/4\n\nCollected on the coast.\n\n"
        "X:1\nT:First\nK:C\nC D |]\n\nNotes on this transcription.\n"
    );
    CHECK(warnings.empty());
    CHECK(melody(score) == Notes{{1, 479, 60}, {481, 479, 62}});
}

TEST_CASE("test_header_only_tune", "[abc][syntax]") {
    const auto [score, warnings]
        = abc::read(kStrictHeader + " \t\nDEF\n\nX:2\nT:Second\nK:C\nC |]");
    CHECK(score.note_num() == 0);
    REQUIRE(warnings.size() == 1);
    CHECK(warnings.front().starts_with("<string>:10:1: ABC [extra-tunes]"));

    const auto [loose, loose_warnings] = abc::read(kHeader + "\nD |]");
    CHECK(loose_warnings.empty());
    CHECK(pitches(loose) == vec<i32>{62});
}

TEST_CASE("test_blank_header_line", "[abc][syntax]") {
    rejects("%abc-2.1\nX:1\nT:First\n \t\nK:C\nC |]", "4:1: ABC [missing-key]");
    rejects("X:1\nT:First\nI:abc-version 2.1\n\nK:C\nC |]", "4:1: ABC [missing-key]");

    const auto [strict, strict_warnings]
        = abc::read("%abc-2.1\nX:1\nT:First\n % comment\nr:remark\nK:C\nC |]");
    CHECK(strict_warnings.empty());
    CHECK(pitches(strict) == vec<i32>{60});

    const auto [loose, loose_warnings]
        = abc::read("%abc-2.1\nX:1\nT:First\nI:abc-version 2.0\n\nK:C\nC |]");
    CHECK(loose_warnings.empty());
    CHECK(pitches(loose) == vec<i32>{60});
}

TEST_CASE("test_file_header_key", "[abc][syntax]") {
    const auto [score, warnings] = abc::read(
        "%abc-2.1\nM:2/4\nL:1/4\nK:G\n\nX:1\nT:Example\nK:clef=bass\nF2 |]", "fields.abc"
    );
    REQUIRE(warnings.size() == 1);
    CHECK(warnings.front().starts_with("fields.abc:4:1: ABC [invalid-file-header-field]"));
    CHECK(melody(score) == Notes{{1, 959, 65}});
    CHECK(*score.time_signatures == pyvec<TimeSignature<Tick>>{{0, 2, 4}});
    CHECK(*score.key_signatures == pyvec<KeySignature<Tick>>{{0, 0, 0}});
}

TEST_CASE("test_field_scope_version", "[abc][syntax]") {
    const auto [score, warnings] = abc::read(
        "Q:1/4=60\nI:abc-version 2.1\n\nX:1\nT:Legacy tune\n"
        "w:old header text\nI:abc-version 2.0\nK:C\nCD"
    );
    REQUIRE(warnings.size() == 1);
    CHECK(warnings.front().starts_with("<string>:1:1: ABC [invalid-file-header-field]"));
    CHECK(*score.tempos == pyvec<Tempo<Tick>>{{0, 500'000}});
}

TEST_CASE("test_deprecation_version", "[abc][syntax]") {
    const auto strict_file = abc::read(
        "%abc-2.1\nA:Bampton\n\nX:1\nT:Legacy tune\nA:Oxford\nI:abc-version 2.0\nK:C\nCD"
    );
    REQUIRE(strict_file.warnings.size() == 1);
    CHECK(strict_file.warnings.front().starts_with("<string>:2:1: ABC [deprecated-field]"));

    const auto strict_tune = abc::read(
        "%abc-2.0\nA:Bampton\n\nX:1\nT:Strict tune\nA:Oxford\nI:abc-version 2.1\nK:C\nCD"
    );
    REQUIRE(strict_tune.warnings.size() == 1);
    CHECK(strict_tune.warnings.front().starts_with("<string>:6:1: ABC [deprecated-field]"));
}

TEST_CASE("test_ignored_tie_bytes", "[abc][syntax]") {
    for (const std::string_view body : {
             "C- C",
             "C-| C",
             "C.-C",
             "C.@-C",
             "C[r:remark]-C",
             "C\\\n% comment\n-C",
         }) {
        CAPTURE(body);
        const auto [score, warnings] = abc::read(kStrictHeader + std::string(body));
        CHECK(warnings.empty());
        CHECK(melody(score) == Notes{{1, 959, 60}});
    }

    const auto [chord, warnings] = abc::read(kStrictHeader + "[C@-E] [CE]");
    CHECK(warnings.empty());
    CHECK(melody(chord) == Notes{{1, 959, 60}, {1, 479, 64}, {481, 479, 64}});
}

TEST_CASE("test_tempo_beats_and_labels", "[abc][syntax]") {
    const std::string prefix = "%abc-2.1\nX:1\nT:Tempo\nM:4/4\nL:1/8\nQ:";
    for (const auto& [value, usec] : {
             std::pair{R"("A &quot;quoted&quot; label \%" 1/4 3/8 1/4 3/8=40)", 300000},
             {R"(3/8 = 50 "Slowly")", 800000},
         }) {
        CAPTURE(value);
        const auto [score, warnings] = abc::read(prefix + value + "\nK:C\nC");
        CHECK(warnings.empty());
        REQUIRE(score.tempos->size() == 1);
        CHECK(score.tempos->front().mspq == usec);
    }

    rejects(prefix + "1/8 1/8 1/8 1/8 1/8=120\nK:C\nC", "6:1: ABC [invalid-tempo]");
    rejects(kHeader + R"(C [Q:1/4=120 "OK" junk] D)", "6:4: ABC [invalid-tempo]");
    rejects(prefix + "1/0=120\nK:C\nC", "6:1: ABC [invalid-fraction]");
}

TEST_CASE("test_history_continuation", "[abc][syntax]") {
    // Sections 3.1.13 and 10.1: bare lines after H: are history until the next field.
    const std::string opening
        = "X:1\nT:History\nH:First line\n  Another line\n"
          "% A comment\nr: A remark\n%%vocalfont Times-Roman 14\nThird line\n";
    const std::string music = "L:1/4\nK:D\nF G |]";
    const auto [score, warnings]
        = abc::read(opening + "I:abc-version 2.1\n" + music, "history.abc");
    REQUIRE(warnings.size() == 1);
    CHECK(warnings.front().starts_with("history.abc:3:1: ABC [deprecated-history]"));
    CHECK(melody(score) == melody(abc::read("X:1\nT:History\n" + music).value));

    const auto [loose, loose_warnings] = abc::read(opening + music);
    CHECK(loose_warnings.empty());
    CHECK(melody(loose) == melody(score));
}

TEST_CASE("test_history_boundary", "[abc][syntax]") {
    const auto [score, warnings] = abc::read(
        "%abc-2.1\nH:File history\nAnother history line\n\n"
        "X:1\nT:Example\nH:Modern history\n%%vocalfont Times-Roman 14\n+:A continuation\n"
        "M:2/4\nK:C\nDEF |]",
        "history.abc"
    );
    REQUIRE(warnings.size() == 1);
    CHECK(warnings.front().starts_with("history.abc:2:1: ABC [deprecated-history]"));
    REQUIRE(score.tracks->size() == 1);
    REQUIRE(score.tracks->front()->notes->size() == 3);
    CHECK(score.tracks->front()->notes->back().time == 241);
    rejects("%abc-2.1\nX:1\nT:Example\nH:History\n\nStray prose\nK:C\nD", "5:1: ABC [missing-key]");
}

TEST_CASE("test_history_instruction_boundary", "[abc][syntax]") {
    for (const auto* instruction : {"I:abc-version 2.1", "%%abc-version 2.1"}) {
        CAPTURE(instruction);
        rejects(
            fmt::format(
                "%abc-2.1\nX:1\nT:Example\nH:History\n{}\nStray prose\nK:C\nD", instruction
            ),
            "6:1: ABC [missing-key]"
        );
    }
}

TEST_CASE("test_remark_continuations", "[abc][syntax]") {
    // ABC 2.1 sections 2.2.5 and 3: removing an r: remark must not affect playback.
    const auto [expected, expected_warnings] = abc::read(kStrictHeader + "C>D [EG] F");
    REQUIRE(expected_warnings.empty());
    REQUIRE(expected.tracks->front()->notes->front().duration == 719);
    for (const auto* music : {
             "C [r:before]>[r:after]D [EG] F",
             "C>D [E\\\n  r: between chord members\nG] F",
             "C>D [E\\\n  % between chord members\nG] F",
         }) {
        CAPTURE(music);
        const auto [actual, warnings] = abc::read(kStrictHeader + music);
        CHECK(warnings.empty());
        CHECK(actual == expected);
    }
}

TEST_CASE("test_inline_remark_tokens", "[abc][syntax]") {
    for (const auto& [with_remark, plain] : {
             std::pair{"C[r:length]2 D |]", "C2 D |]"},
             {"[C[r:chord]E] D |]", "[CE] D |]"},
             {"[C[r:a quote \" in a remark]E] D \"^label\" G", "[CE] D G"},
             {"{a[r:grace][r:second remark]>b}C D |]", "{a>b}C D |]"},
             {"^C[r:tie]-[r:next]C D |]", "^C-C D |]"},
             {"[C[r:discard \\ncontinued remark]E] D |]", "[CE] D |]"},
             {"[C\\\nQ:1/4=90\n[r:chord]E] D |]", "[Q:1/4=90][CE] D |]"},
         }) {
        CAPTURE(with_remark);
        const auto actual   = abc::read(kStrictHeader + with_remark);
        const auto expected = abc::read(kStrictHeader + plain);
        REQUIRE(actual.warnings.empty());
        REQUIRE(expected.warnings.empty());
        CHECK(actual.value == expected.value);
    }
    rejects(kStrictHeader + "C[r:x]/0 D", "7:7: ABC [zero-length-divisor]");
    rejects(kStrictHeader + "C[r:unclosed D", "7:2: ABC [unclosed-inline-field]");
}

TEST_CASE("test_remark_literals", "[abc][syntax]") {
    const auto [score, warnings]
        = abc::read(kStrictHeader + R"([V:lead name="Lead [r: literal]"] "^label [r: literal]" C)");
    REQUIRE(warnings.empty());
    REQUIRE(score.tracks->size() == 1);
    CHECK(score.tracks->front()->name == "Lead [r: literal]");
    CHECK(melody(score) == Notes{{1, 479, 60}});
}

TEST_CASE("test_macro_inline_remarks", "[abc][syntax]") {
    const auto definition = kStrictHeader + "m: ~C2 = E[r:from replacement]2\n";
    for (const auto& [with_remark, plain] : {
             std::pair{"~C[r:inside use]2 D |]", "E2 D |]"},
             {"[G~C[r:inside use]2] D |]", "[GE2] D |]"},
         }) {
        CAPTURE(with_remark);
        const auto actual   = abc::read(definition + with_remark);
        const auto expected = abc::read(kStrictHeader + plain);
        REQUIRE(actual.warnings.empty());
        CHECK(actual.value == expected.value);
    }
    rejects(
        kStrictHeader + "m: ~C2 = [E[r:inside expansion]/0]\n  ~C[r:inside use]2 D",
        "8:3: ABC [zero-length-divisor]"
    );
}

TEST_CASE("test_reference_numbers", "[abc][syntax]") {
    for (const auto* reference : {"", "0003", "987654321098765432109876543210"}) {
        CAPTURE(reference);
        const auto tune   = std::string("X:") + reference + "\nT:Reference\nL:1/4\nK:C\nC D |]";
        const auto result = abc::read("%abc-2.1\n" + tune);
        CHECK(result.warnings.empty());
        CHECK(melody(result.value) == Notes{{1, 479, 60}, {481, 479, 62}});
    }
    for (const auto* reference : {"0", "000", "-4", "third"}) {
        CAPTURE(reference);
        const auto tune   = std::string("X:") + reference + "\nT:Reference\nL:1/4\nK:C\nC D |]";
        const auto result = abc::read("%abc-2.1\n" + tune, "reference.abc");
        REQUIRE(result.warnings.size() == 1);
        CHECK(result.warnings.front().starts_with("reference.abc:2:1: ABC [bad-reference]"));
        CHECK(melody(result.value) == Notes{{1, 479, 60}, {481, 479, 62}});
    }
    const auto loose = abc::read("%abc-2.0\nX:-4\nT:Reference\nL:1/4\nK:C\nC");
    CHECK(loose.warnings.empty());
    CHECK(melody(loose.value) == Notes{{1, 479, 60}});
}

TEST_CASE("test_reserved_characters", "[abc][syntax]") {
    const auto opening = kStrictHeader + R"([V:lead name="Lead @#*;?"] )";
    const auto [score, warnings]
        = abc::read(opening + R"(@[C#E*]; {?[DF@] E#}?G#2 [K:C#] "@#*;?" F |*)");
    const auto [expected, direct_warnings]
        = abc::read(opening + R"([CE] {[DF] E}G2 [K:C#] "@#*;?" F |)");
    CHECK(warnings.empty());
    CHECK(direct_warnings.empty());
    CHECK(score == expected);
    CHECK(pitches(score).back() == 66);
    CHECK(score.tracks->front()->name == "Lead @#*;?");

    // A decoration containing a reserved byte must not become a different decoration.
    CHECK(abc::read(kStrictHeader + "!p#!C").value == abc::read(kStrictHeader + "C").value);
}

TEST_CASE("test_macro_reserved_bytes", "[abc][syntax]") {
    const auto [score, warnings] = abc::read(kStrictHeader + "m: C?2 = D.@-D\nC?2 C");
    CHECK(warnings.empty());
    CHECK(melody(score) == Notes{{1, 959, 62}, {961, 479, 60}});
}

TEST_CASE("test_bang_linebreak", "[abc][syntax]") {
    const auto expected = abc::read(kStrictHeader + "CD|!p!EFGA|]").value;
    for (const std::string_view instruction :
         {"I:linebreak !\n", "I:linebreak <EOL> $ !\n", "%%continueall true\n"}) {
        CAPTURE(instruction);
        const auto opening            = "%abc-2.1\n" + std::string(instruction) + kHeader;
        const auto [actual, warnings] = abc::read(opening + "CD|!+p+EF!GA|]!");
        if (instruction.starts_with("%%")) {
            REQUIRE(warnings.size() == 1);
            CHECK(warnings.front().find("[deprecated-directive]") != std::string::npos);
        } else {
            CHECK(warnings.empty());
        }
        CHECK(actual == expected);
    }
}

TEST_CASE("test_header_decoration_overrides", "[abc][syntax]") {
    const std::string opening     = "%abc-2.1\nI:linebreak !\nI:decoration +\n";
    const auto        expected    = abc::read(kStrictHeader + "CD|!p!EF|]").value;
    const auto        tune_header = kHeader.substr(0, kHeader.find("K:"));

    const auto [bang, bang_warnings]
        = abc::read(opening + tune_header + "I:linebreak <none>\nI:decoration !\nK:C\nCD|!p!EF|]");
    CHECK(bang_warnings.empty());
    CHECK(bang == expected);

    const auto [plus, plus_warnings]
        = abc::read(opening + tune_header + "I:linebreak $\nK:C\nCD|$+p+EF|]");
    CHECK(plus_warnings.empty());
    CHECK(plus == expected);

    const auto [implicit_cleared, warnings]
        = abc::read("%abc-2.1\nI:linebreak !\nI:linebreak <EOL>\n" + kHeader + "CD|!p!EF|]");
    CHECK(warnings.empty());
    CHECK(implicit_cleared == expected);

    const auto [conflict, conflict_warnings]
        = abc::read("%abc-2.1\nI:linebreak !\nI:decoration !\n" + kHeader + "CD|!+p+EF|]");
    REQUIRE(conflict_warnings.size() == 1);
    CHECK(conflict_warnings.front().find("[conflicting-decoration-dialect]") != std::string::npos);
    CHECK(conflict == expected);
}

TEST_CASE("test_plus_shorthand_delimiter", "[abc][syntax]") {
    const auto expected = abc::read(kStrictHeader + "!p!C D !f!E F").value;
    const auto [actual, warnings]
        = abc::read("%abc-2.1\nI:decoration +\nU:h=+p+\n" + kHeader + "hCD EF\ns: * * +f+ *");
    CHECK(warnings.empty());
    CHECK(actual == expected);

    const auto [cleared, clear_warnings]
        = abc::read(kStrictHeader + R"([U:T=!none!] TC [U:T="^label"] TD)");
    CHECK(clear_warnings.empty());
    CHECK(cleared == abc::read(kStrictHeader + "CD").value);

    const auto [invalid, invalid_warnings]
        = abc::read(kStrictHeader + "[U:h=!p!] hC [U:h=!f!junk] hD");
    REQUIRE(invalid_warnings.size() == 1);
    CHECK(invalid_warnings.front().find("[invalid-symbol]") != std::string::npos);
    CHECK(invalid == abc::read(kStrictHeader + "!p!C !p!D").value);
}

TEST_CASE("test_shorthand_written_scope", "[abc][syntax]") {
    const std::string opening = "%abc-2.1\nX:1\nT:Written shorthand\nM:4/4\nL:1/4\nU:H=!trill!\n";
    const std::string key     = "K:C\n";
    for (const auto& [body, expanded] : {
             std::pair{"|: HCDEF | [U:H=!nil!] HGABc :|", "|: !trill!CDEF | GABc :|"},
             {"|: HCDEF |1 [U:H=!nil!] HGABc :|2 HEFGA |]", "|: !trill!CDEF |1 GABc :|2 EFGA |]"},
             {"V:one\nHCDEF |]\nU:H=!nil!\nV:two\nHGABc |]",
              "V:one\n!trill!CDEF |]\nV:two\nGABc |]"},
         }) {
        CAPTURE(body);
        const auto actual   = abc::read(opening + key + body);
        const auto expected = abc::read(opening + key + expanded);
        CHECK(actual.warnings.empty());
        CHECK(expected.warnings.empty());
        CHECK(actual.value == expected.value);
    }

    const auto part_header = opening + "P:B\n" + key;
    const auto actual      = abc::read(part_header + "P:A\nHCDEF | [U:H=!nil!]\nP:B\nHGABc |]");
    const auto expected    = abc::read(part_header + "P:A\n!trill!CDEF |\nP:B\nGABc |]");
    CHECK(actual.warnings.empty());
    CHECK(expected.warnings.empty());
    CHECK(actual.value == expected.value);
}

TEST_CASE("test_macro_before_shorthand", "[abc][syntax]") {
    const auto [score, warnings] = abc::read(
        "%abc-2.1\nm: ~G3 = C3\nX:1\nT:Static macros\nL:1/4\n"
        "m: ~G3 = hG{A}G{F}G\n+: \nm: ~=a3=a{b}a{g}a\nU:h=!p!\nK:C\n"
        "~G3 ~=a3 G3 ~G30 ~G3/2 ~g3 ~F3 A|]"
    );
    const auto [expanded, direct_warnings]
        = abc::read(kStrictHeader + "!p!G{A}G{F}G a{b}a{g}a G3 ~G30 ~G3/2 ~g3 ~F3 A|]");
    CHECK(warnings.empty());
    CHECK(direct_warnings.empty());
    CHECK(melody(score) == melody(expanded));
    CHECK(*score.tracks->front()->notes == *expanded.tracks->front()->notes);
}

TEST_CASE("test_macro_equals_sign", "[abc][syntax]") {
    for (const auto& [definition, music, expanded] : {
             std::tuple{"F=G = CD", "F=G A", "CD A"},
             {"F =G = C=D", "F =G A", "C=D A"},
             {"\"^1=2\"F=E", "\"^1=2\"F A", "E A"},
             {"!a=b!F=E", "!a=b!F A", "E A"},
             {"~G3=C=F2", "~G3 A", "C=F2 A"},
             {"~G3=[Q:1/4 = 96]C3", "~G3 A", "[Q:1/4 = 96]C3 A"},
         }) {
        CAPTURE(definition);
        const auto actual   = abc::read(kStrictHeader + "m: " + definition + '\n' + music + " |]");
        const auto expected = abc::read(kStrictHeader + expanded + " |]");
        CHECK(actual.warnings.empty());
        REQUIRE(expected.warnings.empty());
        CHECK(actual.value == expected.value);
    }
}

TEST_CASE("test_macro_unicode_limits", "[abc][syntax]") {
    const auto annotation = [](i32 length) {
        std::string text = "\"^";
        for (i32 i = 0; i < length; ++i) text += "\U0001D11E";
        return text + '"';
    };
    for (const auto& [target, replacement] : {
             std::pair{annotation(27) + 'C', std::string("D")},
             {std::string("C"), annotation(196) + 'D'},
         }) {
        const auto [score, warnings]
            = abc::read(kStrictHeader + "m: " + target + " = " + replacement + '\n' + target);
        CHECK(warnings.empty());
        CHECK(pitches(score) == vec<i32>{62});
    }

    for (const auto& definition : {
             annotation(28) + "C = D",
             "C = " + annotation(197) + 'D',
         }) {
        const auto [score, warnings]
            = abc::read(kStrictHeader + "m: " + definition + "\nC", "macro.abc");
        REQUIRE(warnings.size() == 1);
        CHECK(warnings.front().starts_with("macro.abc:7:1: ABC [invalid-macro]"));
        CHECK(pitches(score) == vec<i32>{60});
    }
}

TEST_CASE("test_transposing_macro_octaves", "[abc][syntax]") {
    const auto [score, warnings]
        = abc::read(kStrictHeader + "m: ~n3 = n{o}n{m}n\n~G3 ~c3 ~C,3 ~b'3 ~^G3 ~G2 |]");
    const auto [expanded, direct_warnings] = abc::read(
        kStrictHeader + "G{A}G{F}G c{d}c{B}c C,{D,}C,{B,,}C, b'{c''}b'{a'}b' ~^G3 ~G2 |]"
    );
    CHECK(warnings.empty());
    CHECK(direct_warnings.empty());
    CHECK(score == expanded);
}

TEST_CASE("test_macro_single_expansion", "[abc][syntax]") {
    const auto [score, warnings] = abc::read(
        kStrictHeader
        + "m: ~C2=~G2\nm: ~G2=B2\n"
          "~C2 [m:~C2=hE2] [U:h=!p!] ~C2\n"
          "m: ~C2=h\n+: F2\n"
          "[Q:\"~C2\" 1/4=96] \"^~C2 [m:~C2=A2]\" ~C2 ~G2 |]"
    );
    const auto [expanded, direct_warnings]
        = abc::read(kStrictHeader + "~G2 [U:h=!p!] hE2 [Q:\"~C2\" 1/4=96] hF2 B2 |]");
    CHECK(warnings.empty());
    CHECK(direct_warnings.empty());
    CHECK(score == expanded);
}

TEST_CASE("test_implicit_bang_linebreak", "[abc][syntax]") {
    const auto expected          = abc::read(kHeader + "CD|!p!EF|]").value;
    const auto [loose, warnings] = abc::read(kHeader + "C! D|! E% comment\nF|]");
    CHECK(warnings.empty());
    CHECK(loose == abc::read(kHeader + "CD| EF|]").value);
    const auto [mixed, mixed_warnings]
        = abc::read("%abc-2.1\nI:abc-version 2.0\n" + kHeader + "C! D|! +p+EF|]");
    CHECK(mixed_warnings.empty());
    CHECK(mixed == expected);
    rejects("%abc-2.0\nI:abc-version 2.1\n" + kHeader + "C! D", "8:2: ABC [malformed-group]");
}

TEST_CASE("test_body_delimiter_scope", "[abc][syntax]") {
    const auto [score, warnings] = abc::read(
        kStrictHeader + "I:decoration +\nC [I:linebreak !] !p!D\n%%continueall true\nE"
    );
    REQUIRE(warnings.size() == 3);
    for (const auto& warning : warnings)
        CHECK(warning.find("[header-only-instruction]") != std::string::npos);
    CHECK(score == abc::read(kStrictHeader + "C!p!DE").value);
}

TEST_CASE("test_continued_itinerary", "[abc][syntax]") {
    const auto [score, warnings] = abc::read(
        "X:1\nM:4/4\nL:1/4\nP:(B.\n% order continues\n+:A).2\nK:C\n"
        "P:A\nC |\n[P:B]D |"
    );
    CHECK(warnings.empty());
    CHECK(melody(score) == Notes{{1, 479, 62}, {481, 479, 60}, {961, 479, 62}, {1441, 479, 60}});
}

TEST_CASE("test_continued_option_comment", "[abc][syntax]") {
    const auto [score, warnings] = abc::read(
        "%abc-2.1\nX:1\nT:Voice\nM:4/4\nL:1/4\nV:lead name=\"Lead\n% keep the field open\n"
        "+: 100\\%\n+: Singer\" % transpose=12\nK:C\n[V:lead] C\n"
    );
    CHECK(warnings.empty());
    CHECK(melody(score) == Notes{{1, 479, 60}});
    CHECK(score.tracks->front()->name == "Lead 100% Singer");
}

TEST_CASE("test_interrupted_music_continuation", "[abc][syntax]") {
    const auto [score, warnings] = abc::read(
        kHeader
        + "C\\ \t% joined\r\n  % skipped\r\n  K:G\r\n%%vocalfont Times-Roman 14\r\n"
          " F G |\r\nw: one two\r\n%%vocalfont Times-Italic *\r\n  +: three"
    );
    CHECK(warnings.empty());
    CHECK(melody(score) == Notes{{1, 479, 60}, {481, 479, 66}, {961, 479, 67}});
    const auto& lyrics = *score.tracks->front()->lyrics;
    REQUIRE(lyrics.size() == 3);
    CHECK(lyrics[0].text == "one");
    CHECK(lyrics[1].text == "two");
    CHECK(lyrics[2].text == "three");

    rejects(
        kHeader + "C\\\r\n% skipped\r\nK:G\r\n  F/0 % source", "9:4: ABC [zero-length-divisor]"
    );
    rejects(kHeader + "C\\\r\n% skipped\r\n  Q:0", "8:3: ABC [invalid-tempo]");
}

TEST_CASE("test_continued_note_groups", "[abc][syntax]") {
    for (const auto& [music, joined] : {
             std::pair{"[C\\\n%%vocalfont Times-Roman 14\nE] D", "[CE] D"},
             {"[C\\\nI:example\nE] D", "[CE] D"},
             {"F [C\\\nQ:1/4=90\nE] D", "F [Q:1/4=90][CE] D"},
             {"{[C\\\n%%vocalfont Times-Roman 14\nE]} G D", "{[CE]} G D"},
         }) {
        CAPTURE(music);
        const std::string lyrics   = " |]\nw: one two";
        const auto        actual   = abc::read(kStrictHeader + music + lyrics);
        const auto        expected = abc::read(kStrictHeader + joined + lyrics);
        REQUIRE(actual.warnings.empty());
        REQUIRE(expected.warnings.empty());
        CHECK(actual.value == expected.value);
        CHECK(sung(actual.value) == sung(expected.value));
    }

    rejects(
        kStrictHeader + "[C\\\n%%vocalfont Times-Roman 14\nE/0] D", "9:2: ABC [zero-length-divisor]"
    );
}

TEST_CASE("test_continued_macro_scope", "[abc][syntax]") {
    const auto actual = abc::read(
        kStrictHeader
        + "m: ~C2 = D2\n~C2 [E\\\nm: ~C2 = F2\nG] ~C2\\\n"
          "m: ~C2 = A2\n~C2 |]"
    );
    const auto expected = abc::read(kStrictHeader + "D2 [EG] F2 A2 |]");
    REQUIRE(actual.warnings.empty());
    CHECK(actual.value == expected.value);
}

TEST_CASE("test_blank_music_line", "[abc][syntax]") {
    const auto [score, warnings] = abc::read(kHeader + "C\\\n \t\nD\nX:2\nK:D\nE", "case.abc");
    CHECK(melody(score) == Notes{{1, 479, 60}});
    REQUIRE(warnings.size() == 2);
    CHECK(warnings[0].starts_with("case.abc:6:2: ABC [stray-continuation]"));
    CHECK(warnings[1].starts_with("case.abc:9:1: ABC [extra-tunes]"));
}

TEST_CASE("test_continued_quote", "[abc][syntax]") {
    const auto [score, warnings] = abc::read(kStrictHeader + "\"^label \\\n% skipped\n100\\%\"C");
    CHECK(warnings.empty());
    CHECK(melody(score) == Notes{{1, 479, 60}});
}

TEST_CASE("test_quoted_comments", "[abc][syntax]") {
    // ABC 2.1 sections 2.2.5 and 8.2: escape a literal percent, even in an annotation.
    const auto [score, warnings] = abc::read(
        "%abc-2.1\nX:1\nT:100\\% ready % comment\nL:1/4\nK:C\n"
        R"("^note &quot;quote&quot; 50\%" C % D)"
    );
    CHECK(warnings.empty());
    CHECK(melody(score) == Notes{{1, 479, 60}});
    CHECK(score.tracks->front()->name == "100% ready");
    rejects(kStrictHeader + R"(C "^50%" D)", "7:3: ABC [malformed-group]");
}

TEST_CASE("test_continuation_inline_remark", "[abc][syntax]") {
    const auto expected = abc::read(kStrictHeader + "[CE] D |]").value;
    for (const auto body : {
             "[C\\ [r:remark]\nE] D |]",
             "[C\\ [r:a quote \" in a remark][r:another remark]\nE] D |]",
             "\"^text \\\n[r:literal]\" [C\\ [r:remark]\nE] D |]",
         }) {
        CAPTURE(body);
        const auto [score, warnings] = abc::read(kStrictHeader + body);
        CHECK(warnings.empty());
        CHECK(score == expected);
    }
    rejects(kStrictHeader + "[C\\ [r:remark]\nE]/0", "8:3: ABC [zero-length-divisor]");
}

TEST_CASE("test_continued_format_instruction", "[abc][syntax]") {
    const auto [score, warnings] = abc::read(
        "%abc-2.1\nX:1\nT:Layout\nM:4/4\nI:vocalfont\n+: Times-Italic *\nL:1/4\nK:C\n"
        "C D |]\nw:one\nI:vocalfont Times-\n+:Italic *"
    );
    CHECK(warnings.empty());
    CHECK(pitches(score) == vec{60, 62});
    CHECK(sung(score) == Lyrics{{1, "one"}});
}

TEST_CASE("test_field_continuations", "[abc][syntax]") {
    const auto [score, warnings] = abc::read(
        "X:1\nT: Two\n% skipped\n  +: Words\nM:4/4\nL:1/\n+:4\n"
        "K:C trans\n+:pose=2\nC"
    );
    CHECK(warnings.empty());
    CHECK(melody(score) == Notes{{1, 479, 62}});
    CHECK(score.tracks->front()->name == "Two Words");

    rejects("X:1\nK :C\nC", "2:1: ABC [missing-key]");
    const auto orphan = abc::read(kHeader + "C\n  +: orphan", "case.abc");
    REQUIRE(orphan.warnings.size() == 1);
    CHECK(orphan.warnings.front().starts_with("case.abc:7:3: ABC [orphan-field-continuation]"));
}

TEST_CASE("test_voice_id_length", "[abc][syntax]") {
    const std::string id    = "1234567890123456789\xc3\xa9";
    const auto        intro = "X:1\nM:4/4\nL:1/4\nV:" + id + "first\n  +: name=Lead\nK:C\n";
    const auto        body = "[V:" + id + "second] C\n[V:1234567890123456789x name=\"Bass [2]\"] D";
    const auto [score, warnings] = abc::read(intro + body);
    CHECK(warnings.empty());
    REQUIRE(score.tracks->size() == 2);
    CHECK((*score.tracks)[0]->name == "Lead");
    CHECK((*score.tracks)[1]->name == "Bass [2]");
    REQUIRE((*score.tracks)[0]->notes->size() == 1);
    REQUIRE((*score.tracks)[1]->notes->size() == 1);
    CHECK((*(*score.tracks)[0]->notes)[0].pitch == 60);
    CHECK((*(*score.tracks)[1]->notes)[0].pitch == 62);
}

TEST_CASE("test_numeric_voice_ids", "[abc][syntax]") {
    const auto [score, warnings] = abc::read(
        kStrictHeader
        + "[V:01 name=Lead] C [V:1] D [V:001] E |]\n"
          "[V:002 name=Bass] F [V:2] G |]\n"
          "[V:01a name=Other] A |]"
    );
    REQUIRE(warnings.empty());
    REQUIRE(score.tracks->size() == 3);
    CHECK(score.tracks->at(0)->name == "Lead");
    CHECK(score.tracks->at(1)->name == "Bass");
    CHECK(score.tracks->at(2)->name == "Other");
    vec<Notes> voices;
    for (const auto& track : *score.tracks) {
        auto& notes = voices.emplace_back();
        for (const auto& note : *track->notes)
            notes.emplace_back(note.time, note.duration, note.pitch);
    }
    CHECK(
        voices
        == vec<Notes>{
            {{1, 479, 60}, {481, 479, 62}, {961, 479, 64}},
            {{1, 479, 65}, {481, 479, 67}},
            {{1, 479, 69}}
        }
    );
}

TEST_CASE("test_numeric_voice_prefix", "[abc][syntax]") {
    const auto [score, warnings] = abc::read(
        kStrictHeader
        + "[V:12345678901234567890] C\n[V:12345678901234567890a] D\n"
          "[V:12345678901234567890b] E |]"
    );
    REQUIRE(warnings.empty());
    REQUIRE(score.tracks->size() == 2);
    CHECK(score.tracks->at(0)->notes->size() == 1);
    CHECK(score.tracks->at(1)->notes->size() == 2);
}

TEST_CASE("test_tune_boundary", "[abc][syntax]") {
    for (const std::string newline : {"\n", "\r\n", "\r"}) {
        CAPTURE(newline);
        const auto source = "\xef\xbb\xbfX:1" + newline + "M:4/4" + newline + "L:1/4" + newline
                            + "K:C" + newline + "C/ % comment" + newline + newline + "X:2" + newline
                            + "K:G" + newline + "F";
        const auto [score, warnings] = abc::read(source);
        CHECK(melody(score) == Notes{{1, 239, 60}});
        REQUIRE(warnings.size() == 1);
        CHECK(warnings.front().starts_with("<string>:7:1: ABC [extra-tunes]"));
    }
}

TEST_CASE("test_file_charset", "[abc][syntax]") {
    const std::string prefix = "%abc-2.1\n";
    constexpr auto    tune   = "X:1\nT:Caf{}\nK:C\nC\nw: caf{}";
    const auto [utf8, utf8_warnings]
        = abc::read(prefix + "I:abc-charset utf-8\n" + fmt::format(tune, "é", "é"));
    const auto [ascii, ascii_warnings]
        = abc::read(prefix + "%%abc-charset us-ascii\n" + fmt::format(tune, "&eacute;", "\\u00e9"));
    CHECK(utf8_warnings.empty());
    CHECK(ascii_warnings.empty());
    CHECK(ascii == utf8);
    REQUIRE(ascii.tracks->size() == 1);
    CHECK(ascii.tracks->front()->name == "Café");
    CHECK(sung(ascii) == Lyrics{{1, "café"}});

    rejects(prefix + "I:abc-charset us-ascii\n% é\n" + kHeader + "C", "3:3: ABC [invalid-text]");
    rejects(prefix + "% é\nI:abc-charset us-ascii\n" + kHeader + "C", "2:3: ABC [invalid-text]");
    rejects(
        prefix + "I:abc-charset us-\n+:ascii\n% é\n" + kHeader + "C", "4:3: ABC [invalid-text]"
    );
}

TEST_CASE("test_copyright_fields", "[abc][syntax]") {
    const auto plain             = abc::read("%abc-2.1\nX:1\nT:Example\nK:C\nC |]");
    const auto [score, warnings] = abc::read(
        "%abc-2.1\nZ:abc-copyright &copy; Collection\n"
        "X:1\nT:Example\nZ:Transcribed by Alice\nZ:abc-copyright Bob\\u0025\nK:C\nC |]"
    );
    REQUIRE(warnings.empty());
    CHECK(score == plain.value);
    CHECK(abc::write(score).value.find("abc-copyright") == std::string::npos);
    const auto [directives, directive_warnings]
        = abc::read("X:1\nT:Example\nK:C\n%%abc-copyright Example\nC [I:abc-copyright x] |]");
    CHECK(directive_warnings.empty());
    CHECK(directives == abc::read("X:1\nT:Example\nK:C\nC |]").value);
}

TEST_CASE("test_portable_text_escapes", "[abc][syntax]") {
    const auto [title, title_warnings] = abc::read(
        "%abc-2.1\nX:1\nT:"
        R"(&quot;A\u0022 \u266F \U0001D11E &copy; \&copy; &amp;quot; \\u0022)"
        "\nK:C\nC"
    );
    CHECK(title_warnings.empty());
    REQUIRE(title.tracks->size() == 1);
    CHECK(
        title.tracks->front()->name
        == "\"A\" \xE2\x99\xAF \xF0\x9D\x84\x9E \xC2\xA9 &copy; &quot; \\u0022"
    );

    const auto [voice, voice_warnings] = abc::read(
        "%abc-2.1\nX:1\nT:Voices\nK:C\n"
        R"abc(V:one name="\u0022A&quot; \U00110000 \uD800 \u01xy")abc"
        "\nC"
    );
    CHECK(voice_warnings.empty());
    REQUIRE(voice.tracks->size() == 1);
    CHECK(voice.tracks->front()->name == R"("A" \U00110000 \uD800 \u01xy)");
}

TEST_CASE("test_font_switches", "[abc][syntax]") {
    // Section 11.4.2 defines $0–$4 as font switches and $$ as a literal dollar.
    const std::string opening          = "%abc-2.1\nX:1\nT:$1Flute$0 $$2 \\u00243 $5\nL:1/4\nK:C\n";
    const auto [title, title_warnings] = abc::read(opening + "C");
    CHECK(title_warnings.empty());
    REQUIRE(title.tracks->size() == 1);
    CHECK(title.tracks->front()->name == "Flute $2 $3 $5");

    const auto [voice, voice_warnings] = abc::read(
        opening + "V:one name=\"$2Lead$3 $$0$4\"\nCDEF\nw: $1 $2first$0 $3 $$4 $4- last"
    );
    CHECK(voice_warnings.empty());
    REQUIRE(voice.tracks->size() == 1);
    const auto& track = *voice.tracks->front();
    CHECK(track.name == "Lead $0");
    CHECK(*track.lyrics == pyvec<TextMeta<Tick>>{{1, "first"}, {481, "$4"}, {1441, "last"}});

    // A run containing only font changes must be skipped as a unit.
    std::string font_run;
    for (i32 i = 0; i < 32'768; ++i) font_run += "$1";
    const auto [restyled, style_warnings] = abc::read(opening + "C\nw: " + font_run + " first");
    CHECK(style_warnings.empty());
    REQUIRE(restyled.tracks->size() == 1);
    CHECK(*restyled.tracks->front()->lyrics == pyvec<TextMeta<Tick>>{{1, "first"}});

    const auto [unstyled, warnings] = abc::read("X:1\nT:$1Flute$0 $$2\nK:C\nC\nw: cash$$");
    CHECK(warnings.empty());
    REQUIRE(unstyled.tracks->size() == 1);
    CHECK(unstyled.tracks->front()->name == "Flute $2");
    CHECK(*unstyled.tracks->front()->lyrics == pyvec<TextMeta<Tick>>{{1, "cash$"}});
}

TEST_CASE("test_track_name_escapes", "[abc][syntax]") {
    // Section 8.2 defines text mnemonics, named entities and fixed-width escapes for information
    // fields.
    const auto [title, warnings] = abc::read(
        "%abc\nX:1\nT:"
        R"(\"Osten \ua \OE \HO \'e)"
        "\nI:abc-version 2.1\nK:C\nC"
    );
    const auto entities
        = abc::read("%abc-2.1\nX:1\nT:&Ouml;sten &abreve; &OElig; \\u0150 &eacute;\nK:C\nC").value;
    CHECK(warnings.empty());
    REQUIRE(title.tracks->size() == 1);
    REQUIRE(entities.tracks->size() == 1);
    const auto& name = title.tracks->front()->name;
    CHECK(name == "\xC3\x96sten \xC4\x83 \xC5\x92 \xC5\x90 \xC3\xA9");
    CHECK(entities.tracks->front()->name == name);

    const auto [voice, voice_warnings] = abc::read(
        "%abc\nX:1\nT:Title\nL:1/4\nK:C\n"
        R"abc(V:one name="Flute &quot;echo\u0022 (\"e) &copy; \\u266F")abc"
        "\nC"
    );
    CHECK(voice_warnings.empty());
    REQUIRE(voice.tracks->size() == 1);
    CHECK(voice.tracks->front()->name == R"abc(Flute "echo" (ë) © \u266F)abc");
    const auto entity_title = abc::read("%abc\nX:1\nT:Flute &copy;\nK:C\nC").value;
    REQUIRE(entity_title.tracks->size() == 1);
    CHECK(entity_title.tracks->front()->name == "Flute ©");
}

TEST_CASE("test_voice_name_linebreaks", "[abc][syntax]") {
    const auto [score, read_warnings] = abc::read(
        "%abc-2.1\nX:1\nT:Flute\nK:C\n"
        R"(V:one name="Lead\nFlute\\nLiteral")"
        "\nC"
    );
    CHECK(read_warnings.empty());
    REQUIRE(score.tracks->size() == 1);
    CHECK(score.tracks->front()->name == "Lead\nFlute\\nLiteral");

    const auto [text, warnings] = abc::write(score);
    CHECK(warnings.empty());
    CHECK(text.find(R"(name="Lead\nFlute\\nLiteral")") != std::string::npos);
    CHECK(abc::read(text).value.tracks->front()->name == score.tracks->front()->name);

    const auto [title, title_warnings] = abc::read("%abc-2.1\nX:1\nT:Lead\\nFlute\nK:C\nC");
    CHECK(title_warnings.empty());
    CHECK(title.tracks->front()->name == R"(Lead\nFlute)");
}

TEST_CASE("test_field_collisions", "[abc][syntax]") {
    // All three input times project to tick 480. Break exact ties by input order.
    Score<Tick> score(1920);
    for (const auto [time, value] : {std::pair{1921, 1}, {1921, 2}, {1919, 3}}) {
        score.key_signatures->emplace_back(time, value, 0);
        score.time_signatures->emplace_back(time, value, 4);
        score.tempos->push_back(Tempo<Tick>::from_qpm(time, 30 * value));
    }
    const auto [text, warnings] = abc::write(score);
    for (const std::string field : {"[K:D]", "[M:2/4]", "[Q:1/4=60]"}) {
        CAPTURE(field);
        CHECK(text.find(field) != std::string::npos);
        const auto label = "ABC export " + field.substr(1, 1) + ": field [duplicate-time]";
        CHECK(
            std::ranges::count_if(warnings, [&](const auto& w) { return w.starts_with(label); })
            == 1
        );
    }
}

TEST_CASE("test_voice_field_order", "[abc][syntax]") {
    const auto [score, warnings] = abc::read(
        kHeader
        + "V:a\nC2 [K:D][M:2/4][Q:1/4=60] C\n"
          "V:b\nC [K:F][M:3/4][Q:1/4=100] C [K:A][M:5/4][Q:1/4=90] C"
    );
    REQUIRE(score.key_signatures->size() == 3);
    REQUIRE(score.time_signatures->size() == 3);
    REQUIRE(score.tempos->size() == 3);
    CHECK(score.key_signatures->at(1) == KeySignature<Tick>{480, -1, 0});
    CHECK(score.key_signatures->at(2) == KeySignature<Tick>{960, 2, 0});
    CHECK(score.time_signatures->at(1) == TimeSignature<Tick>{480, 3, 4});
    CHECK(score.time_signatures->at(2) == TimeSignature<Tick>{960, 2, 4});
    CHECK(score.tempos->at(1) == Tempo<Tick>{480, 600000});
    CHECK(score.tempos->at(2) == Tempo<Tick>{960, 1000000});
    for (const std::string field : {"key", "meter", "tempo"}) {
        const auto detail = "[conflicting-metadata]: conflicting " + field + " changes at tick 960";
        CHECK(
            std::ranges::count_if(warnings, [&](const auto& w) { return w.find(detail) != w.npos; })
            == 1
        );
    }
}

}   // namespace symusic::abc_test

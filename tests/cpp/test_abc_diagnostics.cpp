#include <algorithm>
#include <array>

#include <fmt/format.h>

#include "abc_test.h"
#include "../../src/io/abc/syntax.h"

namespace symusic::abc_test {

TEST_CASE("test_diagnostic_line_ownership", "[abc][diagnostics]") {
    std::string              text = "C]";
    abc::detail::Diagnostics diagnostics("tune.abc");
    const auto               first = diagnostics.record_line(0, "first");
    diagnostics.record_line(0, "");
    diagnostics.record_line(0, text);
    diagnostics.record_line(0, std::string(4096, 'x'));
    diagnostics.record_line(0, "last");
    text.assign(text.size(), 'D');
    CHECK(first == "first");
    diagnostics.warn({3, 2}, "stray-chord-close", "ignored ']'");
    diagnostics.warn({5, 2}, "example", "last line");

    const auto warnings = diagnostics.take();
    REQUIRE(warnings.size() == 2);
    CHECK(warnings.front() == "tune.abc:3:2: ABC [stray-chord-close]: ignored ']'\n\"C]\"");
    CHECK(warnings.back() == "tune.abc:5:2: ABC [example]: last line\n\"last\"");
}

TEST_CASE("test_continued_source_location", "[abc][diagnostics]") {
    const auto [score, warnings] = abc::read("X:1\r\nM:4/4\r\nK:C\r\nC\\\r\n  ]D", "tune.abc");
    REQUIRE(score.note_num() == 2);
    REQUIRE(warnings.size() == 1);
    CHECK(
        warnings.front()
        == "tune.abc:5:3: ABC [stray-chord-close]: ignored ']' outside a chord\n\"  ]D\""
    );
    CHECK_THROWS_AS(abc::read("X:1\nL:1/4\nCDEF"), std::invalid_argument);
    CHECK_THROWS_AS(abc::read("X:1\nK:C\n[CDE"), std::invalid_argument);
}

TEST_CASE("test_invalid_key_context", "[abc][diagnostics]") {
    rejects("X:1\nK:bad\nC", "2:1: ABC [invalid-key]");
    const auto [score, warnings] = abc::read("X:1\nK:C\n^F [K:bad] F", "case.abc");
    CHECK(pitches(score) == vec<i32>{66, 66});
    REQUIRE(warnings.size() == 1);
    CHECK(warnings.front().find("case.abc:3:5: ABC [invalid-inline-key]") == 0);
}

TEST_CASE("test_backing_note_overflow", "[abc][diagnostics]") {
    const std::string input = "X:1\nM:4/4\nL:1/1920\nK:C\n%%MIDI gchordoff\n"
                              "z2147483547|\n%%MIDI gchord f\n%%MIDI gchordon\n\"C\"C";
    CHECK_THROWS_AS(abc::read(input), std::invalid_argument);
    CHECK(abc::read("X:1\nM:4/4\nL:1/4\nK:C\nz1000001 C").value.note_num() == 1);
}

TEST_CASE("test_invalid_decoration_bytes", "[abc][diagnostics]") {
    CHECK_THROWS_WITH(
        abc::read("X:1\nK:C\n!\xff! CDEF", "bytes.abc"),
        "bytes.abc:3:2: ABC [invalid-text]: ABC text must be valid UTF-8\n\"!\\xff! CDEF\""
    );
}

TEST_CASE("test_invalid_tuning_system", "[abc][diagnostics]") {
    const auto [score, warnings] = abc::read(
        "%%MIDI tuningsystem comma53\n" + kHeader
        + "C [I:MIDI tuningsystem comma53] C [I:MIDI tuningsystem unknown] C F |]"
    );
    CHECK(pitches(score) == vec<i32>{60, 59, 59, 64});
    REQUIRE(warnings.size() == 2);
    CHECK(warnings[0].find("[tuning-system-placement]") != std::string::npos);
    CHECK(warnings[1].find("[invalid-tuning-system]") != std::string::npos);
}

TEST_CASE("test_multimeasure_counts", "[abc][diagnostics]") {
    const auto [score, warnings] = abc::read(kStrictHeader + "[M:3/4] Z | X | D3 |]");
    CHECK(warnings.empty());
    CHECK(melody(score) == Notes{{2881, 1439, 62}});
    rejects(kStrictHeader + "Z0 |]", "7:2: ABC [invalid-measure-rest]");
    rejects(kStrictHeader + "X3/2 |]", "7:2: ABC [invalid-measure-rest]");
    rejects(kStrictHeader + "Z2000000000 |]", "7:1: ABC [expansion-limit]");
}

TEST_CASE("test_skipped_header_includes", "[abc][diagnostics]") {
    const auto [score, warnings] = abc::read_file("testcases/abc_headers/skipped.abc");
    CHECK(melody(score) == Notes{{1, 239, 60}, {241, 239, 62}});
    REQUIRE(warnings.size() == 5);
    CHECK(warnings[0].find("skipped.abc:4:1: ABC [unavailable-include]") != std::string::npos);
    CHECK(warnings[1].find("skipped.abc:5:1: ABC [unavailable-include]") != std::string::npos);
    CHECK(warnings[2].find("skipped.abh:2:1: ABC [invalid-include-content]") != std::string::npos);
    CHECK(warnings[3].find("skipped.abh:3:1: ABC [invalid-include-content]") != std::string::npos);
    CHECK(warnings[4].find("skipped.abc:8:4: ABC [header-only-instruction]") != std::string::npos);
}

TEST_CASE("test_included_file_locations", "[abc][diagnostics]") {
    CHECK_THROWS_MATCHES(
        abc::read_file("testcases/abc_headers/bad.abc"),
        std::invalid_argument,
        Catch::Matchers::MessageMatches(
            Catch::Matchers::ContainsSubstring("bad.abh:2:3: ABC [invalid-unit-length]")
        )
    );
    const auto [score, warnings] = abc::read(
        "%abc-2.1\nI:abc-include defaults.abh\nX:1\nT:Text only\nK:C\nC",
        "testcases/abc_headers/tune.abc"
    );
    REQUIRE(warnings.size() == 1);
    CHECK(warnings.front().find("tune.abc:2:1: ABC [external-include]") != std::string::npos);
    CHECK(pitches(score) == vec<i32>{60});
}

TEST_CASE("test_field_scopes", "[abc][diagnostics]") {
    const auto [score, warnings] = abc::read(
        "%abc-2.1\nL:1/4\nQ:1/4=60\nV:wrong transpose=12\n\n"
        "X:1\nT:Example\nw:not body text\ns:!ppp!\nK:C\n"
        "O:header only\nj:unknown\nC [T:not inline] [N:allowed]D |]",
        "fields.abc"
    );
    REQUIRE(warnings.size() == 7);
    for (const auto& [line, column, code] : {
             std::tuple{3, 1, "invalid-file-header-field"},
             {4, 1, "invalid-file-header-field"},
             {8, 1, "invalid-tune-header-field"},
             {9, 1, "invalid-tune-header-field"},
             {11, 1, "invalid-body-field"},
             {12, 1, "invalid-body-field"},
             {13, 4, "invalid-inline-field"},
         }) {
        const auto prefix = fmt::format("fields.abc:{}:{}: ABC [{}]", line, column, code);
        CHECK(std::ranges::any_of(warnings, [&](const auto& item) {
            return item.starts_with(prefix);
        }));
    }
    CHECK(score == abc::read("%abc-2.1\nL:1/4\n\nX:1\nT:Example\nK:C\nCD |]").value);

    const auto old_defaults = abc::read("Q:1/4=60\n\nX:1\nT:Legacy\nL:1/4\nK:C\nC |]");
    CHECK(old_defaults.warnings.empty());
    CHECK(*old_defaults.value.tempos == pyvec<Tempo<Tick>>{{0, 1'000'000}});
}

TEST_CASE("test_detached_ties", "[abc][diagnostics]") {
    for (const auto& [body, position] : {
             std::pair{"C -C", "7:3"},
             {"C .-C", "7:3"},
             {"C @-C", "7:4"},
             {"C |-C", "7:4"},
             {"C\n-C", "8:1"},
             {"C \\\n-C", "8:1"},
             {"C\\\n -C", "8:2"},
         }) {
        CAPTURE(body);
        const auto [score, warnings] = abc::read(kStrictHeader + body, "ties.abc");
        REQUIRE(warnings.size() == 1);
        CHECK(warnings.front().starts_with(
            std::string("ties.abc:") + position + ": ABC [detached-tie]"
        ));
        CHECK(melody(score) == Notes{{1, 479, 60}, {481, 479, 60}});
    }

    const auto [chord, warnings] = abc::read(kStrictHeader + "[CE] -[CF]");
    REQUIRE(warnings.size() == 1);
    CHECK(chord == abc::read(kStrictHeader + "[CE] [CF]").value);

    const auto [loose, loose_warnings] = abc::read(kHeader + "C -C");
    CHECK(loose_warnings.empty());
    CHECK(melody(loose) == Notes{{1, 959, 60}});
}

TEST_CASE("test_deprecated_tempo_locations", "[abc][diagnostics]") {
    const std::string opening    = "X:1\nT:Tempos\nL:1/8\n";
    const std::string deprecated = "Q:120\nI:abc-version 2.1\nK:C\n"
                                   "|: C\nQ:C=60\nD [L:1/4][Q:120] E :| F";
    const auto [score, warnings] = abc::read(opening + deprecated, "tempo.abc");
    REQUIRE(warnings.size() == 3);
    CHECK(warnings.at(0).starts_with("tempo.abc:4:1: ABC [deprecated-tempo]"));
    CHECK(warnings.at(1).starts_with("tempo.abc:8:1: ABC [deprecated-tempo]"));
    CHECK(warnings.at(2).starts_with("tempo.abc:9:11: ABC [deprecated-tempo]"));

    const auto [canonical, canonical_warnings] = abc::read(
        "%abc-2.1\n" + opening
        + "Q:1/8=120\nK:C\n"
          "|: C\nQ:1/8=60\nD [L:1/4][Q:1/4=120] E :| F"
    );
    CHECK(canonical_warnings.empty());
    CHECK(score == canonical);
    REQUIRE(score.tempos->size() >= 3);
    CHECK(score.tempos->at(0).mspq == 1000000);
    CHECK(score.tempos->at(1).mspq == 2000000);
    CHECK(score.tempos->at(2).mspq == 500000);
}

TEST_CASE("test_deprecated_metadata", "[abc][diagnostics]") {
    const std::string music       = "X:1\nT:Example\nA:Bampton\nE:4\n"
                                    "%%abc-copyright Example\n%%abc-edited-by Example\nK:C\nC";
    const auto [strict, warnings] = abc::read("%abc-2.1\n" + music, "outdated.abc");
    REQUIRE(warnings.size() == 4);
    CHECK(warnings.at(0).starts_with("outdated.abc:4:1: ABC [deprecated-field]"));
    CHECK(warnings.at(1).starts_with("outdated.abc:5:1: ABC [deprecated-field]"));
    CHECK(warnings.at(2).starts_with("outdated.abc:6:1: ABC [deprecated-directive]"));
    CHECK(warnings.at(3).starts_with("outdated.abc:7:1: ABC [deprecated-directive]"));
    const auto [loose, loose_warnings] = abc::read("%abc-2.0\n" + music);
    CHECK(loose_warnings.empty());
    CHECK(melody(strict) == melody(loose));
}

TEST_CASE("test_history_version", "[abc][diagnostics]") {
    for (const auto [file, tune, line] : {std::tuple{"2.1", "2.0", 3}, {"2.0", "2.1", 8}}) {
        CAPTURE(file, tune);
        const auto [score, warnings] = abc::read(
            fmt::format(
                "%abc-2.0\nI:abc-version {}\nH:File\nContinued\n\nX:1\nT:Example\n"
                "H:Tune\nContinued\nI:abc-version {}\nK:C\nD |]",
                file,
                tune
            ),
            "history.abc"
        );
        REQUIRE(warnings.size() == 1);
        CHECK(warnings.front().starts_with(
            fmt::format("history.abc:{}:1: ABC [deprecated-history]", line)
        ));
        CHECK(pitches(score) == vec<i32>{62});
    }
}

TEST_CASE("test_malformed_music", "[abc][diagnostics]") {
    for (const auto& [music, where] : {
             std::pair{"C4 [DF", "6:4: ABC [malformed-group]"},
             {"CD [K:G", "6:4: ABC [unclosed-inline-field]"},
             {"C ^1/0D", "6:3: ABC [invalid-microtone]"},
             {"[I:MIDI program 128] C", "6:2: ABC [invalid-midi-value]"},
             {"C4\\\r\n D/0", "7:3: ABC [zero-length-divisor]"},
             {"C/2147483648", "6:2: ABC [number-range]"},
             {"C {[Dz2147483648]}E", "6:7: ABC [number-range]"},
             {"C [xz//1073741825] D", "6:6: ABC [length-range]"},
             {"C {DE", "6:3: ABC [malformed-group]"},
         }) {
        CAPTURE(music);
        rejects(kHeader + music, where);
    }
    rejects(kStrictHeader + "[C!bad] D E !F] |]", "7:1: ABC [malformed-group]");
}

TEST_CASE("test_invalid_meter_groups", "[abc][diagnostics]") {
    for (const std::string_view meter :
         {"(2+3/8", "2+3)/8", "2++3/8", "(2+0)/8", "(254+2)/8", "(2+3)/0", "(2+3)/5"}) {
        CAPTURE(meter);
        const auto line = fmt::format("M:{}", meter);
        rejects("%abc-2.1\nX:1\nT:Meter\n" + line + "\nK:C\nC |]", "4:1: ABC [invalid-meter]");
    }
    rejects(
        "X:1\nM:2147483648/2147483648\nK:C\nC",
        "2:1: ABC [invalid-meter]: meter denominator must be a power of two"
    );
    rejects(
        "X:1\nM:2147483648/4\nK:C\nC",
        "2:1: ABC [invalid-meter]: meter numerator must be between 1 and 255"
    );
}

TEST_CASE("test_recoverable_music", "[abc][diagnostics]") {
    const auto [score, warnings] = abc::read(kHeader + "C ] D", "case.abc");
    REQUIRE(warnings.size() == 1);
    CHECK(warnings.front().starts_with("case.abc:6:3: ABC [stray-chord-close]"));
    CHECK(melody(score) == Notes{{1, 479, 60}, {481, 479, 62}});
}

TEST_CASE("test_title_position", "[abc][diagnostics]") {
    for (const auto& [fields, column] : {
             std::pair{"K:C\n", 1},
             {"  M:4/4\nT:Late\nK:C\n", 3},
             {"m: ~C2 = CC\nT:Late\nK:C\n", 1},
         }) {
        CAPTURE(fields);
        const auto tune              = std::string("X:1\n") + fields + "C |]";
        const auto [score, warnings] = abc::read("%abc-2.1\n" + tune, "title.abc");
        REQUIRE(warnings.size() == 1);
        CHECK(
            warnings[0].starts_with("title.abc:3:" + std::to_string(column) + ": ABC [title-order]")
        );
        CHECK(pitches(score) == vec<i32>{60});
        CHECK(abc::read(tune).warnings.empty());
    }

    const auto titled = abc::read(
        "%abc-2.0\nL:1/4\n\nX:1\n% a comment\nr: a remark\nT:\nT:Alternate\n"
        "I:abc-version 2.1\nK:C\nC |]"
    );
    CHECK(titled.warnings.empty());
    CHECK(pitches(titled.value) == vec<i32>{60});

    for (const std::string_view instruction : {"I:abc-version 2.1\n", "%%abc-version 2.1\n"}) {
        CAPTURE(instruction);
        const auto late
            = abc::read("%abc-2.0\nX:1\n" + std::string(instruction) + "T:Late\nK:C\nC |]");
        REQUIRE(late.warnings.size() == 1);
        CHECK(late.warnings[0].starts_with("<string>:3:1: ABC [title-order]"));
    }
    CHECK(abc::read("%abc-2.1\nX:1\nI:abc-version 2.0\nK:C\nC |]").warnings.empty());
}

TEST_CASE("test_decoration_delimiters", "[abc][diagnostics]") {
    rejects(kStrictHeader + "C+p+D", "7:2: ABC [decoration-delimiter]");
    rejects(kStrictHeader + "C!", "7:2: ABC [malformed-group]");
    const std::string plus = "%abc-2.1\nI:decoration +\n" + kHeader;
    rejects(plus + "C!p!D", "8:2: ABC [decoration-delimiter]");
    rejects(plus + "[C!p!E] D", "8:3: ABC [decoration-delimiter]");
    rejects(plus + "{[C!p!E]} D", "8:4: ABC [decoration-delimiter]");
    rejects(plus + "CD\ns: *\n+: !p!", "10:4: ABC [decoration-delimiter]");
    rejects(plus + "[U:h=!p!] hC", "8:2: ABC [decoration-delimiter]");
    rejects("%abc-2.1\nI:linebreak !\n" + kHeader + "[C!p!E]", "8:3: ABC [decoration-delimiter]");
}

TEST_CASE("test_macro_locations", "[abc][diagnostics]") {
    rejects(kStrictHeader + "m: ~C2=CD$E/0\nC \\\n  ~C2", "9:3: ABC [zero-length-divisor]");
    const auto [score, warnings]
        = abc::read(kStrictHeader + "m: ~C2=CD\nm: ~n=E\n  ~C2 ] E |]", "case.abc");
    REQUIRE(warnings.size() == 2);
    CHECK(warnings[0].starts_with("case.abc:8:1: ABC [invalid-macro]"));
    CHECK(warnings[1].starts_with("case.abc:9:7: ABC [stray-chord-close]"));
    CHECK(pitches(score) == vec<i32>{60, 62, 64});
}

TEST_CASE("test_symbol_line_errors", "[abc][diagnostics]") {
    const auto [invalid, malformed]
        = abc::read(kStrictHeader + "C D\ns: !p! \"missing\nQ:1/4=90\ns: * !pp!", "case.abc");
    REQUIRE(malformed.size() == 1);
    CHECK(malformed.front().starts_with("case.abc:8:8: ABC [invalid-symbol-line]"));
    CHECK(malformed.front().find("s: !p!") != std::string::npos);
    CHECK(invalid == abc::read(kStrictHeader + "C !pp!D\nQ:1/4=90").value);

    const auto [continued, continuation_warnings]
        = abc::read(kStrictHeader + "C D\ns: !p!\n  +: \"missing", "case.abc");
    REQUIRE(continuation_warnings.size() == 1);
    CHECK(continuation_warnings.front().starts_with("case.abc:9:6: ABC [invalid-symbol-line]"));
    CHECK(continuation_warnings.front().find("  +: ") != std::string::npos);
    CHECK(continued == abc::read(kStrictHeader + "C D").value);

    const auto [empty, empty_warnings] = abc::read(kStrictHeader + "C D\ns:\nE F\ns: !p!");
    CHECK(empty_warnings.empty());
    CHECK(empty == abc::read(kStrictHeader + "C D\n!p!E F").value);

    const auto [excess, surplus] = abc::read(kStrictHeader + "C\ns: !p! !f!");
    REQUIRE(surplus.size() == 1);
    CHECK(surplus.front().find("[extra-symbols]") != std::string::npos);
    CHECK(excess == abc::read(kStrictHeader + "!p!C").value);
}

TEST_CASE("test_malformed_two_bar_overlay", "[abc][diagnostics]") {
    for (const auto& [marked, plain, column] : {
             std::tuple{"C4 | && D4 | E4 |", "C4 | D4 | E4 |", 6},
             {"C4 | D4 | && E4 |", "C4 | D4 | E4 |", 11},
             {"C4 | D4 | E && F4 | G4 |", "C4 | D4 | E F4 | G4 |", 13},
             {"C4 | D4 | && E4 :| F4 |", "C4 | D4 | E4 :| F4 |", 11},
             {"C4 || D4 | && E4 | F4 |", "C4 || D4 | E4 | F4 |", 12},
             {"C4 | D4 | && E4 || F4 |", "C4 | D4 | E4 || F4 |", 11},
         }) {
        CAPTURE(marked);
        const auto [score, warnings] = abc::read(kStrictHeader + marked, "overlay.abc");
        REQUIRE(warnings.size() == 1);
        CHECK(warnings.front().starts_with(
            "overlay.abc:7:" + std::to_string(column) + ": ABC [invalid-two-bar-overlay]"
        ));
        CHECK(score == abc::read(kStrictHeader + plain).value);
    }
}

TEST_CASE("test_part_order_limits", "[abc][diagnostics]") {
    const std::string prefix = "X:1\nM:4/4\nL:1/4\nP:";
    const std::string parts  = "\nK:C\nP:A\nC\nP:B\nD";
    rejects(prefix + "(AB" + parts, "4:1: ABC [invalid-parts]");
    rejects(prefix + "A)" + parts, "4:1: ABC [invalid-parts]");
    rejects(prefix + "A0" + parts, "4:1: ABC [part-count-range]");
    rejects(prefix + "(A65536)65536" + parts, "4:1: ABC [expansion-limit]");
}

TEST_CASE("test_chord_member_limit", "[abc][diagnostics]") {
    const auto chord = "[" + std::string(128, 'C') + "]";
    CHECK(abc::read(kHeader + chord).value.note_num() == 128);
    CHECK(abc::read(kHeader + "{" + chord + "}D").value.note_num() == 129);
    rejects(kHeader + "[" + std::string(129, 'C') + "]", "6:130: ABC [group-limit]");
    rejects(kHeader + "{" + chord + "E}D", "6:132: ABC [group-limit]");
}

TEST_CASE("test_invalid_input_bytes", "[abc][diagnostics]") {
    const auto comment = kHeader + "C % \xc3\xa9";
    for (const char byte : {'\0', '\xff'}) {
        CAPTURE(static_cast<u8>(byte));
        rejects(comment + byte, "6:7: ABC [invalid-text]");
    }
    rejects("", "1:1: ABC [missing-key]");
    rejects("M:4/4\nK:C\nD", "2:1: ABC [missing-reference]");
}

TEST_CASE("test_charset_declarations", "[abc][diagnostics]") {
    const auto [score, warnings] = abc::read(
        "%abc-2.1\nI:abc-charset bogus\nI:abc-charset utf-8\n%%abc-charset utf-8\n"
        "I:abc-charset us-ascii\nX:1\nT:Café\nI:abc-charset us-ascii\nK:C\n"
        "%%abc-charset us-ascii\n[I:abc-charset us-ascii]C |]"
    );
    REQUIRE(warnings.size() == 5);
    CHECK(warnings[0].starts_with("<string>:2:1: ABC [invalid-charset]"));
    CHECK(warnings[1].starts_with("<string>:5:1: ABC [changed-charset]"));
    CHECK(warnings[2].starts_with("<string>:8:1: ABC [header-only-instruction]"));
    CHECK(warnings[3].starts_with("<string>:10:1: ABC [header-only-instruction]"));
    CHECK(warnings[4].starts_with("<string>:11:2: ABC [header-only-instruction]"));
    REQUIRE(score.tracks->size() == 1);
    CHECK(score.tracks->front()->name == "Café");

    const auto unsupported = abc::read("%abc-2.1\nI:abc-charset iso-8859-10\nX:1\nT:ASCII\nK:C\nC");
    REQUIRE(unsupported.warnings.size() == 1);
    CHECK(unsupported.warnings.front().starts_with("<string>:2:1: ABC [unsupported-charset]"));
    CHECK(unsupported.value.note_num() == 1);
}

TEST_CASE("test_charset_instruction_forms", "[abc][diagnostics]") {
    for (const std::string_view command :
         {"I:abc-charset us-ascii", "%%abc-charset us-ascii", "I:abc-charset us-\n+:ascii"}) {
        CAPTURE(command);
        const auto source = fmt::format("%abc-2.1\n{}\n% \xff\n{}C", command, kHeader);
        CHECK_THROWS_MATCHES(
            abc::read(source),
            std::invalid_argument,
            Catch::Matchers::MessageMatches(
                Catch::Matchers::ContainsSubstring("ABC [invalid-text]: ABC text must be US-ASCII")
            )
        );
    }
}

TEST_CASE("test_first_invalid_byte", "[abc][diagnostics]") {
    const std::string preamble    = "%abc-2.1\nI:abc-charset us-ascii\n% ";
    const auto        nul_first   = preamble + '\0' + "é\n" + kHeader + "C";
    const auto        ascii_first = preamble + "é" + '\0' + '\n' + kHeader + "C";
    rejects(nul_first, "3:3: ABC [invalid-text]: ABC text cannot contain a NUL byte");
    rejects(ascii_first, "3:3: ABC [invalid-text]: ABC text must be US-ASCII");
}

TEST_CASE("test_version_field_scope", "[abc][diagnostics]") {
    const auto [score, warnings] = abc::read(
        kStrictHeader + "I:abc-version 2.0\n%%abc-version 2.0\n[I:abc-version 2.0] C |]"
    );
    REQUIRE(warnings.size() == 3);
    CHECK(warnings[0].starts_with("<string>:7:1: ABC [header-only-instruction]"));
    CHECK(warnings[1].starts_with("<string>:8:1: ABC [header-only-instruction]"));
    CHECK(warnings[2].starts_with("<string>:9:2: ABC [header-only-instruction]"));
    CHECK(score == abc::read(kStrictHeader + "C |]").value);
}

TEST_CASE("test_invalid_stress_profile", "[abc][diagnostics]") {
    const auto [score, warnings] = abc::read(
        fmt::format(
            "{}{}%%MIDI ptstress 4 112 1 75 1\n%%MIDI ptstress 1 150 1\n"
            "%%MIDI ptstress 2 30 0 60 0\n%%MIDI ptstress missing.txt\n"
            "%%MIDI ptstress 999999999999 112 1\n%%MIDI ptstress 1 112 inf\n"
            "%%MIDI ptstress 1 112 1 trailing\n"
            "%%MIDI stressmodel 3\nCDEF |]",
            kHeader,
            kStressProfile
        )
    );
    REQUIRE(warnings.size() == 8);
    constexpr std::array codes{
        "invalid-stress-profile",
        "invalid-stress-profile",
        "invalid-stress-profile",
        "stress-file",
        "invalid-stress-profile",
        "invalid-stress-profile",
        "invalid-stress-profile",
        "invalid-stress-model"
    };
    auto warning = warnings.begin();
    for (const auto* code : codes)
        CHECK((warning++)->find(fmt::format("[{}]", code)) != std::string::npos);
    CHECK(velocities(score) == vec<i32>{112, 75, 102, 65});
}

TEST_CASE("test_backing_effect_ranges", "[abc][diagnostics]") {
    for (const auto* directive :
         {"control bass 128 50", "portamento chord 64", "pitchbend bass 0 128"}) {
        CAPTURE(directive);
        CHECK_THROWS_MATCHES(
            abc::read(kHeader + "%%MIDI " + directive + "\nC"),
            std::invalid_argument,
            Catch::Matchers::MessageMatches(
                Catch::Matchers::ContainsSubstring("[invalid-midi-value]")
            )
        );
    }
}

TEST_CASE("test_invalid_voice_targets", "[abc][diagnostics]") {
    const auto [score, warnings] = abc::read(
        kStrictHeader
        + "%%MIDI voice instrument=0 mute\n"
          "%%MIDI voice instrument=1 bank=16385\n"
          "%%MIDI voice Missing mute\nC\n"
    );
    REQUIRE(warnings.size() == 3);
    CHECK(warnings.at(0).find("[invalid-midi-voice]") != std::string::npos);
    CHECK(warnings.at(1).find("[invalid-midi-voice]") != std::string::npos);
    CHECK(warnings.at(2).find("[unknown-midi-voice]") != std::string::npos);
    REQUIRE(score.tracks->size() == 1);
    CHECK(score.tracks->front()->program == 0);
    REQUIRE(score.tracks->front()->notes->size() == 1);
    CHECK(score.tracks->front()->notes->front().pitch == 60);
}

TEST_CASE("test_chord_rest_divisor", "[abc][diagnostics]") {
    for (const auto& [music, column] : {
             std::pair{"[Cz/0]D", 4},
             {"[x/0C]D", 3},
             {"[Z/0]D", 3},
             {"{[CX/0]}D", 5},
         }) {
        CAPTURE(music);
        const auto where = "case.abc:6:" + std::to_string(column) + ": ABC [zero-length-divisor]";
        CHECK_THROWS_MATCHES(
            abc::read(kHeader + music, "case.abc"),
            std::invalid_argument,
            Catch::Matchers::MessageMatches(Catch::Matchers::StartsWith(where))
        );
    }
}

TEST_CASE("test_member_release_range", "[abc][diagnostics]") {
    CHECK_THROWS_MATCHES(
        abc::read(kStrictHeader + "[CE2147483647] D |]", "case.abc"),
        std::invalid_argument,
        Catch::Matchers::MessageMatches(
            Catch::Matchers::StartsWith("case.abc:7:1: ABC [time-range]")
        )
    );
}

TEST_CASE("test_chord_length_overflow", "[abc][diagnostics]") {
    CHECK_THROWS_MATCHES(
        abc::read(kStrictHeader + "[C65536E]65536 D"),
        std::invalid_argument,
        Catch::Matchers::MessageMatches(Catch::Matchers::ContainsSubstring("[time-range]"))
    );
}

TEST_CASE("test_ratio_field_location", "[abc][diagnostics]") {
    CHECK_THROWS_MATCHES(
        abc::read("X:1\nT:\nM:4/4\nL:1/8\nK:C\nC>[I:MIDI ratio 2 0]D z6|", "ratio.abc"),
        std::invalid_argument,
        Catch::Matchers::MessageMatches(
            Catch::Matchers::ContainsSubstring("ratio.abc:6:4: ABC [invalid-midi-value]")
        )
    );
}

TEST_CASE("test_deferred_unit_location", "[abc][diagnostics]") {
    CHECK_THROWS_MATCHES(
        abc::read("X:1\nT:\nM:4/4\nL:1/8\nK:C\nC>[L:0/8]D|", "unit.abc"),
        std::invalid_argument,
        Catch::Matchers::MessageMatches(
            Catch::Matchers::ContainsSubstring("unit.abc:6:4: ABC [invalid-unit-length]")
        )
    );
}

TEST_CASE("test_invalid_single_note_tuning", "[abc][diagnostics]") {
    const auto abc
        = kHeader
          + "[I:MIDI snt 128 61] [I:MIDI snt 60 127.99993896484375] [I:MIDI snt 60 NaN]\n"
            "[I:MIDI snt 60] [I:MIDI snt 60 60x] [I:MIDI snt 60 59.25 extra] C |]";
    const auto [score, warnings] = abc::read(abc, "tuning.abc");
    CHECK(score.note_num() == 1);
    REQUIRE(warnings.size() == 6);
    CHECK_THAT(warnings.front(), Catch::Matchers::StartsWith("tuning.abc:6:2: ABC [invalid-midi]"));
}

}   // namespace symusic::abc_test

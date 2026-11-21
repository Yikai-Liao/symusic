#include <fmt/format.h>

#include "abc_test.h"

namespace symusic::abc_test {

TEST_CASE("test_composite_bar_skip", "[abc][underlay]") {
    // Sections 4.8 and 5.1: a continuous compound bar is one skip boundary.
    for (const std::string_view bar : {"|[|", "|[|]", "|||", "|]|", ".|[|"}) {
        CAPTURE(bar);
        const auto [score, warnings]
            = abc::read(kStrictHeader + fmt::format("C4{}D4|E4|]\nw: one | | three", bar));
        CHECK(warnings.empty());
        CHECK(sung(score) == Lyrics{{1, "one"}, {3841, "three"}});
    }
}

TEST_CASE("test_skipped_ending_lyrics", "[abc][underlay]") {
    const auto [score, warnings] = abc::read(
        kHeader
        + "|: CDEF |\nw: one two three four\n[1 GABc :|\nw: five six seven eight\n"
          "[2 cBAG |]\nw: nine ten eleven twelve\n"
    );
    CHECK(warnings.empty());
    REQUIRE(score.tracks->size() == 1);
    const auto& lyrics = *score.tracks->front()->lyrics;
    REQUIRE(lyrics.size() == 12);
    CHECK(lyrics.front().text == "one");
    CHECK(lyrics[4].text == "five");
    CHECK(lyrics[8].text == "nine");
    CHECK(lyrics[8].time == 5761);
    CHECK(lyrics.back().text == "twelve");
}

TEST_CASE("test_symbol_line_equivalence", "[abc][underlay]") {
    // ABC 2.1 section 4.15: bar skips, blank slots and stacked lines.
    const auto compare = [](const char* lines, const char* inlined) {
        const auto [actual, warnings]          = abc::read(kStrictHeader + lines);
        const auto [expected, inline_warnings] = abc::read(kStrictHeader + inlined);
        CHECK(warnings.empty());
        CHECK(inline_warnings.empty());
        CHECK(actual == expected);
        return actual;
    };
    const auto dynamics = compare(
        "CDEF | G```AB`c\ns: \"^slow\" | !f! ** !fff!", "\"^slow\"CDEF | !f!G```AB`!fff!c"
    );
    REQUIRE(dynamics.tracks->front()->notes->size() == 8);
    const auto unmarked = abc::read(kStrictHeader + "CDEF | G```AB`c").value;
    CHECK(
        dynamics.tracks->front()->notes->back().velocity
        > unmarked.tracks->front()->notes->back().velocity
    );

    const auto backing = compare(
        "C2 C2 Ez A2|\ns: \"C\" * \"Am\" * |\ns: * * !>! * |", "\"C\" C2 C2 \"Am\" !>! Ez A2|"
    );
    REQUIRE(backing.tracks->size() > 1);
    CHECK_FALSE(backing.tracks->back()->notes->empty());
}

TEST_CASE("test_symbol_line_written_settings", "[abc][underlay]") {
    const auto actual   = abc::read(kStrictHeader + "C [I:MIDI deltaloudness 30] D\ns: !<(! !<(!");
    const auto expected = abc::read(kStrictHeader + "!<(!C [I:MIDI deltaloudness 30] !<(!D");
    CHECK(actual.warnings.empty());
    CHECK(expected.warnings.empty());
    CHECK(actual.value == expected.value);
    CHECK(velocities(actual.value) == vec<i32>{105, 125});
    const auto macro = abc::read(
        kStrictHeader
        + "m: ~C2 = C [I:MIDI deltaloudness 30] D\n"
          "~C2\ns: !<(! !<(!"
    );
    CHECK(macro.warnings.empty());
    CHECK(macro.value == expected.value);
}

TEST_CASE("test_postponed_symbols", "[abc][underlay]") {
    // Ties have two slots; a chord has one. A rest-only bar still counts for '|'.
    const auto [actual, warnings] = abc::read(
        kStrictHeader + "C- C {g}[EA] z y x D | z4 |\nE F |]\ns: * !p! !ff! * | | !f! *"
    );
    const auto [expected, inline_warnings]
        = abc::read(kStrictHeader + "C- !p!C {g}!ff![EA] z y x D | z4 |\n!f!E F |]");
    CHECK(warnings.empty());
    CHECK(inline_warnings.empty());
    CHECK(actual == expected);
}

TEST_CASE("test_stacked_symbols", "[abc][underlay]") {
    const auto [actual, warnings]
        = abc::read(kStrictHeader + "C D |\ns: !ped! !ped-up!\ns: !p! *\ns: !ff! *");
    const auto [expected, inline_warnings] = abc::read(kStrictHeader + "!ped!!p!!ff!C !ped-up!D |");
    CHECK(warnings.empty());
    CHECK(inline_warnings.empty());
    CHECK(actual == expected);
    CHECK_FALSE(actual.tracks->front()->pedals->empty());
}

TEST_CASE("test_macro_symbol_rows", "[abc][underlay]") {
    const auto [score, warnings]
        = abc::read(kStrictHeader + "C D |\ns: !p! *\nm: ~C2=CD\ns: !ff! *");
    REQUIRE(warnings.size() == 1);
    CHECK(warnings.front().starts_with("<string>:10:1: ABC [extra-symbols]"));
    CHECK(score == abc::read(kStrictHeader + "!p!C D |").value);
}

TEST_CASE("test_opening_repeat_skip", "[abc][underlay]") {
    for (const std::string_view first : {"C4", "z4"}) {
        CAPTURE(first);
        const auto opening                     = kStrictHeader + "|: " + std::string(first) + " | ";
        const auto [actual, warnings]          = abc::read(opening + "E4 :|\ns: | !ff!\nw: | next");
        const auto [expected, inline_warnings] = abc::read(opening + "!ff!E4 :|\nw: | next");
        CHECK(warnings.empty());
        CHECK(inline_warnings.empty());
        CHECK(actual == expected);
        CHECK(sung(actual) == Lyrics{{1921, "next"}});
    }
}

TEST_CASE("test_repeated_voice_symbols", "[abc][underlay]") {
    const auto [actual, warnings] = abc::read(
        kStrictHeader
        + "V:upper\n|: C D & G A :|\nw: one two three four\ns: !p! *\n+: !f! *\n"
          "V:lower\nE F |\ns: * !ff!\nV:upper\nB c |\ns: !pp! *"
    );
    const auto [expected, inline_warnings] = abc::read(
        kStrictHeader
        + "V:upper\n|: !p!C D & !f!G A :|\nw: one two three four\n"
          "V:lower\nE !ff!F |\nV:upper\n!pp!B c |"
    );
    CHECK(warnings.empty());
    CHECK(inline_warnings.empty());
    CHECK(actual == expected);
}

TEST_CASE("test_postponed_lyrics", "[abc][underlay]") {
    const auto immediate
        = abc::read(kStrictHeader + "C D E F|\nw: doh re mi fa\nG A B c|\nw: sol la ti doh");
    const auto postponed
        = abc::read(kStrictHeader + "C D E F|\nG A B c|\nw: doh re mi fa\n+: sol la ti doh");
    CHECK(immediate.warnings.empty());
    CHECK(postponed.warnings.empty());
    CHECK(
        sung(postponed.value)
        == Lyrics{
            {1, "doh"},
            {481, "re"},
            {961, "mi"},
            {1441, "fa"},
            {1921, "sol"},
            {2401, "la"},
            {2881, "ti"},
            {3361, "doh"},
        }
    );
    CHECK(sung(immediate.value) == sung(postponed.value));
}

TEST_CASE("test_lyric_version_overrides", "[abc][underlay]") {
    const std::string music = "C D |\nE F |\nw: one two\n";
    // Section 5.1 starts the first w: field at the first written note of the voice.
    for (const auto& opening :
         {"%abc-2.0\nI:abc-version 2.1\n" + kHeader,
          std::string("%abc-2.1\nX:1\nT:Lyrics\nI:abc-version 2.0\nL:1/4\nK:C\n"),
          "%abc-2.1% comment\n" + kHeader}) {
        CAPTURE(opening);
        const auto [score, warnings] = abc::read(opening + music);
        CHECK(warnings.empty());
        CHECK(sung(score) == Lyrics{{1, "one"}, {481, "two"}});
    }
    const auto malformed = abc::read("%abc-2.x\nI:abc-version 2.x\n" + kHeader + music);
    REQUIRE(malformed.warnings.size() == 2);
    CHECK(malformed.warnings[0].find("1:6: ABC [invalid-version]") != std::string::npos);
    CHECK(malformed.warnings[1].find("2:1: ABC [invalid-version]") != std::string::npos);
    CHECK(sung(malformed.value) == Lyrics{{1, "one"}, {481, "two"}});
}

TEST_CASE("test_blank_verse", "[abc][underlay]") {
    const auto [score, warnings] = abc::read(kStrictHeader + "C D |\nw:\nE F |\nw: mi");
    CHECK(warnings.empty());
    CHECK(sung(score) == Lyrics{{961, "mi"}});
}

TEST_CASE("test_lyric_skips", "[abc][underlay]") {
    const auto [score, warnings] = abc::read(
        kStrictHeader + "CD EF | GA Bc | d4 |]\nw: syl-la--ble _ * of~the~day a\\-b | last"
    );
    CHECK(warnings.empty());
    CHECK(
        sung(score)
        == Lyrics{
            {1, "syl"},
            {481, "la"},
            {1441, "ble"},
            {2881, "of the day"},
            {3361, "a-b"},
            {3841, "last"},
        }
    );
    const auto skipped = abc::read(kStrictHeader + "CDEF | GABc |\nw: one | two");
    CHECK(sung(skipped.value) == Lyrics{{1, "one"}, {1921, "two"}});
    const auto rest_bar = abc::read(kStrictHeader + "C4 | z4 | D4 |]\nw: one | | two");
    CHECK(rest_bar.warnings.empty());
    CHECK(sung(rest_bar.value) == Lyrics{{1, "one"}, {3841, "two"}});
    const auto excess = abc::read(kStrictHeader + "C\nw: one two");
    REQUIRE(excess.warnings.size() == 1);
    CHECK(excess.warnings.front().find("[extra-lyrics]") != std::string::npos);
    CHECK(sung(excess.value) == Lyrics{{1, "one"}});
}

TEST_CASE("test_held_syllable_punctuation", "[abc][underlay]") {
    // The Canzonetta example in ABC 2.1 section 13.4 ends a line with o_.
    const auto ending = abc::read(kStrictHeader + "C D |]\nw: o_.");
    CHECK(ending.warnings.empty());
    CHECK(sung(ending.value) == Lyrics{{1, "o."}});
    CHECK(sung(abc::read(abc::write(ending.value).value).value) == sung(ending.value));

    const auto continued = abc::read(kStrictHeader + "C D E F |]\nw: o-__?! next");
    CHECK(continued.warnings.empty());
    CHECK(sung(continued.value) == Lyrics{{1, "o?!"}, {1441, "next"}});

    const auto reset = abc::read(kStrictHeader + "C D E F |]\nw: o_.$0 next");
    CHECK(reset.warnings.empty());
    CHECK(sung(reset.value) == Lyrics{{1, "o."}, {961, "next"}});

    for (const auto text : {"o-_$1_$0?$2!$0 next", "o-$1__$0?! next"}) {
        CAPTURE(text);
        const auto styled = abc::read(kStrictHeader + "C D E F |]\nw: " + text);
        CHECK(styled.warnings.empty());
        CHECK(sung(styled.value) == Lyrics{{1, "o?!"}, {1441, "next"}});
    }

    const auto separated = abc::read(kStrictHeader + "C D E F |]\nw: o_ . next");
    CHECK(separated.warnings.empty());
    CHECK(sung(separated.value) == Lyrics{{1, "o"}, {961, "."}, {1441, "next"}});

    const auto excess = abc::read(kStrictHeader + "C D |]\nw: one * tail_!");
    REQUIRE(excess.warnings.size() == 1);
    CHECK(excess.warnings.front().find("[extra-lyrics]") != std::string::npos);
    CHECK(sung(excess.value) == Lyrics{{1, "one"}});
}

TEST_CASE("test_lyric_text_escapes", "[abc][underlay]") {
    // Sections 5.1 and 8.2 distinguish lyric alignment marks from characters encoded in a syllable.
    const std::string music = R"(C D E F | G A B c |])"
                              "\n"
                              R"(w: \~no &eacute;-\u002a a\-b)"
                              "\n"
                              R"(+: \U0000007c of~the~day \&eacute; 50\%)";
    for (const auto& version : kVersionLines) {
        CAPTURE(version);
        const auto [score, warnings] = abc::read(version + kHeader + music);
        CHECK(warnings.empty());
        CHECK(
            sung(score)
            == Lyrics{
                {1, "\xc3\xb1o"},
                {481, "\xc3\xa9"},
                {961, "*"},
                {1441, "a-b"},
                {1921, "|"},
                {2401, "of the day"},
                {2881, "&eacute;"},
                {3361, "50%"},
            }
        );
    }
}

TEST_CASE("test_stanza_numbers", "[abc][underlay]") {
    // Section 5.3 aligns the first note with the text after the stanza number.
    for (const std::string_view number :
         {"1.~",
          "1.~(",
          "2.3)",
          "3. ",
          "04 ",
          "$2"
          "5.-~",
          "1\\-~",
          "1/2 ",
          "1$2.~"}) {
        CAPTURE(number);
        const auto [score, warnings]
            = abc::read(kStrictHeader + fmt::format("C D E F |]\nw: {}Three blind mice", number));
        CHECK(warnings.empty());
        CHECK(sung(score) == Lyrics{{1, "Three"}, {481, "blind"}, {961, "mice"}});
    }
    const auto [continued, warnings]
        = abc::read(kStrictHeader + "C D E F |]\nw: 1.* two\n+: 3rd 4th");
    CHECK(warnings.empty());
    CHECK(sung(continued) == Lyrics{{481, "two"}, {961, "3rd"}, {1441, "4th"}});

    const auto blank = abc::read(kStrictHeader + "C D E F |]\nw: 1. -two");
    CHECK(blank.warnings.empty());
    CHECK(sung(blank.value) == Lyrics{{481, "two"}});

    for (const std::string_view text : {"1\\-~\u00e9tait", "1.~&eacute;tait", "1.~\\u00e9tait"}) {
        CAPTURE(text);
        const auto [score, numbered_warnings]
            = abc::read(kStrictHeader + fmt::format("C D |]\nw: {}", text));
        CHECK(numbered_warnings.empty());
        CHECK(sung(score) == Lyrics{{1, "\u00e9tait"}});
    }

    const auto escaped = abc::read(kStrictHeader + "C D E F |]\nw: \\u0031st second");
    CHECK(escaped.warnings.empty());
    CHECK(sung(escaped.value) == Lyrics{{1, "1st"}, {481, "second"}});
}

TEST_CASE("test_undefined_lyric_escape", "[abc][underlay]") {
    for (const auto* opening : {&kHeader, &kStrictHeader}) {
        const auto [score, warnings] = abc::read(*opening + R"(C D E F |]
w: a\*b c)");
        CHECK(warnings.empty());
        CHECK(sung(score) == Lyrics{{1, "a\\"}, {961, "b"}, {1441, "c"}});
    }
}

TEST_CASE("test_written_lyric_slots", "[abc][underlay]") {
    const auto [score, warnings]
        = abc::read(kStrictHeader + "C- C {g}D z y x E |]\nw: first held host final");
    CHECK(warnings.empty());
    const auto& notes = *score.tracks->front()->notes;
    REQUIRE(notes.size() == 4);
    REQUIRE(notes[2].pitch == 62);
    CHECK(
        sung(score)
        == Lyrics{
            {notes[0].time, "first"},
            {481, "held"},
            {notes[2].time, "host"},
            {notes[3].time, "final"},
        }
    );
}

TEST_CASE("test_postponed_voice_lyrics", "[abc][underlay]") {
    const auto [score, warnings] = abc::read(
        kStrictHeader
        + "[V:upper] C D & G A | E F |\n[V:lower] A B |\nw: alpha beta\n"
          "V:upper\nw: one two three four | five six"
    );
    CHECK(warnings.empty());
    REQUIRE(score.tracks->size() == 3);
    CHECK(sung(score) == Lyrics{{1, "one"}, {481, "two"}, {961, "five"}, {1441, "six"}});
    const auto& overlay = *score.tracks->at(1)->lyrics;
    REQUIRE(overlay.size() == 2);
    CHECK(overlay[0].text == "three");
    CHECK(overlay[1].text == "four");
    const auto& lower = *score.tracks->at(2)->lyrics;
    REQUIRE(lower.size() == 2);
    CHECK(lower[0].text == "alpha");
    CHECK(lower[1].text == "beta");
}

TEST_CASE("test_two_bar_overlay_lyrics", "[abc][underlay]") {
    const auto [score, warnings] = abc::read(
        "%abc-2.1\nX:1\nT:Overlay lyrics\nM:4/4\nL:1/8\nK:C\n"
        "g4 f4 | e6 e2 |\n"
        "&& (d8 | c6) c2|\n"
        "w: ha-la-| lu-yoh\n"
        "+: lu-   |    -yoh"
    );
    REQUIRE(warnings.empty());
    REQUIRE(score.tracks->size() == 2);
    const auto& primary = *score.tracks->front();
    const auto& side    = *score.tracks->back();
    CHECK(onsets(primary) == Onsets{{1, 79}, {961, 77}, {1921, 76}, {3361, 76}});
    CHECK(onsets(side) == Onsets{{1, 74}, {1921, 72}, {3361, 72}});
    CHECK(sung(score) == Lyrics{{1, "ha"}, {961, "la"}, {1921, "lu"}, {3361, "yoh"}});
    REQUIRE(side.lyrics->size() == 2);
    CHECK(side.lyrics->front() == TextMeta<Tick>{1, "lu"});
    CHECK(side.lyrics->back() == TextMeta<Tick>{3361, "yoh"});
}

TEST_CASE("test_repeated_part_verses", "[abc][underlay]") {
    const auto [score, warnings] = abc::read(
        "%abc-2.1\nX:1\nT:Lyrics\nL:1/4\nP:A2\nK:C\nP:A\nC |\nD |]\n"
        "w: verse one\nw: second *"
    );
    CHECK(warnings.empty());
    CHECK(sung(score) == Lyrics{{1, "verse"}, {481, "one"}, {961, "second"}});
}

TEST_CASE("test_repeat_line_lyrics", "[abc][underlay]") {
    SECTION("Notes first visited after the repeat keep their own syllables") {
        const auto [score, warnings]
            = abc::read(kStrictHeader + "|: C D E F :| G A |]\nw: one two three four five six");
        CHECK(warnings.empty());
        const auto notes = melody(score);
        REQUIRE(notes.size() == 10);
        CHECK(
            sung(score)
            == Lyrics{
                {1, "one"},
                {481, "two"},
                {961, "three"},
                {1441, "four"},
                {3841, "five"},
                {4321, "six"}
            }
        );
    }

    SECTION("A new music line restarts lyric playback inside the repeated passage") {
        const auto [score, warnings]
            = abc::read(kStrictHeader + "|: C2 D2 |\nw: one two\nE2 F2 :|\nw: three four");
        CHECK(warnings.empty());
        CHECK(
            sung(score)
            == Lyrics{
                {1, "one"},
                {961, "two"},
                {1921, "three"},
                {2881, "four"},
                {5761, "three"},
                {6721, "four"}
            }
        );
    }

    SECTION("A fresh part visit selects the next written verse") {
        const auto [score, warnings] = abc::read(
            "%abc-2.1\nX:1\nT:Lyrics\nM:4/4\nL:1/4\nP:A2\nK:C\nP:A\n"
            "|: C D E F :|\nw: one two three four\nw: five six seven eight"
        );
        CHECK(warnings.empty());
        CHECK(
            sung(score)
            == Lyrics{
                {1, "one"},
                {481, "two"},
                {961, "three"},
                {1441, "four"},
                {3841, "five"},
                {4321, "six"},
                {4801, "seven"},
                {5281, "eight"}
            }
        );
    }
}

TEST_CASE("test_macro_lyric_slots", "[abc][underlay]") {
    // Sections 5.1 and 9.1: replacements share a source location, not a note slot.
    const std::string words = "\nw: one two three four";
    const auto [score, warnings]
        = abc::read(kStrictHeader + "m: ~C2=CD\n~C2 ~C2 |]\ns: * !p! * !f!" + words);
    CHECK(warnings.empty());
    CHECK(sung(score) == Lyrics{{1, "one"}, {481, "two"}, {961, "three"}, {1441, "four"}});
    CHECK(score == abc::read(kStrictHeader + "C!p!D C!f!D |]" + words).value);
}

TEST_CASE("test_separated_verses", "[abc][underlay]") {
    for (const auto field : {"I:propagate-accidentals not", "U:H=!trill!", "m: ~C2=~G2"}) {
        CAPTURE(field);
        const auto [score, warnings] = abc::read(
            fmt::format(
                "%abc-2.1\nX:1\nT:Lyrics\nL:1/4\nP:A2\nK:C\nP:A\nC |]\n"
                "w: first\n{}\nw: second",
                field
            )
        );
        REQUIRE(warnings.size() == 1);
        CHECK(warnings.front().find("[extra-lyrics]") != std::string::npos);
        CHECK(sung(score) == Lyrics{{1, "first"}, {481, "first"}});
    }
}

TEST_CASE("test_continued_lyric_formatting", "[abc][underlay]") {
    for (const auto directive : {
             "%%vocalfont Times-Italic *",
             "%%staffsep 2cm",
             "I:begintext\n%% Printed text\nI:endtext",
             "%%begintext\n%% Printed text\nI:endtext",
         }) {
        CAPTURE(directive);
        const auto [score, warnings]
            = abc::read(kStrictHeader + fmt::format("C D |]\nw:one\n{}\n+:two", directive));
        CHECK(warnings.empty());
        CHECK(sung(score) == Lyrics{{1, "one"}, {481, "two"}});
    }
}

TEST_CASE("test_tied_roll_lyrics", "[abc][underlay]") {
    // Section 5.1 gives each written note of the tie a syllable, irrespective of the ornament
    // pulses.
    for (const auto& version : kVersionLines) {
        CAPTURE(version);
        const auto [score, warnings]
            = abc::read(version + kHeader + "RD- D | A2 |]\nw: one two | last");
        REQUIRE(warnings.empty());
        REQUIRE(score.tracks->size() == 1);
        CHECK(
            *score.tracks->front()->lyrics
            == pyvec<TextMeta<Tick>>{{1, "one"}, {481, "two"}, {961, "last"}}
        );
    }
}

TEST_CASE("test_repeated_overlay_roll_lyrics", "[abc][underlay]") {
    const auto [score, warnings] = abc::read(
        kStrictHeader
        + "%%MIDI drummap C 36\n"
          "|: [I:MIDI channel 10] C4 & [I:MIDI channel 1] RD2- D2 :|\n"
          "w: beat roll held"
    );
    REQUIRE(warnings.empty());
    REQUIRE(score.tracks->size() == 2);
    const auto& drums   = *score.tracks->front();
    const auto& overlay = *score.tracks->back();
    CHECK(drums.is_drum);
    CHECK_FALSE(overlay.is_drum);
    CHECK(note_spans(drums) == Notes{{1, 1919, 36}, {1921, 1919, 36}});

    Notes roll;
    for (i32 onset : {1, 1921}) {
        for (i32 pitch : {62, 64, 62, 60, 62}) {
            roll.emplace_back(onset, 383, pitch);
            onset += 384;
        }
    }
    CHECK(note_spans(overlay) == roll);
    // The continuation takes a syllable halfway through the roll. A same-line repeat skips sung
    // slots.
    CHECK(sung(score) == Lyrics{{1, "beat"}});
    CHECK(*overlay.lyrics == pyvec<TextMeta<Tick>>{{1, "roll"}, {961, "held"}});
}

TEST_CASE("test_roll_lyric_slot", "[abc][underlay]") {
    const auto [score, warnings] = abc::read(
        "%abc-2.1\nX:1\nT:\nM:4/4\nL:1/8\nK:C\nC !roll!D3 E2 F2 |]\nw: one two three four"
    );
    REQUIRE(warnings.empty());
    const auto& track = *score.tracks->front();
    REQUIRE(track.notes->size() == 8);
    REQUIRE(track.lyrics->size() == 4);
    CHECK(track.lyrics->at(1) == TextMeta<Tick>{241, "two"});
    CHECK(track.lyrics->at(2) == TextMeta<Tick>{961, "three"});
}

}   // namespace symusic::abc_test

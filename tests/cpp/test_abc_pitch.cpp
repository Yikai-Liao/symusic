#include <algorithm>
#include <array>

#include <fmt/format.h>

#include "abc_test.h"

namespace symusic::abc_test {

TEST_CASE("test_key_voice_options", "[abc][pitch]") {
    struct Case {
        std::string_view options;
        i32              pitch;
    };
    constexpr std::array cases{
        Case{"bass3+8 m=d octave=1 t=-2", 82},
        Case{"octave=-1 clef=treble-8 middle=B transpose=2", 38},
        Case{"octave=1 clef=alto-8 t=+1", 61},
        Case{"octave=1 clef=bass-8 octave=0", 48},
        Case{"clef=unknown+8 ignored-8 middle=c stafflines=1", 60},
        Case{"Bass Middle=d Transpose=-2 Octave=1", 70},
    };
    for (const auto& [options, pitch] : cases) {
        for (const std::string_view field : {"K:C ", "K:C\nV:lead "}) {
            CAPTURE(field, options);
            std::string input = "X:1\nM:4/4\nL:1/4\n";
            input.append(field).append(options).append("\nC|");
            const auto [score, warnings] = abc::read(input);
            REQUIRE(warnings.empty());
            REQUIRE(score.note_num() == 1);
            CHECK(score.tracks->front()->notes->front().pitch == pitch);
        }
    }
}

TEST_CASE("test_clef_octave_independence", "[abc][pitch]") {
    for (const auto key : {std::string_view("K:"), std::string_view("V:lead ")}) {
        CAPTURE(key);
        const auto change = [key](std::string_view settings) {
            return "[" + std::string(key) + std::string(settings) + "] ";
        };
        const auto [score, warnings] = abc::read(
            kStrictHeader + change("clef=treble+8 octave=1 transpose=-2") + "C "
            + change("stafflines=1 clef=unknown+8") + "C " + change("clef=treble") + "D "
            + change("clef=bass-8") + "E " + change("octave=0") + "F |]"
        );
        REQUIRE(warnings.empty());
        CHECK(pitches(score) == vec{82, 82, 72, 62, 51});
    }
}

TEST_CASE("test_pitch_options_rhythm", "[abc][pitch]") {
    const std::string opening = "%abc-2.1\nX:1\nT:Voice rhythm\nM:4/4\nL:1/8\n"
                                "V:lead octave=1\nV:side transpose=-2\nK:C\n";
    struct Case {
        std::string_view body;
        Notes            prefix;
    };
    for (const auto& [body, prefix] : {
             Case{"[V:lead] C>[V:lead]DEF GABC | [V:side] C8|]", {{1, 359, 72}, {361, 119, 74}}},
             Case{"[V:lead] C>[V:side] C8 | [V:lead] DEF GABC |]", {{1, 359, 72}, {361, 119, 74}}},
             Case{
                 "[V:lead] C>[V:lead octave=0]DEF GABC | [V:side] C8|]",
                 {{1, 359, 72}, {361, 119, 62}}
             },
             Case{
                 "[V:lead] {c}[V:lead octave=0] CDEF GABC | [V:side] C8|]",
                 {{1, 59, 84}, {61, 179, 60}}
             },
         }) {
        CAPTURE(body);
        const auto [score, warnings] = abc::read(opening + std::string(body));
        REQUIRE(warnings.empty());
        REQUIRE(score.tracks->size() == 2);
        const auto lead = note_spans(*score.tracks->front());
        REQUIRE(lead.size() >= prefix.size());
        CHECK(std::equal(prefix.begin(), prefix.end(), lead.begin()));
        CHECK(note_spans(*score.tracks->back()) == Notes{{1, 1919, 58}});
    }
}

TEST_CASE("test_transpose_voice_scope", "[abc][pitch]") {
    const auto [score, warnings] = abc::read(
        "%abc-2.1\nX:1\nT:Voice transposition\nM:4/4\nL:1/4\n"
        "V:lead transpose=-2\nV:side transpose=12\nK:G transpose=5\n"
        "[V:lead] F [K:transpose=1] F [V:side] F F [V:lead] F F |] [V:side] F F |]"
    );
    REQUIRE(warnings.empty());
    REQUIRE(score.tracks->size() == 2);
    CHECK(pitches(*score.tracks->front()) == vec{64, 67, 67, 67});
    CHECK(pitches(*score.tracks->back()) == vec{71, 71, 71, 71});
    CHECK(*score.key_signatures == pyvec<KeySignature<Tick>>{{0, 1, 0}});
}

TEST_CASE("test_closing_key_voice_scope", "[abc][pitch]") {
    for (const auto& [options, side] : {
             std::pair{"octave=0 clef=treble", 66},
             {"clef=treble", 78},
             {"octave=-1", 66},
         }) {
        CAPTURE(options);
        const auto [score, warnings] = abc::read(
            fmt::format(
                "%abc-2.1\nX:1\nT:Voice octaves\nM:4/4\nL:1/4\n"
                "V:lead clef=bass-8\nV:side clef=treble+8 octave=1\nK:G {}\n"
                "[V:lead] F |]\n[V:side] F |]",
                options
            )
        );
        REQUIRE(warnings.empty());
        REQUIRE(score.tracks->size() == 2);
        CHECK(score.tracks->front()->notes->front().pitch == 54);
        CHECK(score.tracks->back()->notes->front().pitch == side);
        CHECK(*score.key_signatures == pyvec<KeySignature<Tick>>{{0, 1, 0}});
    }
}

TEST_CASE("test_inline_transpose_replacement", "[abc][pitch]") {
    const auto [score, warnings] = abc::read(
        "%abc-2.1\nX:1\nT:Transposition changes\nM:4/4\nL:1/4\n"
        "K:G transpose=5\nF [K:transpose=-2] F [K:transpose=0] F G |]"
    );
    REQUIRE(warnings.empty());
    CHECK(pitches(score) == vec{71, 64, 66, 67});
}

TEST_CASE("test_key_without_signature", "[abc][pitch]") {
    for (const std::string_view field : {"", "none", "clef=bass m=d", "bass3", "m=d t=-2"}) {
        CAPTURE(field);
        std::string input = "X:1\nM:4/4\nL:1/4\nK:";
        input.append(field).append("\nF|");
        const auto [score, warnings] = abc::read(input);
        REQUIRE(warnings.empty());
        REQUIRE(score.note_num() == 1);
        CHECK(score.tracks->front()->notes->front().pitch == (field == "m=d t=-2" ? 63 : 65));
        CHECK(score.key_signatures->front() == KeySignature<Tick>{0, 0, 0});
    }
}

TEST_CASE("test_inline_key_reset", "[abc][pitch]") {
    const auto [score, warnings] = abc::read(
        "%abc-2.1\nX:1\nT:Key changes\nM:4/4\nL:1/4\nK:Dm\nB [K:] B | [K:D] F [K:none] F |]"
    );
    REQUIRE(warnings.empty());
    const auto& notes = *score.tracks->front()->notes;
    REQUIRE(notes.size() == 4);
    CHECK(notes[0].pitch == 70);
    CHECK(notes[1].pitch == 71);
    CHECK(notes[2].pitch == 66);
    CHECK(notes[3].pitch == 65);
}

TEST_CASE("test_key_options_accidentals", "[abc][pitch]") {
    const auto [score, warnings] = abc::read(
        "%abc-2.1\nX:1\nT:Clef changes\nM:4/4\nL:1/4\nK:C\n"
        "^F [K:clef=bass] F [K:middle=B] F [K:transpose=2] F | F "
        "[K:transpose=0] ^F [K:C] F [K:C clef=treble] F |]"
    );
    REQUIRE(warnings.empty());
    CHECK(pitches(score) == vec<i32>{66, 66, 66, 68, 67, 66, 65, 65});
    const auto& signatures = *score.key_signatures;
    REQUIRE(signatures.size() == 3);
    CHECK(signatures[0] == KeySignature<Tick>{0, 0, 0});
    CHECK(signatures[1] == KeySignature<Tick>{2880, 0, 0});
    CHECK(signatures[2] == KeySignature<Tick>{3360, 0, 0});
}

TEST_CASE("test_partial_key_options", "[abc][pitch]") {
    const auto [score, warnings] = abc::read(
        "X:1\nM:4/4\nL:1/4\nK:Dm\nB [K:bass3 =B t=-2] B [K:middle=d] B | "
        "[K:] B [K:D t=0] F [K:none] F|"
    );
    REQUIRE(warnings.empty());
    const auto& notes = *score.tracks->front()->notes;
    REQUIRE(notes.size() == 6);
    CHECK(notes[0].pitch == 70);
    CHECK(notes[1].pitch == 69);
    CHECK(notes[2].pitch == 69);
    CHECK(notes[3].pitch == 69);
    CHECK(notes[4].pitch == 66);
    CHECK(notes[5].pitch == 65);
    const auto& signatures = *score.key_signatures;
    REQUIRE(signatures.size() >= 2);
    CHECK(signatures[1] == KeySignature<Tick>{480, -1, 1});
    CHECK(signatures.back() == KeySignature<Tick>{2400, 0, 0});
}

TEST_CASE("test_bagpipe_keys", "[abc][pitch]") {
    const auto [score, warnings] = abc::read("X:1\nM:4/4\nL:1/4\nK:HP\nCFG | [K:Hp] CFG|");
    REQUIRE(warnings.empty());
    const auto& notes = *score.tracks->front()->notes;
    REQUIRE(notes.size() == 6);
    CHECK(notes[0].pitch == 61);
    CHECK(notes[1].pitch == 66);
    CHECK(notes[2].pitch == 67);
    CHECK(notes[3].pitch == 61);
    CHECK(notes[4].pitch == 66);
    CHECK(notes[5].pitch == 67);
    const auto& signatures = *score.key_signatures;
    REQUIRE(signatures.size() == 2);
    CHECK(signatures[0] == KeySignature<Tick>{0, 0, 0});
    CHECK(signatures[1] == KeySignature<Tick>{1440, 2, 0});
}

TEST_CASE("test_tied_accidental_octave", "[abc][pitch]") {
    // Sections 4.6 and 4.11 require the two segments to have the same pitch.
    for (const std::string_view change : {"octave=1", "treble+8"}) {
        CAPTURE(change);
        const auto [raised, raised_warnings]
            = abc::read(kStrictHeader + fmt::format("C-| [K:{}]C D |]", change));
        CHECK(raised_warnings.empty());
        CHECK(melody(raised) == Notes{{1, 479, 60}, {481, 479, 72}, {961, 479, 74}});

        const auto [same, same_warnings]
            = abc::read(kStrictHeader + fmt::format("^C-| [K:{}]C, D |]", change));
        CHECK(same_warnings.empty());
        CHECK(melody(same) == Notes{{1, 959, 61}, {961, 479, 74}});
    }
}

TEST_CASE("test_effective_accidental_octave", "[abc][pitch]") {
    // Sections 4.6 and 11.3: raising C and writing C, cancels the octave shift.
    for (const std::string_view change : {"octave=1", "treble+8"}) {
        CAPTURE(change);
        const auto [score, warnings] = abc::read(
            kStrictHeader
            + fmt::format("[I:propagate-accidentals octave]^C [K:{}] C C, c |]", change)
        );
        CHECK(warnings.empty());
        CHECK(pitches(score) == vec<i32>{61, 72, 61, 84});
    }
}

TEST_CASE("test_split_overlay_accidentals", "[abc][pitch]") {
    const auto [score, warnings] = abc::read(kStrictHeader + "(& ^F & _F &) F f |]");
    CHECK(warnings.empty());
    REQUIRE(score.tracks->size() == 2);
    const auto& primary = *score.tracks->front()->notes;
    REQUIRE(primary.size() == 3);
    CHECK(primary[0].pitch == 66);
    CHECK(primary[1].pitch == 65);
    CHECK(primary[2].pitch == 77);
    REQUIRE(score.tracks->back()->notes->size() == 1);
    CHECK(score.tracks->back()->notes->front().pitch == 64);
}

TEST_CASE("test_unsplit_overlay_accidentals", "[abc][pitch]") {
    const auto [score, warnings] = abc::read(kStrictHeader + "(& ^F &) F f |]");
    CHECK(warnings.empty());
    CHECK(pitches(score) == vec<i32>{66, 66, 78});
}

TEST_CASE("test_macro_written_accidental_policy", "[abc][pitch]") {
    const std::string header              = "%abc-2.1\nX:1\nT:Macro settings\nM:2/4\nL:1/4\nK:C\n";
    const std::string music               = "^F f [I:propagate-accidentals octave] | ^F f |]";
    const auto [expanded, warnings]       = abc::read(header + "m: ~C2 = " + music + "\n~C2");
    const auto [written, direct_warnings] = abc::read(header + music);
    CHECK(warnings.empty());
    CHECK(direct_warnings.empty());
    CHECK(pitches(expanded) == vec<i32>{66, 78, 66, 77});
    CHECK(expanded == written);

    const std::string same_bar      = "^F f [I:propagate-accidentals octave] ^F f";
    const auto        same_location = abc::read(header + "m: ~C2 = " + same_bar + "\n~C2");
    const auto        inline_notes  = abc::read(header + same_bar);
    CHECK(same_location.warnings.empty());
    CHECK(inline_notes.warnings.empty());
    CHECK(pitches(same_location.value) == vec<i32>{66, 78, 66, 78});
    CHECK(same_location.value == inline_notes.value);
}

}   // namespace symusic::abc_test

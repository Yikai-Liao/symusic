#include <catch2/catch_test_macros.hpp>

#include "symusic/io/abc.h"

using namespace symusic;

TEST_CASE("ABC renders repeat endings and tied accidentals", "[abc]") {
    auto [score, warnings] = abc::read("X:1\nM:4/4\nL:1/4\nK:C\n|: ^F2- | F |1 C :|2 D | E");
    REQUIRE(score.ticks_per_quarter == 480);
    REQUIRE(score.tracks->size() == 1);
    const auto& notes = *score.tracks->front()->notes;
    REQUIRE(notes.size() == 5);
    CHECK(notes[0] == Note<Tick>{1, 1439, 66, 105});
    CHECK(notes[1].pitch == 60);
    CHECK(notes[2].pitch == 66);
    CHECK(notes[2].duration == 1439);
    CHECK(notes[3].pitch == 62);
    CHECK(notes[4].pitch == 64);
}

TEST_CASE("ABC locates a recovered error after a continuation", "[abc]") {
    const auto [score, warnings] = abc::read("X:1\r\nM:4/4\r\nK:C\r\nC\\\r\n  ]D", "tune.abc");
    REQUIRE(score.note_num() == 2);
    REQUIRE(warnings.size() == 1);
    CHECK(warnings.front() == "tune.abc:5:3: ABC [stray-chord-close]: ignored ']' outside a chord");
    CHECK_THROWS_AS(abc::read("X:1\nL:1/4\nCDEF"), std::invalid_argument);
    CHECK_THROWS_AS(abc::read("X:1\nK:C\n[CDE"), std::invalid_argument);
}

TEST_CASE("ABC renders slash chord backing and melody separately", "[abc]") {
    auto [score, warnings] = abc::read("X:1\nM:4/4\nL:1/4\nK:C\n\"C/E\"CDEF|");
    REQUIRE(warnings.empty());
    REQUIRE(score.tracks->size() == 3);
    const auto& bass   = *(*score.tracks)[1]->notes;
    const auto& chords = *(*score.tracks)[2]->notes;
    REQUIRE(bass.size() == 2);
    CHECK(bass[0] == Note<Tick>{1, 239, 40, 80});
    REQUIRE(chords.size() == 6);
    CHECK(chords[0] == Note<Tick>{481, 239, 52, 75});
    CHECK(chords[1].pitch == 55);
    CHECK(chords[2].pitch == 60);
}

TEST_CASE("ABC repeat jumps restore notation without rewinding time", "[abc]") {
    const auto [score, warnings] = abc::read("X:1\nM:4/4\nL:1/4\nK:C\n|: F [K:G][L:1/8] F2 :| F2");
    REQUIRE(warnings.empty());
    const auto& notes = *score.tracks->front()->notes;
    REQUIRE(notes.size() == 5);
    CHECK(notes[2] == Note<Tick>{961, 479, 65, 105});
    CHECK(notes[3].time == 1441);
    CHECK(notes[4].time == 1921);
}

TEST_CASE("ABC validates backing note endpoints", "[abc]") {
    const std::string input = "X:1\nM:4/4\nL:1/1920\nK:C\n%%MIDI gchordoff\n"
                              "z2147483547\n%%MIDI gchord f\n%%MIDI gchordon\n\"C\"C";
    CHECK_THROWS_AS(abc::read(input), std::invalid_argument);
    CHECK(abc::read("X:1\nM:4/4\nL:1/4\nK:C\nz1000001 C").value.note_num() == 1);
}

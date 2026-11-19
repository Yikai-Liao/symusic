#include <catch2/matchers/catch_matchers_string.hpp>
#include <catch2/catch_test_macros.hpp>

#include "symusic/conversion.h"
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
                              "z2147483547|\n%%MIDI gchord f\n%%MIDI gchordon\n\"C\"C";
    CHECK_THROWS_AS(abc::read(input), std::invalid_argument);
    CHECK(abc::read("X:1\nM:4/4\nL:1/4\nK:C\nz1000001 C").value.note_num() == 1);
}

TEST_CASE("ABC ornament neighbors use the key signature", "[abc]") {
    const auto [score, warnings] = abc::read("X:1\nM:2/4\nL:1/8\nK:D\n^g~f");
    REQUIRE(warnings.empty());
    const auto& notes = *score.tracks->front()->notes;
    REQUIRE(notes.size() == 3);
    CHECK(notes[0] == Note<Tick>{1, 239, 80, 105});
    CHECK(notes[1] == Note<Tick>{241, 59, 79, 80});
    CHECK(notes[2] == Note<Tick>{301, 179, 78, 80});
}


TEST_CASE("ABC bend envelopes follow tied releases", "[abc]") {
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

TEST_CASE("ABC shapes finish before later channel commands at the same tick", "[abc]") {
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

TEST_CASE("ABC export reports loss and still emits readable notation", "[abc]") {
    auto [score, warnings] = abc::read("X:1\nM:4/4\nL:1/4\nK:C\nCDEF");
    REQUIRE(warnings.empty());
    score.tracks->front()->notes->front().time = 0;
    const auto written                         = abc::write(score);
    REQUIRE(written.warnings.size() == 1);
    CHECK(written.warnings.front().starts_with("ABC export score [note-start-offset]"));
    const auto reparsed = abc::read(written.value);
    REQUIRE(reparsed.value.note_num() == 4);
    CHECK(reparsed.value.tracks->front()->notes->front().time == 1);
    score.tracks->front()->notes->front().duration = -1;
    CHECK_THROWS_WITH(
        abc::write(score), "ABC export track 1 (''): note duration cannot be negative"
    );
}

TEST_CASE("ABC export validates durations before converting floating units", "[abc]") {
    const auto score    = abc::read("X:1\nM:4/4\nL:1/4\nK:C\nC").value;
    auto       quarters = convert<Quarter>(score);
    auto       seconds  = convert<Second>(score);
    quarters.tracks->front()->notes->front().duration = 0.0001f;
    CHECK(
        abc::read(abc::write(quarters).value).value.tracks->front()->notes->front().duration == 1
    );
    seconds.tracks->front()->notes->front().duration = -1;
    CHECK_THROWS_AS(abc::write(seconds), std::invalid_argument);
}

TEST_CASE("ABC diagnostics reject invalid file bytes before playback", "[abc]") {
    CHECK_THROWS_WITH(
        abc::read("X:1\nK:C\n!\xff! CDEF", "legacy.abc"),
        "legacy.abc:3:2: ABC [invalid-text]: ABC text must be valid UTF-8"
    );
}

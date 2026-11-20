#include <algorithm>
#include <array>
#include <tuple>

#include <catch2/matchers/catch_matchers_string.hpp>
#include <catch2/catch_test_macros.hpp>
#include <fmt/format.h>

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
    CHECK(
        warnings.front()
        == "tune.abc:5:3: ABC [stray-chord-close]: ignored ']' outside a chord\n\"  ]D\""
    );
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

TEST_CASE("ABC chord symbols accept Unicode accidentals", "[abc]") {
    const std::string header = "%abc-2.1\nX:1\nT:Harmony\nM:4/4\nL:1/4\nK:C\n";
    const auto [expected, direct_warnings]
        = abc::read(header + "\"Bb/D\"CDEF | \"G/Bb\"GABc | \"F#/C#\"defg |]");
    REQUIRE(direct_warnings.empty());
    REQUIRE(expected.tracks->size() == 3);

    const auto [score, warnings] = abc::read(
        header
        + "\"B\u266d/D\u266e\"CDEF | \"G/B\\u266d\"GABc | defg |]\n"
          "s: * * * * | * * * * | \"F\\U0000266f/c\u266f\""
    );
    CHECK(warnings.empty());
    CHECK(score == expected);
    const auto& bass = *score.tracks->at(1)->notes;
    REQUIRE(bass.size() == 6);
    CHECK(bass[0].pitch == 38);   // Natural D is the first chord's inversion.
    CHECK(bass[2].pitch == 46);   // B-flat is an added bass under G major.
    CHECK(bass[4].pitch == 37);   // C-sharp is the last chord's inversion.
}

TEST_CASE("ABC chord accidentals retain their register", "[abc]") {
    const auto [score, warnings] = abc::read(
        "X:1\nT:\nM:4/4\nL:1/4\nK:C\n%%MIDI gchord fc\n"
        "\"Cb\"C4 | \"B#\"D4 | \"Cbb\"E4 | \"B##\"F4 | \"D/cbb\"G4 |]"
    );
    REQUIRE(warnings.empty());
    REQUIRE(score.tracks->size() == 3);
    vec<int> bass;
    vec<int> chords;
    for (const auto& note : *score.tracks->at(1)->notes) bass.push_back(note.pitch);
    for (const auto& note : *score.tracks->at(2)->notes) chords.push_back(note.pitch);
    CHECK(bass == vec<int>{35, 48, 34, 49, 34});
    CHECK(chords == vec<int>{47, 51, 54, 60, 64, 67, 46, 50, 53, 61, 65, 68, 50, 54, 57});
}

TEST_CASE("An ABC slash bass sounds below the chord across the octave boundary", "[abc]") {
    const auto [score, warnings]
        = abc::read("%abc-2.1\nX:1\nT:\nM:4/4\nL:1/4\nK:C\n%%MIDI gchord fc\n\"Cb/B#\"C4 |]");
    REQUIRE(warnings.empty());
    REQUIRE(score.tracks->size() == 3);
    const auto& bass   = *score.tracks->at(1)->notes;
    const auto& chords = *score.tracks->at(2)->notes;
    REQUIRE(bass.size() == 1);
    REQUIRE(chords.size() == 3);
    CHECK(bass.front().pitch == 36);
    CHECK(chords.front().pitch == 47);
}

TEST_CASE("ABC chord quality aliases keep their sevenths and ninths", "[abc]") {
    // Section 4.18 applies the aliases to the quality, not just to bare triads.
    const std::string header = "%abc-2.1\nX:1\nT:\nM:4/4\nL:1/4\nK:C\n";
    for (const auto& [alias, canonical] : {
             std::pair{"Amin7", "Am7"},
             {"Amin9", "Am9"},
             {"Amaj6", "A6"},
             {"A+7/c#(Em)", "Aaug7/C#"},
         }) {
        CAPTURE(alias, canonical);
        const auto [actual, warnings] = abc::read(header + '"' + alias + "\"C4 |]");
        const auto expected           = abc::read(header + '"' + canonical + "\"C4 |]");
        REQUIRE(warnings.empty());
        REQUIRE(expected.warnings.empty());
        REQUIRE(expected.value.tracks->size() == 3);
        CHECK(actual == expected.value);
    }
}

TEST_CASE("ABC chordname definitions take precedence over built-in aliases", "[abc]") {
    const auto [score, warnings] = abc::read(
        "%abc-2.1\nX:1\nT:\nM:4/4\nL:1/4\nK:C\n"
        "%%MIDI gchord fc\n%%MIDI chordname min7 0 5 7\n\"Amin7\"C4 | \"Am7\"D4 |]"
    );
    REQUIRE(warnings.empty());
    REQUIRE(score.tracks->size() == 3);
    vec<int> alias;
    vec<int> canonical;
    for (const auto& note : *score.tracks->at(2)->notes) {
        auto& pitches = note.time < 1920 ? alias : canonical;
        pitches.push_back(note.pitch);
    }
    CHECK(alias == vec<int>{57, 62, 64});
    CHECK(canonical == vec<int>{57, 60, 64, 67});
}

TEST_CASE("ABC extended chord inversions put the requested tone first", "[abc]") {
    const auto [score, warnings] = abc::read(
        "%abc-2.1\nX:1\nT:\nM:4/4\nL:1/4\nK:C\n"
        "%%MIDI gchord fghijk\n\"C9/d\"C4 | \"C9/A\"C4 |]"
    );
    REQUIRE(warnings.empty());
    REQUIRE(score.tracks->size() == 3);
    const auto& bass = *score.tracks->at(1)->notes;
    REQUIRE(bass.size() == 2);
    CHECK(bass[0].pitch == 38);
    CHECK(bass[1].pitch == 45);

    // D is the lowest chord tone; A is not in C9 and leaves the voicing unchanged.
    vec<int> inversion;
    vec<int> added_bass;
    for (const auto& note : *score.tracks->at(2)->notes) {
        auto& pitches = note.time < 1920 ? inversion : added_bass;
        pitches.push_back(note.pitch);
    }
    CHECK(inversion == vec<int>{62, 64, 67, 70, 72});
    CHECK(added_bass == vec<int>{48, 52, 55, 58, 62});
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

TEST_CASE("ABC key and voice fields share pitch options", "[abc]") {
    struct Case {
        std::string_view options;
        int              pitch;
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

TEST_CASE("ABC clef and written octave changes are independent", "[abc]") {
    const std::string opening = "%abc-2.1\nX:1\nT:Pitch changes\nM:4/4\nL:1/4\nK:C\n";
    for (const auto key : {std::string_view("K:"), std::string_view("V:lead ")}) {
        CAPTURE(key);
        const auto change = [key](std::string_view settings) {
            return "[" + std::string(key) + std::string(settings) + "] ";
        };
        const auto [score, warnings] = abc::read(
            opening + change("clef=treble+8 octave=1 transpose=-2") + "C "
            + change("stafflines=1 clef=unknown+8") + "C " + change("clef=treble") + "D "
            + change("clef=bass-8") + "E " + change("octave=0") + "F |]"
        );
        REQUIRE(warnings.empty());
        REQUIRE(score.tracks->size() == 1);
        vec<int> pitches;
        for (const auto& note : *score.tracks->front()->notes) pitches.push_back(note.pitch);
        CHECK(pitches == vec{82, 82, 72, 62, 51});
    }
}

TEST_CASE("Voice selection and pitch options preserve the written rhythm", "[abc]") {
    const std::string opening = "%abc-2.1\nX:1\nT:Voice rhythm\nM:4/4\nL:1/8\n"
                                "V:lead octave=1\nV:side transpose=-2\nK:C\n";
    using Event               = std::tuple<int, int, int>;
    using Events              = vec<Event>;
    const auto played         = [](const Track<Tick>& track) {
        Events events;
        for (const auto& n : *track.notes) events.emplace_back(n.time, n.duration, n.pitch);
        return events;
    };
    struct Case {
        std::string_view body;
        Events           prefix;
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
        const auto lead = played(*score.tracks->front());
        REQUIRE(lead.size() >= prefix.size());
        CHECK(std::equal(prefix.begin(), prefix.end(), lead.begin()));
        CHECK(played(*score.tracks->back()) == Events{{1, 1919, 58}});
    }
}

TEST_CASE("ABC 2.1 key transposition follows the selected voice", "[abc]") {
    const auto [score, warnings] = abc::read(
        "%abc-2.1\nX:1\nT:Voice transposition\nM:4/4\nL:1/4\n"
        "V:lead transpose=-2\nV:side transpose=12\nK:G transpose=5\n"
        "[V:lead] F [K:transpose=1] F [V:side] F F [V:lead] F F |] [V:side] F F |]"
    );
    REQUIRE(warnings.empty());
    REQUIRE(score.tracks->size() == 2);
    const auto pitches = [](const Track<Tick>& track) {
        vec<int> result;
        for (const auto& note : *track.notes) result.push_back(note.pitch);
        return result;
    };
    CHECK(pitches(*score.tracks->front()) == vec{64, 67, 67, 67});
    CHECK(pitches(*score.tracks->back()) == vec{71, 71, 71, 71});
    CHECK(*score.key_signatures == pyvec<KeySignature<Tick>>{{0, 1, 0}});
}

TEST_CASE("ABC 2.1 closing key applies octave and clef to the selected voice", "[abc]") {
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

TEST_CASE("ABC 2.1 inline transposition replaces the opening value", "[abc]") {
    const auto [score, warnings] = abc::read(
        "%abc-2.1\nX:1\nT:Transposition changes\nM:4/4\nL:1/4\n"
        "K:G transpose=5\nF [K:transpose=-2] F [K:transpose=0] F G |]"
    );
    REQUIRE(warnings.empty());
    REQUIRE(score.tracks->size() == 1);
    vec<int> pitches;
    for (const auto& note : *score.tracks->front()->notes) pitches.push_back(note.pitch);
    CHECK(pitches == vec{71, 64, 66, 67});
}

TEST_CASE("ABC accepts key fields without a signature", "[abc]") {
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

TEST_CASE("ABC inline keys clear the signature", "[abc]") {
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

TEST_CASE("ABC key fields with only pitch options preserve measure accidentals", "[abc]") {
    const auto [score, warnings] = abc::read(
        "%abc-2.1\nX:1\nT:Clef changes\nM:4/4\nL:1/4\nK:C\n"
        "^F [K:clef=bass] F [K:middle=B] F [K:transpose=2] F | F "
        "[K:transpose=0] ^F [K:C] F [K:C clef=treble] F |]"
    );
    REQUIRE(warnings.empty());
    vec<int> pitches;
    for (const auto& note : *score.tracks->front()->notes) pitches.push_back(note.pitch);
    CHECK(pitches == vec<int>{66, 66, 66, 68, 67, 66, 65, 65});
    const auto& signatures = *score.key_signatures;
    REQUIRE(signatures.size() == 3);
    CHECK(signatures[0] == KeySignature<Tick>{0, 0, 0});
    CHECK(signatures[1] == KeySignature<Tick>{2880, 0, 0});
    CHECK(signatures[2] == KeySignature<Tick>{3360, 0, 0});
}

TEST_CASE("ABC partial inline key options retain playback settings", "[abc]") {
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

TEST_CASE("ABC bagpipe keys distinguish the signature from the pipe scale", "[abc]") {
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
        "legacy.abc:3:2: ABC [invalid-text]: ABC text must be valid UTF-8\n\"!\\xff! CDEF\""
    );
}

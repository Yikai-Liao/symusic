#include <filesystem>
#include <fstream>
#include <random>
#include <stdexcept>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_exception.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>
#include <fmt/format.h>

#include "symusic/io/abc.h"

namespace {
using namespace symusic;
const std::string header        = "X:1\nM:4/4\nL:1/4\nQ:1/4=120\nK:C\n";
const std::string titled_header = "X:1\nT:\nM:4/4\nL:1/4\nK:C\n";
const std::string strict_header = "%abc-2.1\nX:1\nT:Example\nM:4/4\nL:1/4\nK:C\n";

void rejects(std::string_view source, const std::string& diagnostic) {
    CHECK_THROWS_MATCHES(
        abc::read(source, "case.abc"),
        std::invalid_argument,
        Catch::Matchers::MessageMatches(Catch::Matchers::StartsWith("case.abc:" + diagnostic))
    );
}

using Notes = vec<std::tuple<int, int, int>>;   // onset, duration, pitch

Notes melody(const Score<Tick>& score) {
    REQUIRE(score.tracks->size() == 1);
    Notes result;
    for (const auto& n : *score.tracks->front()->notes)
        result.emplace_back(n.time, n.duration, n.pitch);
    return result;
}

vec<int> pitches(const Score<Tick>& score) {
    REQUIRE(score.tracks->size() == 1);
    vec<int> result;
    for (const auto& note : *score.tracks->front()->notes) result.push_back(note.pitch);
    return result;
}

using Lyrics = vec<std::pair<int, std::string>>;

Lyrics sung(const Score<Tick>& score) {
    REQUIRE(score.tracks->size() >= 1);
    Lyrics result;
    for (const auto& lyric : *score.tracks->front()->lyrics)
        result.emplace_back(lyric.time, lyric.text);
    return result;
}

struct Passage {
    const char* music;
    Notes       notes;
};

TEST_CASE("ABC multi-measure rests keep every bar of the accompaniment", "[abc]") {
    const auto opening
        = strict_header + "%%MIDI drum dddd 36 37 38 39 91 92 93 94\n%%MIDI drumon\n";
    const auto expected = abc::read(opening + "\"C\"z4 | z4 | D4 |]");
    REQUIRE(expected.warnings.empty());
    REQUIRE(expected.value.tracks->size() == 4);
    CHECK(expected.value.tracks->front()->notes->front().time == 3841);
    CHECK(expected.value.tracks->back()->notes->at(4).time == 1921);
    for (const auto* rest : {"Z2", "X2"}) {
        CAPTURE(rest);
        const auto actual = abc::read(opening + "\"C\"" + rest + " | D4 |]");
        CHECK(actual.warnings.empty());
        CHECK(actual.value == expected.value);
    }
}

vec<Notes> drum_tracks(const Score<Tick>& score) {
    vec<Notes> tracks;
    for (const auto& track : *score.tracks) {
        if (!track->is_drum) continue;
        auto& notes = tracks.emplace_back();
        for (const auto& note : *track->notes)
            notes.emplace_back(note.time, note.duration, note.pitch);
    }
    return tracks;
}

TEST_CASE("Legacy drum marks require a written drumon in the same voice", "[abc]") {
    const std::string pattern      = "%%MIDI drum dddd 36 37 38 39 91 92 93 94\n";
    const auto        unconfigured = abc::read(titled_header + pattern + "!drum! CDEF |]");
    CHECK(unconfigured.warnings.empty());
    CHECK(drum_tracks(unconfigured.value).empty());

    const auto other_voice = abc::read(
        titled_header + "V:1\n" + pattern + "!drum! CDEF | GABc |]\nV:2\n"
        + "%%MIDI drum dddd 41 42 43 44 91 92 93 94\n[I:MIDI drumon] z4 | z4 |]"
    );
    REQUIRE(other_voice.warnings.empty());
    CHECK(
        drum_tracks(other_voice.value)
        == vec<Notes>{
            {{1, 479, 41},
             {481, 479, 42},
             {961, 479, 43},
             {1441, 479, 44},
             {1921, 479, 41},
             {2401, 479, 42},
             {2881, 479, 43},
             {3361, 479, 44}}
        }
    );

    const auto skipped = abc::read(
        "X:1\nT:\nM:4/4\nL:1/4\nP:A\nK:C\n" + pattern
        + "P:A\n!drum! CDEF |\nP:B\n[I:MIDI drumon] GABc |]"
    );
    REQUIRE(skipped.warnings.empty());
    CHECK(
        drum_tracks(skipped.value)
        == vec<Notes>{{{1, 479, 36}, {481, 479, 37}, {961, 479, 38}, {1441, 479, 39}}}
    );
}

TEST_CASE("Legacy drum marks gate backing at the note or chord onset", "[abc]") {
    const std::string opening = titled_header + "%%MIDI drum dddd 36 37 38 39 91 92 93 94\n";
    const auto chords = abc::read(opening + "[I:MIDI drumon] [!nodrum!CE]DEF | GA [B!drum!c] c |]");
    REQUIRE(chords.warnings.empty());
    CHECK(drum_tracks(chords.value) == vec<Notes>{{{2881, 479, 38}, {3361, 479, 39}}});

    const auto grace = abc::read(opening + "[I:MIDI drumon] {[!nodrum!CE]} DEFG | !drum! ABcd |]");
    REQUIRE(grace.warnings.empty());
    CHECK(
        drum_tracks(grace.value)
        == vec<Notes>{{{1921, 479, 36}, {2401, 479, 37}, {2881, 479, 38}, {3361, 479, 39}}}
    );

    const auto repeat
        = abc::read(opening + "[I:MIDI drumon] |: C !nodrum! DE !drum! F :| !nodrum! GABc |]");
    REQUIRE(repeat.warnings.empty());
    CHECK(
        drum_tracks(repeat.value)
        == vec<Notes>{{{1, 479, 36}, {1441, 479, 39}, {1921, 479, 36}, {3361, 479, 39}}}
    );
}

TEST_CASE("MIDI comma53 quantizes ordinary and fractional notes", "[abc]") {
    const auto [score, warnings] = abc::read(
        titled_header + "CDEF | [I:MIDI tuningsystem comma53] CDEF | ^/C _/D ^3/4C _3/4D |]"
    );
    REQUIRE(warnings.empty());
    CHECK(pitches(score) == vec<int>{60, 62, 64, 65, 59, 62, 64, 64, 60, 61, 60, 61});
    const auto& bends = *score.tracks->front()->pitch_bends;
    CHECK(
        vec<PitchBend<Tick>>(bends.begin(), bends.end())
        == vec<PitchBend<Tick>>{
            {1921, 4095},
            {2401, 154},
            {2881, 309},
            {3361, 4018},
            {3841, 1854},
            {4321, 2395},
            {4801, 2782},
            {5281, 1468}
        }
    );
}

TEST_CASE("MIDI comma53 follows written order and survives other tuning settings", "[abc]") {
    const auto [score, warnings] = abc::read(
        titled_header
        + "V:1\n|: C4 | [I:MIDI tuningsystem comma53] C4 :| "
          "[I:MIDI C 61] C4 | [I:MIDI temperamentnormal] C4 |]\n"
          "V:2\nC4 |]"
    );
    REQUIRE(warnings.empty());
    REQUIRE(score.tracks->size() == 2);
    vec<int> lead;
    for (const auto& note : *score.tracks->front()->notes) lead.push_back(note.pitch);
    CHECK(lead == vec<int>{60, 59, 60, 59, 60, 59});
    const auto& bass = *score.tracks->at(1);
    REQUIRE(bass.notes->size() == 1);
    CHECK(bass.notes->front().pitch == 59);
    REQUIRE(bass.pitch_bends->size() == 1);
    CHECK(bass.pitch_bends->front() == PitchBend<Tick>{1, 4095});
}

TEST_CASE("ABC export preserves the performed notes and bends from comma53", "[abc]") {
    const auto original
        = abc::read(titled_header + "%%MIDI tuningsystem comma53\nC ^/D E F | G A B c |]");
    REQUIRE(original.warnings.empty());
    const auto written = abc::write(original.value);
    REQUIRE(written.warnings.size() == 2);
    CHECK(written.warnings[0].find("[midi-bend-timing]") != std::string::npos);
    CHECK(written.warnings[1].find("[midi-tied-release]") != std::string::npos);
    const auto played = abc::read(written.value);
    REQUIRE(played.warnings.empty());
    REQUIRE(played.value.tracks->size() == 1);
    const auto& expected = *original.value.tracks->front();
    const auto& actual   = *played.value.tracks->front();
    CHECK(*actual.notes == *expected.notes);
    CHECK(*actual.pitch_bends == *expected.pitch_bends);
}

TEST_CASE("An invalid MIDI tuning system does not change the active one", "[abc]") {
    const auto [score, warnings] = abc::read(
        "%%MIDI tuningsystem comma53\n" + titled_header
        + "C [I:MIDI tuningsystem comma53] C [I:MIDI tuningsystem unknown] C F |]"
    );
    CHECK(pitches(score) == vec<int>{60, 59, 59, 64});
    REQUIRE(warnings.size() == 2);
    CHECK(warnings[0].find("[tuning-system-placement]") != std::string::npos);
    CHECK(warnings[1].find("[invalid-tuning-system]") != std::string::npos);
}

TEST_CASE("Multi-measure rests supply ordinary bar boundaries", "[abc]") {
    for (const auto& [compact, expanded] : {
             std::pair{"C4 | Z2 & E4 | F4 |]", "C4 | z4 | z4 & E4 | F4 |]"},
             {"|: C4 | X2 | D4 :| E4 |]", "|: C4 | z4 | z4 | D4 :| E4 |]"},
             {"C4 | Z2 | D4 |]\nw: one | | | two\ns: * | | | !p!",
              "C4 | z4 | z4 | D4 |]\nw: one | | | two\ns: * | | | !p!"},
         }) {
        CAPTURE(compact);
        const auto actual   = abc::read(strict_header + compact);
        const auto expected = abc::read(strict_header + expanded);
        REQUIRE(expected.warnings.empty());
        CHECK(actual.warnings.empty());
        CHECK(actual.value == expected.value);
    }
}

TEST_CASE("A full-bar rest uses a positive integer measure count", "[abc]") {
    const auto [score, warnings] = abc::read(strict_header + "[M:3/4] Z | X | D3 |]");
    CHECK(warnings.empty());
    CHECK(melody(score) == Notes{{2881, 1439, 62}});
    rejects(strict_header + "Z0 |]", "7:2: ABC [invalid-measure-rest]");
    rejects(strict_header + "X3/2 |]", "7:2: ABC [invalid-measure-rest]");
    rejects(strict_header + "Z2000000000 |]", "7:1: ABC [expansion-limit]");
}

TEST_CASE("ABC header includes apply at their written location", "[abc]") {
    const auto [score, warnings] = abc::read_file("testcases/abc_headers/tune.abc");
    CAPTURE(warnings);
    const auto [expected, direct_warnings]
        = abc::read("%abc-2.1\nX:1\nT:Header définitions\nM:3/4\nL:1/4\nK:C\n!p!G3 A |]");
    CHECK(warnings.empty());
    CHECK(direct_warnings.empty());
    CHECK(score == expected);
    CHECK(pitches(score) == vec<int>{67, 69});
}

TEST_CASE("Nested ABC headers retain insertion order and diagnose a cycle", "[abc]") {
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

TEST_CASE("An include cycle leaves room for later headers", "[abc]") {
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

TEST_CASE("ABC skips missing, escaped, and body include requests", "[abc]") {
    const auto [score, warnings] = abc::read_file("testcases/abc_headers/skipped.abc");
    CHECK(melody(score) == Notes{{1, 239, 60}, {241, 239, 62}});
    REQUIRE(warnings.size() == 5);
    CHECK(warnings[0].find("skipped.abc:4:1: ABC [unavailable-include]") != std::string::npos);
    CHECK(warnings[1].find("skipped.abc:5:1: ABC [unavailable-include]") != std::string::npos);
    CHECK(warnings[2].find("skipped.abh:2:1: ABC [invalid-include-content]") != std::string::npos);
    CHECK(warnings[3].find("skipped.abh:3:1: ABC [invalid-include-content]") != std::string::npos);
    CHECK(warnings[4].find("skipped.abc:8:4: ABC [header-only-instruction]") != std::string::npos);
}

TEST_CASE("ABC include diagnostics retain the physical file and line", "[abc]") {
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
    CHECK(pitches(score) == vec<int>{60});
}

TEST_CASE("ABC tunebook text does not enter the first tune", "[abc]") {
    const auto [score, warnings] = abc::read(
        "%abc-2.1\nL:1/4\n\nCollected on the coast.\n\n"
        "X:1\nT:First\nK:C\nC D |]\n\nNotes on this transcription.\n"
    );
    CHECK(warnings.empty());
    CHECK(melody(score) == Notes{{1, 479, 60}, {481, 479, 62}});
}

TEST_CASE("A strict header-only tune ends at its first blank line", "[abc]") {
    const auto [score, warnings]
        = abc::read(strict_header + " \t\nDEF\n\nX:2\nT:Second\nK:C\nC |]");
    CHECK(score.note_num() == 0);
    REQUIRE(warnings.size() == 1);
    CHECK(warnings.front().starts_with("<string>:10:1: ABC [extra-tunes]"));

    const auto [legacy, legacy_warnings] = abc::read(header + "\nD |]");
    CHECK(legacy_warnings.empty());
    CHECK(pitches(legacy) == vec<int>{62});
}

TEST_CASE("A strict tune header cannot continue across a blank line", "[abc]") {
    rejects("%abc-2.1\nX:1\nT:First\n \t\nK:C\nC |]", "4:1: ABC [missing-key]");
    rejects("X:1\nT:First\nI:abc-version 2.1\n\nK:C\nC |]", "4:1: ABC [missing-key]");

    const auto [strict, strict_warnings]
        = abc::read("%abc-2.1\nX:1\nT:First\n % comment\nr:remark\nK:C\nC |]");
    CHECK(strict_warnings.empty());
    CHECK(pitches(strict) == vec<int>{60});

    const auto [legacy, legacy_warnings]
        = abc::read("%abc-2.1\nX:1\nT:First\nI:abc-version 2.0\n\nK:C\nC |]");
    CHECK(legacy_warnings.empty());
    CHECK(pitches(legacy) == vec<int>{60});
}

TEST_CASE("A key outside a tune does not close its header", "[abc]") {
    const auto [score, warnings] = abc::read(
        "%abc-2.1\nM:2/4\nL:1/4\nK:G\n\nX:1\nT:Example\nK:clef=bass\nF2 |]", "fields.abc"
    );
    REQUIRE(warnings.size() == 1);
    CHECK(warnings.front().starts_with("fields.abc:4:1: ABC [invalid-file-header-field]"));
    CHECK(melody(score) == Notes{{1, 959, 65}});
    CHECK(*score.time_signatures == pyvec<TimeSignature<Tick>>{{0, 2, 4}});
    CHECK(*score.key_signatures == pyvec<KeySignature<Tick>>{{0, 0, 0}});
}

TEST_CASE("ABC 2.1 fields obey their file, tune, body and inline scopes", "[abc]") {
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

TEST_CASE("Field scope follows each header's own declared version", "[abc]") {
    const auto [score, warnings] = abc::read(
        "Q:1/4=60\nI:abc-version 2.1\n\nX:1\nT:Legacy tune\n"
        "w:old header text\nI:abc-version 2.0\nK:C\nCD"
    );
    REQUIRE(warnings.size() == 1);
    CHECK(warnings.front().starts_with("<string>:1:1: ABC [invalid-file-header-field]"));
    CHECK(*score.tempos == pyvec<Tempo<Tick>>{{0, 500'000}});
}

TEST_CASE("A deprecation uses its own header version", "[abc]") {
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

TEST_CASE("A dotted slur close does not shorten the following note", "[abc]") {
    const auto [expected, plain_warnings] = abc::read(strict_header + ".(C(DE)G)F |]");
    const auto [dotted, dotted_warnings]  = abc::read(strict_header + ".(C(DE)G.)F |]");
    const auto [staccato, short_warnings] = abc::read(strict_header + ".(C(DE)G.).F |]");
    CHECK(plain_warnings.empty());
    CHECK(dotted_warnings.empty());
    CHECK(short_warnings.empty());
    CHECK(dotted == expected);
    CHECK(melody(dotted).back() == std::tuple{1921, 479, 65});
    CHECK(melody(staccato).back() == std::tuple{1921, 239, 65});
}

TEST_CASE("A nested slur leaves the outer legato active", "[abc]") {
    // Section 4.11 permits both nested slurs and slurs around a single note.
    const auto [score, warnings] = abc::read(strict_header + "%%MIDI trim 1/8\nC ((D)EF) G |]");
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

TEST_CASE("ABC 2.1 diagnoses detached ties without joining attacks", "[abc]") {
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
        const auto [score, warnings] = abc::read(strict_header + body, "ties.abc");
        REQUIRE(warnings.size() == 1);
        CHECK(warnings.front().starts_with(
            std::string("ties.abc:") + position + ": ABC [detached-tie]"
        ));
        CHECK(melody(score) == Notes{{1, 479, 60}, {481, 479, 60}});
    }

    const auto [chord, warnings] = abc::read(strict_header + "[CE] -[CF]");
    REQUIRE(warnings.size() == 1);
    CHECK(chord == abc::read(strict_header + "[CE] [CF]").value);

    const auto [legacy, legacy_warnings] = abc::read(header + "C -C");
    CHECK(legacy_warnings.empty());
    CHECK(melody(legacy) == Notes{{1, 959, 60}});
}

TEST_CASE("A tie carries an accidental only at the same octave", "[abc]") {
    // Sections 4.6 and 4.11 require the two segments to have the same pitch.
    for (const std::string_view change : {"octave=1", "treble+8"}) {
        CAPTURE(change);
        const auto [raised, raised_warnings]
            = abc::read(strict_header + fmt::format("C-| [K:{}]C D |]", change));
        CHECK(raised_warnings.empty());
        CHECK(melody(raised) == Notes{{1, 479, 60}, {481, 479, 72}, {961, 479, 74}});

        const auto [same, same_warnings]
            = abc::read(strict_header + fmt::format("^C-| [K:{}]C, D |]", change));
        CHECK(same_warnings.empty());
        CHECK(melody(same) == Notes{{1, 959, 61}, {961, 479, 74}});
    }
}

TEST_CASE("Octave-scoped accidentals follow the effective octave", "[abc]") {
    // Sections 4.6 and 11.3: raising C and writing C, cancels the octave shift.
    for (const std::string_view change : {"octave=1", "treble+8"}) {
        CAPTURE(change);
        const auto [score, warnings] = abc::read(
            strict_header
            + fmt::format("[I:propagate-accidentals octave]^C [K:{}] C C, c |]", change)
        );
        CHECK(warnings.empty());
        CHECK(pitches(score) == vec<int>{61, 72, 61, 84});
    }
}

TEST_CASE("Ignored music bytes preserve adjacent tie suffixes", "[abc]") {
    for (const std::string_view body : {
             "C- C",
             "C-| C",
             "C.-C",
             "C.@-C",
             "C[r:remark]-C",
             "C\\\n% comment\n-C",
         }) {
        CAPTURE(body);
        const auto [score, warnings] = abc::read(strict_header + std::string(body));
        CHECK(warnings.empty());
        CHECK(melody(score) == Notes{{1, 959, 60}});
    }

    const auto [chord, warnings] = abc::read(strict_header + "[C@-E] [CE]");
    CHECK(warnings.empty());
    CHECK(melody(chord) == Notes{{1, 959, 60}, {1, 479, 64}, {481, 479, 64}});
}

TEST_CASE("Tempo fields honor written beat lengths and quoted labels", "[abc]") {
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
    rejects(header + R"(C [Q:1/4=120 "OK" junk] D)", "6:4: ABC [invalid-tempo]");
    rejects(prefix + "1/0=120\nK:C\nC", "6:1: ABC [invalid-fraction]");
}

TEST_CASE("Deprecated tempos use the active note unit", "[abc]") {
    // ABC 2.1 section 10.1 recommends accepting both unit-relative forms.
    const auto [score, warnings]
        = abc::read(header + R"(C [L:1/8][Q:120] D [L:1/16][Q:C=120] E [Q:"a ] label" 1/4=120] F)");
    CHECK(warnings.empty());
    REQUIRE(score.tempos->size() == 4);
    CHECK(score.tempos->at(1).mspq == 1000000);
    CHECK(score.tempos->at(2).mspq == 2000000);
    CHECK(score.tempos->at(3).mspq == 500000);
}

TEST_CASE("Strict ABC diagnoses each written legacy tempo without changing playback", "[abc]") {
    const std::string opening    = "X:1\nT:Tempos\nL:1/8\n";
    const std::string legacy     = "Q:120\nI:abc-version 2.1\nK:C\n"
                                   "|: C\nQ:C=60\nD [L:1/4][Q:120] E :| F";
    const auto [score, warnings] = abc::read(opening + legacy, "tempo.abc");
    REQUIRE(warnings.size() == 3);
    CHECK(warnings.at(0).starts_with("tempo.abc:4:1: ABC [deprecated-tempo]"));
    CHECK(warnings.at(1).starts_with("tempo.abc:8:1: ABC [deprecated-tempo]"));
    CHECK(warnings.at(2).starts_with("tempo.abc:9:11: ABC [deprecated-tempo]"));

    const auto [modern, modern_warnings] = abc::read(
        "%abc-2.1\n" + opening
        + "Q:1/8=120\nK:C\n"
          "|: C\nQ:1/8=60\nD [L:1/4][Q:1/4=120] E :| F"
    );
    CHECK(modern_warnings.empty());
    CHECK(score == modern);
    REQUIRE(score.tempos->size() >= 3);
    CHECK(score.tempos->at(0).mspq == 1000000);
    CHECK(score.tempos->at(1).mspq == 2000000);
    CHECK(score.tempos->at(2).mspq == 500000);
}

TEST_CASE("Strict ABC diagnoses deprecated metadata fields and directives", "[abc]") {
    const std::string music       = "X:1\nT:Example\nA:Bampton\nE:4\n"
                                    "%%abc-copyright Example\n%%abc-edited-by Example\nK:C\nC";
    const auto [strict, warnings] = abc::read("%abc-2.1\n" + music, "outdated.abc");
    REQUIRE(warnings.size() == 4);
    CHECK(warnings.at(0).starts_with("outdated.abc:4:1: ABC [deprecated-field]"));
    CHECK(warnings.at(1).starts_with("outdated.abc:5:1: ABC [deprecated-field]"));
    CHECK(warnings.at(2).starts_with("outdated.abc:6:1: ABC [deprecated-directive]"));
    CHECK(warnings.at(3).starts_with("outdated.abc:7:1: ABC [deprecated-directive]"));
    const auto [loose, legacy_warnings] = abc::read("%abc-2.0\n" + music);
    CHECK(legacy_warnings.empty());
    CHECK(melody(strict) == melody(loose));
}

TEST_CASE("ABC accepts deprecated history continuation in a tune header", "[abc]") {
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

    const auto [legacy, legacy_warnings] = abc::read(opening + music);
    CHECK(legacy_warnings.empty());
    CHECK(melody(legacy) == melody(score));
}

TEST_CASE("ABC history stops at the next field or blank line", "[abc]") {
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

TEST_CASE("ABC history warnings respect each header's version", "[abc]") {
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
        CHECK(pitches(score) == vec<int>{62});
    }
}

TEST_CASE("ABC instructions end bare history in both spellings", "[abc]") {
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

TEST_CASE("Remarks neither interrupt music nor split a continued line", "[abc]") {
    // ABC 2.1 sections 2.2.5 and 3: removing an r: remark must not affect playback.
    const auto [expected, expected_warnings] = abc::read(strict_header + "C>D [EG] F");
    REQUIRE(expected_warnings.empty());
    REQUIRE(expected.tracks->front()->notes->front().duration == 719);
    for (const auto* music : {
             "C [r:before]>[r:after]D [EG] F",
             "C>D [E\\\n  r: between chord members\nG] F",
             "C>D [E\\\n  % between chord members\nG] F",
         }) {
        CAPTURE(music);
        const auto [actual, warnings] = abc::read(strict_header + music);
        CHECK(warnings.empty());
        CHECK(actual == expected);
    }
}

TEST_CASE("Inline remarks are transparent inside ABC music tokens", "[abc]") {
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
        const auto actual   = abc::read(strict_header + with_remark);
        const auto expected = abc::read(strict_header + plain);
        REQUIRE(actual.warnings.empty());
        REQUIRE(expected.warnings.empty());
        CHECK(actual.value == expected.value);
    }
    rejects(strict_header + "C[r:x]/0 D", "7:7: ABC [zero-length-divisor]");
    rejects(strict_header + "C[r:unclosed D", "7:2: ABC [unclosed-inline-field]");
}

TEST_CASE("Music remarks do not consume quoted or field text", "[abc]") {
    const auto [score, warnings]
        = abc::read(strict_header + R"([V:lead name="Lead [r: literal]"] "^label [r: literal]" C)");
    REQUIRE(warnings.empty());
    REQUIRE(score.tracks->size() == 1);
    CHECK(score.tracks->front()->name == "Lead [r: literal]");
    CHECK(melody(score) == Notes{{1, 479, 60}});
}

TEST_CASE("Inline remarks are transparent to macro uses and their output", "[abc]") {
    const auto definition = strict_header + "m: ~C2 = E[r:from replacement]2\n";
    for (const auto& [with_remark, plain] : {
             std::pair{"~C[r:inside use]2 D |]", "E2 D |]"},
             {"[G~C[r:inside use]2] D |]", "[GE2] D |]"},
         }) {
        CAPTURE(with_remark);
        const auto actual   = abc::read(definition + with_remark);
        const auto expected = abc::read(strict_header + plain);
        REQUIRE(actual.warnings.empty());
        CHECK(actual.value == expected.value);
    }
    rejects(
        strict_header + "m: ~C2 = [E[r:inside expansion]/0]\n  ~C[r:inside use]2 D",
        "8:3: ABC [zero-length-divisor]"
    );
}

TEST_CASE("Grace groups apply broken rhythm to their own attack weights", "[abc]") {
    // ABC 2.1 sections 4.4 and 4.12: a grace pair uses the written dotted ratio.
    const auto [score, warnings] = abc::read(strict_header + "{a>b}C D |]");
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
        const auto actual   = abc::read(strict_header + marked);
        const auto expected = abc::read(strict_header + weighted);
        CHECK(actual.warnings.empty());
        CHECK(expected.warnings.empty());
        CHECK(actual.value == expected.value);
    }
}

TEST_CASE("Dangling or unequal grace rhythm leaves the grace and host notes playable", "[abc]") {
    const auto [score, warnings] = abc::read(strict_header + "{>ab>}C {a>b2}D |]", "grace.abc");
    const auto expected          = abc::read(strict_header + "{ab}C {ab2}D |]");
    REQUIRE(warnings.size() == 3);
    CHECK(warnings.at(0).starts_with("grace.abc:7:2: ABC [unexpected-broken-rhythm]"));
    CHECK(warnings.at(1).starts_with("grace.abc:7:5: ABC [unfinished-broken-rhythm]"));
    CHECK(warnings.at(2).starts_with("grace.abc:7:11: ABC [unequal-broken-rhythm]"));
    CHECK(expected.warnings.empty());
    CHECK(score == expected.value);
}

TEST_CASE("Strict ABC accepts empty or positive reference fields", "[abc]") {
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
    const auto legacy = abc::read("%abc-2.0\nX:-4\nT:Reference\nL:1/4\nK:C\nC");
    CHECK(legacy.warnings.empty());
    CHECK(melody(legacy.value) == Notes{{1, 479, 60}});
}

TEST_CASE("Named tempo uses the compatibility rate", "[abc]") {
    const auto [score, warnings] = abc::read(strict_header + R"(C [Q:"Andante"] D)");
    CHECK(warnings.empty());
    REQUIRE(score.tempos->size() == 2);
    CHECK(score.tempos->back().mspq == 681818);
}

TEST_CASE("A text-only header tempo keeps the last playable tempo", "[abc]") {
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

TEST_CASE("Written rhythm", "[abc]") {
    const Passage cases[] = {
        {"C// z/ D3/2", {{1, 119, 60}, {361, 719, 62}}},
        {"C/3 z/5 D", {{1, 159, 60}, {257, 479, 62}}},
        {"[C/3E/3]2 F", {{1, 959, 60}, {11, 949, 64}, {961, 479, 65}}},
        {"[Cz/3] D", {{1, 479, 60}, {481, 479, 62}}},
        {"(3CDE F", {{1, 319, 60}, {321, 319, 62}, {641, 319, 64}, {961, 479, 65}}},
        {"(3:4:2 CD F", {{1, 639, 60}, {641, 639, 62}, {1281, 479, 65}}},
        {"C>!tenuto![K:G]F", {{1, 719, 60}, {721, 239, 66}}},
        {"[Cz2E]/2 X D", {{1, 239, 60}, {11, 229, 64}, {2161, 479, 62}}},
    };
    for (const auto& [music, notes] : cases) {
        CAPTURE(music);
        const auto [score, warnings] = abc::read(header + music);
        CHECK(warnings.empty());
        CHECK(melody(score) == notes);
    }
}

TEST_CASE("A written unison retains both chord members", "[abc]") {
    // Section 4.17 gives the two heads one written onset and duration.
    const auto [score, warnings]
        = abc::read(strict_header + "%%MIDI chordattack 0\n[D2D2]2 A |]\nw: one two");
    REQUIRE(warnings.empty());
    CHECK(melody(score) == Notes{{1, 1919, 62}, {1, 1919, 62}, {1921, 479, 69}});
    CHECK(sung(score) == Lyrics{{1, "one"}, {1921, "two"}});
}

TEST_CASE("MIDI chord spread includes repeated pitches in written order", "[abc]") {
    for (const auto& [setup, chord, gap] : {
             std::tuple{"", "[DED]", 10},
             {"%%MIDI chordattack 4\n", "[DED]", 4},
             {"%%MIDI chordattack 4\n", "!arpeggio![DED]", 12},
         }) {
        CAPTURE(setup, chord);
        const auto [score, warnings] = abc::read(strict_header + setup + chord + " A |]");
        REQUIRE(warnings.empty());
        CHECK(
            melody(score)
            == Notes{
                {1, 479, 62},
                {1 + gap, 479 - gap, 64},
                {1 + 2 * gap, 479 - 2 * gap, 62},
                {481, 479, 69}
            }
        );
    }
}

TEST_CASE("An invisible bar closes an overlay without a stray bracket", "[abc]") {
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

TEST_CASE("Double bars set the following overlay origin in free meter", "[abc]") {
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

TEST_CASE("Malformed music", "[abc]") {
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
        rejects(header + music, where);
    }
    rejects(strict_header + "[C!bad] D E !F] |]", "7:1: ABC [malformed-group]");
}

TEST_CASE("Invalid meter groups report the written field", "[abc]") {
    for (const std::string_view meter :
         {"(2+3/8", "2+3)/8", "2++3/8", "(2+0)/8", "(254+2)/8", "(2+3)/0", "(2+3)/5"}) {
        CAPTURE(meter);
        const auto line = fmt::format("M:{}", meter);
        rejects("%abc-2.1\nX:1\nT:Meter\n" + line + "\nK:C\nC |]", "4:1: ABC [invalid-meter]");
    }
}

TEST_CASE("Recoverable music errors preserve the melody", "[abc]") {
    const auto [score, warnings] = abc::read(header + "C ] D", "case.abc");
    REQUIRE(warnings.size() == 1);
    CHECK(warnings.front().starts_with("case.abc:6:3: ABC [stray-chord-close]"));
    CHECK(melody(score) == Notes{{1, 479, 60}, {481, 479, 62}});
}

TEST_CASE("ABC 2.1 requires a title field immediately after the reference", "[abc]") {
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
        CHECK(pitches(score) == vec<int>{60});
        CHECK(abc::read(tune).warnings.empty());
    }

    const auto titled = abc::read(
        "%abc-2.0\nL:1/4\n\nX:1\n% a comment\nr: a remark\nT:\nT:Alternate\n"
        "I:abc-version 2.1\nK:C\nC |]"
    );
    CHECK(titled.warnings.empty());
    CHECK(pitches(titled.value) == vec<int>{60});

    for (const std::string_view instruction : {"I:abc-version 2.1\n", "%%abc-version 2.1\n"}) {
        CAPTURE(instruction);
        const auto late
            = abc::read("%abc-2.0\nX:1\n" + std::string(instruction) + "T:Late\nK:C\nC |]");
        REQUIRE(late.warnings.size() == 1);
        CHECK(late.warnings[0].starts_with("<string>:3:1: ABC [title-order]"));
    }
    CHECK(abc::read("%abc-2.1\nX:1\nI:abc-version 2.0\nK:C\nC |]").warnings.empty());
}

TEST_CASE("ABC reserved music characters do not invalidate note groups", "[abc]") {
    const auto opening = strict_header + R"([V:lead name="Lead @#*;?"] )";
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
    CHECK(abc::read(strict_header + "!p#!C").value == abc::read(strict_header + "C").value);
}

TEST_CASE("Macros match before reserved music bytes are ignored", "[abc]") {
    const auto [score, warnings] = abc::read(strict_header + "m: C?2 = D.@-D\nC?2 C");
    CHECK(warnings.empty());
    CHECK(melody(score) == Notes{{1, 959, 62}, {961, 479, 60}});
}

TEST_CASE("A bang linebreak enables plus decorations without changing the melody", "[abc]") {
    const auto expected = abc::read("%abc-2.1\n" + titled_header + "CD|!p!EFGA|]").value;
    for (const std::string_view instruction :
         {"I:linebreak !\n", "I:linebreak <EOL> $ !\n", "%%continueall true\n"}) {
        CAPTURE(instruction);
        const auto opening            = "%abc-2.1\n" + std::string(instruction) + titled_header;
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

TEST_CASE("ABC header instructions override the file decoration dialect", "[abc]") {
    const std::string opening     = "%abc-2.1\nI:linebreak !\nI:decoration +\n";
    const auto        expected    = abc::read("%abc-2.1\n" + titled_header + "CD|!p!EF|]").value;
    const auto        tune_header = titled_header.substr(0, titled_header.find("K:"));

    const auto [bang, bang_warnings]
        = abc::read(opening + tune_header + "I:linebreak <none>\nI:decoration !\nK:C\nCD|!p!EF|]");
    CHECK(bang_warnings.empty());
    CHECK(bang == expected);

    const auto [plus, plus_warnings]
        = abc::read(opening + tune_header + "I:linebreak $\nK:C\nCD|$+p+EF|]");
    CHECK(plus_warnings.empty());
    CHECK(plus == expected);

    const auto [implicit_cleared, warnings]
        = abc::read("%abc-2.1\nI:linebreak !\nI:linebreak <EOL>\n" + titled_header + "CD|!p!EF|]");
    CHECK(warnings.empty());
    CHECK(implicit_cleared == expected);

    const auto [conflict, conflict_warnings]
        = abc::read("%abc-2.1\nI:linebreak !\nI:decoration !\n" + titled_header + "CD|!+p+EF|]");
    REQUIRE(conflict_warnings.size() == 1);
    CHECK(conflict_warnings.front().find("[conflicting-decoration-dialect]") != std::string::npos);
    CHECK(conflict == expected);
}

TEST_CASE("Strict ABC checks named decoration delimiters at their use sites", "[abc]") {
    rejects(strict_header + "C+p+D", "7:2: ABC [decoration-delimiter]");
    rejects(strict_header + "C!", "7:2: ABC [malformed-group]");
    const std::string plus = "%abc-2.1\nI:decoration +\n" + titled_header;
    rejects(plus + "C!p!D", "8:2: ABC [decoration-delimiter]");
    rejects(plus + "[C!p!E] D", "8:3: ABC [decoration-delimiter]");
    rejects(plus + "{[C!p!E]} D", "8:4: ABC [decoration-delimiter]");
    rejects(plus + "CD\ns: *\n+: !p!", "10:4: ABC [decoration-delimiter]");
    rejects(plus + "[U:h=!p!] hC", "8:2: ABC [decoration-delimiter]");
    rejects(
        "%abc-2.1\nI:linebreak !\n" + titled_header + "[C!p!E]", "8:3: ABC [decoration-delimiter]"
    );
}

TEST_CASE("Plus decoration mode is shared by symbols and shorthand definitions", "[abc]") {
    const auto expected = abc::read("%abc-2.1\n" + titled_header + "!p!C D !f!E F").value;
    const auto [actual, warnings]
        = abc::read("%abc-2.1\nI:decoration +\nU:h=+p+\n" + titled_header + "hCD EF\ns: * * +f+ *");
    CHECK(warnings.empty());
    CHECK(actual == expected);

    const auto [cleared, clear_warnings]
        = abc::read(strict_header + R"([U:T=!none!] TC [U:T="^label"] TD)");
    CHECK(clear_warnings.empty());
    CHECK(cleared == abc::read(strict_header + "CD").value);

    const auto [invalid, invalid_warnings]
        = abc::read(strict_header + "[U:h=!p!] hC [U:h=!f!junk] hD");
    REQUIRE(invalid_warnings.size() == 1);
    CHECK(invalid_warnings.front().find("[invalid-symbol]") != std::string::npos);
    CHECK(invalid == abc::read(strict_header + "!p!C !p!D").value);
}

TEST_CASE(
    "ABC shorthand follows written definitions through navigation and voice changes", "[abc]"
) {
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

TEST_CASE("ABC static macros expand before shorthand decorations", "[abc]") {
    const auto [score, warnings] = abc::read(
        "%abc-2.1\nm: ~G3 = C3\nX:1\nT:Static macros\nL:1/4\n"
        "m: ~G3 = hG{A}G{F}G\n+: \nm: ~=a3=a{b}a{g}a\nU:h=!p!\nK:C\n"
        "~G3 ~=a3 G3 ~G30 ~G3/2 ~g3 ~F3 A|]"
    );
    const auto [expanded, direct_warnings]
        = abc::read(strict_header + "!p!G{A}G{F}G a{b}a{g}a G3 ~G30 ~G3/2 ~g3 ~F3 A|]");
    CHECK(warnings.empty());
    CHECK(direct_warnings.empty());
    CHECK(melody(score) == melody(expanded));
    CHECK(*score.tracks->front()->notes == *expanded.tracks->front()->notes);
}

TEST_CASE("An ABC macro target can contain an equals sign", "[abc]") {
    for (const auto& [definition, music, expanded] : {
             std::tuple{"F=G = CD", "F=G A", "CD A"},
             {"F =G = C=D", "F =G A", "C=D A"},
             {"\"^1=2\"F=E", "\"^1=2\"F A", "E A"},
             {"!a=b!F=E", "!a=b!F A", "E A"},
             {"~G3=C=F2", "~G3 A", "C=F2 A"},
             {"~G3=[Q:1/4 = 96]C3", "~G3 A", "[Q:1/4 = 96]C3 A"},
         }) {
        CAPTURE(definition);
        const auto actual   = abc::read(strict_header + "m: " + definition + '\n' + music + " |]");
        const auto expected = abc::read(strict_header + expanded + " |]");
        CHECK(actual.warnings.empty());
        REQUIRE(expected.warnings.empty());
        CHECK(actual.value == expected.value);
    }
}

TEST_CASE("ABC macro limits count Unicode characters", "[abc]") {
    const auto annotation = [](int length) {
        std::string text = "\"^";
        for (int i = 0; i < length; ++i) text += "\U0001D11E";
        return text + '"';
    };
    for (const auto& [target, replacement] : {
             std::pair{annotation(27) + 'C', std::string("D")},
             {std::string("C"), annotation(196) + 'D'},
         }) {
        const auto [score, warnings]
            = abc::read(strict_header + "m: " + target + " = " + replacement + '\n' + target);
        CHECK(warnings.empty());
        CHECK(pitches(score) == vec<int>{62});
    }

    for (const auto& definition : {
             annotation(28) + "C = D",
             "C = " + annotation(197) + 'D',
         }) {
        const auto [score, warnings]
            = abc::read(strict_header + "m: " + definition + "\nC", "macro.abc");
        REQUIRE(warnings.size() == 1);
        CHECK(warnings.front().starts_with("macro.abc:7:1: ABC [invalid-macro]"));
        CHECK(pitches(score) == vec<int>{60});
    }
}

TEST_CASE("ABC transposing macros follow written diatonic octaves", "[abc]") {
    const auto [score, warnings]
        = abc::read(strict_header + "m: ~n3 = n{o}n{m}n\n~G3 ~c3 ~C,3 ~b'3 ~^G3 ~G2 |]");
    const auto [expanded, direct_warnings] = abc::read(
        strict_header + "G{A}G{F}G c{d}c{B}c C,{D,}C,{B,,}C, b'{c''}b'{a'}b' ~^G3 ~G2 |]"
    );
    CHECK(warnings.empty());
    CHECK(direct_warnings.empty());
    CHECK(score == expanded);
}

TEST_CASE("ABC macro changes apply in written order without rescanning the replacement", "[abc]") {
    const auto [score, warnings] = abc::read(
        strict_header
        + "m: ~C2=~G2\nm: ~G2=B2\n"
          "~C2 [m:~C2=hE2] [U:h=!p!] ~C2\n"
          "m: ~C2=h\n+: F2\n"
          "[Q:\"~C2\" 1/4=96] \"^~C2 [m:~C2=A2]\" ~C2 ~G2 |]"
    );
    const auto [expanded, direct_warnings]
        = abc::read(strict_header + "~G2 [U:h=!p!] hE2 [Q:\"~C2\" 1/4=96] hF2 B2 |]");
    CHECK(warnings.empty());
    CHECK(direct_warnings.empty());
    CHECK(score == expanded);
}

TEST_CASE("ABC macro diagnostics point to the original physical line", "[abc]") {
    rejects(strict_header + "m: ~C2=CD$E/0\nC \\\n  ~C2", "9:3: ABC [zero-length-divisor]");
    const auto [score, warnings]
        = abc::read(strict_header + "m: ~C2=CD\nm: ~n=E\n  ~C2 ] E |]", "case.abc");
    REQUIRE(warnings.size() == 2);
    CHECK(warnings[0].starts_with("case.abc:8:1: ABC [invalid-macro]"));
    CHECK(warnings[1].starts_with("case.abc:9:7: ABC [stray-chord-close]"));
    CHECK(pitches(score) == vec<int>{60, 62, 64});
}

TEST_CASE("Legacy bang linebreak detection stops at music boundaries", "[abc]") {
    const auto expected           = abc::read(header + "CD|!p!EF|]").value;
    const auto [legacy, warnings] = abc::read(header + "C! D|! E% comment\nF|]");
    CHECK(warnings.empty());
    CHECK(legacy == abc::read(header + "CD| EF|]").value);
    const auto [mixed, mixed_warnings]
        = abc::read("%abc-2.1\nI:abc-version 2.0\n" + header + "C! D|! +p+EF|]");
    CHECK(mixed_warnings.empty());
    CHECK(mixed == expected);
    rejects("%abc-2.0\nI:abc-version 2.1\n" + titled_header + "C! D", "8:2: ABC [malformed-group]");
}

TEST_CASE("Body instructions cannot change the header's notation dialect", "[abc]") {
    const auto [score, warnings] = abc::read(
        strict_header + "I:decoration +\nC [I:linebreak !] !p!D\n%%continueall true\nE"
    );
    REQUIRE(warnings.size() == 3);
    for (const auto& warning : warnings)
        CHECK(warning.find("[header-only-instruction]") != std::string::npos);
    CHECK(score == abc::read(strict_header + "C!p!DE").value);
}

TEST_CASE("Loose repeat and ending routes", "[abc]") {
    struct Route {
        const char* music;
        vec<int>    pitches;
    };
    const Route cases[] = {
        {"C4 || D4 :| E4 |", {60, 62, 60, 62, 64}},
        {"|: C [1 D :| [2 E :| [3 F |", {60, 62, 60, 64, 60, 65}},
        {"|: CD [\"1\"] EF :| [\"2\"] GA | Bc", {60, 62, 64, 65, 60, 62, 64, 65, 67, 69, 71, 72}},
    };
    for (const auto& [music, expected] : cases) {
        CAPTURE(music);
        const auto score = abc::read(header + music).value;
        CHECK(pitches(score) == expected);
    }
}

TEST_CASE("ABC 2.1 infers the opening of an unmarked repeat", "[abc]") {
    struct Route {
        const char* music;
        vec<int>    notes;
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
        const auto [score, warnings] = abc::read(strict_header + music);
        CHECK(warnings.empty());
        CHECK(pitches(score) == expected);
    }
}

TEST_CASE("Paired repeat colons set the pass count", "[abc]") {
    // ABC 2.1 section 4.8: the matched triple repeat plays three times.
    const auto [score, warnings] = abc::read(strict_header + "|:: C4 D4 ::| E4 |]");
    CHECK(warnings.empty());
    CHECK(pitches(score) == vec{60, 62, 60, 62, 60, 62, 64});
}

TEST_CASE("Separate implicit repeats keep their own endings", "[abc]") {
    const auto [score, warnings] = abc::read(strict_header + "C D :| E F | [1 G A :| [2 B c |]");
    CHECK(warnings.empty());
    CHECK(pitches(score) == vec{60, 62, 60, 62, 64, 65, 67, 69, 64, 65, 71, 72});
}

TEST_CASE("A repeat ending can start with a field change", "[abc]") {
    // Section 13.1 puts M: between the close and [2 in Old Sir Simon the King.
    const auto [score, warnings]
        = abc::read(strict_header + "|: C D | [1 E F :|\\\nM:3/4\n[2 G A B |]");
    CHECK(warnings.empty());
    CHECK(pitches(score) == vec{60, 62, 64, 65, 60, 62, 67, 69, 71});
    REQUIRE(score.time_signatures->size() == 2);
    const auto& meter = score.time_signatures->back();
    CHECK(meter.time == 2880);
    CHECK(meter.numerator == 3);
    CHECK(meter.denominator == 4);
}

TEST_CASE("An intermediate ending applies its opening fields only on its pass", "[abc]") {
    const auto [score, warnings]
        = abc::read(strict_header + "|:: C D | [1 E F ::| [M:3/4] [2 G A B ::| [3 c d e f |]");
    CHECK(warnings.empty());
    CHECK(pitches(score) == vec{60, 62, 64, 65, 60, 62, 67, 69, 71, 60, 62, 72, 74, 76, 77});
    CHECK(
        score.time_signatures->collect()
        == vec<TimeSignature<Tick>>{{0, 4, 4}, {2880, 3, 4}, {4320, 4, 4}}
    );
}

TEST_CASE("A dotted bar does not shorten the next note", "[abc]") {
    // Section 4.8 prefixes a bar line with a dot. A dot before a note is staccato.
    for (const std::string_view bar : {"|", "[|", "[|]", "||", "|]", ":|", "::", ":||:"}) {
        CAPTURE(bar);
        const auto [plain, plain_warnings]
            = abc::read(strict_header + fmt::format("C4{}D4 |]", bar));
        const auto [dotted, warnings] = abc::read(strict_header + fmt::format("C4.{}D4 |]", bar));
        REQUIRE(plain_warnings.empty());
        CHECK(warnings.empty());
        CHECK(dotted == plain);
    }
    const auto [staccato, warnings] = abc::read(strict_header + "C4 | .D4 |]");
    CHECK(warnings.empty());
    CHECK(melody(staccato) == Notes{{1, 1919, 60}, {1921, 959, 62}});
}

TEST_CASE("A composite bar advances lyrics once", "[abc]") {
    // Sections 4.8 and 5.1: a continuous compound bar is one skip boundary.
    for (const std::string_view bar : {"|[|", "|[|]", "|||", "|]|", ".|[|"}) {
        CAPTURE(bar);
        const auto [score, warnings]
            = abc::read(strict_header + fmt::format("C4{}D4|E4|]\nw: one | | three", bar));
        CHECK(warnings.empty());
        CHECK(sung(score) == Lyrics{{1, "one"}, {3841, "three"}});
    }
}

TEST_CASE("A repeat bar accepts a thick edge", "[abc]") {
    for (const std::string_view close : {":|]", ":[|", ":|[|"}) {
        CAPTURE(close);
        const auto [score, warnings]
            = abc::read(strict_header + fmt::format("|[|:C4{}D4|]", close));
        CHECK(warnings.empty());
        CHECK(pitches(score) == vec{60, 60, 62});
    }
}

TEST_CASE("Loose repeat counts", "[abc]") {
    struct Route {
        const char* music;
        vec<int>    pitches;
    };
    const Route cases[] = {
        {"|:: C :| D", {60, 60, 60, 62}},
        {"C::| D", {60, 60, 60, 62}},
        {"|:: C ::|::: D :::| E", {60, 60, 60, 62, 62, 62, 62, 64}},
        {"|:: C [1,2 D ::| [3 E | F", {60, 62, 60, 62, 60, 64, 65}},
    };
    for (const auto& [music, expected] : cases) {
        CAPTURE(music);
        const auto [score, warnings] = abc::read(header + music);
        CHECK(warnings.empty());
        CHECK(pitches(score) == expected);
    }
    // Multiple repeat colons do not require a corresponding numbered ending.
    const auto [score, warnings] = abc::read(header + "|" + std::string(65, ':') + " C :|");
    CHECK(warnings.empty());
    REQUIRE(score.tracks->size() == 1);
    CHECK(score.tracks->front()->notes->size() == 66);

    const auto bare = abc::read(header + "C " + std::string(4096, ':') + " D", "case.abc");
    REQUIRE(bare.warnings.size() == 1);
    CHECK(bare.warnings.front().starts_with("case.abc:6:3: ABC [ambiguous-repeat]"));
    CHECK(melody(bare.value) == Notes{{1, 479, 60}, {481, 479, 60}, {961, 479, 62}});
}

TEST_CASE("Repeat playback skips ordinary bars in an unselected ending", "[abc]") {
    constexpr auto passes = 2000;
    auto           music  = header + "|" + std::string(passes - 1, ':') + " C [1 D ";
    // Scanning these bars on every pass would exceed the navigation work budget.
    for (int i = 0; i < passes; ++i) music += "| ";
    const auto [score, warnings] = abc::read(music + ":| E");
    CHECK(warnings.empty());
    REQUIRE(score.tracks->size() == 1);
    const auto& notes = *score.tracks->front()->notes;
    REQUIRE(notes.size() == passes + 2);
    CHECK(notes[1].pitch == 62);
    CHECK(notes[passes].pitch == 60);
    CHECK(notes.back().pitch == 64);
}

TEST_CASE("Loose repeat routes preserve trailing endings", "[abc]") {
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

TEST_CASE("Part visits select numbered endings", "[abc]") {
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

TEST_CASE("Compact and separate ending markers reset the same accidentals", "[abc]") {
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

TEST_CASE("An alternate ending inherits the measure boundary of the skipped ending", "[abc]") {
    for (const auto* first : {"[1", "|1", "| [1"}) {
        for (const auto* second : {":|2", ":| [2"}) {
            CAPTURE(first, second);
            const auto [score, warnings]
                = abc::read(header + "|: C ^F " + first + " D2 " + second + " F2 |] G2");
            CHECK(warnings.empty());
            REQUIRE(score.tracks->size() == 1);
            const auto& notes = *score.tracks->front()->notes;
            REQUIRE(notes.size() == 7);
            CHECK(notes[5].pitch == 65);
            CHECK(notes[5].velocity == (std::string_view(first).starts_with('|') ? 105 : 95));
        }
    }
}

TEST_CASE("Skipping an ending does not open its lyric line", "[abc]") {
    const auto [score, warnings] = abc::read(
        header
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

TEST_CASE("Symbol lines play like inline decorations and chord symbols", "[abc]") {
    // ABC 2.1 section 4.15: bar skips, blank slots and stacked lines.
    const auto compare = [](const char* lines, const char* inlined) {
        const auto [actual, warnings]          = abc::read(strict_header + lines);
        const auto [expected, inline_warnings] = abc::read(strict_header + inlined);
        CHECK(warnings.empty());
        CHECK(inline_warnings.empty());
        CHECK(actual == expected);
        return actual;
    };
    const auto dynamics = compare(
        "CDEF | G```AB`c\ns: \"^slow\" | !f! ** !fff!", "\"^slow\"CDEF | !f!G```AB`!fff!c"
    );
    REQUIRE(dynamics.tracks->front()->notes->size() == 8);
    const auto unmarked = abc::read(strict_header + "CDEF | G```AB`c").value;
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

TEST_CASE("Postponed symbol lines follow written note slots", "[abc]") {
    // Ties have two slots; a chord has one. A rest-only bar still counts for '|'.
    const auto [actual, warnings] = abc::read(
        strict_header + "C- C {g}[EA] z y x D | z4 |\nE F |]\ns: * !p! !ff! * | | !f! *"
    );
    const auto [expected, inline_warnings]
        = abc::read(strict_header + "C- !p!C {g}!ff![EA] z y x D | z4 |\n!f!E F |]");
    CHECK(warnings.empty());
    CHECK(inline_warnings.empty());
    CHECK(actual == expected);
}

TEST_CASE("Stacked symbol lines apply every row in written order", "[abc]") {
    const auto [actual, warnings]
        = abc::read(strict_header + "C D |\ns: !ped! !ped-up!\ns: !p! *\ns: !ff! *");
    const auto [expected, inline_warnings] = abc::read(strict_header + "!ped!!p!!ff!C !ped-up!D |");
    CHECK(warnings.empty());
    CHECK(inline_warnings.empty());
    CHECK(actual == expected);
    CHECK_FALSE(actual.tracks->front()->pedals->empty());
}

TEST_CASE("A macro definition separates symbol rows", "[abc]") {
    const auto [score, warnings]
        = abc::read(strict_header + "C D |\ns: !p! *\nm: ~C2=CD\ns: !ff! *");
    REQUIRE(warnings.size() == 1);
    CHECK(warnings.front().starts_with("<string>:10:1: ABC [extra-symbols]"));
    CHECK(score == abc::read(strict_header + "!p!C D |").value);
}

TEST_CASE("An opening repeat does not consume the first underlay bar skip", "[abc]") {
    for (const std::string_view first : {"C4", "z4"}) {
        CAPTURE(first);
        const auto opening                     = strict_header + "|: " + std::string(first) + " | ";
        const auto [actual, warnings]          = abc::read(opening + "E4 :|\ns: | !ff!\nw: | next");
        const auto [expected, inline_warnings] = abc::read(opening + "!ff!E4 :|\nw: | next");
        CHECK(warnings.empty());
        CHECK(inline_warnings.empty());
        CHECK(actual == expected);
        CHECK(sung(actual) == Lyrics{{1921, "next"}});
    }
}

TEST_CASE("Symbol lines follow written voices and survive repeat expansion", "[abc]") {
    const auto [actual, warnings] = abc::read(
        strict_header
        + "V:upper\n|: C D & G A :|\nw: one two three four\ns: !p! *\n+: !f! *\n"
          "V:lower\nE F |\ns: * !ff!\nV:upper\nB c |\ns: !pp! *"
    );
    const auto [expected, inline_warnings] = abc::read(
        strict_header
        + "V:upper\n|: !p!C D & !f!G A :|\nw: one two three four\n"
          "V:lower\nE !ff!F |\nV:upper\n!pp!B c |"
    );
    CHECK(warnings.empty());
    CHECK(inline_warnings.empty());
    CHECK(actual == expected);
}

TEST_CASE("Malformed symbol lines identify the source field", "[abc]") {
    const auto [invalid, malformed]
        = abc::read(strict_header + "C D\ns: !p! \"missing\nQ:1/4=90\ns: * !pp!", "case.abc");
    REQUIRE(malformed.size() == 1);
    CHECK(malformed.front().starts_with("case.abc:8:8: ABC [invalid-symbol-line]"));
    CHECK(malformed.front().find("s: !p!") != std::string::npos);
    CHECK(invalid == abc::read(strict_header + "C !pp!D\nQ:1/4=90").value);

    const auto [continued, continuation_warnings]
        = abc::read(strict_header + "C D\ns: !p!\n  +: \"missing", "case.abc");
    REQUIRE(continuation_warnings.size() == 1);
    CHECK(continuation_warnings.front().starts_with("case.abc:9:6: ABC [invalid-symbol-line]"));
    CHECK(continuation_warnings.front().find("  +: ") != std::string::npos);
    CHECK(continued == abc::read(strict_header + "C D").value);

    const auto [empty, empty_warnings] = abc::read(strict_header + "C D\ns:\nE F\ns: !p!");
    CHECK(empty_warnings.empty());
    CHECK(empty == abc::read(strict_header + "C D\n!p!E F").value);

    const auto [excess, surplus] = abc::read(strict_header + "C\ns: !p! !f!");
    REQUIRE(surplus.size() == 1);
    CHECK(surplus.front().find("[extra-symbols]") != std::string::npos);
    CHECK(excess == abc::read(strict_header + "!p!C").value);
}

TEST_CASE("ABC 2.1 aligns postponed lyrics", "[abc]") {
    const auto immediate
        = abc::read(strict_header + "C D E F|\nw: doh re mi fa\nG A B c|\nw: sol la ti doh");
    const auto postponed
        = abc::read(strict_header + "C D E F|\nG A B c|\nw: doh re mi fa\n+: sol la ti doh");
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

TEST_CASE("ABC version overrides the default lyric alignment", "[abc]") {
    const auto music  = "C D |\nE F |\nw: one two\n";
    const auto modern = abc::read("%abc-2.0\nI:abc-version 2.1\n" + titled_header + music);
    const auto legacy = abc::read(
        std::string("%abc-2.1\n") + "X:1\nT:Lyrics\nI:abc-version 2.0\nL:1/4\nK:C\n" + music
    );
    CHECK(sung(modern.value) == Lyrics{{1, "one"}, {481, "two"}});
    CHECK(sung(legacy.value) == Lyrics{{961, "one"}, {1441, "two"}});
    const auto commented = abc::read("%abc-2.1% comment\n" + titled_header + music);
    CHECK(commented.warnings.empty());
    CHECK(sung(commented.value) == sung(modern.value));
    const auto malformed = abc::read(std::string("%abc-2.x\nI:abc-version 2.x\n") + header + music);
    REQUIRE(malformed.warnings.size() == 2);
    CHECK(malformed.warnings[0].find("1:6: ABC [invalid-version]") != std::string::npos);
    CHECK(malformed.warnings[1].find("2:1: ABC [invalid-version]") != std::string::npos);
}

TEST_CASE("Blank lyric fields claim a passage", "[abc]") {
    const auto [score, warnings] = abc::read(strict_header + "C D |\nw:\nE F |\nw: mi");
    CHECK(warnings.empty());
    CHECK(sung(score) == Lyrics{{961, "mi"}});
}

TEST_CASE("Lyric syllables and bar skips", "[abc]") {
    const auto [score, warnings] = abc::read(
        strict_header + "CD EF | GA Bc | d4 |]\nw: syl-la--ble _ * of~the~day a\\-b | last"
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
    const auto skipped = abc::read(strict_header + "CDEF | GABc |\nw: one | two");
    CHECK(sung(skipped.value) == Lyrics{{1, "one"}, {1921, "two"}});
    const auto rest_bar = abc::read(strict_header + "C4 | z4 | D4 |]\nw: one | | two");
    CHECK(rest_bar.warnings.empty());
    CHECK(sung(rest_bar.value) == Lyrics{{1, "one"}, {3841, "two"}});
    const auto excess = abc::read(strict_header + "C\nw: one two");
    REQUIRE(excess.warnings.size() == 1);
    CHECK(excess.warnings.front().find("[extra-lyrics]") != std::string::npos);
    CHECK(sung(excess.value) == Lyrics{{1, "one"}});
}

TEST_CASE("Punctuation stays with a held lyric syllable", "[abc]") {
    // The Canzonetta example in ABC 2.1 section 13.4 ends a line with o_.
    const auto ending = abc::read(strict_header + "C D |]\nw: o_.");
    CHECK(ending.warnings.empty());
    CHECK(sung(ending.value) == Lyrics{{1, "o."}});
    CHECK(sung(abc::read(abc::write(ending.value).value).value) == sung(ending.value));

    const auto continued = abc::read(strict_header + "C D E F |]\nw: o-__?! next");
    CHECK(continued.warnings.empty());
    CHECK(sung(continued.value) == Lyrics{{1, "o?!"}, {1441, "next"}});

    const auto reset = abc::read(strict_header + "C D E F |]\nw: o_.$0 next");
    CHECK(reset.warnings.empty());
    CHECK(sung(reset.value) == Lyrics{{1, "o."}, {961, "next"}});

    for (const auto text : {"o-_$1_$0?$2!$0 next", "o-$1__$0?! next"}) {
        CAPTURE(text);
        const auto styled = abc::read(strict_header + "C D E F |]\nw: " + text);
        CHECK(styled.warnings.empty());
        CHECK(sung(styled.value) == Lyrics{{1, "o?!"}, {1441, "next"}});
    }

    const auto separated = abc::read(strict_header + "C D E F |]\nw: o_ . next");
    CHECK(separated.warnings.empty());
    CHECK(sung(separated.value) == Lyrics{{1, "o"}, {961, "."}, {1441, "next"}});

    const auto excess = abc::read(strict_header + "C D |]\nw: one * tail_!");
    REQUIRE(excess.warnings.size() == 1);
    CHECK(excess.warnings.front().find("[extra-lyrics]") != std::string::npos);
    CHECK(sung(excess.value) == Lyrics{{1, "one"}});
}

TEST_CASE("ABC 2.1 decodes lyric text without reinterpreting alignment marks", "[abc]") {
    const std::string music  = R"(C D E F | G A B c |])"
                               "\n"
                               R"(w: \~no &eacute;-\u002a a\-b)"
                               "\n"
                               R"(+: \U0000007c of~the~day \&eacute; 50\%)";
    const auto        modern = abc::read(strict_header + music);
    CHECK(modern.warnings.empty());
    CHECK(
        sung(modern.value)
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

    const auto legacy = abc::read(header + music);
    CHECK(legacy.warnings.empty());
    CHECK(
        sung(legacy.value)
        == Lyrics{
            {1, "\\ no"},
            {481, "&eacute;"},
            {961, "\\u002a"},
            {1441, "a-b"},
            {1921, "\\U0000007c"},
            {2401, "of the day"},
            {2881, "&eacute;"},
            {3361, "50%"},
        }
    );
}

TEST_CASE("ABC stanza numbers do not take a lyric slot", "[abc]") {
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
            = abc::read(strict_header + fmt::format("C D E F |]\nw: {}Three blind mice", number));
        CHECK(warnings.empty());
        CHECK(sung(score) == Lyrics{{1, "Three"}, {481, "blind"}, {961, "mice"}});
    }
    const auto [continued, warnings]
        = abc::read(strict_header + "C D E F |]\nw: 1.* two\n+: 3rd 4th");
    CHECK(warnings.empty());
    CHECK(sung(continued) == Lyrics{{481, "two"}, {961, "3rd"}, {1441, "4th"}});

    const auto blank = abc::read(strict_header + "C D E F |]\nw: 1. -two");
    CHECK(blank.warnings.empty());
    CHECK(sung(blank.value) == Lyrics{{481, "two"}});

    for (const std::string_view text : {"1\\-~\u00e9tait", "1.~&eacute;tait", "1.~\\u00e9tait"}) {
        CAPTURE(text);
        const auto [score, numbered_warnings]
            = abc::read(strict_header + fmt::format("C D |]\nw: {}", text));
        CHECK(numbered_warnings.empty());
        CHECK(sung(score) == Lyrics{{1, "\u00e9tait"}});
    }

    const auto escaped = abc::read(strict_header + "C D E F |]\nw: \\u0031st second");
    CHECK(escaped.warnings.empty());
    CHECK(sung(escaped.value) == Lyrics{{1, "1st"}, {481, "second"}});
}

TEST_CASE("An undefined lyric escape does not hide a note skip", "[abc]") {
    for (const auto* opening : {&header, &strict_header}) {
        const auto [score, warnings] = abc::read(*opening + R"(C D E F |]
w: a\*b c)");
        CHECK(warnings.empty());
        CHECK(sung(score) == Lyrics{{1, "a\\"}, {961, "b"}, {1441, "c"}});
    }
}

TEST_CASE("Written lyrics count ties but not grace notes or rests", "[abc]") {
    const auto [score, warnings]
        = abc::read(strict_header + "C- C {g}D z y x E |]\nw: first held host final");
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

TEST_CASE("Postponed lyrics stay on the written voice", "[abc]") {
    const auto [score, warnings] = abc::read(
        strict_header
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

TEST_CASE("The ABC 2.1 two-bar overlay keeps its written lyrics", "[abc]") {
    const auto [score, warnings] = abc::read(
        "%abc-2.1\nX:1\nT:Overlay lyrics\nM:4/4\nL:1/8\nK:C\n"
        "g4 f4 | e6 e2 |\n"
        "&& (d8 | c6) c2|\n"
        "w: ha-la-| lu-yoh\n"
        "+: lu-   |    -yoh"
    );
    REQUIRE(warnings.empty());
    REQUIRE(score.tracks->size() == 2);
    using Onsets        = vec<std::pair<int, int>>;
    const auto& primary = *score.tracks->front();
    const auto& side    = *score.tracks->back();
    const auto  onsets  = [](const auto& track) {
        Onsets result;
        for (const auto& note : *track.notes) result.emplace_back(note.time, note.pitch);
        return result;
    };
    CHECK(onsets(primary) == Onsets{{1, 79}, {961, 77}, {1921, 76}, {3361, 76}});
    CHECK(onsets(side) == Onsets{{1, 74}, {1921, 72}, {3361, 72}});
    CHECK(sung(score) == Lyrics{{1, "ha"}, {961, "la"}, {1921, "lu"}, {3361, "yoh"}});
    REQUIRE(side.lyrics->size() == 2);
    CHECK(side.lyrics->front() == TextMeta<Tick>{1, "lu"});
    CHECK(side.lyrics->back() == TextMeta<Tick>{3361, "yoh"});
}

TEST_CASE("ABC 2.1 two-bar overlays keep separate percussion mapping passes", "[abc]") {
    const auto [score, warnings] = abc::read(
        "%abc-2.1\n%%MIDI drummap C 35\nX:1\nT:Overlay percussion\nM:2/4\nL:1/4\nK:C\n"
        "%%MIDI channel 10\n"
        "C2 | C2 | && [I:MIDI drummap C 38] C2 | C2 | C2 |"
    );
    REQUIRE(warnings.empty());
    REQUIRE(score.tracks->size() == 2);
    const auto& primary = *score.tracks->front();
    const auto& overlay = *score.tracks->back();
    REQUIRE(primary.is_drum);
    REQUIRE(overlay.is_drum);
    using Onsets      = vec<std::pair<int, int>>;
    const auto onsets = [](const auto& track) {
        Onsets result;
        for (const auto& note : *track.notes) result.emplace_back(note.time, note.pitch);
        return result;
    };
    CHECK(onsets(primary) == Onsets{{1, 35}, {961, 35}, {1921, 35}});
    CHECK(onsets(overlay) == Onsets{{1, 38}, {961, 38}});
}

TEST_CASE("Generated drums follow the overlay that declares them", "[abc]") {
    const auto [score, warnings] = abc::read(
        "X:1\nT:Overlay drums\nM:4/4\nL:1/4\nK:C\n"
        "%%MIDI drum dd 36 42\n"
        "C2 & [I:MIDI drumon] !p!E- & G2 | F2 & E2 & B | C2 |"
    );
    REQUIRE(warnings.empty());
    REQUIRE(score.tracks->size() == 4);
    CHECK_FALSE(score.tracks->at(1)->is_drum);
    CHECK(score.tracks->at(1)->notes->front().pitch == 64);
    CHECK(score.tracks->at(2)->is_drum);
    const auto& drums = *score.tracks->at(2)->notes;
    REQUIRE(drums.size() == 4);
    CHECK(drums[0] == Note<Tick>{1, 919, 36, 80});
    CHECK(drums[1] == Note<Tick>{481, 959, 36, 80});
    CHECK(drums[2] == Note<Tick>{921, 919, 42, 80});
    CHECK(drums[3] == Note<Tick>{1441, 959, 36, 80});
    CHECK_FALSE(score.tracks->at(3)->is_drum);
    CHECK(score.tracks->at(3)->notes->front().pitch == 67);
}

TEST_CASE("Drum gates and patterns follow their own overlay lanes", "[abc]") {
    const auto [score, warnings] = abc::read(
        "X:1\nT:Overlay drums\nM:4/4\nL:1/4\nK:C\n"
        "%%MIDI drum dd 36 42\n"
        "C4 & [I:MIDI drumon] E4 & [I:MIDI drum dd 37 43] [I:MIDI drumon] G4 |\n"
        "[I:MIDI drumoff] D4 & [I:MIDI drumoff] F4 & A4 | B4 |]"
    );
    REQUIRE(warnings.empty());
    REQUIRE(score.tracks->size() == 5);
    const auto& first  = *score.tracks->at(2);
    const auto& second = *score.tracks->at(4);
    REQUIRE(first.is_drum);
    REQUIRE(second.is_drum);
    CHECK(*first.notes == pyvec<Note<Tick>>{{1, 959, 36, 80}, {961, 959, 42, 80}});
    CHECK(
        *second.notes
        == pyvec<Note<Tick>>{
            {1, 959, 37, 80},
            {961, 959, 43, 80},
            {1921, 959, 37, 80},
            {2881, 959, 43, 80},
            {3841, 959, 37, 80},
            {4801, 959, 43, 80}
        }
    );
}

TEST_CASE("An overlay drum clock follows repeats and silent measures", "[abc]") {
    const std::string header = "X:1\nT:Drum clock\nM:4/4\nL:1/4\nK:C\n%%MIDI drum dd 36 42\n";
    using Hits               = pyvec<Note<Tick>>;
    const auto check_part    = [&](std::string_view body, const Hits& expected) {
        const auto [score, warnings] = abc::read(header + std::string(body));
        REQUIRE(warnings.empty());
        REQUIRE(score.tracks->size() == 3);
        const auto& drums = *score.tracks->back();
        REQUIRE(drums.is_drum);
        CHECK(*drums.notes == expected);
    };
    SECTION("the overlay gate is replayed on each pass") {
        check_part(
            "|: C4 & [I:MIDI drumon] E4 | D4 & [I:MIDI drumoff] F4 :| G4 |]",
            {{1, 959, 36, 80}, {961, 959, 42, 80}, {3841, 959, 36, 80}, {4801, 959, 42, 80}}
        );
    }
    SECTION("the pattern keeps moving while the overlay melody is absent") {
        check_part(
            "C4 & [I:MIDI drumon] E4 | D4 | F4 & G4 | A4 |]",
            {{1, 959, 36, 80},
             {961, 959, 42, 80},
             {1921, 959, 36, 80},
             {2881, 959, 42, 80},
             {3841, 959, 36, 80},
             {4801, 959, 42, 80},
             {5761, 959, 36, 80},
             {6721, 959, 42, 80}}
        );
    }
}

TEST_CASE("Coincident bar restarts do not duplicate a one-slot drum hit", "[abc]") {
    const auto [score, warnings] = abc::read(
        "X:1\nT:Drum clock\nM:4/4\nL:1/4\nK:C\n"
        "%%MIDI drum d 35 80\n%%MIDI drumon\nCDEF | GAB |"
    );
    REQUIRE(warnings.empty());
    REQUIRE(score.tracks->size() == 2);
    REQUIRE(score.tracks->back()->is_drum);
    CHECK(
        *score.tracks->back()->notes == pyvec<Note<Tick>>{{1, 1919, 35, 80}, {1921, 1440, 35, 80}}
    );
}

TEST_CASE("An overlay generates its own harmony with its own gate and program", "[abc]") {
    const auto [score, warnings] = abc::read(
        "X:1\nT:Overlay harmony\nM:4/4\nL:1/4\nK:C\n"
        "\"C\"C4 & [I:MIDI gchordoff] [I:MIDI chordprog 5] [I:MIDI bassvol 31] \"Am\"E4 |\n"
        "[I:MIDI gchordoff] D4 & [I:MIDI gchordon] F4 | B4 |]"
    );
    REQUIRE(warnings.empty());
    REQUIRE(score.tracks->size() == 6);
    using Notes      = pyvec<Note<Tick>>;
    const auto track = [&](size_t index) -> const auto& { return *score.tracks->at(index); };
    CHECK(*track(1).notes == Notes{{1, 239, 36, 80}, {961, 239, 36, 80}});
    CHECK(track(2).program == 0);
    REQUIRE(track(2).notes->size() == 6);
    CHECK(track(2).notes->front() == Note<Tick>{481, 239, 48, 75});
    CHECK(*track(3).notes == Notes{{1, 1919, 64, 105}, {1921, 1919, 65, 105}});
    CHECK(
        *track(4).notes
        == Notes{{1921, 239, 45, 31}, {2881, 239, 45, 31}, {3841, 239, 45, 31}, {4801, 239, 45, 31}}
    );
    CHECK(track(5).program == 5);
    REQUIRE(track(5).notes->size() == 12);
    CHECK(track(5).notes->front() == Note<Tick>{2401, 239, 57, 75});
    CHECK(track(5).notes->back() == Note<Tick>{5281, 239, 64, 75});
}

TEST_CASE("Overlay drones retain independent gates and follow their melodies", "[abc]") {
    const auto [score, warnings] = abc::read(
        "X:1\nT:Overlay drones\nM:4/4\nL:1/4\nK:C\n"
        "%%MIDI drone 40 47 35 61 62\n"
        "C4 & [I:MIDI droneon] E4 & [I:MIDI droneon] G4 |\n"
        "D4 & [I:MIDI droneoff] F4 & A4 | B4 & G4 & [I:MIDI droneoff] F4 |]"
    );
    REQUIRE(warnings.empty());
    REQUIRE(score.tracks->size() == 5);
    using Notes        = pyvec<Note<Tick>>;
    const auto& first  = *score.tracks->at(2);
    const auto& second = *score.tracks->at(4);
    CHECK(first.program == 40);
    CHECK(second.program == 40);
    CHECK(*first.notes == Notes{{2, 1921, 47, 61}, {3, 1920, 35, 62}});
    CHECK(*second.notes == Notes{{2, 3841, 47, 61}, {3, 3840, 35, 62}});
}

TEST_CASE("A repeat closes a two-bar overlay before jumping", "[abc]") {
    const std::string opening    = "%abc-2.1\nX:1\nT:Repeated overlay\nM:2/4\nL:1/4\nK:C\n";
    const auto [score, warnings] = abc::read(opening + "A2 | B |: CD | EF | && G2 | A2 :| B2 |]");
    REQUIRE(warnings.empty());
    REQUIRE(score.tracks->size() == 2);
    const auto& primary = *score.tracks->front()->notes;
    REQUIRE(primary.size() == 11);
    CHECK(primary.back().time == 5281);
    vec<int> side;
    for (const auto& note : *score.tracks->back()->notes) side.push_back(note.time);
    CHECK(side == vec<int>{1441, 2401, 3361, 4321});
}

TEST_CASE("A two-bar overlay rejoins inside a numbered ending", "[abc]") {
    const std::string opening = "%abc-2.1\nX:1\nT:Ending overlay\nM:2/4\nL:1/4\nK:C\n";
    const auto [score, warnings]
        = abc::read(opening + "|: C2 :| [2-3 D2 | E2 | && F2 | G2 || A2 |]");
    REQUIRE(warnings.empty());
    REQUIRE(score.tracks->size() == 2);
    const auto& primary = *score.tracks->front()->notes;
    REQUIRE(primary.size() == 8);
    CHECK(primary.back().time == 6721);
    vec<int> side;
    for (const auto& note : *score.tracks->back()->notes) side.push_back(note.time);
    CHECK(side == vec<int>{1921, 2881, 4801, 5761});
}

TEST_CASE("A malformed two-bar overlay warns without rewinding the voice", "[abc]") {
    for (const auto& [marked, plain, column] : {
             std::tuple{"C4 | && D4 | E4 |", "C4 | D4 | E4 |", 6},
             {"C4 | D4 | && E4 |", "C4 | D4 | E4 |", 11},
             {"C4 | D4 | E && F4 | G4 |", "C4 | D4 | E F4 | G4 |", 13},
             {"C4 | D4 | && E4 :| F4 |", "C4 | D4 | E4 :| F4 |", 11},
             {"C4 || D4 | && E4 | F4 |", "C4 || D4 | E4 | F4 |", 12},
             {"C4 | D4 | && E4 || F4 |", "C4 | D4 | E4 || F4 |", 11},
         }) {
        CAPTURE(marked);
        const auto [score, warnings] = abc::read(strict_header + marked, "overlay.abc");
        REQUIRE(warnings.size() == 1);
        CHECK(warnings.front().starts_with(
            "overlay.abc:7:" + std::to_string(column) + ": ABC [invalid-two-bar-overlay]"
        ));
        CHECK(score == abc::read(strict_header + plain).value);
    }
}

TEST_CASE("Repeated parts select postponed verses", "[abc]") {
    const auto [score, warnings] = abc::read(
        "%abc-2.1\nX:1\nT:Lyrics\nL:1/4\nP:A2\nK:C\nP:A\nC |\nD |]\n"
        "w: verse one\nw: second *"
    );
    CHECK(warnings.empty());
    CHECK(sung(score) == Lyrics{{1, "verse"}, {481, "one"}, {961, "second"}});
}

TEST_CASE("A repeat jump within a music line does not restart its lyrics", "[abc]") {
    SECTION("Notes first visited after the repeat keep their own syllables") {
        const auto [score, warnings]
            = abc::read(strict_header + "|: C D E F :| G A |]\nw: one two three four five six");
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
            = abc::read(strict_header + "|: C2 D2 |\nw: one two\nE2 F2 :|\nw: three four");
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

TEST_CASE("Macro expansion gives each note its own lyric", "[abc]") {
    // Sections 5.1 and 9.1: replacements share a source location, not a note slot.
    const std::string words = "\nw: one two three four";
    const auto [score, warnings]
        = abc::read(strict_header + "m: ~C2=CD\n~C2 ~C2 |]\ns: * !p! * !f!" + words);
    CHECK(warnings.empty());
    CHECK(sung(score) == Lyrics{{1, "one"}, {481, "two"}, {961, "three"}, {1441, "four"}});
    CHECK(score == abc::read(strict_header + "C!p!D C!f!D |]" + words).value);
}

TEST_CASE("Intervening fields separate lyric verses", "[abc]") {
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

TEST_CASE("Numbered ending lists select large and overlapping ranges", "[abc]") {
    const auto [score, warnings] = abc::read(header + "|: C [7-8,1,3-33,3 D :| [2 E || F");
    CHECK(warnings.empty());
    const auto notes = melody(score);
    REQUIRE(notes.size() == 67);
    CHECK(notes[1] == std::tuple{481, 479, 62});
    CHECK(notes[3] == std::tuple{1441, 479, 64});
    CHECK(notes[5] == std::tuple{2401, 479, 62});
    CHECK(notes[65] == std::tuple{31201, 479, 62});
    CHECK(notes[66] == std::tuple{31681, 479, 65});

    rejects(header + "|: C [0 D :|", "6:7: ABC [ending-range]");
    rejects(header + "|: C [3-2 D :|", "6:7: ABC [invalid-ending]");
    rejects(header + "|: C [1-2-3 D :|", "6:6: ABC [malformed-group]");
    rejects(header + "|: C [1-2147483647 D :|", "6:4: ABC [expansion-limit]");
}

TEST_CASE("Nested part order", "[abc]") {
    // ABC 2.1 section 3.1.9: nested groups can repeat and dots are ignored.
    const std::string prefix     = "%abc-2.1\nX:1\nT:Parts\nM:4/4\nL:1/4\nP:";
    const std::string parts      = "\nK:C\nP:A\nC\nP:B\nD";
    const auto [score, warnings] = abc::read(prefix + "(A.(BA)2)2" + parts);
    CHECK(warnings.empty());
    CHECK(pitches(score) == vec{60, 62, 60, 62, 60, 60, 62, 60, 62, 60});
    CHECK(abc::read(prefix + "A1.2" + parts).value.note_num() == 12);
}

TEST_CASE("ABC reordered parts use their written key and unit length", "[abc]") {
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

TEST_CASE("ABC skipped endings keep the notation of the next written passage", "[abc]") {
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

TEST_CASE("A part label inside a bar keeps the written accidentals", "[abc]") {
    // ABC 2.1 sections 3.1.9 and 11.3: a change of part can fall inside a bar.
    const std::string prefix = "%abc-2.1\nX:1\nT:Accidentals\nM:2/4\nL:1/4\nP:";
    const auto check = [&](std::string_view order, std::string_view music, vec<int> expected) {
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

TEST_CASE("Consecutive parts keep grace groups and broken rhythm across the label", "[abc]") {
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

TEST_CASE("Part jumps retain the written lengths on both sides of broken rhythm", "[abc]") {
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

TEST_CASE("Timed fields and marks leave broken rhythm intact", "[abc]") {
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

TEST_CASE("Chained rhythm across parts compares adjusted lengths", "[abc]") {
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

TEST_CASE("A shortened note can start a second rhythm pair", "[abc]") {
    const std::string prefix = "%abc-2.1\nX:1\nT:Broken rhythm\nM:4/4\nL:1/4\nP:";
    for (const auto order : {"AB", "B"}) {
        CAPTURE(order);
        const auto start  = prefix + order + "\nK:C\n";
        const auto actual = abc::read(start + "[P:A]C>D>[P:B]E/2 F");
        CHECK(actual.warnings.empty());
        CHECK(actual.value == abc::read(start + "[P:A]C3/2 D3/4[P:B]E/4 F").value);
    }
}

TEST_CASE("Part jumps and repeats preserve the written notes of a tuplet", "[abc]") {
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

TEST_CASE("Broken rhythm across part labels preserves abcMIDI harp attacks", "[abc]") {
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

TEST_CASE("Fermata and grace length commands follow written order", "[abc]") {
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
        strict_header
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

TEST_CASE("Closing an explicit overlay starts a fresh accidental scope", "[abc]") {
    const auto [score, warnings] = abc::read(strict_header + "(& ^F & _F &) F f |]");
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

TEST_CASE("An explicit overlay without a split carries accidentals through its close", "[abc]") {
    const auto [score, warnings] = abc::read(strict_header + "(& ^F &) F f |]");
    CHECK(warnings.empty());
    CHECK(pitches(score) == vec<int>{66, 66, 78});
}

TEST_CASE("Written meter governs rests and tuplets across navigation", "[abc]") {
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

TEST_CASE("Invalid part order and playback expansion", "[abc]") {
    const std::string prefix = "X:1\nM:4/4\nL:1/4\nP:";
    const std::string parts  = "\nK:C\nP:A\nC\nP:B\nD";
    rejects(prefix + "(AB" + parts, "4:1: ABC [invalid-parts]");
    rejects(prefix + "A)" + parts, "4:1: ABC [invalid-parts]");
    rejects(prefix + "A0" + parts, "4:1: ABC [part-count-range]");
    rejects(prefix + "(A65536)65536" + parts, "4:1: ABC [expansion-limit]");
}

TEST_CASE("Continued part itinerary", "[abc]") {
    const auto [score, warnings] = abc::read(
        "X:1\nM:4/4\nL:1/4\nP:(B.\n% order continues\n+:A).2\nK:C\n"
        "P:A\nC |\n[P:B]D |"
    );
    CHECK(warnings.empty());
    CHECK(melody(score) == Notes{{1, 479, 62}, {481, 479, 60}, {961, 479, 62}, {1441, 479, 60}});
}

TEST_CASE("Comments after a continued field quote do not become voice options", "[abc]") {
    const auto [score, warnings] = abc::read(
        "%abc-2.1\nX:1\nT:Voice\nM:4/4\nL:1/4\nV:lead name=\"Lead\n% keep the field open\n"
        "+: 100\\%\n+: Singer\" % transpose=12\nK:C\n[V:lead] C\n"
    );
    CHECK(warnings.empty());
    CHECK(melody(score) == Notes{{1, 479, 60}});
    CHECK(score.tracks->front()->name == "Lead 100% Singer");
}

TEST_CASE("Music continuation across fields and directives", "[abc]") {
    const auto [score, warnings] = abc::read(
        header
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

    rejects(header + "C\\\r\n% skipped\r\nK:G\r\n  F/0 % source", "9:4: ABC [zero-length-divisor]");
    rejects(header + "C\\\r\n% skipped\r\n  Q:0", "8:3: ABC [invalid-tempo]");
}

TEST_CASE("A continued chord or grace group survives intervening lines", "[abc]") {
    for (const auto& [music, joined] : {
             std::pair{"[C\\\n%%vocalfont Times-Roman 14\nE] D", "[CE] D"},
             {"[C\\\nI:example\nE] D", "[CE] D"},
             {"F [C\\\nQ:1/4=90\nE] D", "F [Q:1/4=90][CE] D"},
             {"{[C\\\n%%vocalfont Times-Roman 14\nE]} G D", "{[CE]} G D"},
         }) {
        CAPTURE(music);
        const std::string lyrics   = " |]\nw: one two";
        const auto        actual   = abc::read(strict_header + music + lyrics);
        const auto        expected = abc::read(strict_header + joined + lyrics);
        REQUIRE(actual.warnings.empty());
        REQUIRE(expected.warnings.empty());
        CHECK(actual.value == expected.value);
        CHECK(sung(actual.value) == sung(expected.value));
    }

    rejects(
        strict_header + "[C\\\n%%vocalfont Times-Roman 14\nE/0] D", "9:2: ABC [zero-length-divisor]"
    );
}

TEST_CASE("Macro changes within a continued line apply in written order", "[abc]") {
    const auto actual = abc::read(
        strict_header
        + "m: ~C2 = D2\n~C2 [E\\\nm: ~C2 = F2\nG] ~C2\\\n"
          "m: ~C2 = A2\n~C2 |]"
    );
    const auto expected = abc::read(strict_header + "D2 [EG] F2 A2 |]");
    REQUIRE(actual.warnings.empty());
    CHECK(actual.value == expected.value);
}

TEST_CASE("Continued music keeps inline and physical MIDI aliases in order", "[abc]") {
    const auto actual
        = abc::read(strict_header + "[I:MIDIdef soft vol 64] C [E\\\n%%MIDIx soft\nG] D |]");
    const auto expected = abc::read(strict_header + "C [I:MIDI vol 64][EG] D |]");
    REQUIRE(actual.warnings.empty());
    CHECK(actual.value == expected.value);
}

TEST_CASE("Blank line stops music continuation", "[abc]") {
    const auto [score, warnings] = abc::read(header + "C\\\n \t\nD\nX:2\nK:D\nE", "case.abc");
    CHECK(melody(score) == Notes{{1, 479, 60}});
    REQUIRE(warnings.size() == 2);
    CHECK(warnings[0].starts_with("case.abc:6:2: ABC [stray-continuation]"));
    CHECK(warnings[1].starts_with("case.abc:9:1: ABC [extra-tunes]"));
}

TEST_CASE("Quote spans music continuation", "[abc]") {
    const auto [score, warnings] = abc::read(strict_header + "\"^label \\\n% skipped\n100\\%\"C");
    CHECK(warnings.empty());
    CHECK(melody(score) == Notes{{1, 479, 60}});
}

TEST_CASE("Quotes do not hide comments", "[abc]") {
    // ABC 2.1 sections 2.2.5 and 8.2: escape a literal percent, even in an annotation.
    const auto [score, warnings] = abc::read(
        "%abc-2.1\nX:1\nT:100\\% ready % comment\nL:1/4\nK:C\n"
        R"("^note &quot;quote&quot; 50\%" C % D)"
    );
    CHECK(warnings.empty());
    CHECK(melody(score) == Notes{{1, 479, 60}});
    CHECK(score.tracks->front()->name == "100% ready");
    rejects(strict_header + R"(C "^50%" D)", "7:3: ABC [malformed-group]");
}

TEST_CASE("Quoted annotations are transparent to broken rhythm", "[abc]") {
    for (const char placement : std::string_view("^_<>@")) {
        CAPTURE(placement);
        const auto [score, warnings]
            = abc::read(strict_header + fmt::format(R"(C "{}text" >D EF |])", placement));
        CHECK(warnings.empty());
        CHECK(
            melody(score) == Notes{{1, 719, 60}, {721, 239, 62}, {961, 479, 64}, {1441, 479, 65}}
        );
    }
}

TEST_CASE("Chord symbols do not break a broken-rhythm pair", "[abc]") {
    const auto prefix                      = strict_header + "%%MIDI gchord fccc\n";
    const auto [expected, common_warnings] = abc::read(prefix + R"("C"C>"F"D EF |])");
    REQUIRE(common_warnings.empty());
    REQUIRE(expected.tracks->size() == 3);
    CHECK(expected.tracks->front()->notes->at(1).time == 721);

    const auto [score, warnings] = abc::read(prefix + R"("C"C"F">D EF |])");
    CHECK(warnings.empty());
    CHECK(score == expected);
}

TEST_CASE("An inline remark can follow a music continuation backslash", "[abc]") {
    const auto expected = abc::read(strict_header + "[CE] D |]").value;
    for (const auto body : {
             "[C\\ [r:remark]\nE] D |]",
             "[C\\ [r:a quote \" in a remark][r:another remark]\nE] D |]",
             "\"^text \\\n[r:literal]\" [C\\ [r:remark]\nE] D |]",
         }) {
        CAPTURE(body);
        const auto [score, warnings] = abc::read(strict_header + body);
        CHECK(warnings.empty());
        CHECK(score == expected);
    }
    rejects(strict_header + "[C\\ [r:remark]\nE]/0", "8:3: ABC [zero-length-divisor]");
}

TEST_CASE("Formatting can separate lyric continuations", "[abc]") {
    for (const auto directive : {
             "%%vocalfont Times-Italic *",
             "%%staffsep 2cm",
             "I:begintext\n%% Printed text\nI:endtext",
             "%%begintext\n%% Printed text\nI:endtext",
         }) {
        CAPTURE(directive);
        const auto [score, warnings]
            = abc::read(strict_header + fmt::format("C D |]\nw:one\n{}\n+:two", directive));
        CHECK(warnings.empty());
        CHECK(sung(score) == Lyrics{{1, "one"}, {481, "two"}});
    }
}

TEST_CASE("A continued formatting instruction owns its field continuation", "[abc]") {
    const auto [score, warnings] = abc::read(
        "%abc-2.1\nX:1\nT:Layout\nM:4/4\nI:vocalfont\n+: Times-Italic *\nL:1/4\nK:C\n"
        "C D |]\nw:one\nI:vocalfont Times-\n+:Italic *"
    );
    CHECK(warnings.empty());
    CHECK(pitches(score) == vec{60, 62});
    CHECK(sung(score) == Lyrics{{1, "one"}});
}

TEST_CASE("Field continuation rules", "[abc]") {
    const auto [score, warnings] = abc::read(
        "X:1\nT: Two\n% skipped\n  +: Words\nM:4/4\nL:1/\n+:4\n"
        "K:C trans\n+:pose=2\nC"
    );
    CHECK(warnings.empty());
    CHECK(melody(score) == Notes{{1, 479, 62}});
    CHECK(score.tracks->front()->name == "Two Words");

    rejects("X:1\nK :C\nC", "2:1: ABC [missing-key]");
    const auto orphan = abc::read(header + "C\n  +: orphan", "case.abc");
    REQUIRE(orphan.warnings.size() == 1);
    CHECK(orphan.warnings.front().starts_with("case.abc:7:3: ABC [orphan-field-continuation]"));
}

TEST_CASE("Rest before a repeat is music", "[abc]") {
    const auto [score, warnings] = abc::read(header + "|: C\nX:|\nD");
    const auto spaced            = abc::read(header + "|: C\nX :|\nD");
    CHECK(warnings == spaced.warnings);
    CHECK(melody(score) == melody(spaced.value));
    CHECK(score.note_num() == 3);
}

TEST_CASE("Voice ID uses the first twenty characters", "[abc]") {
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

TEST_CASE("Numeric voice IDs ignore leading zeroes", "[abc]") {
    const auto [score, warnings] = abc::read(
        strict_header
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

TEST_CASE("MIDI voice instruments can address equivalent numeric IDs", "[abc]") {
    const auto [score, warnings] = abc::read(
        "%abc-2.1\nX:1\nT:\nM:4/4\nL:1/4\n"
        "%%MIDI voice 02 instrument=59\n"
        "V:0001\nV:2\nK:C\n[V:1] CDEF |]\n[V:0002] GABc |]"
    );
    REQUIRE(warnings.empty());
    REQUIRE(score.tracks->size() == 2);
    CHECK(score.tracks->at(0)->program == 0);
    CHECK(score.tracks->at(1)->program == 58);
    CHECK(score.tracks->at(0)->notes->size() == 4);
    CHECK(score.tracks->at(1)->notes->size() == 4);
}

TEST_CASE("A numeric voice and a text voice with its prefix stay distinct", "[abc]") {
    const auto [score, warnings] = abc::read(
        strict_header
        + "[V:12345678901234567890] C\n[V:12345678901234567890a] D\n"
          "[V:12345678901234567890b] E |]"
    );
    REQUIRE(warnings.empty());
    REQUIRE(score.tracks->size() == 2);
    CHECK(score.tracks->at(0)->notes->size() == 1);
    CHECK(score.tracks->at(1)->notes->size() == 2);
}

TEST_CASE("Ornament attacks", "[abc]") {
    const Passage cases[] = {
        {".F G", {{1, 239, 65}, {481, 479, 67}}},
        {"~F G", {{1, 119, 67}, {121, 359, 65}, {481, 479, 67}}},
        {"RF", {{1, 95, 65}, {97, 95, 67}, {193, 95, 65}, {289, 95, 64}, {385, 95, 65}}},
        {"[I:MIDI harpmode 1] ~F3 G",
         {{1, 239, 65}, {241, 239, 65}, {481, 959, 65}, {1441, 479, 67}}},
    };
    for (const auto& [music, notes] : cases) {
        CAPTURE(music);
        const auto [score, warnings] = abc::read(header + music);
        CHECK(warnings.empty());
        CHECK(melody(score) == notes);
    }
}

TEST_CASE("The default roll shorthand and explicit roll play identically", "[abc]") {
    for (const std::string_view mode : {"", "[I:MIDI harpmode 1] "}) {
        CAPTURE(mode);
        const auto opening                         = strict_header + std::string(mode);
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

TEST_CASE("ABC 2.1 accepts standard marks that abc2midi leaves unperformed", "[abc]") {
    const auto             plain = abc::read(strict_header + "CDEF | GABc |]").value;
    const std::string_view marks[]
        = {"trill(", "trill)", "lowermordent", "uppermordent",  "mordent",   "pralltriller",
           "turn",   "turnx",  "invertedturn", "invertedturnx", "+",         "plus",
           "snap",   "slide",  "wedge",        "upbow",         "downbow",   "open",
           "thumb",  "segno",  "coda",         "D.S.",          "D.C.",      "dacoda",
           "dacapo", "fine",   "shortphrase",  "mediumphrase",  "longphrase"};
    for (const auto mark : marks) {
        CAPTURE(mark);
        const auto [score, warnings]
            = abc::read(strict_header + fmt::format("C !{}!DEF | GABc |]", mark));
        CHECK(warnings.empty());
        CHECK(score == plain);
    }

    const auto [score, warnings]
        = abc::read(strict_header + "MC !dacoda!D E !fine!F | O G S A B c !D.S.! |]");
    CHECK(warnings.empty());
    CHECK(score == plain);

    const auto unknown = abc::read(strict_header + "C !unrecognized-mark!DEF | GABc |]");
    REQUIRE(unknown.warnings.size() == 1);
    CHECK_THAT(
        unknown.warnings.front(), Catch::Matchers::ContainsSubstring("[ignored-decoration]")
    );
    CHECK(unknown.value == plain);
}

TEST_CASE("ABC ornament shorthands are distinct from named marks", "[abc]") {
    const auto plain = abc::read(strict_header + "CDEF |]").value;
    const auto bare  = abc::read(strict_header + "PC SD uE vF |]");
    CHECK(bare.warnings.empty());
    CHECK(bare.value == plain);

    const auto named = abc::read(strict_header + "!P!C !S!D !u!E !v!F |]");
    REQUIRE(named.warnings.size() == 4);
    for (const auto& warning : named.warnings)
        CHECK_THAT(warning, Catch::Matchers::ContainsSubstring("[ignored-decoration]"));
    CHECK(named.value == plain);
}

TEST_CASE("Shorthand bar decorations leave following notes unchanged", "[abc]") {
    for (const auto& opening : {strict_header, titled_header}) {
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

TEST_CASE("Chord capacity", "[abc]") {
    const auto chord = "[" + std::string(128, 'C') + "]";
    CHECK(abc::read(header + chord).value.note_num() == 128);
    CHECK(abc::read(header + "{" + chord + "}D").value.note_num() == 129);
    rejects(header + "[" + std::string(129, 'C') + "]", "6:130: ABC [group-limit]");
    rejects(header + "{" + chord + "E}D", "6:132: ABC [group-limit]");
}

TEST_CASE("Tune boundary", "[abc]") {
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

TEST_CASE("Invalid bytes", "[abc]") {
    const auto comment = header + "C % \xc3\xa9";
    for (const char byte : {'\0', '\xff'}) {
        CAPTURE(static_cast<unsigned char>(byte));
        rejects(comment + byte, "6:7: ABC [invalid-text]");
    }
    rejects("", "1:1: ABC [missing-key]");
    rejects("M:4/4\nK:C\nD", "2:1: ABC [missing-reference]");
}

TEST_CASE("ABC 2.1 reads the file charset before decoding text fields", "[abc]") {
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

    rejects(prefix + "I:abc-charset us-ascii\n% é\n" + header + "C", "3:3: ABC [invalid-text]");
    rejects(prefix + "% é\nI:abc-charset us-ascii\n" + header + "C", "2:3: ABC [invalid-text]");
    rejects(prefix + "I:abc-charset us-\n+:ascii\n% é\n" + header + "C", "4:3: ABC [invalid-text]");
}

TEST_CASE("ABC 2.1 diagnoses unsupported, changed, and misplaced charset declarations", "[abc]") {
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

    const auto legacy = abc::read("%abc-2.1\nI:abc-charset iso-8859-10\nX:1\nT:ASCII\nK:C\nC");
    REQUIRE(legacy.warnings.size() == 1);
    CHECK(legacy.warnings.front().starts_with("<string>:2:1: ABC [unsupported-charset]"));
    CHECK(legacy.value.note_num() == 1);
}

TEST_CASE("Charset diagnostics agree across instruction spellings", "[abc]") {
    for (const std::string_view command :
         {"I:abc-charset us-ascii", "%%abc-charset us-ascii", "I:abc-charset us-\n+:ascii"}) {
        CAPTURE(command);
        const auto source = fmt::format("%abc-2.1\n{}\n% \xff\n{}C", command, titled_header);
        CHECK_THROWS_MATCHES(
            abc::read(source),
            std::invalid_argument,
            Catch::Matchers::MessageMatches(
                Catch::Matchers::ContainsSubstring("ABC [invalid-text]: ABC text must be US-ASCII")
            )
        );
    }
}

TEST_CASE("A charset violation points to the first invalid byte", "[abc]") {
    const std::string preamble    = "%abc-2.1\nI:abc-charset us-ascii\n% ";
    const auto        nul_first   = preamble + '\0' + "é\n" + titled_header + "C";
    const auto        ascii_first = preamble + "é" + '\0' + '\n' + titled_header + "C";
    rejects(nul_first, "3:3: ABC [invalid-text]: ABC text cannot contain a NUL byte");
    rejects(ascii_first, "3:3: ABC [invalid-text]: ABC text must be US-ASCII");
}

TEST_CASE("ABC version declarations are confined to headers", "[abc]") {
    const auto [score, warnings] = abc::read(
        strict_header + "I:abc-version 2.0\n%%abc-version 2.0\n[I:abc-version 2.0] C |]"
    );
    REQUIRE(warnings.size() == 3);
    CHECK(warnings[0].starts_with("<string>:7:1: ABC [header-only-instruction]"));
    CHECK(warnings[1].starts_with("<string>:8:1: ABC [header-only-instruction]"));
    CHECK(warnings[2].starts_with("<string>:9:2: ABC [header-only-instruction]"));
    CHECK(score == abc::read(strict_header + "C |]").value);
}
}   // namespace

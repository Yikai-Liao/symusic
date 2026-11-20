#include <algorithm>
#include <array>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_exception.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>
#include <fmt/format.h>
#include <minimidi/MiniMidi.hpp>

#include "symusic/conversion.h"
#include "symusic/io/abc.h"
#include "symusic/io/midi.h"
#include "symusic/ops.h"
#include "symusic/soa.h"

namespace {
using namespace symusic;
const std::string          header         = "X:1\nT:\nM:4/4\nL:1/4\nK:C\n";
constexpr std::string_view stress_profile = "%%MIDI ptstress 4 112 1.4 75 0.6 102 1.2 65 0.8\n";

vec<int> velocities(const Score<Tick>& score, size_t track = 0) {
    REQUIRE(track < score.tracks->size());
    vec<int> result;
    for (const auto& note : *score.tracks->at(track)->notes) result.push_back(note.velocity);
    return result;
}

TEST_CASE("Stress models shape timing or articulation", "[abc]") {
    for (const auto model : {1, 2}) {
        CAPTURE(model);
        const auto [score, warnings] = abc::read(
            fmt::format("{}%%MIDI stressmodel {}\n{}CDEF |]", header, model, stress_profile)
        );
        REQUIRE(warnings.empty());
        REQUIRE(score.tracks->size() == 1);
        CHECK(velocities(score) == vec<int>{112, 75, 102, 65});
        const auto& notes = *score.tracks->front()->notes;
        REQUIRE(notes.size() == 4);
        vec<i32> times;
        vec<i32> lengths;
        for (const auto& note : notes) {
            times.push_back(note.time);
            lengths.push_back(note.duration);
        }
        if (model == 1) {
            CHECK(times == vec<i32>{1, 481, 961, 1441});
            CHECK(lengths == vec<i32>{479, 205, 411, 277});
        } else {
            CHECK(times == vec<i32>{1, 673, 961, 1537});
            CHECK(lengths == vec<i32>{671, 287, 575, 383});
        }
        const auto written = abc::write(score);
        CHECK(*abc::read(written.value).value.tracks->front()->notes == notes);
    }
}

TEST_CASE("Model 1 samples each trill attack on the written clock", "[abc]") {
    const auto [score, warnings]
        = abc::read(fmt::format("{}%%MIDI stressmodel 1\n{}C TD2 E |]", header, stress_profile));
    REQUIRE(warnings.empty());
    const auto& notes = *score.tracks->front()->notes;
    REQUIRE(notes.size() == 18);
    for (int i = 0; i < 16; ++i) {
        CAPTURE(i);
        const auto pitch    = i % 2 ? i8{62} : i8{64};
        const auto velocity = i < 8 ? i8{75} : i8{102};
        CHECK(notes[i + 1] == Note<Tick>{481 + 60 * i, i < 8 ? 24 : 50, pitch, velocity});
    }
    CHECK(notes[17] == Note<Tick>{1441, 277, 64, 65});
}

TEST_CASE("Expanded ornaments span the complete tie", "[abc]") {
    for (const auto& [mark, pattern, count] : {
             std::tuple{"R", vec<i8>{62, 64, 62, 60, 62}, 5},
             {"!trill!", vec<i8>{64, 62}, 16},
         }) {
        CAPTURE(mark);
        const auto [score, warnings] = abc::read(header + "C " + mark + "D- D F |]");
        REQUIRE(warnings.empty());
        REQUIRE(score.tracks->size() == 1);
        const auto& notes = *score.tracks->front()->notes;
        REQUIRE(notes.size() == static_cast<size_t>(count) + 2);
        CHECK(notes.front() == Note<Tick>{1, 479, 60, 105});
        const int step = 960 / count;
        for (int i = 0; i < count; ++i) {
            CAPTURE(i);
            CHECK(
                notes[i + 1]
                == Note<Tick>{
                    481 + i * step,
                    step - 1,
                    pattern[i % pattern.size()],
                    i * step == 480 ? i8{95} : i8{80}
                }
            );
        }
        CHECK(notes.back() == Note<Tick>{1441, 479, 65, 80});
    }
}

TEST_CASE("A tied roll keeps its route across a bar and chord", "[abc]") {
    const auto [score, warnings]
        = abc::read("%abc-2.1\n" + header + "C3 RD- | [I:MIDI program 41] [FD] E F G |]");
    REQUIRE(warnings.empty());
    REQUIRE(score.tracks->size() == 2);
    const auto& opening = *score.tracks->front();
    const auto& changed = *score.tracks->back();
    CHECK(opening.program == 0);
    CHECK(changed.program == 41);
    const auto& roll = *opening.notes;
    REQUIRE(roll.size() == 6);
    const std::array<i8, 5> pitches{62, 64, 62, 60, 62};
    for (auto pulse = pitches.begin(); pulse != pitches.end(); ++pulse) {
        const auto index = pulse - pitches.begin();
        CHECK(roll.at(index + 1).time == 1441 + 192 * index);
        CHECK(roll.at(index + 1).duration == 191);
        CHECK(roll.at(index + 1).pitch == *pulse);
    }
    const auto& later = *changed.notes;
    REQUIRE(later.size() == 4);
    CHECK(later.front().time == 1921);
    CHECK(later.front().pitch == 65);
    CHECK(later.at(1).time == 2401);
    CHECK(later.at(1).pitch == 64);
}

TEST_CASE("A tied roll provides legacy pulse and ABC 2.1 note lyric slots", "[abc]") {
    const auto [legacy, legacy_warnings] = abc::read(header + "RD- D | A2 |]\nw: one two | last");
    REQUIRE(legacy_warnings.empty());
    REQUIRE(legacy.tracks->size() == 1);
    CHECK(
        *legacy.tracks->front()->lyrics
        == pyvec<TextMeta<Tick>>{{1, "one"}, {193, "two"}, {961, "last"}}
    );

    const auto [modern, modern_warnings]
        = abc::read("%abc-2.1\n" + header + "RD- D | A2 |]\nw: one two | last");
    REQUIRE(modern_warnings.empty());
    REQUIRE(modern.tracks->size() == 1);
    CHECK(
        *modern.tracks->front()->lyrics
        == pyvec<TextMeta<Tick>>{{1, "one"}, {481, "two"}, {961, "last"}}
    );
}

TEST_CASE("Model 1 uses the duration of the whole tied note for stress velocity", "[abc]") {
    const auto opening = fmt::format("{}%%MIDI stressmodel 1\n{}", header, stress_profile);
    for (const auto [phrase, duration, gain] : {
             std::tuple{"C F- F G |]", 959, 75},
             {"C F- F- F |]", 1439, 80},
             {"C F2- | F2 G A |]", 1919, 80},
             {"C F-G A |]", 479, 75},
         }) {
        CAPTURE(phrase);
        const auto [score, warnings] = abc::read(opening + phrase);
        REQUIRE(warnings.empty());
        const auto& notes = *score.tracks->front()->notes;
        REQUIRE(notes.size() >= 2);
        CHECK(notes[1].time == 481);
        CHECK(notes[1].pitch == 65);
        CHECK(notes[1].duration == duration);
        CHECK(notes[1].velocity == gain);
    }
}

TEST_CASE("Tied members outgrow stress independently of other chord members", "[abc]") {
    const auto [score, warnings] = abc::read(
        fmt::format(
            "{}%%MIDI stressmodel 1\n{}%%MIDI chordattack 0\nC [F-A]2 | F2 G A |]",
            header,
            stress_profile
        )
    );
    REQUIRE(warnings.empty());
    const auto& notes = *score.tracks->front()->notes;
    REQUIRE(notes.size() == 5);
    CHECK(notes[1] == Note<Tick>{481, 1919, 65, 80});
    CHECK(notes[2] == Note<Tick>{481, 618, 69, 75});
    CHECK(notes[3] == Note<Tick>{2401, 411, 67, 102});
}

TEST_CASE("Tied stress fallback uses the original one-shot volume", "[abc]") {
    for (const auto [mark, gain] : {
             std::pair{"vol 39", 39},
             {"vol 0", 0},
             {"volinc -78", 2},
             {"volinc 10", 90},
         }) {
        CAPTURE(mark);
        const auto [score, warnings] = abc::read(
            fmt::format(
                "{}%%MIDI stressmodel 1\n{}C [I:MIDI {}]F2- | F2 G A |]",
                header,
                stress_profile,
                mark
            )
        );
        REQUIRE(warnings.empty());
        const auto& notes = *score.tracks->front()->notes;
        REQUIRE(notes.size() == (gain ? 4 : 3));
        if (gain != 0) {
            CHECK(notes[1].velocity == gain);
            CHECK(notes[1].duration == 1919);
        }
        CHECK(notes[notes.size() - 2] == Note<Tick>{2401, 411, 67, 102});
    }
}

TEST_CASE("A tie that falls back to zero velocity does not export a note", "[abc]") {
    const auto [score, warnings] = abc::read(
        header
        + "%%MIDI stressmodel 1\n%%MIDI ptstress 4 112 1.4 100 .6 102 1.2 65 .8\n"
          "%%MIDI controlstring 11 50 60\nC [I:MIDI volinc -95]!shape!F2- | F2 G A |]"
    );
    REQUIRE(warnings.empty());
    const auto& track = *score.tracks->front();
    const auto& notes = *track.notes;
    REQUIRE(notes.size() == 3);
    CHECK(notes[1] == Note<Tick>{2401, 411, 67, 102});
    const auto& controls = *track.controls;
    REQUIRE(controls.size() == 3);
    CHECK(controls[0] == ControlChange<Tick>{481, 11, 50});
    CHECK(controls[1] == ControlChange<Tick>{1440, 11, 60});
    CHECK(controls[2] == ControlChange<Tick>{2400, 11, 127});
}

TEST_CASE("A roll crossing a stress boundary weights the overlapped segments", "[abc]") {
    const auto [score, warnings]
        = abc::read(fmt::format("{}%%MIDI stressmodel 1\n{}C RD2 E |]", header, stress_profile));
    REQUIRE(warnings.empty());
    const auto& notes = *score.tracks->front()->notes;
    REQUIRE(notes.size() == 7);
    CHECK(notes[1] == Note<Tick>{481, 81, 62, 75});
    CHECK(notes[2] == Note<Tick>{673, 81, 64, 75});
    CHECK(notes[3] == Note<Tick>{865, 122, 62, 75});
    CHECK(notes[4] == Note<Tick>{1057, 164, 60, 102});
    CHECK(notes[5] == Note<Tick>{1249, 164, 62, 102});
    CHECK(notes[6] == Note<Tick>{1441, 277, 64, 65});
}

TEST_CASE("Model 2 keeps trill and following note velocities on their hosts", "[abc]") {
    const auto [score, warnings] = abc::read(fmt::format("{}{}C TD EF |]", header, stress_profile));
    REQUIRE(warnings.empty());
    const auto& notes = *score.tracks->front()->notes;
    REQUIRE(notes.size() == 6);
    CHECK(notes[1] == Note<Tick>{673, 95, 64, 75});
    CHECK(notes[2] == Note<Tick>{769, 95, 62, 75});
    CHECK(notes[3] == Note<Tick>{865, 95, 64, 75});
    CHECK(notes[4] == Note<Tick>{961, 575, 64, 102});
    CHECK(notes[5] == Note<Tick>{1537, 383, 65, 65});
}

TEST_CASE("Trim and slurs apply to the attacks of an expanded trill", "[abc]") {
    for (const auto [phrase, legato_attacks] : {
             std::pair{"C TD EF |]", 0},
             {"(C TD) EF |]", 6},
             {"(C TD E) F |]", 8},
         }) {
        CAPTURE(phrase);
        const auto [score, warnings] = abc::read(header + "%%MIDI trim 1/16\n" + phrase);
        REQUIRE(warnings.empty());
        const auto& notes = *score.tracks->front()->notes;
        REQUIRE(notes.size() == 11);
        for (int i = 0; i < 8; ++i) {
            CHECK(notes[i + 1].time == 481 + 60 * i);
            CHECK(notes[i + 1].duration == (i < legato_attacks ? 59 : 29));
        }
        CHECK(notes[9].time == 961);
    }
}

TEST_CASE("MIDI expand suppresses stress trim but keeps its velocities", "[abc]") {
    const auto [score, warnings] = abc::read(
        fmt::format(
            "{}%%MIDI stressmodel 1\n{}%%MIDI expand 1/16\nC TD EF |]", header, stress_profile
        )
    );
    REQUIRE(warnings.empty());
    const auto& notes = *score.tracks->front()->notes;
    REQUIRE(notes.size() == 11);
    CHECK(notes[0] == Note<Tick>{1, 509, 60, 112});
    CHECK(notes[1] == Note<Tick>{481, 89, 64, 75});
    CHECK(notes[8] == Note<Tick>{901, 89, 62, 75});
    CHECK(notes[9] == Note<Tick>{961, 509, 64, 102});
}

TEST_CASE("Chord spread adjusts pending expanded releases", "[abc]") {
    for (const auto version : {"", "%abc-2.1\n"}) {
        for (const auto gap : {0, 4, 10}) {
            CAPTURE(version, gap);
            const auto [score, warnings] = abc::read(
                version + header
                + fmt::format("%%MIDI chordattack {}\n%%MIDI expand 1/24\nC [DF] E G | z4 |]", gap)
            );
            REQUIRE(warnings.empty());
            const auto&              notes = *score.tracks->front()->notes;
            vec<std::pair<i32, i32>> gates;
            for (const auto& note : notes) gates.emplace_back(note.time, note.end());
            CHECK(
                gates
                == vec<std::pair<i32, i32>>{
                    {1, 500 + gap},
                    {481, 980 - gap},
                    {481 + gap, 980 - gap},
                    {961, 1460},
                    {1441, 1940}
                }
            );
        }
    }
}

TEST_CASE("An expanded ornament still has pending releases at the next chord", "[abc]") {
    const auto [score, warnings]
        = abc::read(header + "%%MIDI chordattack 4\n%%MIDI expand 1/24\nTC [DF] E G | z8 |]");
    REQUIRE(warnings.empty());
    const auto& notes = *score.tracks->front()->notes;
    REQUIRE(notes.size() == 12);
    CHECK(notes.at(6).time == 361);
    CHECK(notes.at(6).end() == 440);
    CHECK(notes.at(7).time == 421);
    CHECK(notes.at(7).end() == 504);
    CHECK(notes.at(8).time == 481);
    CHECK(notes.at(9).time == 485);
    CHECK(notes.at(10).time == 961);
}

TEST_CASE("A tied roll enters the release queue before a later chord", "[abc]") {
    const auto [score, warnings]
        = abc::read(header + "%%MIDI chordattack 4\n%%MIDI expand 1/24\nRC-C [DF] E | z8 |]");
    REQUIRE(warnings.empty());
    const auto& notes = *score.tracks->front()->notes;
    REQUIRE(notes.size() == 8);
    CHECK(notes.at(3).time == 577);
    CHECK(notes.at(3).end() == 788);
    CHECK(notes.at(4).time == 769);
    CHECK(notes.at(4).end() == 984);
    CHECK(notes.at(5).time == 961);
    CHECK(notes.at(6).time == 965);
}

TEST_CASE("An expanded chord may release after the next written attack", "[abc]") {
    const auto [score, warnings] = abc::read(
        "%abc-2.1\n" + header + "%%MIDI chordattack 10\n%%MIDI expand 1/24\nC [DFA] {c}E G | z8 |]"
    );
    REQUIRE(warnings.empty());
    const auto& notes = *score.tracks->front()->notes;
    REQUIRE(notes.size() == 7);
    CHECK(notes.at(3).end() == 980);
    CHECK(notes.at(4).time == 961);
    CHECK(notes.at(5).time == 1081);
    CHECK(notes.at(6).time == 1441);
}

TEST_CASE("ABC import discards copyright fields and directives", "[abc]") {
    const auto plain = abc::read("%abc-2.1\nX:1\nT:Example\nK:C\nC |]");
    const auto [score, warnings] = abc::read(
        "%abc-2.1\nZ:abc-copyright &copy; Collection\n"
        "X:1\nT:Example\nZ:Transcribed by Alice\nZ:abc-copyright Bob\\u0025\nK:C\nC |]"
    );
    REQUIRE(warnings.empty());
    CHECK(score == plain.value);
    CHECK(abc::write(score).value.find("abc-copyright") == std::string::npos);
    const auto [legacy, legacy_warnings]
        = abc::read("X:1\nT:Example\nK:C\n%%abc-copyright Example\nC [I:abc-copyright x] |]");
    CHECK(legacy_warnings.empty());
    CHECK(legacy == abc::read("X:1\nT:Example\nK:C\nC |]").value);
}
TEST_CASE("Single-note SysEx tuning leaves the other chord pitches alone", "[abc]") {
    const auto [score, warnings]
        = abc::read(header + "%%MIDI snt 60 60.25\n[CE] D F G |]", "single-note.abc");
    REQUIRE(warnings.empty());
    CHECK(score == abc::read(header + "[CE] D F G |]").value);
    REQUIRE(score.tracks->size() == 1);
    const auto& track = *score.tracks->front();
    REQUIRE(track.notes->size() == 5);
    CHECK(track.notes->at(0).pitch == 60);
    CHECK(track.notes->at(1).pitch == 64);
    CHECK(track.pitch_bends->empty());
}
TEST_CASE("A cut uses grace-note articulation", "[abc]") {
    for (const auto& [directive, lead, host, following] : {
             std::tuple{"expand 1/24", 139, 379, 499},
             {"trim 1/8", 119, 299, 419},
         }) {
        CAPTURE(directive);
        const auto opening = "%abc-2.1\n" + header + fmt::format("%%MIDI {}\n", directive);
        const auto [score, warnings] = abc::read(opening + "C ~D EF | z4 |]");
        REQUIRE(warnings.empty());
        const auto& notes = *score.tracks->front()->notes;
        REQUIRE(notes.size() == 5);
        CHECK(notes[1] == Note<Tick>{481, lead, 64, 80});
        CHECK(notes[2] == Note<Tick>{601, host, 62, 65});
        CHECK(notes[3] == Note<Tick>{961, following, 64, 80});
    }
}

TEST_CASE("A proportional cut borrows from its host", "[abc]") {
    const auto opening = header + "%%MIDI grace 1/3\n";
    for (const auto& [music, lead, host, following] : {
             std::tuple{"C ~D2 E | z4 |]", 319, 639, 1441},
             {"C ~D>E F | z4 |]", 239, 479, 1201},
         }) {
        CAPTURE(music);
        const auto [score, warnings] = abc::read(opening + music);
        REQUIRE(warnings.empty());
        const auto& notes = *score.tracks->front()->notes;
        REQUIRE(notes.size() >= 4);
        CHECK(notes[1] == Note<Tick>{481, lead, 64, 80});
        CHECK(notes[2] == Note<Tick>{482 + lead, host, 62, 80});
        CHECK(notes[3].time == following);
    }
}

TEST_CASE("A three-unit roll cuts to both neighbors", "[abc]") {
    const std::string opening = "X:1\nT:\nM:4/4\nL:1/8\nK:C\n";
    for (const auto& [setup, music, written_out] : {
             std::tuple{"", "C ~D6/2 E2 F2", "C D {E}D {C}D E2 F2"},
             {"", "C ~D30000001/10000000 E2", "C {E}D30000001/10000000 E2"},
             {"", "C ~D3- D E2", "C D {E}D {C}D- D E2"},
             {"", "C [L:1/4]~D3/2 E F", "C [L:1/4]{E}D3/2 E F"},
             {"", "C (3:2:1~D3 E2 F2", "C {E}D2 E2 F2"},
             {"", "C H~D3 E2 F2", "C {E}D6 E2 F2"},
             {"", "C ~D2>E2 F2", "C {E}D3 E F2"},
             {"%%MIDI fermatafixed\n", "C H~D2 E2 F2", "C D {E}D {C}D E2 F2"},
             {"%%MIDI grace 1/3\n", "C {a}~D3 E2", "C {a}D {E}D {C}D E2"},
             {"%%MIDI grace 1/3\n",
              "C {a}[I:MIDI fermatafixed]H~D2 E2 F2",
              "C {a}[I:MIDI fermatafixed]D {E}D {C}D E2 F2"},
             {stress_profile.data(), "C ~D3 E2 F2", "C D {E}D {C}D E2 F2"},
         }) {
        CAPTURE(setup, music);
        const auto performed = abc::read(opening + setup + music + " |]");
        const auto written   = abc::read(opening + setup + written_out + " |]");
        REQUIRE(performed.warnings.empty());
        REQUIRE(written.warnings.empty());
        CHECK(*performed.value.tracks->front()->notes == *written.value.tracks->front()->notes);
    }
}

TEST_CASE("Broken rhythm scales the written roll host", "[abc]") {
    const std::string opening = "%abc-2.1\nX:1\nT:\nM:4/4\nL:1/8\nK:C\n";
    for (const auto& [music, written_out] : {
             std::pair{"C ~D3>E3 F", "C D3/2 {E}D3/2 {C}D3/2 E3/2 F"},
             {"C D3>~E3 F", "C D9/2 E/2 {F}E/2 {D}E/2 F"},
         }) {
        CAPTURE(music);
        const auto performed = abc::read(opening + music + " |]");
        const auto written   = abc::read(opening + written_out + " |]");
        REQUIRE(performed.warnings.empty());
        REQUIRE(written.warnings.empty());
        CHECK(*performed.value.tracks->front()->notes == *written.value.tracks->front()->notes);
    }
}

TEST_CASE("A three-unit roll places its written lyric on the first attack", "[abc]") {
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

TEST_CASE("Stress applies to the sounding interval of a staccato note", "[abc]") {
    for (const auto model : {1, 2}) {
        CAPTURE(model);
        const auto [score, warnings] = abc::read(
            fmt::format(
                "{}%%MIDI stressmodel {}\n{}C .D2 E | GABc |]", header, model, stress_profile
            )
        );
        REQUIRE(warnings.empty());
        const auto& notes = *score.tracks->front()->notes;
        REQUIRE(notes.size() == 7);
        CHECK(
            notes[1] == (model == 1 ? Note<Tick>{481, 205, 62, 75} : Note<Tick>{673, 287, 62, 75})
        );
        CHECK(notes[2].time == (model == 1 ? 1441 : 1537));
        CHECK(notes[3].time == 1921);
    }
}

TEST_CASE("Stress maps a dotted chord member's release independently", "[abc]") {
    for (const auto model : {1, 2}) {
        for (const auto [phrase, shortens_d] : {
                 std::pair{"C [D.F]2 E |]", false},
                 {"C .[DF]2 E |]", true},
             }) {
            CAPTURE(model, phrase);
            const auto [score, warnings] = abc::read(
                fmt::format(
                    "%abc-2.1\n{}%%MIDI stressmodel {}\n{}%%MIDI chordattack 0\n{}",
                    header,
                    model,
                    stress_profile,
                    phrase
                )
            );
            REQUIRE(warnings.empty());
            const auto& notes = *score.tracks->front()->notes;
            REQUIRE(notes.size() == 4);
            const auto start      = model == 1 ? 481 : 673;
            const auto short_gate = model == 1 ? 205 : 287;
            const auto full_gate  = model == 1 ? 618 : 863;
            CHECK(notes[1] == Note<Tick>{start, shortens_d ? short_gate : full_gate, 62, 75});
            CHECK(notes[2] == Note<Tick>{start, short_gate, 65, 75});
            CHECK(notes[3].time == (model == 1 ? 1441 : 1537));
        }
    }
}

TEST_CASE("A long staccato can fit the stress articulation window", "[abc]") {
    const auto [score, warnings]
        = abc::read(fmt::format("{}%%MIDI stressmodel 1\n{}.C4 | DEFG |]", header, stress_profile));
    REQUIRE(warnings.empty());
    const auto& notes = *score.tracks->front()->notes;
    REQUIRE(notes.size() == 5);
    CHECK(notes[0] == Note<Tick>{1, 685, 60, 112});
    CHECK(notes[1].time == 1921);
}

TEST_CASE("Grace notes borrow from the sounded portion of a staccato host", "[abc]") {
    const auto [score, warnings]
        = abc::read(fmt::format("{}{}C {{a}}.D2 E | GABc |]", header, stress_profile));
    REQUIRE(warnings.empty());
    const auto& notes = *score.tracks->front()->notes;
    REQUIRE(notes.size() == 8);
    CHECK(notes[1] == Note<Tick>{673, 71, 81, 75});
    CHECK(notes[2] == Note<Tick>{745, 215, 62, 75});
    CHECK(notes[3] == Note<Tick>{1537, 383, 64, 65});
}

TEST_CASE("Stress timing preserves grace attacks across a segment boundary", "[abc]") {
    const auto [score, warnings]
        = abc::read(fmt::format("{}{}C3/4 {{abc}}D5/4 EF |]", header, stress_profile));
    REQUIRE(warnings.empty());
    const auto& notes = *score.tracks->front()->notes;
    REQUIRE(notes.size() == 7);
    CHECK(notes[1] == Note<Tick>{505, 167, 81, 112});
    CHECK(notes[2] == Note<Tick>{673, 71, 83, 75});
    CHECK(notes[3] == Note<Tick>{745, 71, 72, 75});
    CHECK(notes[4] == Note<Tick>{817, 143, 62, 75});
    CHECK(notes[5].time == 961);
}

TEST_CASE("Fermatas extend the time mapped by either stress model", "[abc]") {
    for (const auto model : {1, 2}) {
        CAPTURE(model);
        const auto [score, warnings] = abc::read(
            fmt::format(
                "{}%%MIDI stressmodel {}\n{}C H[DF] E | G4 |]", header, model, stress_profile
            )
        );
        REQUIRE(warnings.empty());
        const auto& notes = *score.tracks->front()->notes;
        REQUIRE(notes.size() == 5);
        if (model == 1) {
            CHECK(notes[1] == Note<Tick>{481, 618, 62, 75});
            CHECK(notes[2] == Note<Tick>{491, 608, 65, 75});
            CHECK(notes[3] == Note<Tick>{1441, 277, 64, 65});
        } else {
            CHECK(notes[1] == Note<Tick>{673, 863, 62, 75});
            CHECK(notes[2] == Note<Tick>{683, 853, 65, 75});
            CHECK(notes[3] == Note<Tick>{1537, 383, 64, 65});
        }
        CHECK(notes[4].time == 1921);
    }
}

TEST_CASE("Stress maps a fixed fermata on a rest before the next attack", "[abc]") {
    const auto [score, warnings] = abc::read(
        fmt::format("{}{}%%MIDI fermatafixed\nC Hz2 D | E4 |]", header, stress_profile)
    );
    REQUIRE(warnings.empty());
    const auto& notes = *score.tracks->front()->notes;
    REQUIRE(notes.size() == 3);
    CHECK(notes[1] == Note<Tick>{1921, 671, 62, 95});
    CHECK(notes[2] == Note<Tick>{2593, 1919, 64, 112});
}

TEST_CASE("Proportional grace steals from the unwarped fermata duration", "[abc]") {
    const auto [score, warnings] = abc::read(
        fmt::format("{}{}%%MIDI grace 1/2\nC {{a}}HD E | F4 |]", header, stress_profile)
    );
    REQUIRE(warnings.empty());
    const auto& notes = *score.tracks->front()->notes;
    REQUIRE(notes.size() == 5);
    CHECK(notes[1] == Note<Tick>{673, 287, 81, 75});
    CHECK(notes[2] == Note<Tick>{961, 575, 62, 102});
    CHECK(notes[3] == Note<Tick>{1537, 383, 64, 65});
}

TEST_CASE("A fermata directive after a grace cannot consume the host", "[abc]") {
    const auto [score, warnings] = abc::read(
        fmt::format(
            "{}{}%%MIDI grace 3/4\n{{a}}[I:MIDI fermatafixed] HC3 | D4 |]", header, stress_profile
        )
    );
    REQUIRE(warnings.empty());
    const auto& notes = *score.tracks->front()->notes;
    REQUIRE(notes.size() == 3);
    CHECK(notes[0] == Note<Tick>{1, 1103, 81, 112});
    CHECK(notes[1] == Note<Tick>{1105, 815, 60, 102});
    CHECK(notes[2].time == 1921);
}

TEST_CASE("Stress timing preserves the order of grace and harp attacks", "[abc]") {
    const auto [score, warnings] = abc::read(
        header
        + "%%MIDI harpmode 1\n%%MIDI grace 3/4\n"
          "%%MIDI ptstress 4 112 2 75 .5 102 1 65 .5\nC3/4 {c}~D EF |]"
    );
    REQUIRE(warnings.empty());
    const auto& notes = *score.tracks->front()->notes;
    REQUIRE(notes.size() == 7);
    CHECK(notes[1] == Note<Tick>{721, 179, 72, 112});
    CHECK(notes[2] == Note<Tick>{901, 59, 62, 112});
    CHECK(notes[3] == Note<Tick>{961, 59, 62, 75});
    CHECK(notes[4] == Note<Tick>{1021, 119, 62, 75});
    CHECK(notes[5].time == 1141);
}

TEST_CASE("A stress change between grace and host keeps the grace profile", "[abc]") {
    const auto [score, warnings] = abc::read(
        fmt::format("{}{}{{c}}[I:MIDI ptstress 2 64 1 61 1] CDEF |]", header, stress_profile)
    );
    REQUIRE(warnings.empty());
    const auto& notes = *score.tracks->front()->notes;
    REQUIRE(notes.size() == 5);
    CHECK(notes[0] == Note<Tick>{1, 167, 72, 112});
    CHECK(notes[1] == Note<Tick>{169, 359, 60, 64});
    CHECK(notes[2].time == 529);
}

TEST_CASE("Stress timing survives beataccents and repeat jumps", "[abc]") {
    const auto [score, warnings] = abc::read(
        fmt::format("{}{}|: CDEF | [I:MIDI beataccents] GABc :|", header, stress_profile)
    );
    REQUIRE(warnings.empty());
    CHECK(
        velocities(score)
        == vec<int>{112, 75, 102, 65, 105, 80, 95, 80, 105, 80, 95, 80, 105, 80, 95, 80}
    );
    const auto& notes = *score.tracks->front()->notes;
    REQUIRE(notes.size() == 16);
    auto next = notes.begin();
    for (const auto bar : {0, 1920, 3840, 5760}) {
        for (const auto pulse : {1, 673, 961, 1537}) CHECK((next++)->time == bar + pulse);
    }
}

TEST_CASE("Stress settings change forward in the written tune", "[abc]") {
    const auto [score, warnings] = abc::read(
        fmt::format(
            "{}CD\n{}EF |\n%%MIDI ptstress 4 63 .5 84 1.5 91 .75 120 1.25\n"
            "GABc | [I:MIDI stressmodel 1] cBAG | [I:MIDI stressmodel 2] FEDC |]",
            header,
            stress_profile
        )
    );
    REQUIRE(warnings.empty());
    CHECK(
        velocities(score)
        == vec<int>{112, 75, 102, 65, 63, 84, 91, 120, 63, 84, 91, 120, 63, 84, 91, 120}
    );
    vec<i32> times;
    for (const auto& note : *score.tracks->front()->notes) times.push_back(note.time);
    CHECK(
        times
        == vec<i32>{
            1,
            673,
            961,
            1537,
            1921,
            2161,
            2881,
            3241,
            3841,
            4321,
            4801,
            5281,
            5761,
            6001,
            6721,
            7081
        }
    );
}

TEST_CASE("Stress profiles follow source order across voices", "[abc]") {
    const auto [score, warnings] = abc::read(
        fmt::format(
            "{}{}[V:1] CDEF |]\n%%MIDI ptstress 4 63 .5 84 1.5 91 .75 120 1.25\n"
            "[V:2] GABc |]",
            header,
            stress_profile
        )
    );
    REQUIRE(warnings.empty());
    REQUIRE(score.tracks->size() == 2);
    CHECK(velocities(score, 0) == vec<int>{112, 75, 102, 65});
    CHECK(velocities(score, 1) == vec<int>{63, 84, 91, 120});
    CHECK(score.tracks->at(0)->notes->at(1).time == 673);
    CHECK(score.tracks->at(1)->notes->at(1).time == 241);
}

TEST_CASE("Stress articulation weights the actual fractional segment", "[abc]") {
    const auto [score, warnings] = abc::read(
        header
        + "%%MIDI stressmodel 1\n%%MIDI ptstress 4 110 1.5 80 .5 100 1.25 90 .75\n"
          "C7/4 D E/4 F |]"
    );
    REQUIRE(warnings.empty());
    const auto& notes = *score.tracks->front()->notes;
    REQUIRE(notes.size() == 4);
    CHECK(notes[1] == Note<Tick>{841, 339, 62, 80});
    CHECK(notes[2].time == 1321);
}

TEST_CASE("Long notes bypass stress articulation", "[abc]") {
    const auto [score, warnings]
        = abc::read(fmt::format("{}%%MIDI stressmodel 1\n{}C3 D |]", header, stress_profile));
    REQUIRE(warnings.empty());
    const auto& notes = *score.tracks->front()->notes;
    REQUIRE(notes.size() == 2);
    CHECK(notes[0] == Note<Tick>{1, 1439, 60, 105});
    CHECK(notes[1] == Note<Tick>{1441, 277, 62, 65});
}

TEST_CASE("Timing profiles normalize all segments to the measure", "[abc]") {
    const auto [score, warnings]
        = abc::read(header + "%%MIDI ptstress 2 110 2 70 .5\nCDEF | G4 |]");
    REQUIRE(warnings.empty());
    const auto& notes = *score.tracks->front()->notes;
    REQUIRE(notes.size() == 5);
    CHECK(notes[2] == Note<Tick>{1537, 191, 64, 70});
    CHECK(notes[3] == Note<Tick>{1729, 191, 65, 70});
    CHECK(notes[4].time == 1921);
}

TEST_CASE("Stress timing uses the current meter", "[abc]") {
    const auto [score, warnings]
        = abc::read(fmt::format("{}{}CDEF | [M:3/4] GAB | c3 |]", header, stress_profile));
    REQUIRE(warnings.empty());
    const auto& notes = *score.tracks->front()->notes;
    REQUIRE(notes.size() == 8);
    CHECK(notes[4].time == 1921);
    CHECK(notes[5].time == 2497);
    CHECK(notes[6].time == 2929);
    CHECK(notes[7].time == 3361);
}

TEST_CASE("Invalid stress directives leave the last profile active", "[abc]") {
    const auto [score, warnings] = abc::read(
        fmt::format(
            "{}{}%%MIDI ptstress 4 112 1 75 1\n%%MIDI ptstress 1 150 1\n"
            "%%MIDI ptstress 2 30 0 60 0\n%%MIDI ptstress missing.txt\n"
            "%%MIDI ptstress 999999999999 112 1\n%%MIDI ptstress 1 112 inf\n"
            "%%MIDI ptstress 1 112 1 trailing\n"
            "%%MIDI stressmodel 3\nCDEF |]",
            header,
            stress_profile
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
    CHECK(velocities(score) == vec<int>{112, 75, 102, 65});
}

TEST_CASE("A failed stress alias does not override beataccents", "[abc]") {
    const auto [score, warnings] = abc::read(
        header
        + "%%MIDIdef good ptstress 4 112 1.4 75 .6 102 1.2 65 .8\n"
          "%%MIDIdef regular beataccents\n%%MIDIdef bad ptstress 0\n"
          "%%MIDIx good regular bad\nCDEF |]"
    );
    REQUIRE(warnings.size() == 1);
    CHECK(warnings.front().find("[invalid-stress-profile]") != std::string::npos);
    CHECK(velocities(score) == vec<int>{105, 80, 95, 80});
    CHECK(score.tracks->front()->notes->at(1).time == 673);
}

TEST_CASE("A stress file is read relative to its tune", "[abc]") {
    const auto [score, warnings] = abc::read_file("testcases/abc_headers/stress.abc");
    REQUIRE(warnings.size() == 1);
    CHECK(warnings.front().find("[stress-file]") != std::string::npos);
    CHECK(velocities(score) == vec<int>{112, 75, 102, 65});
    CHECK(score.tracks->front()->notes->at(1).time == 673);
}

TEST_CASE("Random chord attack staggers members without moving releases", "[abc]") {
    const auto source = header
                        + "%%MIDI chordattack 4\n%%MIDI randomchordattack 10\n"
                          "|: [CEG] [DFA] :| [I:MIDI randomchordattack 0] [CEG] D |]";
    const auto played = abc::read(source);
    REQUIRE(played.warnings.empty());
    REQUIRE(played.value.tracks->size() == 1);
    const auto& notes = *played.value.tracks->front()->notes;
    REQUIRE(notes.size() == 16);
    vec<int> gaps;
    bool     varied_within_chord = false;
    auto     next                = notes.begin();
    for (const auto start : {0, 480, 960, 1440}) {
        const auto& first  = *next++;
        const auto& middle = *next++;
        const auto& last   = *next++;
        CHECK(first.time == start + 1);
        CHECK(first.end() == start + 480);
        CHECK(middle.end() == first.end());
        CHECK(last.end() == first.end());
        const auto left  = middle.time - first.time;
        const auto right = last.time - middle.time;
        for (const auto gap : {left, right}) {
            CHECK(gap >= 0);
            CHECK(gap < 10);
            gaps.push_back(gap);
        }
        varied_within_chord |= left != right;
    }
    CHECK(varied_within_chord);
    CHECK_FALSE(std::equal(gaps.begin(), gaps.begin() + 4, gaps.begin() + 4));
    for (const auto time : {1921, 1925, 1929, 2401}) CHECK((next++)->time == time);
    CHECK(*abc::read(source).value.tracks->front()->notes == notes);
}

TEST_CASE("Arpeggio triples only the next chord's random attack range", "[abc]") {
    const auto opening = header + "%%MIDI randomchordattack 10\n";
    const auto plain   = abc::read(opening + "[CEG] [DFA] G |]");
    const auto marked  = abc::read(opening + "!arpeggio![CEG] [DFA] G |]");
    const auto tripled = abc::read(
        header + "%%MIDI randomchordattack 30\n[CEG] [I:MIDI randomchordattack 10] [DFA] G |]"
    );
    REQUIRE(plain.warnings.empty());
    REQUIRE(marked.warnings.empty());
    REQUIRE(tripled.warnings.empty());
    const auto& ordinary    = *plain.value.tracks->front()->notes;
    const auto& arpeggiated = *marked.value.tracks->front()->notes;
    REQUIRE(ordinary.size() == 7);
    REQUIRE(arpeggiated.size() == ordinary.size());
    CHECK(arpeggiated[0].time == ordinary[0].time);
    CHECK(arpeggiated == *tripled.value.tracks->front()->notes);
    CHECK(arpeggiated != ordinary);
    for (size_t i = 1; i < 3; ++i) {
        const auto gap = arpeggiated[i].time - arpeggiated[i - 1].time;
        CHECK(gap >= 0);
        CHECK(gap < 30);
        CHECK(arpeggiated[i].end() == ordinary[i].end());
    }
    CHECK(std::equal(arpeggiated.begin() + 3, arpeggiated.end(), ordinary.begin() + 3));
}

TEST_CASE("ABC 2.1 note ornaments attached to layout do not reach a later note", "[abc]") {
    const auto opening = "%abc-2.1\n" + header + "%%MIDI chordattack 4\n";
    for (const auto& [marked, plain] : {
             std::pair{"C> .y2 D E |]", "C>D E |]"},
             {"C !fermata! y D E |]", "C D E |]"},
             {"C !arpeggio! y [DFA] |]", "C [DFA] |]"},
             {"C !arpeggio! | [DFA] |]", "C | [DFA] |]"},
             {"!arpeggio! C [DFA] |]", "C [DFA] |]"},
             {"C !ff! y D E |]", "C !ff! D E |]"},
         }) {
        CAPTURE(marked);
        const auto actual   = abc::read(opening + marked);
        const auto expected = abc::read(opening + plain);
        REQUIRE(actual.warnings.empty());
        REQUIRE(expected.warnings.empty());
        CHECK(actual.value == expected.value);
    }

    const auto [marked, warnings] = abc::read(opening + "C !arpeggio![DFA] |]");
    REQUIRE(warnings.empty());
    const auto& notes = *marked.tracks->front()->notes;
    REQUIRE(notes.size() == 4);
    CHECK(notes[1].time == 481);
    CHECK(notes[2].time == 493);
    CHECK(notes[3].time == 505);
}

TEST_CASE("Chord ties do not count as new staggered attacks", "[abc]") {
    const auto [score, warnings] = abc::read(header + "%%MIDI chordattack 4\n[CEG-] [FAG] D E |]");
    REQUIRE(warnings.empty());
    const auto&              notes = *score.tracks->front()->notes;
    vec<std::pair<int, int>> attacks;
    for (const auto& note : notes) attacks.emplace_back(note.pitch, note.time);
    CHECK(
        attacks
        == vec<std::pair<int, int>>{
            {60, 1}, {64, 5}, {67, 5}, {65, 481}, {69, 485}, {62, 961}, {64, 1441}
        }
    );
    CHECK(notes[2].end() == 960);
}

TEST_CASE("Each chord route determines whether its pitch is transposed", "[abc]") {
    const auto [score, warnings] = abc::read(
        header
        + "%%MIDI transpose 5\n%%MIDI channel 10\n%%MIDI makechordchannels 1\n"
          "[CE] D [FGA] E |]"
    );
    REQUIRE(warnings.empty());
    REQUIRE(score.tracks->size() == 2);
    using Row = std::tuple<i32, i32, int>;
    for (const auto& track : *score.tracks) {
        vec<Row> notes;
        for (const auto& note : *track->notes)
            notes.emplace_back(note.time, note.duration, note.pitch);
        if (track->is_drum)
            CHECK(
                notes
                == vec<Row>{
                    {1, 479, 60}, {481, 479, 62}, {961, 479, 65}, {981, 459, 69}, {1441, 479, 64}
                }
            );
        else
            CHECK(notes == vec<Row>{{11, 469, 69}, {971, 469, 72}});
    }
}

TEST_CASE("Repeat entry restores an allocated MIDI channel for controls", "[abc]") {
    const auto [score, warnings] = abc::read(
        header
        + "%%MIDI transpose 5\nA A A [I:MIDI channel 10] "
          "[I:MIDI makechordchannels 1] [CE] |: [I:MIDI control 7 42] "
          "D [I:MIDI channel 3] E F G :|"
    );
    REQUIRE(warnings.empty());
    REQUIRE(score.tracks->size() == 3);
    const auto extra = std::ranges::find_if(*score.tracks, [](const auto& track) {
        return !track->controls->empty();
    });
    REQUIRE(extra != score.tracks->end());
    const auto& track = **extra;
    CHECK_FALSE(track.is_drum);
    REQUIRE(track.notes->size() == 1);
    CHECK(track.notes->front().pitch == 69);
    CHECK(*track.controls == pyvec<ControlChange<Tick>>{{1920, 7, 42}, {3840, 7, 42}});
}

TEST_CASE("Each chord-channel allocation keeps the previous channels distinct", "[abc]") {
    const auto [score, warnings] = abc::read(
        header
        + "%%MIDI program 17\n%%MIDI makechordchannels 1\n"
          "[CE] [I:MIDI makechordchannels 1] [FA] B c |]"
    );
    REQUIRE(warnings.empty());
    REQUIRE(score.tracks->size() == 3);
    const std::array expected = {vec<int>{60}, vec<int>{64, 65, 71, 72}, vec<int>{69}};
    for (size_t index = 0; index < expected.size(); ++index) {
        const auto& track = *score.tracks->at(index);
        CHECK(track.program == 17);
        vec<int> pitches;
        for (const auto& note : *track.notes) pitches.push_back(note.pitch);
        CHECK(pitches == expected[index]);
    }
}

TEST_CASE("Explicit channel and program commands address an allocated chord channel", "[abc]") {
    const auto [score, warnings] = abc::read(
        header
        + "%%MIDI makechordchannels 1\n[CE] [I:MIDI channel 2] "
          "[I:MIDI control 7 42] [I:MIDI program 2 17] [FA] B c |]"
    );
    REQUIRE(warnings.empty());
    REQUIRE(score.tracks->size() == 3);
    const auto& original = *score.tracks->at(1);
    CHECK(original.program == 0);
    REQUIRE(original.notes->size() == 1);
    CHECK(original.notes->front().pitch == 64);
    CHECK(*original.controls == pyvec<ControlChange<Tick>>{{480, 7, 42}});
    const auto& changed = *score.tracks->at(2);
    CHECK(changed.program == 17);
    REQUIRE(changed.notes->size() == 1);
    CHECK(changed.notes->front().pitch == 69);
    CHECK(changed.notes->front().time == 491);
}

TEST_CASE("The chord channel allocator skips percussion and detects exhaustion", "[abc]") {
    const auto [score, warnings] = abc::read(
        header
        + "%%MIDI makechordchannels 10\n%%MIDI makechordchannels 5\n"
          "[I:MIDI makechordchannels 0] [CE] D E F |]"
    );
    REQUIRE(warnings.empty());
    REQUIRE(score.tracks->size() == 2);
    REQUIRE(score.tracks->back()->notes->size() == 1);
    CHECK_FALSE(score.tracks->back()->is_drum);
    CHECK(score.tracks->back()->notes->front().pitch == 64);
    CHECK_THROWS_MATCHES(
        abc::read(header + "%%MIDI makechordchannels 9\n%%MIDI makechordchannels 6\n[CE] D E F |]"),
        std::invalid_argument,
        Catch::Matchers::MessageMatches(
            Catch::Matchers::ContainsSubstring("[midi-channels-exhausted]")
        )
    );
}

TEST_CASE("Chord channels are numbered across voices and backing tracks", "[abc]") {
    const auto opening = header + "[V:a] \"C\"[I:MIDI makechordchannels 1] [CE] D E F |]\n";
    const auto [score, warnings] = abc::read(
        opening
        + "[V:b] [I:MIDI makechordchannels 1] [CG] [I:MIDI program 2 17] "
          "[CF] [I:MIDI program 7 19] [DA] B |]"
    );
    REQUIRE(warnings.empty());
    REQUIRE(score.tracks->size() == 7);
    const auto& extra = *score.tracks->at(5);
    CHECK(extra.program == 0);
    REQUIRE(extra.notes->size() == 2);
    CHECK(extra.notes->at(0).pitch == 67);
    CHECK(extra.notes->at(1).pitch == 65);
    const auto& changed = *score.tracks->at(6);
    CHECK(changed.program == 19);
    REQUIRE(changed.notes->size() == 1);
    CHECK(changed.notes->front().pitch == 69);
    CHECK(changed.notes->front().time == 971);
}

TEST_CASE("Overlay chord channels follow track order rather than onset order", "[abc]") {
    const auto [score, warnings] = abc::read(
        header
        + "C4 & [I:MIDI makechordchannels 1] [CE] [I:MIDI program 4 19] [DF] G A | "
          "[I:MIDI makechordchannels 1] [CG] [I:MIDI program 2 17] [DA] B c |]"
    );
    REQUIRE(warnings.empty());
    REQUIRE(score.tracks->size() == 6);
    const auto& primary = *score.tracks->at(2);
    CHECK(primary.program == 17);
    REQUIRE(primary.notes->size() == 1);
    CHECK(primary.notes->front().pitch == 69);
    CHECK(primary.notes->front().time == 2411);
    const auto& secondary = *score.tracks->at(5);
    CHECK(secondary.program == 19);
    REQUIRE(secondary.notes->size() == 1);
    CHECK(secondary.notes->front().pitch == 65);
    CHECK(secondary.notes->front().time == 491);
}

TEST_CASE("A tied chord attack inherits its route until release", "[abc]") {
    const auto [score, warnings] = abc::read(
        header
        + "%%MIDI transpose 5\nA B [I:MIDI channel 10] "
          "[I:MIDI makechordchannels 1] [CE^F-] [FA_G] |]"
    );
    REQUIRE(warnings.empty());
    const auto extra = std::ranges::find_if(*score.tracks, [](const auto& track) {
        return !track->is_drum && !track->notes->empty() && track->notes->front().time > 1;
    });
    REQUIRE(extra != score.tracks->end());
    using Row = std::tuple<i32, i32, int>;
    vec<Row> notes;
    for (const auto& note : *(*extra)->notes)
        notes.emplace_back(note.time, note.duration, note.pitch);
    CHECK(notes == vec<Row>{{971, 469, 69}, {971, 949, 71}, {1451, 469, 71}});
}

TEST_CASE("A silent overlay lane receives every performed backing bar", "[abc]") {
    const std::string opening = header + "C4 & D4 & [I:MIDI bassprog 12] \"C\"E4 | ";
    using Bass                = vec<i32>;
    for (const std::string_view ending : {"F4 | G4 |]", "(& F4 | G4 & A4 | B4 &) |]"}) {
        CAPTURE(ending);
        const auto [score, warnings] = abc::read(opening + std::string(ending));
        REQUIRE(warnings.empty());
        const auto bass = std::ranges::find_if(*score.tracks, [](const auto& track) {
            return track->program == 12;
        });
        REQUIRE(bass != score.tracks->end());
        Bass onsets;
        for (const auto& note : *(*bass)->notes) {
            CHECK(note.pitch == 36);
            onsets.push_back(note.time);
        }
        CHECK(onsets == Bass{1, 961, 1921, 2881, 3841, 4801});
    }
}

TEST_CASE("Lowercase bass roots do not bypass chord quality validation", "[abc]") {
    const auto accepted = abc::read(header + "\"c7\" C [Q:1/4=96] D E F |]");
    REQUIRE(accepted.warnings.empty());
    REQUIRE(accepted.value.tracks->size() == 2);
    CHECK(accepted.value.tracks->back()->notes->front().pitch == 36);
    CHECK(accepted.value.tempos->back() == Tempo<Tick>{480, 625000});

    const auto rejected = abc::read(header + "\"cunknown\" C [Q:1/4=96] D E F |]");
    REQUIRE(rejected.warnings.size() == 1);
    CHECK_THAT(rejected.warnings.front(), Catch::Matchers::ContainsSubstring("[unknown-chord]"));
    CHECK(rejected.value.tracks->size() == 1);
    CHECK(rejected.value.tempos->back() == Tempo<Tick>{481, 625000});
}

TEST_CASE("Out-of-range chord pitches cannot wrap into the MIDI range", "[abc]") {
    const auto [score, warnings] = abc::read(
        header + "%%MIDI chordname wide -2147483640 0 4 2147483647\n\"Cwide/C\" CDEF |]"
    );
    REQUIRE(warnings.empty());
    // The extreme intervals affect the calculated voicing. They cannot wrap into audible notes.
    REQUIRE(score.tracks->size() == 2);
    const auto& notes = *score.tracks->back()->notes;
    REQUIRE(notes.size() == 4);
    CHECK(notes[0].pitch == 48);
    CHECK(notes[1].pitch == 52);
    CHECK(notes[2].pitch == 48);
    CHECK(notes[3].pitch == 52);
}

TEST_CASE("A full chord name registry still permits earlier definitions to change", "[abc]") {
    auto abc_text = header;
    for (int i = 0; i < 51; ++i) abc_text += fmt::format("%%MIDI chordname q{} 0\n", i);
    abc_text += "%%MIDI chordname q51 0\n%%MIDI chordname q50 2\n%%MIDI chordname q0 4\n"
                "\"Cq0\" CDEF | \"Cq50\" CDEF |]";
    const auto [score, warnings] = abc::read(abc_text);
    REQUIRE(warnings.size() == 2);
    for (const auto& warning : warnings)
        CHECK_THAT(warning, Catch::Matchers::ContainsSubstring("the chord name registry is full"));
    REQUIRE(score.tracks->size() == 3);
    const auto& notes = *score.tracks->back()->notes;
    REQUIRE(notes.size() == 4);
    CHECK(notes[0].pitch == 52);
    CHECK(notes[1].pitch == 52);
    CHECK(notes[2].pitch == 48);
    CHECK(notes[3].pitch == 48);
}

TEST_CASE("Short chord members keep their release and written attack order", "[abc]") {
    const auto [score, warnings]
        = abc::read("%abc-2.1\n" + header + "%%MIDI chordattack 90\n[CE.G]/4 A B c |]");
    REQUIRE(warnings.empty());
    const auto& notes = *score.tracks->front()->notes;
    REQUIRE(notes.size() == 6);
    CHECK(notes[0].time == 1);
    CHECK(notes[1].pitch == 64);
    CHECK(notes[2].pitch == 67);
    CHECK(notes[1].time <= notes[2].time);
    CHECK(notes[2].duration > 0);
    CHECK(notes[1].end() == 120);
    CHECK(notes[2].end() == 60);
    CHECK(notes[3].time == 121);
}

TEST_CASE("MIDI effects target bass and chord backing at their written times", "[abc]") {
    const auto [score, warnings] = abc::read(
        header
        + "%%MIDI bassprog 12\n%%MIDI chordprog 25\n"
          "%%MIDI control bass 7 41\n%%MIDI control chord 10 23\n"
          "%%MIDI portamento chord 13\n%%MIDI pitchbend bass 0 66\n"
          "\"C\"CDEF | [I:MIDI control bass 7 67] [I:MIDI noportamento chord] "
          "[I:MIDI pitchbend chord 1 65] \"G\"GABc |"
    );
    REQUIRE(warnings.empty());
    REQUIRE(score.tracks->size() == 3);
    const auto& lead  = *score.tracks->at(0);
    const auto& bass  = *score.tracks->at(1);
    const auto& chord = *score.tracks->at(2);
    CHECK(lead.controls->empty());
    CHECK(lead.pitch_bends->empty());
    CHECK(bass.program == 12);
    CHECK(chord.program == 25);
    CHECK(bass.notes->size() == 4);
    CHECK(chord.notes->size() == 12);
    CHECK(
        vec<ControlChange<Tick>>(bass.controls->begin(), bass.controls->end())
        == vec<ControlChange<Tick>>{{0, 7, 41}, {1920, 7, 67}}
    );
    CHECK(
        vec<ControlChange<Tick>>(chord.controls->begin(), chord.controls->end())
        == vec<ControlChange<Tick>>{{0, 10, 23}, {0, 65, 127}, {0, 5, 13}, {1920, 65, 0}}
    );
    REQUIRE(bass.pitch_bends->size() == 1);
    CHECK(bass.pitch_bends->front() == PitchBend<Tick>{0, 256});
    REQUIRE(chord.pitch_bends->size() == 1);
    CHECK(chord.pitch_bends->front() == PitchBend<Tick>{1920, 129});

    const auto exported = abc::write(score);
    const auto played   = abc::read(exported.value);
    REQUIRE(played.warnings.empty());
    REQUIRE(played.value.tracks->size() == 3);
    for (size_t i = 0; i < score.tracks->size(); ++i) {
        const auto& before = *score.tracks->at(i);
        const auto& after  = *played.value.tracks->at(i);
        CHECK(after.program == before.program);
        CHECK(*after.notes == *before.notes);
        CHECK(*after.controls == *before.controls);
        CHECK(*after.pitch_bends == *before.pitch_bends);
    }
}

TEST_CASE("Backing sustain follows performed time and stays with the opening program", "[abc]") {
    const auto [score, warnings] = abc::read(
        header
        + "%%MIDI bassprog 12\n"
          "\"C\"C2 [I:MIDI control bass 64 0] EF & "
          "[I:MIDI control bass 64 127] \"Am\"E4 | "
          "[I:MIDI control bass 64 127] G2 [I:MIDI bassprog 16] "
          "[I:MIDI control bass 64 0] A2 & [I:MIDI control bass 64 0] E4 |"
    );
    REQUIRE(warnings.empty());
    REQUIRE(score.tracks->size() == 7);
    const auto& original = *score.tracks->at(1);
    const auto& changed  = *score.tracks->at(2);
    const auto& overlay  = *score.tracks->at(5);
    CHECK(original.program == 12);
    CHECK(changed.program == 16);
    CHECK(overlay.program == 0);
    CHECK(
        vec<Pedal<Tick>>(original.pedals->begin(), original.pedals->end())
        == vec<Pedal<Tick>>{{1920, 960}}
    );
    CHECK(*overlay.pedals == pyvec<Pedal<Tick>>{{0, 1920}});
    CHECK(changed.pedals->empty());
    REQUIRE(changed.controls->size() == 1);
    CHECK(changed.controls->front() == ControlChange<Tick>{2880, 64, 0});
}

TEST_CASE("Backing MIDI effects validate the same data ranges as voice effects", "[abc]") {
    for (const auto* directive :
         {"control bass 128 50", "portamento chord 64", "pitchbend bass 0 128"}) {
        CAPTURE(directive);
        CHECK_THROWS_MATCHES(
            abc::read(header + "%%MIDI " + directive + "\nC"),
            std::invalid_argument,
            Catch::Matchers::MessageMatches(
                Catch::Matchers::ContainsSubstring("[invalid-midi-value]")
            )
        );
    }
}

TEST_CASE("ABC 2.1 starts at the same dynamics as an explicit mezzo-forte", "[abc]") {
    const auto opening     = "%abc-2.1\nX:1\nT:Dynamics\nM:4/4\nL:1/4\nK:C\n";
    const auto modern      = abc::read(std::string(opening) + "CDEF & GABC |");
    const auto explicit_mf = abc::read(std::string(opening) + "!mf!CDEF & !mf!GABC |");
    REQUIRE(modern.warnings.empty());
    CHECK(explicit_mf.warnings.empty());
    CHECK(modern.value == explicit_mf.value);
    CHECK(velocities(modern.value) == vec<int>{90, 80, 80, 80});
    CHECK(velocities(modern.value, 1) == vec<int>{90, 80, 80, 80});

    const auto legacy = abc::read(header + "CDEF |");
    CHECK(legacy.warnings.empty());
    CHECK(velocities(legacy.value) == vec<int>{105, 80, 95, 80});
    const auto override = abc::read(std::string(opening) + "%%MIDI beat 99 82 61 2\nCDEF |");
    CHECK(override.warnings.empty());
    CHECK(velocities(override.value) == vec<int>{99, 61, 82, 61});
}

TEST_CASE("MIDI beat zero uses the current meter numerator", "[abc]") {
    for (const auto& [meter, phrase, expected] : {
             std::tuple{"4/4", "CDEFGABc |", vec<int>{98, 61, 61, 61, 80, 61, 61, 61}},
             {"6/4", "CDEFGABc |", {98, 61, 61, 61, 61, 61, 80, 61}},
         }) {
        CAPTURE(meter);
        const auto [score, warnings] = abc::read(
            fmt::format("X:1\nM:{}\nL:1/4\nK:C\n%%MIDI beat 98 80 61 0\n{}", meter, phrase)
        );
        CHECK(warnings.empty());
        CHECK(velocities(score) == expected);
    }
}

TEST_CASE("A written meter resets beat spacing but keeps the levels", "[abc]") {
    const auto explicit_beat     = header + "%%MIDI beat 98 80 61 1\n";
    const auto [score, warnings] = abc::read(explicit_beat + "CDEF | [M:4/4] GABc |]");
    CHECK(warnings.empty());
    CHECK(velocities(score) == vec<int>{98, 80, 80, 80, 98, 61, 80, 61});

    const auto changed = abc::read(header + "!f!CDEF | [M:3/4] GAB |]");
    CHECK(changed.warnings.empty());
    CHECK(velocities(changed.value) == vec<int>{105, 95, 95, 95, 105, 80, 80});

    const auto pattern = abc::read(explicit_beat + "%%MIDI beatstring pm\nCDEF | [M:2/4] GA |]");
    CHECK(pattern.warnings.empty());
    CHECK(velocities(pattern.value) == vec<int>{61, 61, 80, 61, 61, 80});
}

TEST_CASE("A repeat restores notation without resetting beat spacing", "[abc]") {
    const auto [score, warnings] = abc::read(
        header + "%%MIDI beat 98 80 61 1\n|: CDEF | [M:3/4] [I:MIDI beat 99 81 62 1] GAB :|"
    );
    CHECK(warnings.empty());
    CHECK(velocities(score) == vec<int>{98, 80, 80, 80, 99, 81, 81, 99, 81, 81, 81, 99, 81, 81});
}

TEST_CASE("A delayed meter change keeps the grace's original beat spacing", "[abc]") {
    const auto [score, warnings] = abc::read(header + "%%MIDI beat 98 80 61 1\nC{a}[M:3/4]D E |]");
    CHECK(warnings.empty());
    CHECK(velocities(score) == vec<int>{98, 80, 61, 61});
}

TEST_CASE("Four-letter ABC dynamics share the three-letter levels and scope", "[abc]") {
    const auto opening  = "%abc-2.1\n" + header;
    const auto actual   = abc::read(opening + "CD | EF |\ns: !pppp! * | !ffff! *");
    const auto expected = abc::read(opening + "!ppp!CD | !fff!EF |");
    CHECK(actual.warnings.empty());
    CHECK(expected.warnings.empty());
    CHECK(actual.value == expected.value);
    CHECK(velocities(actual.value) == vec<int>{30, 20, 127, 125});
}

TEST_CASE("ABC accents emphasize the written attack without changing the dynamic", "[abc]") {
    const auto opening = "%abc-2.1\n" + header;
    for (const auto* mark : {"L", "!accent!", "!>!", "!emphasis!", "!sfz!"}) {
        CAPTURE(mark);
        const auto result = abc::read(opening + "C " + mark + "D E F |");
        CHECK(result.warnings.empty());
        CHECK(velocities(result.value) == vec<int>{90, 95, 80, 80});
    }
    const auto chord = abc::read(opening + "%%MIDI chordattack 0\n!accent![CE] [F!>!A] G");
    CHECK(chord.warnings.empty());
    CHECK(velocities(chord.value) == vec<int>{105, 105, 80, 95, 80});
    const auto detached = abc::read(opening + "C- !accent!C !accent!z E |");
    CHECK(detached.warnings.empty());
    CHECK(velocities(detached.value) == vec<int>{90, 80});
    const auto grace = abc::read(opening + "C {g}!accent!D E |");
    CHECK(grace.warnings.empty());
    CHECK(velocities(grace.value) == vec<int>{90, 80, 80, 80});
    const auto absolute = abc::read(opening + "!fff!C !accent!D [I:MIDI vol 44]!accent!E F |");
    CHECK(absolute.warnings.empty());
    CHECK(velocities(absolute.value) == vec<int>{127, 127, 44, 125});
    const auto legacy = abc::read(header + "C LD !accent!E [F!accent!A] |");
    CHECK(legacy.warnings.empty());
    CHECK(legacy.value == abc::read(header + "C D E [FA] |").value);
}

TEST_CASE("ABC voice instruments can target the current or a named voice", "[abc]") {
    // ABC 2.1 section 11.2 uses one-based instruments independently of programbase.
    const auto [score, warnings] = abc::read(
        "%abc-2.1\nX:1\nT:\nM:4/4\nL:1/4\n"
        "%%MIDI voice Bass instrument=59\n"
        "V:Lead\nV:Bass\nK:C\n"
        "%%MIDI programbase 0\n"
        "V:Lead\n%%MIDI voice instrument=74\nC\n"
        "[I:MIDI voice Bass instrument=128] D\n"
        "V:Bass\nE\n"
        "[I:MIDI voice Lead instrument=1] F\n"
        "V:Lead\nG\n"
    );
    REQUIRE(warnings.empty());
    REQUIRE(score.tracks->size() == 3);
    const auto& new_lead = *score.tracks->at(0);
    const auto& lead     = *score.tracks->at(1);
    const auto& bass     = *score.tracks->at(2);
    CHECK(lead.program == 73);
    REQUIRE(lead.notes->size() == 2);
    CHECK(lead.notes->at(0).pitch == 60);
    CHECK(lead.notes->at(1).pitch == 62);
    CHECK(new_lead.program == 0);
    REQUIRE(new_lead.notes->size() == 1);
    CHECK(new_lead.notes->front().pitch == 67);
    CHECK(new_lead.notes->front().time == 961);
    CHECK(bass.program == 127);
    REQUIRE(bass.notes->size() == 2);
    CHECK(bass.notes->at(0).pitch == 64);
    CHECK(bass.notes->at(0).time == 1);
    CHECK(bass.notes->at(1).pitch == 65);
}

TEST_CASE("ABC 2.1 chordprog uses General MIDI instrument numbers", "[abc]") {
    // ABC 2.1 section 11.2 gives chordprog 20 for Church Organ.
    const auto chord_program = [](const std::string& text) {
        const auto [score, warnings] = abc::read(text + "\"C\"CDEF |]");
        REQUIRE(warnings.empty());
        REQUIRE(score.tracks->size() == 3);
        REQUIRE(score.tracks->at(2)->notes->size() == 6);
        return score.tracks->at(2)->program;
    };
    const std::string strict = "%abc-2.1\n" + header;
    CHECK(chord_program(strict + "%%MIDI chordprog 20\n") == 19);
    CHECK(chord_program(strict + "[I:MIDI chordprog 128]") == 127);
    CHECK(chord_program(header + "%%MIDI chordprog 20\n") == 20);
    CHECK(chord_program(strict + "%%MIDI chordprog 20\n%%MIDI programbase 0\n") == 20);
    CHECK_THROWS_AS(abc::read(strict + "%%MIDI chordprog 0\n\"C\"CDEF |]"), std::invalid_argument);
}

TEST_CASE("A named MIDI voice instruction can precede the voice declarations", "[abc]") {
    const auto [score, warnings] = abc::read(
        "%abc-2.1\nX:1\nT:\nM:4/4\nL:1/4\n"
        "%%MIDI voice 12345678901234567890bass instrument=59\nK:C\n"
        "V:Lead\nC\nV:12345678901234567890other\nE\n"
    );
    REQUIRE(warnings.empty());
    REQUIRE(score.tracks->size() == 2);
    CHECK(score.tracks->at(0)->program == 0);
    CHECK(score.tracks->at(1)->program == 58);
}

TEST_CASE("MIDI voice banks survive ABC export and select before MIDI program change", "[abc]") {
    const auto [score, warnings] = abc::read(
        "%abc-2.1\n" + header
        + "[I:MIDI voice instrument=53 bank=131] C "
          "[I:MIDI voice instrument=53 bank=2] D [I:MIDI voice instrument=53] E"
    );
    REQUIRE(warnings.empty());
    REQUIRE(score.tracks->size() == 3);
    const auto check = [](const Score<Tick>& value) {
        REQUIRE(value.tracks->size() == 3);
        const auto& gm   = *value.tracks->at(0);
        const auto& low  = *value.tracks->at(1);
        const auto& high = *value.tracks->at(2);
        for (const auto* track : {&gm, &low, &high}) {
            CHECK(track->program == 52);
            REQUIRE(track->notes->size() == 1);
        }
        CHECK(gm.controls->empty());
        CHECK(gm.notes->front().time == 961);
        REQUIRE(low.controls->size() == 2);
        CHECK(low.controls->at(0) == ControlChange<Tick>{0, 0, 0});
        CHECK(low.controls->at(1) == ControlChange<Tick>{0, 32, 1});
        CHECK(low.notes->front().time == 481);
        REQUIRE(high.controls->size() == 2);
        CHECK(high.controls->at(0) == ControlChange<Tick>{0, 0, 1});
        CHECK(high.controls->at(1) == ControlChange<Tick>{0, 32, 2});
        CHECK(high.notes->front().time == 1);
    };
    check(score);
    const auto exported                     = abc::write(score);
    const auto [reparsed, ignored_warnings] = abc::read(exported.value);
    check(reparsed);

    const auto                     bytes = reparsed.dumps<DataFormat::MIDI>();
    const minimidi::MidiFileView<> midi(bytes);
    size_t                         banked_tracks = 0;
    for (const auto& track : midi) {
        size_t bank_controls = 0;
        bool   program_sent  = false;
        for (const auto& message : track) {
            if (message.type() == minimidi::MessageType::ProgramChange) {
                program_sent = true;
            } else if (message.type() == minimidi::MessageType::ControlChange) {
                const auto& control = message.cast<minimidi::ControlChange>();
                if (control.control_number() == 0 || control.control_number() == 32) {
                    CHECK_FALSE(program_sent);
                    ++bank_controls;
                }
            }
        }
        if (bank_controls != 0) {
            CHECK(bank_controls == 2);
            CHECK(program_sent);
            ++banked_tracks;
        }
    }
    CHECK(banked_tracks == 2);
}

TEST_CASE("MIDI voice mute suppresses all lanes without moving lyrics", "[abc]") {
    const auto [score, warnings] = abc::read(
        "%abc-2.1\n" + header
        + "V:Muted\nC\n%%MIDI voice mute\nD E & G A B |\nw: a b c d e f\n"
          "V:Audible\nF2\n"
    );
    REQUIRE(warnings.empty());
    size_t              audible = 0;
    vec<TextMeta<Tick>> lyrics;
    for (const auto& track : *score.tracks) {
        for (const auto& note : *track->notes) {
            CHECK(note.time == 1);
            CHECK((note.pitch == 60 || note.pitch == 65));
            ++audible;
        }
        lyrics.insert(lyrics.end(), track->lyrics->begin(), track->lyrics->end());
    }
    CHECK(audible == 2);
    REQUIRE(lyrics.size() == 6);
    CHECK(lyrics.at(1) == TextMeta<Tick>{481, "b"});
    CHECK(lyrics.at(5) == TextMeta<Tick>{961, "f"});
}

TEST_CASE("A bare voice selection puts later target commands inside the part", "[abc]") {
    const auto [score, warnings] = abc::read(
        "X:1\nT:Parts\nP:BA\nM:4/4\nL:1/4\nV:Lead name=\"Lead\"\nV:Bass name=\"Bass\"\nK:C\n"
        "P:A\n[V:Lead] C4 | [V:Bass] D4 |\nP:B\n"
        "[V:Bass][V:Lead][I:MIDI voice Bass mute] E4 | [V:Bass] F4 |]"
    );
    REQUIRE(warnings.empty());
    REQUIRE(score.tracks->size() == 1);
    const auto& lead = *score.tracks->front();
    REQUIRE(lead.name == "Lead");
    REQUIRE(lead.notes->size() == 2);
    CHECK(lead.notes->at(0).pitch == 64);
    CHECK(lead.notes->at(1).pitch == 60);
}

TEST_CASE("Invalid or unresolved MIDI voice instructions do not change playback", "[abc]") {
    const auto [score, warnings] = abc::read(
        "%abc-2.1\n" + header
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

TEST_CASE("Extended overlays start before their internal bar lines", "[abc]") {
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

TEST_CASE("Adjacent extended overlays align both new and idle lanes", "[abc]") {
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

TEST_CASE("An unused overlay lane cannot tie through another group", "[abc]") {
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

TEST_CASE("ABC export identifies its version and creator", "[abc]") {
    const auto [text, warnings] = abc::write(Score<Tick>(480));
    CHECK(warnings.empty());
    constexpr std::string_view prefix = "%abc-2.1\nX:1\nT:Untitled\nI:abc-creator symusic-";
    REQUIRE(text.starts_with(prefix));
    const auto end = text.find('\n', prefix.size());
    REQUIRE(end != std::string::npos);
    REQUIRE(end > prefix.size());
    CHECK(text[prefix.size()] >= '0');
    CHECK(text[prefix.size()] <= '9');
    CHECK(
        std::string_view(text).substr(end).starts_with(
            "\nI:linebreak <none>\nM:none\nL:1/4\nQ:1/4=120\nK:C\n"
        )
    );
    CHECK(abc::read(text).warnings.empty());
}

TEST_CASE("ABC export keeps free meter until the first supplied signature", "[abc]") {
    auto score             = abc::read(header + "[V:a] CDEF |]\n[V:b] G2 A2 |]").value;
    *score.time_signatures = {{960, 3, 8}};

    const auto [text, warnings] = abc::write(score);
    REQUIRE(warnings.size() == 1);
    CHECK(warnings.front().find("[midi-field-timing]") != std::string::npos);
    CHECK(text.find("\nI:linebreak <none>\nM:none\n") != std::string::npos);
    CHECK(text.find("[M:3/8]") != std::string::npos);

    const auto [loaded, read_warnings] = abc::read(text);
    REQUIRE(read_warnings.empty());
    REQUIRE(loaded.time_signatures->size() == 2);
    // Free meter reads as the 4/4 pulse that abc2midi writes.
    CHECK(loaded.time_signatures->front() == TimeSignature<Tick>{0, 4, 4});
    CHECK(loaded.time_signatures->back() == TimeSignature<Tick>{960, 3, 8});
    REQUIRE(loaded.tracks->size() == 2);
    for (size_t voice = 0; voice < loaded.tracks->size(); ++voice)
        CHECK(*loaded.tracks->at(voice)->notes == *score.tracks->at(voice)->notes);

    score.time_signatures->insert(score.time_signatures->begin(), {0, 2, 4});
    CHECK(abc::write(score).value.find("\nI:linebreak <none>\nM:2/4\n") != std::string::npos);
}

TEST_CASE("ABC export repeats music fields within each participating voice", "[abc]") {
    auto score = abc::read(header + "[V:a] C D |]\n[V:b] G4 |]\n[V:c] E |]").value;
    score.time_signatures->emplace_back(720, 3, 8);
    score.key_signatures->emplace_back(720, 1, 0);
    score.tempos->emplace_back(720, 625'000);
    score.time_signatures->emplace_back(2400, 2, 4);

    const auto [text, warnings] = abc::write(score);
    const auto second           = text.find("\nV:2\n");
    const auto third            = text.find("\nV:3\n");
    REQUIRE(second != std::string::npos);
    REQUIRE(third != std::string::npos);
    const std::string_view abc(text);
    const auto             first_voice  = abc.substr(0, second);
    const auto             second_voice = abc.substr(second, third - second);
    const auto             short_voice  = abc.substr(third);
    for (const std::string_view field : {"[M:3/8]", "[K:G]", "[Q:1/4=96]"}) {
        CAPTURE(field);
        CHECK(first_voice.find(field) != std::string_view::npos);
        CHECK(second_voice.find(field) != std::string_view::npos);
        CHECK(short_voice.find(field) == std::string_view::npos);
    }
    CHECK(first_voice.find("[M:2/4]") != std::string_view::npos);
    CHECK(second_voice.find("[M:2/4]") == std::string_view::npos);
    CHECK(short_voice.find("[M:2/4]") == std::string_view::npos);
    CHECK(std::ranges::any_of(warnings, [](const auto& warning) {
        return warning.find("[midi-field-release]") != std::string::npos;
    }));

    const auto [loaded, read_warnings] = abc::read(text);
    CHECK(read_warnings.empty());
    CHECK(loaded == score);
}

TEST_CASE("ABC tempo export preserves the full MIDI tempo range", "[abc]") {
    auto score    = abc::read(header + "CDEF |]").value;
    *score.tempos = {{0, 16'777'215}, {480, 7'689'274}, {960, 1}, {1440, 500'000}};

    const auto [text, warnings]        = abc::write(score);
    const auto [loaded, read_warnings] = abc::read(text);
    CHECK(warnings.empty());
    CHECK(read_warnings.empty());
    CHECK(*loaded.tempos == *score.tempos);
    REQUIRE(loaded.tracks->size() == 1);
    CHECK(*loaded.tracks->front()->notes == *score.tracks->front()->notes);
}

TEST_CASE("ABC export escapes title and voice text without changing its meaning", "[abc]") {
    auto              score     = abc::read(header + "C").value;
    const std::string name      = R"(Flute "echo" &copy; 50% \)";
    score.tracks->front()->name = name;

    const auto [text, warnings] = abc::write(score);
    CHECK(warnings.empty());
    constexpr std::string_view escaped = R"(Flute &quot;echo&quot; \&copy; 50\% \\)";
    CHECK(text.find(fmt::format("\nT:{}\n", escaped)) != std::string::npos);
    CHECK(text.find(fmt::format(" name=\"{}\"\n", escaped)) != std::string::npos);

    const auto [loaded, read_warnings] = abc::read(text);
    CHECK(read_warnings.empty());
    REQUIRE(loaded.tracks->size() == 1);
    CHECK(loaded.tracks->front()->name == name);
    CHECK(*loaded.tracks->front()->notes == *score.tracks->front()->notes);
}

TEST_CASE("ABC text accepts portable quote and Unicode spellings", "[abc]") {
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

TEST_CASE("ABC 2.1 text discards font switches and preserves literal dollars", "[abc]") {
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
    for (int i = 0; i < 32'768; ++i) font_run += "$1";
    const auto [restyled, style_warnings] = abc::read(opening + "C\nw: " + font_run + " first");
    CHECK(style_warnings.empty());
    REQUIRE(restyled.tracks->size() == 1);
    CHECK(*restyled.tracks->front()->lyrics == pyvec<TextMeta<Tick>>{{1, "first"}});

    const auto [legacy, legacy_warnings] = abc::read("X:1\nT:$1Flute$0 $$2\nK:C\nC\nw: cash$$");
    CHECK(legacy_warnings.empty());
    REQUIRE(legacy.tracks->size() == 1);
    CHECK(legacy.tracks->front()->name == "$1Flute$0 $$2");
    CHECK(*legacy.tracks->front()->lyrics == pyvec<TextMeta<Tick>>{{1, "cash$$"}});
}

TEST_CASE("ABC export escapes dollars in titles, voice names and lyrics", "[abc]") {
    auto        score = abc::read(header + "C").value;
    const auto& track = score.tracks->front();
    track->name       = "Lead $1 $$";
    track->lyrics->emplace_back(1, "$0 $$");
    const auto [text, warnings] = abc::write(score);
    CHECK(warnings.empty());
    CHECK(text.find("\nT:Lead $$1 $$$$\n") != std::string::npos);
    CHECK(text.find(" name=\"Lead $$1 $$$$\"\n") != std::string::npos);
    const auto [loaded, read_warnings] = abc::read(text);
    CHECK(read_warnings.empty());
    CHECK(loaded == score);
}

TEST_CASE("ABC 2.1 accents honor tune versions without renaming legacy tracks", "[abc]") {
    // The tune-local version applies even though T: must immediately follow X:.
    const auto [modern, warnings] = abc::read(
        "%abc\nX:1\nT:"
        R"(\"Osten \ua \OE \HO \'e)"
        "\nI:abc-version 2.1\nK:C\nC"
    );
    const auto entities
        = abc::read("%abc-2.1\nX:1\nT:&Ouml;sten &abreve; &OElig; \\u0150 &eacute;\nK:C\nC").value;
    CHECK(warnings.empty());
    REQUIRE(modern.tracks->size() == 1);
    REQUIRE(entities.tracks->size() == 1);
    const auto& name = modern.tracks->front()->name;
    CHECK(name == "\xC3\x96sten \xC4\x83 \xC5\x92 \xC5\x90 \xC3\xA9");
    CHECK(entities.tracks->front()->name == name);

    const auto [legacy, legacy_warnings] = abc::read(
        "%abc\nX:1\nT:Title\nL:1/4\nK:C\n"
        R"abc(V:one name="Flute \"echo\" &copy; \\u266F")abc"
        "\nC"
    );
    CHECK(legacy_warnings.empty());
    REQUIRE(legacy.tracks->size() == 1);
    CHECK(legacy.tracks->front()->name == R"(Flute "echo" &copy; \u266F)");
    const auto raw_title = abc::read("%abc\nX:1\nT:Flute &copy;\nK:C\nC").value;
    REQUIRE(raw_title.tracks->size() == 1);
    CHECK(raw_title.tracks->front()->name == "Flute &copy;");
}

TEST_CASE("ABC voice names preserve line breaks independently of titles", "[abc]") {
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

TEST_CASE("ABC export preserves text controls without writing them into a field", "[abc]") {
    auto        score = abc::read(header + "C").value;
    std::string name  = "Lead\r\n";
    name += '\0';
    name += "\tFlute";
    name += '\x7f';
    score.tracks->front()->name = name;

    const auto [text, warnings] = abc::write(score);
    REQUIRE(warnings.empty());
    CHECK(text.find_first_of(std::string_view("\0\r\t\x7f", 4)) == std::string::npos);
    constexpr std::string_view title = R"(T:Lead\u000d\u000a\u0000\u0009Flute\u007f)";
    CHECK(text.find(std::string(title) + '\n') != std::string::npos);
    CHECK(text.find(R"(name="Lead\u000d\n\u0000\u0009Flute\u007f")") != std::string::npos);

    const auto loaded = abc::read(text);
    REQUIRE(loaded.warnings.empty());
    REQUIRE(loaded.value.tracks->size() == 1);
    CHECK(loaded.value.tracks->front()->name == name);

    const auto from_title = abc::read("%abc-2.1\nX:1\n" + std::string(title) + "\nK:C\nC");
    REQUIRE(from_title.warnings.empty());
    REQUIRE(from_title.value.tracks->size() == 1);
    CHECK(from_title.value.tracks->front()->name == name);
}

TEST_CASE("ABC 2.1 export preserves written lyric slots and escapes", "[abc]") {
    auto  score = abc::read(
                      header
                      + "%%MIDI chordattack 0\n"
                        "C z D [EG] F G A B c d e f g a b c' d' e' f' g' a' b' c'' d''"
    )
                      .value;
    auto& track = *score.tracks->front();
    track.lyrics->emplace_back(1, "1st");
    track.lyrics->emplace_back(1441, "a-b *|_~$+ &copy; 50% \\");
    track.lyrics->emplace_back(1921, std::string(65, 'x'));
    track.lyrics->emplace_back(2401, std::string(60, 'y'));
    for (const auto tick : {5761, 6241, 6721, 7201, 7681, 8161, 8641})
        track.lyrics->emplace_back(tick, std::string(68, 'w'));
    track.lyrics->emplace_back(10561, "last syllable");

    const auto [text, warnings] = abc::write(score);
    CHECK(warnings.empty());
    // Sections 5.1 and 3.3: skips and continued fields do not consume extra notes.
    CHECK(text.find("\n+: ") != std::string::npos);
    const auto [loaded, read_warnings] = abc::read(text);
    CHECK(read_warnings.empty());
    REQUIRE(loaded.tracks->size() == 1);
    CHECK(*loaded.tracks->front()->lyrics == *track.lyrics);
    CHECK(*loaded.tracks->front()->notes == *track.notes);
}

TEST_CASE("ABC export inserts a tied continuation for lyrics within a note", "[abc]") {
    const auto score
        = abc::read("%abc-2.1\nX:1\nT:Lyrics\nL:1/4\nK:C\nC-C D |]\nw: held word last\n").value;
    REQUIRE(score.tracks->front()->notes->size() == 2);
    REQUIRE(score.tracks->front()->lyrics->size() == 3);

    const auto [text, warnings] = abc::write(score);
    REQUIRE(warnings.size() == 1);
    CHECK(warnings.front().find("[midi-tied-lyric]") != std::string::npos);
    const auto [loaded, read_warnings] = abc::read(text);
    CHECK(read_warnings.empty());
    REQUIRE(loaded.tracks->size() == 1);
    CHECK(*loaded.tracks->front()->lyrics == *score.tracks->front()->lyrics);
    CHECK(*loaded.tracks->front()->notes == *score.tracks->front()->notes);
}

TEST_CASE("ABC export warns when abc2midi can split a lyric", "[abc]") {
    auto  score  = abc::read(header + "C D E |]").value;
    auto& lyrics = *score.tracks->front()->lyrics;
    lyrics.emplace_back(1, "first");
    lyrics.emplace_back(481, std::string(197, 'a'));
    CHECK(abc::write(score).warnings.empty());

    lyrics.back().text += 'a';
    lyrics.emplace_back(961, "last");
    const auto [text, warnings] = abc::write(score);
    REQUIRE(warnings.size() == 1);
    CHECK_THAT(warnings.front(), Catch::Matchers::ContainsSubstring("[midi-lyric-width]"));
    CHECK_THAT(warnings.front(), Catch::Matchers::ContainsSubstring("tick 481"));
    const auto [loaded, read_warnings] = abc::read(text);
    CHECK(read_warnings.empty());
    CHECK(loaded == score);
}

TEST_CASE("ABC export preserves a same-pitch reattack", "[abc]") {
    for (const int duration : {1, 2, 240}) {
        CAPTURE(duration);
        auto  score = abc::read(header + "C").value;
        auto& notes = *score.tracks->front()->notes;
        notes       = {{1, duration, 60, 80}, {1, duration, 64, 80}, {1 + duration, 30, 60, 90}};
        const auto [text, warnings] = abc::write(score);
        REQUIRE(warnings.size() == 1);
        if (duration == 1) {
            CHECK_THAT(
                warnings.front(), Catch::Matchers::ContainsSubstring("[midi-short-reattack]")
            );
        } else {
            CHECK_THAT(
                warnings.front(), Catch::Matchers::ContainsSubstring("[midi-chord-release]")
            );
            CHECK_THAT(
                warnings.front(),
                Catch::Matchers::ContainsSubstring(fmt::format("tick {} (480 TPQ)", 1 + duration))
            );
            CHECK_THAT(warnings.front(), Catch::Matchers::ContainsSubstring("pitch 64"));
        }
        const auto [loaded, read_warnings] = abc::read(text);
        CHECK(read_warnings.empty());
        CHECK(loaded == score);
    }
}

TEST_CASE("ABC export warns when note boundaries require a tie", "[abc]") {
    auto  score = abc::read(header + "C").value;
    auto& notes = *score.tracks->front()->notes;
    for (const Note<Tick> short_note : {Note<Tick>{1, 480, 64, 90}, {481, 240, 64, 90}}) {
        CAPTURE(short_note.time);
        notes                       = {{1, 960, 60, 90}, short_note};
        const auto [text, warnings] = abc::write(score);
        REQUIRE(warnings.size() == 1);
        CHECK_THAT(warnings.front(), Catch::Matchers::ContainsSubstring("[midi-tied-release]"));
        CHECK_THAT(warnings.front(), Catch::Matchers::ContainsSubstring("tick 961"));
        const auto [loaded, read_warnings] = abc::read(text);
        CHECK(read_warnings.empty());
        CHECK(loaded == score);
    }
}

TEST_CASE("ABC export warns when long holds need tied atoms", "[abc]") {
    auto  score   = abc::read(header + "C").value;
    auto& note    = score.tracks->front()->notes->front();
    note.duration = 1024 * 480;
    CHECK(abc::write(score).warnings.empty());

    ++note.duration;
    const auto [text, warnings] = abc::write(score);
    REQUIRE(warnings.size() == 1);
    CHECK_THAT(warnings.front(), Catch::Matchers::ContainsSubstring("[midi-tied-release]"));
    CHECK_THAT(warnings.front(), Catch::Matchers::ContainsSubstring("tick 491522"));
    const auto [loaded, read_warnings] = abc::read(text);
    CHECK(read_warnings.empty());
    CHECK(loaded == score);
}

TEST_CASE("ABC export retries a constrained onset", "[abc]") {
    for (const bool reverse : {false, true}) {
        CAPTURE(reverse);
        Score<Tick> score(480);
        // The first twelve tracks leave three lanes for the last track.
        for (u8 program = 0; program < 12; ++program) {
            auto track = std::make_shared<Track<Tick>>("", program, false);
            track->notes->emplace_back(15, 240, 61, 83);
            score.tracks->push_back(std::move(track));
        }
        auto lead    = std::make_shared<Track<Tick>>("Lead", 30, false);
        *lead->notes = {
            {15, 240, 50, 70},
            {15, 240, 51, 71},
            {15, 240, 60, 72},
            {15, 240, 61, 71},
            {15, 240, 62, 71},
        };
        auto attacks = std::array{
            Note<Tick>{45, 30, 60, 80}, Note<Tick>{45, 30, 61, 81}, Note<Tick>{45, 30, 62, 82}
        };
        if (reverse) std::ranges::reverse(attacks);
        for (const auto& note : attacks) lead->notes->push_back(note);
        score.tracks->push_back(std::move(lead));

        const auto [loaded, warnings] = abc::read(abc::write(score).value);
        CHECK(warnings.empty());
        CHECK(loaded.tracks->size() == 15);
        const auto rows = [](const auto& song) {
            vec<std::tuple<i32, i32, i8, i8, u8>> result;
            for (const auto& track : *song.tracks)
                for (const auto& note : *track->notes)
                    result.emplace_back(
                        note.time, note.duration, note.pitch, note.velocity, track->program
                    );
            std::ranges::sort(result);
            return result;
        };
        CHECK(rows(loaded) == rows(score));
    }
}

TEST_CASE("ABC export bounds placement retries", "[abc]") {
    struct LaneRange {
        int  first;
        int  last;
        bool includes_zero = false;
    };
    // Independent groups compete for disjoint lanes. Taking lane 0 for the
    // first attack leaves the last group short and forces a long retry.
    const std::array<LaneRange, 15> allowed{{
        {0, 1},
        {6, 8},
        {6, 8},
        {6, 8},
        {9, 11},
        {9, 11},
        {9, 11},
        {12, 14},
        {12, 14},
        {12, 14},
        {2, 5, true},
        {2, 5, true},
        {2, 5, true},
        {2, 5, true},
        {2, 5, true},
    }};
    Score<Tick>                     score(480);
    auto                            track = std::make_shared<Track<Tick>>();
    for (i8 lane = 0; lane < 15; ++lane) track->notes->emplace_back(15, 240, 20, 60 + lane);
    i8 pitch = 40;
    for (const auto& [first, last, includes_zero] : allowed) {
        for (i8 lane = 0; lane < 15; ++lane)
            if ((lane < first || lane > last) && !(includes_zero && lane == 0))
                track->notes->emplace_back(15, 240, pitch, 60 + lane);
        track->notes->emplace_back(45, 30, pitch, 50 + pitch);
        ++pitch;
    }
    score.tracks->push_back(std::move(track));
    CHECK_THROWS_MATCHES(
        abc::write(score),
        std::invalid_argument,
        Catch::Matchers::MessageMatches(
            Catch::Matchers::ContainsSubstring("[voice-search-limit]")
            && Catch::Matchers::ContainsSubstring("tick 45 (480 TPQ) with 15 available voices")
            && Catch::Matchers::ContainsSubstring("100000 retry candidates")
        )
    );
}

TEST_CASE("ABC export reports exhausted voice limits", "[abc]") {
    for (const auto [tpq, melodic, drums] :
         {std::tuple{480, 16, 0}, std::tuple{960, 16, 0}, std::tuple{480, 15, 16}}) {
        CAPTURE(tpq, melodic, drums);
        Score<Tick> score(tpq);
        for (int index = 0; index < melodic + drums; ++index) {
            auto track = std::make_shared<Track<Tick>>("", 0, index >= melodic);
            track->notes->emplace_back(tpq / 32, tpq / 16, 60, 80);
            score.tracks->push_back(std::move(track));
        }
        CHECK_THROWS_MATCHES(
            abc::write(score),
            std::invalid_argument,
            Catch::Matchers::MessageMatches(
                Catch::Matchers::ContainsSubstring("[voice-placement]")
                && Catch::Matchers::ContainsSubstring("tick 15 (480 TPQ) in 0 available voices")
                && Catch::Matchers::ContainsSubstring(
                    drums ? "30 ABC voices" : "15 melodic ABC voices"
                )
            )
        );
    }
}

TEST_CASE("ABC lyrics prefer attacks in split voices and diagnose missing slots", "[abc]") {
    auto  score = abc::read(header + "C4 z2 D").value;
    auto& track = *score.tracks->front();
    track.notes->emplace_back(481, 480, 60, 80);
    track.lyrics->emplace_back(481, "attack");
    track.lyrics->emplace_back(481, "held");
    track.lyrics->emplace_back(481, "excess");
    track.lyrics->emplace_back(2401, "silent");
    track.lyrics->emplace_back(2881, "");

    const auto [text, warnings] = abc::write(score);
    for (const std::string_view code :
         {"[split-voice]", "[midi-tied-lyric]", "[omitted-lyrics]", "[omitted-empty-lyric]"}) {
        CAPTURE(code);
        CHECK(std::ranges::any_of(warnings, [&](const auto& w) {
            return w.find(code) != std::string::npos;
        }));
    }
    const auto [loaded, read_warnings] = abc::read(text);
    CHECK(read_warnings.empty());
    REQUIRE(loaded.tracks->size() == 2);
    const auto& held   = *loaded.tracks->at(0)->lyrics;
    const auto& attack = *loaded.tracks->at(1)->lyrics;
    REQUIRE(held.size() == 1);
    REQUIRE(attack.size() == 1);
    CHECK(held.front().time == 481);
    CHECK(held.front().text == "held");
    CHECK(attack.front().time == 481);
    CHECK(attack.front().text == "attack");
}

TEST_CASE("ABC free meter reads as the common time pulse", "[abc]") {
    const std::string prefix   = "%abc-2.1\nX:1\nT:Free\nL:1/4\n";
    const auto        original = abc::read(prefix + "M:none\nK:C\nC [M:3/4] D3 | [M:none] E F |]");
    REQUIRE(original.warnings.empty());
    const auto& score = original.value;
    CHECK(*score.time_signatures == pyvec<TimeSignature<Tick>>{{0, 4, 4}, {480, 3, 4}, {1920, 4, 4}});

    const auto exported = abc::write(score);
    const auto reloaded = abc::read(exported.value);
    CHECK(reloaded.warnings.empty());
    CHECK(reloaded.value == score);
    const auto midi = Score<Tick>::parse<DataFormat::MIDI>(score.dumps<DataFormat::MIDI>());
    CHECK(*midi.time_signatures == *score.time_signatures);
}
TEST_CASE("ABC additive meters become plain fractions", "[abc]") {
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
TEST_CASE("Trailing ABC rests do not extend the score", "[abc]") {
    const std::string prefix = "%abc-2.1\nX:1\nT:Rests\nM:3/4\nL:1/4\nK:C\n";
    for (const auto* music : {"C3 | Z2 |]", "C3 | X2 |]", "C3 | z3 |]", "C3 | x3 |]"}) {
        CAPTURE(music);
        const auto original = abc::read(prefix + music);
        REQUIRE(original.warnings.empty());
        CHECK(original.value.end() == original.value.tracks->front()->end());
        CHECK(original.value.end() == 1440);
    }
}
TEST_CASE("Omitting the meter is free meter", "[abc]") {
    const std::string music         = "K:C\nCDEF||GABc|";
    const auto [implicit, warnings] = abc::read("X:1\nT:Free meter\n" + music);
    const auto explicit_m           = abc::read("X:1\nT:Free meter\nM:none\n" + music).value;
    REQUIRE(warnings.empty());
    const auto& notes = *implicit.tracks->front()->notes;
    CHECK(notes == *explicit_m.tracks->front()->notes);
    REQUIRE(notes.size() == 8);
    CHECK(notes[1].time == 241);
    CHECK(notes[4].velocity == 95);
}

TEST_CASE("Field collisions", "[abc]") {
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

TEST_CASE("Voice field order", "[abc]") {
    const auto [score, warnings] = abc::read(
        header
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

TEST_CASE("Grace tie release", "[abc]") {
    struct Case {
        const char* music;
        int         duration, next_onset;
        size_t      count;
    };
    for (const auto& [music, duration, next_onset, count] : {
             Case{"C- {C}D E", 599, 601, 3},
             Case{"{C-C}D E", 239, 241, 3},
             Case{"C- {[C]}D E", 599, 601, 3},
             Case{"{C-}C E", 479, 481, 2},
         }) {
        CAPTURE(music);
        for (const bool strict : {false, true}) {
            CAPTURE(strict);
            const auto opening           = (strict ? "%abc-2.1\n" : "") + header;
            const auto [score, warnings] = abc::read(opening + "%%MIDI expand 1/8\n" + music);
            REQUIRE(warnings.empty());
            REQUIRE(score.tracks->size() == 1);
            const auto& notes = *score.tracks->front()->notes;
            REQUIRE(notes.size() == count);
            CHECK(notes[0].time == 1);
            CHECK(notes[0].pitch == 60);
            CHECK(notes[0].duration == duration + (strict ? 60 : 0));
            CHECK(notes[1].time == next_onset);
        }
    }
}

TEST_CASE("Chord rest divisor", "[abc]") {
    for (const auto& [music, column] : {
             std::pair{"[Cz/0]D", 4},
             {"[x/0C]D", 3},
             {"[Z/0]D", 3},
             {"{[CX/0]}D", 5},
         }) {
        CAPTURE(music);
        const auto where = "case.abc:6:" + std::to_string(column) + ": ABC [zero-length-divisor]";
        CHECK_THROWS_MATCHES(
            abc::read(header + music, "case.abc"),
            std::invalid_argument,
            Catch::Matchers::MessageMatches(Catch::Matchers::StartsWith(where))
        );
    }
}

TEST_CASE("Chord length factors", "[abc]") {
    // ABC 2.1 section 4.17: the first member sets the chord's step duration.
    const auto intro = "%abc-2.1\n" + header + "%%MIDI chordattack 0\n";
    for (const auto& [music, c_gate, e_gate, g_gate] : {
             std::tuple{"[C2E3G]3/2 D", 1439, 2159, 719},
             {"[C2E3G]/ D", 479, 719, 239},
         }) {
        CAPTURE(music);
        const auto [score, warnings] = abc::read(intro + music);
        REQUIRE(warnings.empty());
        const auto& notes = *score.tracks->front()->notes;
        REQUIRE(notes.size() == 4);
        CHECK(notes[0] == Note<Tick>{1, c_gate, 60, 90});
        CHECK(notes[1] == Note<Tick>{1, e_gate, 64, 90});
        CHECK(notes[2] == Note<Tick>{1, g_gate, 67, 90});
        CHECK(notes.back().time == c_gate + 2);
    }
}

TEST_CASE("Chord members follow written rhythm and grace timing", "[abc]") {
    const auto intro = "%abc-2.1\n" + header + "%%MIDI chordattack 0\n";
    for (const auto& [music, c_gate, e_gate, d_time] : {
             std::tuple{"[CE2]>D", 719, 1439, 721},
             {"(3:2:1[CE2]D", 319, 639, 321},
             {"{A}[CE2]D", 359, 839, 481},
             {"{[CE2]}D", 119, 239, 121},
             {"H[CE2]D", 959, 1919, 961},
             {"[I:MIDI fermatafixed]H[CE2]D", 959, 1439, 961},
             {"[C.E2]D", 479, 479, 481},
         }) {
        CAPTURE(music);
        const auto [score, warnings] = abc::read(intro + music + " |]");
        REQUIRE(warnings.empty());
        const auto& notes = *score.tracks->front()->notes;
        const auto  c     = std::ranges::find(notes, 60, &Note<Tick>::pitch);
        const auto  e     = std::ranges::find(notes, 64, &Note<Tick>::pitch);
        const auto  d     = std::ranges::find(notes, 62, &Note<Tick>::pitch);
        REQUIRE(c != notes.end());
        REQUIRE(e != notes.end());
        REQUIRE(d != notes.end());
        CHECK(c->duration == c_gate);
        CHECK(e->duration == e_gate);
        CHECK(d->time == d_time);
    }
}

TEST_CASE("Loose ABC uses the first chord member's duration", "[abc]") {
    const auto [score, warnings] = abc::read(header + "%%MIDI chordattack 0\n[CE3] D |]");
    REQUIRE(warnings.empty());
    const auto& notes = *score.tracks->front()->notes;
    REQUIRE(notes.size() == 3);
    CHECK(notes[0].duration == 479);
    CHECK(notes[1].duration == 479);
    CHECK(notes[2].time == 481);
}

TEST_CASE("Ties retain the latest release of overlapping chord members", "[abc]") {
    const auto opening = "%abc-2.1\n" + header + "%%MIDI chordattack 0\n";
    for (const auto& [music, gate, d_time] : {
             std::tuple{"[CE3-] E D", 1439, 961},
             {"[CE3-] E4 D", 2399, 2401},
             {"[CE4-] E- E D", 1919, 1441},
             {"E- [CE3-] E D", 1919, 1441},
         }) {
        CAPTURE(music);
        const auto [score, warnings] = abc::read(opening + music + " |]");
        REQUIRE(warnings.empty());
        const auto& notes = *score.tracks->front()->notes;
        REQUIRE(notes.size() == 3);
        const auto e = std::ranges::find(notes, 64, &Note<Tick>::pitch);
        const auto d = std::ranges::find(notes, 62, &Note<Tick>::pitch);
        REQUIRE(e != notes.end());
        REQUIRE(d != notes.end());
        CHECK(e->time == 1);
        CHECK(e->duration == gate);
        CHECK(d->time == d_time);
    }
}

TEST_CASE("Stress measures the written span of overlapping tied members", "[abc]") {
    const auto [score, warnings] = abc::read(
        fmt::format(
            "%abc-2.1\n{}%%MIDI stressmodel 1\n{}%%MIDI chordattack 0\n[CE2-] E D2 |]",
            header,
            stress_profile
        )
    );
    REQUIRE(warnings.empty());
    const auto& notes = *score.tracks->front()->notes;
    REQUIRE(notes.size() == 3);
    CHECK(notes[1] == Note<Tick>{1, 959, 64, 112});
    CHECK(notes[2].time == 961);
}

TEST_CASE("A roll spans the full duration of an overlapping continuation", "[abc]") {
    const auto opening           = "%abc-2.1\n" + header + "%%MIDI chordattack 0\n";
    const auto [score, warnings] = abc::read(opening + "RE- [CE3-] E C |]");
    REQUIRE(warnings.empty());
    const auto [reference, reference_warnings] = abc::read(opening + "RE4 |]");
    REQUIRE(reference_warnings.empty());
    const auto&     notes = *score.tracks->front()->notes;
    vec<Note<Tick>> roll;
    vec<i32>        other_onsets;
    for (const auto& note : notes) {
        if (note.pitch == 60)
            other_onsets.push_back(note.time);
        else
            roll.push_back(note);
    }
    CHECK(std::ranges::equal(roll, *reference.tracks->front()->notes));
    CHECK(other_onsets == vec<i32>{481, 1441});
}

TEST_CASE("A chord member can sustain past the end of its percussion voice", "[abc]") {
    const auto [score, warnings]
        = abc::read("%abc-2.1\n" + header + "%%MIDI channel 10\n%%MIDI chordattack 0\n[CE8] |]");
    REQUIRE(warnings.empty());
    REQUIRE(score.tracks->size() == 1);
    const auto& track = *score.tracks->front();
    REQUIRE(track.is_drum);
    REQUIRE(track.notes->size() == 2);
    CHECK(track.notes->at(0) == Note<Tick>{1, 479, 60, 90});
    CHECK(track.notes->at(1) == Note<Tick>{1, 3839, 64, 90});
}

TEST_CASE("Unequal chord members do not extend generated backing", "[abc]") {
    const auto opening = "%abc-2.1\n" + header
                         + "%%MIDI chordattack 0\n%%MIDI drum dddd 36 37 38 39\n%%MIDI drumon\n";
    const auto [reference, reference_warnings] = abc::read(opening + "z3 [CE] |]");
    REQUIRE(reference_warnings.empty());
    const auto [score, warnings] = abc::read(opening + "z3 [CE8] |]");
    REQUIRE(warnings.empty());
    REQUIRE(reference.tracks->size() == 2);
    REQUIRE(score.tracks->size() == 2);
    REQUIRE(reference.tracks->back()->notes->size() == 4);
    REQUIRE(score.tracks->back()->is_drum);
    CHECK(*score.tracks->back()->notes == *reference.tracks->back()->notes);
    REQUIRE(score.tracks->front()->notes->size() == 2);
    CHECK(score.tracks->front()->notes->at(1).duration == 3839);
}

TEST_CASE("An unequal chord validates each member's release", "[abc]") {
    CHECK_THROWS_MATCHES(
        abc::read("%abc-2.1\n" + header + "[CE2147483647] D |]", "case.abc"),
        std::invalid_argument,
        Catch::Matchers::MessageMatches(
            Catch::Matchers::StartsWith("case.abc:7:1: ABC [time-range]")
        )
    );
}

TEST_CASE("Chord factors share equal member lengths", "[abc]") {
    const auto [score, warnings] = abc::read(header + "%%MIDI chordattack 0\n[C2E2G2]3/2 D");
    REQUIRE(warnings.empty());
    const auto& notes = *score.tracks->front()->notes;
    REQUIRE(notes.size() == 4);
    for (size_t i = 0; i < 3; ++i) CHECK(notes[i].duration == 719);
    CHECK(notes.back().time == 721);
}

TEST_CASE("ABC 2.1 fingerings within a chord do not affect playback", "[abc]") {
    const auto opening            = "%abc-2.1\n" + header + "%%MIDI chordattack 0\n";
    const auto [marked, warnings] = abc::read(opening + "[!1!C!3!E!5!G] D |]");
    REQUIRE(warnings.empty());
    CHECK(marked == abc::read(opening + "[CEG] D |]").value);
}

TEST_CASE("A chord's inner staccato applies only to the marked member", "[abc]") {
    const auto opening = "%abc-2.1\n" + header + "%%MIDI chordattack 0\n";
    // ABC 2.1 section 4.17 defines the affected pitch, not the exact MIDI gate.
    for (const auto& [music, short_pitches] : {
             std::pair{"[.CEG] D", vec{60}},
             {"[C.EG] D", {64}},
             {".[CEG] D", {60, 64, 67}},
         }) {
        CAPTURE(music);
        const auto [score, warnings] = abc::read(opening + music);
        REQUIRE(warnings.empty());
        const auto& notes = *score.tracks->front()->notes;
        REQUIRE(notes.size() == 4);
        for (const int pitch : {60, 64, 67}) {
            const auto note
                = std::ranges::find_if(notes, [pitch](const auto& n) { return n.pitch == pitch; });
            REQUIRE(note != notes.end());
            CHECK(note->time == 1);
            const bool short_gate = std::ranges::find(short_pitches, pitch) != short_pitches.end();
            CHECK(note->duration == (short_gate ? 239 : 479));
        }
        CHECK(notes.back().time == 481);
    }
}

TEST_CASE("ABC 2.1 tenuto keeps the written gate without changing rhythm", "[abc]") {
    for (const auto& [directive, unmarked_gate] : {
             std::pair{"trim", 359},
             {"expand", 599},
         }) {
        CAPTURE(directive);
        const auto body              = fmt::format("%%MIDI {} 1/4\nC !tenuto!D E |]", directive);
        const auto [score, warnings] = abc::read("%abc-2.1\n" + header + body);
        REQUIRE(warnings.empty());
        const auto& notes = *score.tracks->front()->notes;
        REQUIRE(notes.size() == 3);
        CHECK(notes[0].time == 1);
        CHECK(notes[0].duration == unmarked_gate);
        CHECK(notes[1].time == 481);
        CHECK(notes[1].duration == 479);
        CHECK(notes[2].time == 961);
        CHECK(notes[2].duration == unmarked_gate);

        const auto [legacy, legacy_warnings] = abc::read(header + body);
        REQUIRE(legacy_warnings.empty());
        CHECK(legacy.tracks->front()->notes->at(1).duration == unmarked_gate);
    }
}

TEST_CASE("ABC 2.1 tenuto applies to the marked chord or member", "[abc]") {
    const auto opening = "%abc-2.1\n" + header + "%%MIDI chordattack 0\n%%MIDI trim 1/4\n";
    for (const auto& [music, c_gate, e_gate] : {
             std::tuple{"[C!tenuto!E] G |]", 359, 479},
             {"!tenuto![CE] G |]", 479, 479},
             {"[CE] G |]\ns: !tenuto! *", 479, 479},
         }) {
        CAPTURE(music);
        const auto [score, warnings] = abc::read(opening + music);
        REQUIRE(warnings.empty());
        const auto& notes = *score.tracks->front()->notes;
        REQUIRE(notes.size() == 3);
        CHECK(notes[0].pitch == 60);
        CHECK(notes[0].duration == c_gate);
        CHECK(notes[1].pitch == 64);
        CHECK(notes[1].duration == e_gate);
        CHECK(notes[2].time == 481);
        CHECK(notes[2].duration == 359);
    }

    const auto [tied, warnings] = abc::read(
        "%abc-2.1\n" + header + "%%MIDI chordattack 0\n%%MIDI expand 1/4\n[CE]-[C!tenuto!E] G |]"
    );
    REQUIRE(warnings.empty());
    const auto& notes = *tied.tracks->front()->notes;
    REQUIRE(notes.size() == 3);
    CHECK(notes[0].pitch == 60);
    CHECK(notes[0].duration == 1079);
    CHECK(notes[1].pitch == 64);
    CHECK(notes[1].duration == 959);
    CHECK(notes[2].time == 961);
}

TEST_CASE("ABC 2.1 inverted fermata plays the same hold as an ordinary fermata", "[abc]") {
    for (const auto& [directive, held_gate] : {
             std::pair{"fermatafixed", 1439},
             {"fermataproportional", 1919},
         }) {
        CAPTURE(directive);
        const auto opening = "%abc-2.1\n" + header + fmt::format("%%MIDI {}\n", directive);
        const auto [score, warnings] = abc::read(opening + "C !invertedfermata!D2 E |]");
        const auto ordinary          = abc::read(opening + "C !fermata!D2 E |]");
        REQUIRE(warnings.empty());
        REQUIRE(ordinary.warnings.empty());
        CHECK(score == ordinary.value);
        const auto& notes = *score.tracks->front()->notes;
        REQUIRE(notes.size() == 3);
        CHECK(notes[1].time == 481);
        CHECK(notes[1].duration == held_gate);
        CHECK(notes[2].time == 482 + held_gate);
    }
}

TEST_CASE("Chord member dynamics affect subsequent attacks in written order", "[abc]") {
    // The carry and numeric levels follow abc2midi's dynamic convention.
    const auto [score, warnings]
        = abc::read("X:1\nM:4/4\nL:1/4\nU:h=!ff!\nK:C\n%%MIDI chordattack 0\n!p![ChEG] D");
    REQUIRE(warnings.empty());
    const auto& notes = *score.tracks->front()->notes;
    REQUIRE(notes.size() == 4);
    CHECK(notes[0].velocity == 60);
    CHECK(notes[1].velocity == 120);
    CHECK(notes[2].velocity == 120);
    CHECK(notes[3].velocity == 110);
}

TEST_CASE("Queued chord attacks use the shorthand definition at their written position", "[abc]") {
    const std::string opening
        = "%abc-2.1\nX:1\nT:\nM:4/4\nL:1/4\nU:h=!p!\nK:C\n%%MIDI chordattack 0\n";
    for (const auto& [music, expanded] : {
             std::pair{"[ChE] [U:h=!ff!] [ChE] G", "[C!p!E] [C!ff!E] G"},
             {"[ChE]>[U:h=!ff!] [ChE] G", "[C!p!E]>[C!ff!E] G"},
             {"{[ChE]} [U:h=!ff!] [ChE] G", "{[C!p!E]} [C!ff!E] G"},
             {"[hC\\\nU:h=!ff!\nhE] G", "[!p!C!ff!E] G"},
         }) {
        CAPTURE(music);
        const auto [marked, warnings] = abc::read(opening + music);
        CHECK(warnings.empty());
        CHECK(marked == abc::read(opening + expanded).value);
    }
}

TEST_CASE("Grace chord decorations do not consume the host's timing", "[abc]") {
    const auto [score, warnings]
        = abc::read("%abc-2.1\n" + header + "%%MIDI chordattack 0\n{[C.!p!E]} G D");
    REQUIRE(warnings.empty());
    const auto& notes = *score.tracks->front()->notes;
    REQUIRE(notes.size() == 4);
    CHECK(notes[0].duration == 119);
    CHECK(notes[0].velocity == 90);
    CHECK(notes[1].duration == 59);
    CHECK(notes[1].velocity == 60);
    CHECK(notes[2].time == 121);
    CHECK(notes[2].velocity == 35);
    CHECK(notes[3].time == 481);
}

TEST_CASE("A terminal chord tie uses the marked member's staccato gate", "[abc]") {
    const auto [score, warnings] = abc::read(header + "%%MIDI chordattack 0\n[CE]- [C.E] G");
    REQUIRE(warnings.empty());
    const auto& notes = *score.tracks->front()->notes;
    REQUIRE(notes.size() == 3);
    CHECK(notes[0].duration == 959);
    CHECK(notes[1].duration == 719);
    CHECK(notes[2].time == 961);
}

TEST_CASE("Chord length overflow is diagnosed", "[abc]") {
    CHECK_THROWS_MATCHES(
        abc::read("%abc-2.1\n" + header + "[C65536E]65536 D"),
        std::invalid_argument,
        Catch::Matchers::MessageMatches(Catch::Matchers::ContainsSubstring("[time-range]"))
    );
}

TEST_CASE("Grace chord length factors", "[abc]") {
    const auto [score, warnings]
        = abc::read("%abc-2.1\n" + header + "%%MIDI chordattack 0\n{[C2E]2 G}A4 B");
    REQUIRE(warnings.empty());
    const auto& notes = *score.tracks->front()->notes;
    REQUIRE(notes.size() == 5);
    CHECK(notes[0].duration == 479);
    CHECK(notes[1].duration == 239);
    CHECK(notes[2].time == 481);
    CHECK(notes[3].time == 601);
    CHECK(notes[4].time == 1921);
}

TEST_CASE("Tuplet counts unequal atoms", "[abc]") {
    const auto [score, warnings]
        = abc::read("%abc-2.1\n" + header + "%%MIDI chordattack 0\n(3::3 [C2E]/ y z/ D2 F");
    REQUIRE(warnings.empty());
    const auto& notes = *score.tracks->front()->notes;
    REQUIRE(notes.size() == 4);
    CHECK(notes[0].duration == 319);
    CHECK(notes[1].duration == 159);
    CHECK(notes[2].time == 481);
    CHECK(notes[2].duration == 639);
    CHECK(notes[3].time == 1121);
}

TEST_CASE("Tuplet defaults use the written meter", "[abc]") {
    struct Case {
        const char* opening;
        const char* music;
        int         next_onset;
    };
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
            = abc::read(std::string("X:1\nM:") + opening + "\nL:1/4\nK:C\n" + music);
        REQUIRE(warnings.empty());
        CHECK(score.tracks->front()->notes->back().time == next_onset);
    }
}

TEST_CASE("Repeat restores the tuplet meter", "[abc]") {
    const auto [score, warnings] = abc::read(header + "|: (5ABCDE [M:6/8] z :| F");
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

TEST_CASE("Repeat entry publishes the last meter in its voice at that tick", "[abc]") {
    const auto [score, warnings] = abc::read(header + "|: [M:6/8] C [M:3/4] D :| E");
    CHECK(warnings.empty());
    REQUIRE(score.time_signatures->size() == 4);
    CHECK(score.time_signatures->at(2) == TimeSignature<Tick>{960, 6, 8});
}

TEST_CASE("A repeat jump keeps the active guitar accompaniment meter", "[abc]") {
    const auto [score, warnings] = abc::read(
        "%abc-2.1\nX:1\nT:Changed meter\nM:6/8\nL:1/8\nK:C\n"
        "|: \"G\"G6 | [M:4/4][L:1/4] \"C\"C4 :|"
    );
    REQUIRE(warnings.empty());
    REQUIRE(score.tracks->size() == 3);
    CHECK(
        *score.tracks->at(1)->notes
        == pyvec<Note<Tick>>{
            {1, 239, 43, 80},
            {721, 239, 43, 80},
            {1441, 239, 36, 80},
            {2401, 239, 36, 80},
            {3361, 239, 43, 80},
            {4321, 239, 43, 80},
            {4801, 239, 36, 80},
            {5761, 239, 36, 80},
        }
    );
    vec<i32> chord_onsets;
    for (const auto& note : *score.tracks->at(2)->notes)
        if (chord_onsets.empty() || chord_onsets.back() != note.time)
            chord_onsets.push_back(note.time);
    CHECK(chord_onsets == vec<i32>{481, 1201, 1921, 2881, 3841, 5281, 6241});
    REQUIRE(score.time_signatures->size() == 4);
    CHECK(score.time_signatures->at(2) == TimeSignature<Tick>{3360, 6, 8});
}

TEST_CASE("A timed meter change preserves the accumulated beats at a repeat bar", "[abc]") {
    const auto [score, warnings] = abc::read(
        "%abc-2.1\nX:1\nT:Pickup after changing meter\nM:6/8\nL:1/8\nP:ABABA\nK:C\n"
        "[P:A]D|\"G\"G6:|\n[P:B]d|\"G\"G6|\"G\"G5:|[M:4/4]"
    );
    REQUIRE(warnings.empty());
    REQUIRE(score.tracks->size() == 3);
    const auto onsets = [&](size_t track) {
        vec<i32> times;
        for (const auto& note : *score.tracks->at(track)->notes)
            if (note.time >= 9121 && note.time <= 12500
                && (times.empty() || times.back() != note.time))
                times.push_back(note.time);
        return times;
    };
    // The first pickup closes at the new meter. Later short bars keep its pattern phase.
    CHECK(onsets(1) == vec<i32>{9121, 9361, 10321, 11041, 12001});
    CHECK(onsets(2) == vec<i32>{9841, 10801, 11521, 12481});
}

TEST_CASE("Written meter groups determine beat accents", "[abc]") {
    const auto check = [](std::string_view meter, std::string_view music, vec<int> expected) {
        CAPTURE(meter, music);
        const auto [score, warnings]
            = abc::read(fmt::format("X:1\nM:{}\nL:1/8\nK:C\n{}", meter, music));
        REQUIRE(warnings.empty());
        vec<int> velocities;
        for (const auto& note : *score.tracks->front()->notes) velocities.push_back(note.velocity);
        CHECK(velocities == expected);
        CHECK(score.time_signatures->front() == TimeSignature<Tick>{0, 7, 8});
    };
    check("(2+3+2)/8", "CDEFGAB", {105, 80, 95, 80, 80, 95, 80});
    check("2+3+2/8", "!p!CDEFGAB", {60, 35, 50, 35, 35, 50, 35});
    check("7/8", "CDEFGAB", {105, 80, 80, 80, 80, 80, 80});
    check("2+3+2/8", "|: CDEFGAB | [M:3+2+2/8] CDEFGAB :|", {105, 80, 95, 80, 80, 95, 80,
                                                             105, 80, 80, 95, 80, 95, 80,
                                                             105, 80, 95, 80, 80, 95, 80,
                                                             105, 80, 80, 95, 80, 95, 80});
}

TEST_CASE("Hornpipe keeps its opening subdivision after a meter change", "[abc]") {
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

TEST_CASE("A meter change can fall inside a swung pair", "[abc]") {
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

TEST_CASE("Opening free meter permits hornpipe swing", "[abc]") {
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

TEST_CASE("Body hornpipe in cut time uses the opening bar phase", "[abc]") {
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

TEST_CASE("Later meter does not enable hornpipe swing", "[abc]") {
    const auto [score, warnings]
        = abc::read("X:1\nT:\nM:6/8\nL:1/8\nK:C\n[R:hornpipe] C>DEFGA | [M:4/4] CDEF GABC |]");
    REQUIRE(warnings.empty());
    const auto& notes = *score.tracks->front()->notes;
    REQUIRE(notes.size() == 14);
    CHECK(notes[1].time == 321);   // The explicit pair still uses the 2:1 ratio.
    CHECK(notes[7].time == 1681);
    CHECK(notes[7].duration == 239);
}

TEST_CASE("Body rhythm applies to the pair completed after the field", "[abc]") {
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

TEST_CASE("Rhythm follows written order across voices and repeats", "[abc]") {
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

TEST_CASE("Hornpipe resets the written broken ratio", "[abc]") {
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

TEST_CASE("Broken ratios do not follow performed navigation", "[abc]") {
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

TEST_CASE("Body ratio errors point to the directive", "[abc]") {
    CHECK_THROWS_MATCHES(
        abc::read("X:1\nT:\nM:4/4\nL:1/8\nK:C\nC>[I:MIDI ratio 2 0]D z6|", "ratio.abc"),
        std::invalid_argument,
        Catch::Matchers::MessageMatches(
            Catch::Matchers::ContainsSubstring("ratio.abc:6:4: ABC [invalid-midi-value]")
        )
    );
}

TEST_CASE("Broken rhythm compares note lengths across a unit field", "[abc]") {
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

TEST_CASE("Unit and tempo fields follow the adjusted rhythm boundary", "[abc]") {
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

TEST_CASE("Grace attacks use the unit at their group", "[abc]") {
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

TEST_CASE("Grace notes follow written tuplet timing", "[abc]") {
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
        const auto [score, warnings] = abc::read(header + std::string(music));
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

TEST_CASE("A grace host leaves its new meter in force", "[abc]") {
    const auto [score, warnings] = abc::read("X:1\nT:\nM:4/4\nL:1/4\nK:C\nD {F}[M:6/8]F Z G |]");
    REQUIRE(warnings.empty());
    const auto& notes = *score.tracks->front()->notes;
    REQUIRE(notes.size() == 4);
    CHECK(notes.back().time == 2401);
}

TEST_CASE("A grace chord does not consume a tuplet slot", "[abc]") {
    // Sections 4.12–4.13 place grace outside the counted notes.
    const auto [score, warnings] = abc::read(header + "(3C{[de]}E F G |]");
    CHECK(warnings.empty());
    const auto& notes = *score.tracks->front()->notes;
    REQUIRE(notes.size() == 6);
    CHECK(notes[1] == Note<Tick>{321, 79, 74, 80});
    CHECK(notes[3] == Note<Tick>{401, 239, 64, 80});
    CHECK(notes[4] == Note<Tick>{641, 319, 65, 80});
    CHECK(notes[5].time == 961);
}

TEST_CASE("Harp subdivisions keep each note's unit", "[abc]") {
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

TEST_CASE("Fermata holds the note after its written rhythm is resolved", "[abc]") {
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

TEST_CASE("A unit change alone does not hide the host's fermata from its grace", "[abc]") {
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

TEST_CASE("Invalid deferred unit reports its field location", "[abc]") {
    CHECK_THROWS_MATCHES(
        abc::read("X:1\nT:\nM:4/4\nL:1/8\nK:C\nC>[L:0/8]D|", "unit.abc"),
        std::invalid_argument,
        Catch::Matchers::MessageMatches(
            Catch::Matchers::ContainsSubstring("unit.abc:6:4: ABC [invalid-unit-length]")
        )
    );
}
TEST_CASE("MIDI transposition follows performed track order", "[abc]") {
    const std::string voices = "[V:one] CDEF |]\n[V:two] GABc |]";
    const auto        check  = [](const std::string& text, const vec<vec<int>>& expected) {
        const auto [score, warnings] = abc::read("%abc-2.1\n" + text);
        REQUIRE(warnings.empty());
        REQUIRE(score.tracks->size() == expected.size());
        for (size_t i = 0; i < expected.size(); ++i) {
            vec<int> pitches;
            for (const auto& note : *score.tracks->at(i)->notes) pitches.push_back(note.pitch);
            CHECK(pitches == expected[i]);
        }
    };
    check(
        header + "%%MIDI transpose 5\nI:MIDI rtranspose -2\n" + voices,
        {{63, 65, 67, 68}, {70, 72, 74, 75}}
    );
    check(header + "%%MIDI rtranspose 3\n" + voices, {{66, 68, 70, 71}, {73, 75, 77, 78}});
    check(
        "X:1\nT:\nM:4/4\nL:1/4\nV:one\nV:two\nK:C\n%%MIDI transpose 5\n" + voices,
        {{60, 62, 64, 65}, {72, 74, 76, 77}}
    );
    check(
        header + "[V:one] C [I:MIDI transpose 5] DEF |]\n[V:two] GABc |]",
        {{65, 67, 69, 70}, {72, 74, 76, 77}}
    );
    check(header + "%%MIDI rtranspose 3\nC4 & D4 |]", {{66}, {68}});
    check(header + "|: C [I:MIDI rtranspose 1] D :| E F |]", {{60, 63, 61, 64, 66, 67}});
    check(
        header + "\"C\" C [I:MIDI rtranspose 2] D E F |]",
        {{62, 66, 68, 69}, {40, 42}, {54, 58, 61, 54, 58, 61}}
    );
    check(
        header + "%%MIDI rtranspose 2\n%%MIDI droneon\nCDEF |]\n%%MIDI droneoff",
        {{64, 66, 68, 69}, {51, 39}}
    );
    // The MIDI replay is independent of the ABC 2.1 K: transposition.
    check(
        "X:1\nT:\nM:4/4\nL:1/4\nK:C transpose=3\n%%MIDI transpose 5\n"
        "[V:one] C [K:transpose=0] DEF |]\n[V:two] GABc |]",
        {{68, 67, 69, 70}, {75, 77, 79, 80}}
    );
}

TEST_CASE("A repeat before the first completed bar restores pickup eligibility", "[abc]") {
    const auto first = abc::read(header + "|: \"C\"C D :| E F G A |]");
    REQUIRE(first.warnings.empty());
    CHECK(velocities(first.value) == vec<int>{105, 80, 105, 80, 105, 80, 95, 80});
    REQUIRE(first.value.tracks->size() == 3);
    vec<i32> bass_attacks;
    for (const auto& note : *first.value.tracks->at(1)->notes) bass_attacks.push_back(note.time);
    CHECK(bass_attacks == vec<i32>{1, 961, 1921, 2881});

    const auto later = abc::read(header + "CDEF |: G A :| B c |]");
    REQUIRE(later.warnings.empty());
    CHECK(velocities(later.value) == vec<int>{105, 80, 95, 80, 105, 80, 95, 80, 105, 80});
}

TEST_CASE("Out-of-range MIDI accompaniment spans use one bar", "[abc]") {
    const auto check = [](std::string_view command, std::string_view music) {
        const auto read = [&](int bars) {
            return abc::read(fmt::format("{}%%MIDI {} {}\n{}", header, command, bars, music));
        };
        const auto single_bar = read(1);
        REQUIRE(single_bar.warnings.empty());
        REQUIRE(single_bar.value.tracks->size() > 1);
        for (const int count : {0, -1, 11}) {
            CAPTURE(command, count);
            const auto fallback = read(count);
            CHECK(fallback.warnings.empty());
            CHECK(fallback.value == single_bar.value);
        }
    };
    check("gchordbars", "%%MIDI gchord fghc\n\"C\"CDEF | GABc |]");
    check("drumbars", "%%MIDI drum dddd 36 37 38 39\n%%MIDI drumon\nCDEF | GABc |]");
}

TEST_CASE("A multi-bar chord pattern queues its next slot at each measure boundary", "[abc]") {
    const auto [score, warnings] = abc::read(
        header + "%%MIDI gchordbars 10\n%%MIDI gchord fghc\n\"C\"CDEF | GABc | cBAG | FEDC |]"
    );
    REQUIRE(warnings.empty());
    REQUIRE(score.tracks->size() == 3);
    CHECK(*score.tracks->at(1)->notes == pyvec<Note<Tick>>{{1, 4779, 36, 80}});
    CHECK(
        *score.tracks->at(2)->notes
        == pyvec<Note<Tick>>{
            {1921, 4779, 48, 75},
            {3841, 3840, 52, 75},
            {5761, 1920, 48, 75},
            {5761, 1920, 52, 75},
            {5761, 1920, 55, 75}
        }
    );
}

TEST_CASE(
    "Chord attack spacing passes from the conductor and melody to later MIDI tracks", "[abc]"
) {
    const auto [score, warnings]
        = abc::read(header + "[V:one] [CEG] [I:MIDI chordattack 4] DEF |]\n[V:two] [GBe] ABc |]");
    REQUIRE(warnings.empty());
    REQUIRE(score.tracks->size() == 2);
    for (const auto& track : *score.tracks) {
        REQUIRE(track->notes->size() == 6);
        CHECK(track->notes->at(0).time == 1);
        CHECK(track->notes->at(1).time == 5);
        CHECK(track->notes->at(2).time == 9);
        CHECK(track->notes->at(2).end() == 480);
    }
}

TEST_CASE("A configured chord gap also applies when backing slots shorten releases", "[abc]") {
    for (const auto& [setup, track] : {
             std::pair{"%%MIDI gchordbars 10\n%%MIDI gchord fghc\n\"C\"", 1u},
             {"%%MIDI drumbars 10\n%%MIDI drum dddd 36 37 38 39\n%%MIDI drumon\n", 1u},
         }) {
        CAPTURE(setup);
        const auto [score, warnings]
            = abc::read(header + "%%MIDI chordattack 4\n" + setup + "CDEF | GABc | cBAG | FEDC |]");
        REQUIRE(warnings.empty());
        REQUIRE(score.tracks->size() > track);
        REQUIRE(!score.tracks->at(track)->notes->empty());
        CHECK(score.tracks->at(track)->notes->front().duration == 4791);
    }
}

TEST_CASE("Single-note tuning directives do not change the score", "[abc]") {
    const auto [score, warnings] = abc::read(
        "%abc-2.1\nX:1\nT:Tuning\nM:4/4\nL:1/4\n%%MIDI snt 60 61.5\nK:C\n"
        "[V:1] |: C [I:MIDI snt 60 62.125] C z2 :|\n"
        "[V:2] z [I:MIDI snt 61 60.001] [I:MIDI snt 61 63.25] E z2 |]"
    );
    for (const auto& warning : warnings) UNSCOPED_INFO(warning);
    REQUIRE(warnings.empty());
    const auto plain = abc::read(
        "%abc-2.1\nX:1\nT:Tuning\nM:4/4\nL:1/4\nK:C\n"
        "[V:1] |: C C z2 :|\n[V:2] z E z2 |]"
    );
    REQUIRE(plain.warnings.empty());
    CHECK(score == plain.value);
}
TEST_CASE("A delayed drone reuses the program delta on its two attacks", "[abc]") {
    const auto [score, warnings]
        = abc::read(header + "C [I:MIDI droneon] D E [I:MIDI droneoff] F |]");
    REQUIRE(warnings.empty());
    REQUIRE(score.tracks->size() == 2);
    CHECK(
        score.tracks->back()->notes->collect()
        == vec<Note<Tick>>{{962, 1441, 45, 80}, {1443, 960, 33, 80}}
    );
}

TEST_CASE("Invalid ABC single-note tuning reports the directive and keeps playing", "[abc]") {
    const auto abc
        = header
          + "[I:MIDI snt 128 61] [I:MIDI snt 60 127.99993896484375] [I:MIDI snt 60 NaN]\n"
            "[I:MIDI snt 60] [I:MIDI snt 60 60x] [I:MIDI snt 60 59.25 extra] C |]";
    const auto [score, warnings] = abc::read(abc, "tuning.abc");
    CHECK(score.note_num() == 1);
    REQUIRE(warnings.size() == 6);
    CHECK_THAT(warnings.front(), Catch::Matchers::StartsWith("tuning.abc:6:2: ABC [invalid-midi]"));
}

}   // namespace

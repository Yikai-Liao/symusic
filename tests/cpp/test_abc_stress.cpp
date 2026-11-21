#include <fmt/format.h>

#include "abc_test.h"

namespace symusic::abc_test {

TEST_CASE("test_stress_models", "[abc][stress]") {
    for (const auto model : {1, 2}) {
        CAPTURE(model);
        const auto [score, warnings] = abc::read(
            fmt::format("{}%%MIDI stressmodel {}\n{}CDEF |]", kHeader, model, kStressProfile)
        );
        REQUIRE(warnings.empty());
        REQUIRE(score.tracks->size() == 1);
        CHECK(velocities(score) == vec<i32>{112, 75, 102, 65});
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

TEST_CASE("test_trill_stress_clock", "[abc][stress]") {
    const auto [score, warnings]
        = abc::read(fmt::format("{}%%MIDI stressmodel 1\n{}C TD2 E |]", kHeader, kStressProfile));
    REQUIRE(warnings.empty());
    const auto& notes = *score.tracks->front()->notes;
    REQUIRE(notes.size() == 18);
    for (i32 i = 0; i < 16; ++i) {
        CAPTURE(i);
        const auto pitch    = i % 2 ? i8{62} : i8{64};
        const auto velocity = i < 8 ? i8{75} : i8{102};
        CHECK(notes[i + 1] == Note<Tick>{481 + 60 * i, i < 8 ? 24 : 50, pitch, velocity});
    }
    CHECK(notes[17] == Note<Tick>{1441, 277, 64, 65});
}

TEST_CASE("test_tied_note_weight", "[abc][stress]") {
    const auto opening = fmt::format("{}%%MIDI stressmodel 1\n{}", kHeader, kStressProfile);
    for (const auto [phrase, duration, gain] : {
             std::tuple{"C F- F G |]", 959, 75},
             {"C F- F- F |]", 1439, 80},
             {"C F2- | F2 G A |]", 1919, 80},
             // Section 4.11 only defines same-pitch ties. abc2midi keeps the full F gate here.
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

TEST_CASE("test_tied_member_weight", "[abc][stress]") {
    const auto [score, warnings] = abc::read(
        fmt::format(
            "{}%%MIDI stressmodel 1\n{}%%MIDI chordattack 0\nC [F-A]2 | F2 G A |]",
            kHeader,
            kStressProfile
        )
    );
    REQUIRE(warnings.empty());
    const auto& notes = *score.tracks->front()->notes;
    REQUIRE(notes.size() == 5);
    CHECK(notes[1] == Note<Tick>{481, 1919, 65, 80});
    CHECK(notes[2] == Note<Tick>{481, 618, 69, 75});
    CHECK(notes[3] == Note<Tick>{2401, 411, 67, 102});
}

TEST_CASE("test_tied_volume_fallback", "[abc][stress]") {
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
                kHeader,
                kStressProfile,
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

TEST_CASE("test_silent_tie_fallback", "[abc][stress]") {
    const auto [score, warnings] = abc::read(
        kHeader
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

TEST_CASE("test_roll_segment_weights", "[abc][stress]") {
    const auto [score, warnings]
        = abc::read(fmt::format("{}%%MIDI stressmodel 1\n{}C RD2 E |]", kHeader, kStressProfile));
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

TEST_CASE("test_trill_host_velocity", "[abc][stress]") {
    const auto [score, warnings]
        = abc::read(fmt::format("{}{}C TD EF |]", kHeader, kStressProfile));
    REQUIRE(warnings.empty());
    const auto& notes = *score.tracks->front()->notes;
    REQUIRE(notes.size() == 6);
    CHECK(notes[1] == Note<Tick>{673, 95, 64, 75});
    CHECK(notes[2] == Note<Tick>{769, 95, 62, 75});
    CHECK(notes[3] == Note<Tick>{865, 95, 64, 75});
    CHECK(notes[4] == Note<Tick>{961, 575, 64, 102});
    CHECK(notes[5] == Note<Tick>{1537, 383, 65, 65});
}

TEST_CASE("test_expanded_stress_gate", "[abc][stress]") {
    const auto [score, warnings] = abc::read(
        fmt::format(
            "{}%%MIDI stressmodel 1\n{}%%MIDI expand 1/16\nC TD EF |]", kHeader, kStressProfile
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

TEST_CASE("test_staccato_interval", "[abc][stress]") {
    for (const auto model : {1, 2}) {
        CAPTURE(model);
        const auto [score, warnings] = abc::read(
            fmt::format(
                "{}%%MIDI stressmodel {}\n{}C .D2 E | GABc |]", kHeader, model, kStressProfile
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

TEST_CASE("test_staccato_member_release", "[abc][stress]") {
    for (const auto model : {1, 2}) {
        for (const auto [phrase, shortens_d] : {
                 std::pair{"C [D.F]2 E |]", false},
                 {"C .[DF]2 E |]", true},
             }) {
            CAPTURE(model, phrase);
            const auto [score, warnings] = abc::read(
                fmt::format(
                    "%abc-2.1\n{}%%MIDI stressmodel {}\n{}%%MIDI chordattack 0\n{}",
                    kHeader,
                    model,
                    kStressProfile,
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

TEST_CASE("test_long_staccato_gate", "[abc][stress]") {
    const auto [score, warnings] = abc::read(
        fmt::format("{}%%MIDI stressmodel 1\n{}.C4 | DEFG |]", kHeader, kStressProfile)
    );
    REQUIRE(warnings.empty());
    const auto& notes = *score.tracks->front()->notes;
    REQUIRE(notes.size() == 5);
    CHECK(notes[0] == Note<Tick>{1, 685, 60, 112});
    CHECK(notes[1].time == 1921);
}

TEST_CASE("test_grace_segment_boundary", "[abc][stress]") {
    const auto [score, warnings]
        = abc::read(fmt::format("{}{}C3/4 {{abc}}D5/4 EF |]", kHeader, kStressProfile));
    REQUIRE(warnings.empty());
    const auto& notes = *score.tracks->front()->notes;
    REQUIRE(notes.size() == 7);
    CHECK(notes[1] == Note<Tick>{505, 167, 81, 112});
    CHECK(notes[2] == Note<Tick>{673, 71, 83, 75});
    CHECK(notes[3] == Note<Tick>{745, 71, 72, 75});
    CHECK(notes[4] == Note<Tick>{817, 143, 62, 75});
    CHECK(notes[5].time == 961);
}

TEST_CASE("test_fermata_models", "[abc][stress]") {
    for (const auto model : {1, 2}) {
        CAPTURE(model);
        const auto [score, warnings] = abc::read(
            fmt::format(
                "{}%%MIDI stressmodel {}\n{}C H[DF] E | G4 |]", kHeader, model, kStressProfile
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

TEST_CASE("test_fixed_rest_fermata", "[abc][stress]") {
    const auto [score, warnings] = abc::read(
        fmt::format("{}{}%%MIDI fermatafixed\nC Hz2 D | E4 |]", kHeader, kStressProfile)
    );
    REQUIRE(warnings.empty());
    const auto& notes = *score.tracks->front()->notes;
    REQUIRE(notes.size() == 3);
    CHECK(notes[1] == Note<Tick>{1921, 671, 62, 80});
    CHECK(notes[2] == Note<Tick>{2593, 1919, 64, 112});
}

TEST_CASE("test_grace_harp_order", "[abc][stress]") {
    const auto [score, warnings] = abc::read(
        kHeader
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

TEST_CASE("test_deferred_grace_profile", "[abc][stress]") {
    const auto [score, warnings] = abc::read(
        fmt::format("{}{}{{c}}[I:MIDI ptstress 2 64 1 61 1] CDEF |]", kHeader, kStressProfile)
    );
    REQUIRE(warnings.empty());
    const auto& notes = *score.tracks->front()->notes;
    REQUIRE(notes.size() == 5);
    CHECK(notes[0] == Note<Tick>{1, 167, 72, 112});
    CHECK(notes[1] == Note<Tick>{169, 359, 60, 64});
    CHECK(notes[2].time == 529);
}

TEST_CASE("test_repeat_beataccents", "[abc][stress]") {
    const auto opening           = kHeader + "%%MIDI beat 105 95 80 2\n";
    const auto [score, warnings] = abc::read(
        fmt::format("{}{}|: CDEF | [I:MIDI beataccents] GABc :|", opening, kStressProfile)
    );
    REQUIRE(warnings.empty());
    CHECK(
        velocities(score)
        == vec<i32>{112, 75, 102, 65, 105, 80, 95, 80, 105, 80, 95, 80, 105, 80, 95, 80}
    );
    const auto& notes = *score.tracks->front()->notes;
    REQUIRE(notes.size() == 16);
    auto next = notes.begin();
    for (const auto bar : {0, 1920, 3840, 5760}) {
        for (const auto pulse : {1, 673, 961, 1537}) CHECK((next++)->time == bar + pulse);
    }
}

TEST_CASE("test_profile_changes", "[abc][stress]") {
    const auto [score, warnings] = abc::read(
        fmt::format(
            "{}CD\n{}EF |\n%%MIDI ptstress 4 63 .5 84 1.5 91 .75 120 1.25\n"
            "GABc | [I:MIDI stressmodel 1] cBAG | [I:MIDI stressmodel 2] FEDC |]",
            kHeader,
            kStressProfile
        )
    );
    REQUIRE(warnings.empty());
    CHECK(
        velocities(score)
        == vec<i32>{112, 75, 102, 65, 63, 84, 91, 120, 63, 84, 91, 120, 63, 84, 91, 120}
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

TEST_CASE("test_profile_voice_order", "[abc][stress]") {
    const auto [score, warnings] = abc::read(
        fmt::format(
            "{}{}[V:1] CDEF |]\n%%MIDI ptstress 4 63 .5 84 1.5 91 .75 120 1.25\n"
            "[V:2] GABc |]",
            kHeader,
            kStressProfile
        )
    );
    REQUIRE(warnings.empty());
    REQUIRE(score.tracks->size() == 2);
    CHECK(velocities(score, 0) == vec<i32>{112, 75, 102, 65});
    CHECK(velocities(score, 1) == vec<i32>{63, 84, 91, 120});
    CHECK(score.tracks->at(0)->notes->at(1).time == 673);
    CHECK(score.tracks->at(1)->notes->at(1).time == 241);
}

TEST_CASE("test_fractional_stress_weight", "[abc][stress]") {
    const auto [score, warnings] = abc::read(
        kHeader
        + "%%MIDI stressmodel 1\n%%MIDI ptstress 4 110 1.5 80 .5 100 1.25 90 .75\n"
          "C7/4 D E/4 F |]"
    );
    REQUIRE(warnings.empty());
    const auto& notes = *score.tracks->front()->notes;
    REQUIRE(notes.size() == 4);
    CHECK(notes[1] == Note<Tick>{841, 339, 62, 80});
    CHECK(notes[2].time == 1321);
}

TEST_CASE("test_long_note_gate", "[abc][stress]") {
    const auto [score, warnings]
        = abc::read(fmt::format("{}%%MIDI stressmodel 1\n{}C3 D |]", kHeader, kStressProfile));
    REQUIRE(warnings.empty());
    const auto& notes = *score.tracks->front()->notes;
    REQUIRE(notes.size() == 2);
    CHECK(notes[0] == Note<Tick>{1, 1439, 60, 90});
    CHECK(notes[1] == Note<Tick>{1441, 277, 62, 65});
}

TEST_CASE("test_normalized_segments", "[abc][stress]") {
    const auto [score, warnings]
        = abc::read(kHeader + "%%MIDI ptstress 2 110 2 70 .5\nCDEF | G4 |]");
    REQUIRE(warnings.empty());
    const auto& notes = *score.tracks->front()->notes;
    REQUIRE(notes.size() == 5);
    CHECK(notes[2] == Note<Tick>{1537, 191, 64, 70});
    CHECK(notes[3] == Note<Tick>{1729, 191, 65, 70});
    CHECK(notes[4].time == 1921);
}

TEST_CASE("test_changing_meter", "[abc][stress]") {
    const auto [score, warnings]
        = abc::read(fmt::format("{}{}CDEF | [M:3/4] GAB | c3 |]", kHeader, kStressProfile));
    REQUIRE(warnings.empty());
    const auto& notes = *score.tracks->front()->notes;
    REQUIRE(notes.size() == 8);
    CHECK(notes[4].time == 1921);
    CHECK(notes[5].time == 2497);
    CHECK(notes[6].time == 2929);
    CHECK(notes[7].time == 3361);
}

TEST_CASE("test_invalid_stress_alias", "[abc][stress]") {
    const auto opening           = kHeader + "%%MIDI beat 105 95 80 2\n";
    const auto [score, warnings] = abc::read(
        opening
        + "%%MIDIdef good ptstress 4 112 1.4 75 .6 102 1.2 65 .8\n"
          "%%MIDIdef regular beataccents\n%%MIDIdef bad ptstress 0\n"
          "%%MIDIx good regular bad\nCDEF |]"
    );
    REQUIRE(warnings.size() == 1);
    CHECK(warnings.front().find("[invalid-stress-profile]") != std::string::npos);
    CHECK(velocities(score) == vec<i32>{105, 80, 95, 80});
    CHECK(score.tracks->front()->notes->at(1).time == 673);
}

TEST_CASE("test_relative_stress_file", "[abc][stress]") {
    const auto [score, warnings] = abc::read_file("testcases/abc_headers/stress.abc");
    REQUIRE(warnings.size() == 1);
    CHECK(warnings.front().find("[stress-file]") != std::string::npos);
    CHECK(velocities(score) == vec<i32>{112, 75, 102, 65});
    CHECK(score.tracks->front()->notes->at(1).time == 673);
}

TEST_CASE("test_overlapping_member_weight", "[abc][stress]") {
    const auto [score, warnings] = abc::read(
        fmt::format(
            "%abc-2.1\n{}%%MIDI stressmodel 1\n{}%%MIDI chordattack 0\n[CE2-] E D2 |]",
            kHeader,
            kStressProfile
        )
    );
    REQUIRE(warnings.empty());
    const auto& notes = *score.tracks->front()->notes;
    REQUIRE(notes.size() == 3);
    CHECK(notes[1] == Note<Tick>{1, 959, 64, 112});
    CHECK(notes[2].time == 961);
}

}   // namespace symusic::abc_test

#include <algorithm>

#include <fmt/format.h>

#include "abc_test.h"

namespace symusic::abc_test {

static vec<Notes> drum_tracks(const Score<Tick>& score) {
    vec<Notes> tracks;
    for (const auto& track : *score.tracks)
        if (track->is_drum) tracks.push_back(note_spans(*track));
    return tracks;
}

TEST_CASE("test_slash_chord_tracks", "[abc][accompaniment]") {
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

TEST_CASE("test_chord_unicode_accidentals", "[abc][accompaniment]") {
    const auto [expected, direct_warnings]
        = abc::read(kStrictHeader + "\"Bb/D\"CDEF | \"G/Bb\"GABc | \"F#/C#\"defg |]");
    REQUIRE(direct_warnings.empty());
    REQUIRE(expected.tracks->size() == 3);

    const auto [score, warnings] = abc::read(
        kHeader
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

TEST_CASE("test_chord_accidental_register", "[abc][accompaniment]") {
    const auto [score, warnings] = abc::read(
        "X:1\nT:\nM:4/4\nL:1/4\nK:C\n%%MIDI gchord fc\n"
        "\"Cb\"C4 | \"B#\"D4 | \"Cbb\"E4 | \"B##\"F4 | \"D/cbb\"G4 |]"
    );
    REQUIRE(warnings.empty());
    REQUIRE(score.tracks->size() == 3);
    vec<i32> bass;
    vec<i32> chords;
    for (const auto& note : *score.tracks->at(1)->notes) bass.push_back(note.pitch);
    for (const auto& note : *score.tracks->at(2)->notes) chords.push_back(note.pitch);
    CHECK(bass == vec<i32>{35, 48, 34, 49, 34});
    CHECK(chords == vec<i32>{47, 51, 54, 60, 64, 67, 46, 50, 53, 61, 65, 68, 50, 54, 57});
}

TEST_CASE("test_slash_bass_octave", "[abc][accompaniment]") {
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

TEST_CASE("test_chord_quality_aliases", "[abc][accompaniment]") {
    // Section 4.18 applies the aliases to the quality, not just to bare triads.
    for (const auto& [alias, canonical] : {
             std::pair{"Amin7", "Am7"},
             {"Amin9", "Am9"},
             {"Amaj6", "A6"},
             {"A+7/c#(Em)", "Aaug7/C#"},
         }) {
        CAPTURE(alias, canonical);
        const auto [actual, warnings] = abc::read(kStrictHeader + '"' + alias + "\"C4 |]");
        const auto expected           = abc::read(kStrictHeader + '"' + canonical + "\"C4 |]");
        REQUIRE(warnings.empty());
        REQUIRE(expected.warnings.empty());
        REQUIRE(expected.value.tracks->size() == 3);
        CHECK(actual == expected.value);
    }
}

TEST_CASE("test_chordname_precedence", "[abc][accompaniment]") {
    const auto [score, warnings] = abc::read(
        "%abc-2.1\nX:1\nT:\nM:4/4\nL:1/4\nK:C\n"
        "%%MIDI gchord fc\n%%MIDI chordname min7 0 5 7\n\"Amin7\"C4 | \"Am7\"D4 |]"
    );
    REQUIRE(warnings.empty());
    REQUIRE(score.tracks->size() == 3);
    vec<i32> alias;
    vec<i32> canonical;
    for (const auto& note : *score.tracks->at(2)->notes) {
        auto& pitches = note.time < 1920 ? alias : canonical;
        pitches.push_back(note.pitch);
    }
    CHECK(alias == vec<i32>{57, 62, 64});
    CHECK(canonical == vec<i32>{57, 60, 64, 67});
}

TEST_CASE("test_macro_chordname_order", "[abc][accompaniment]") {
    const std::string header = "%abc-2.1\nX:1\nT:Chord names\nM:1/4\nL:1/4\nK:C\n%%MIDI gchord c\n";
    const std::string music  = "\"Cfoo\" C | [I:MIDI chordname foo 0 3 7] \"Cfoo\" D |]";
    const auto        actual = abc::read(header + "m: ~C2 = " + music + "\n~C2");
    const auto        expected = abc::read(header + music);
    REQUIRE(actual.warnings.size() == 1);
    REQUIRE(expected.warnings.size() == 1);
    CHECK(actual.warnings.front().find("[unknown-chord]") != std::string::npos);
    CHECK(actual.value == expected.value);
    REQUIRE(actual.value.tracks->size() == 2);
    CHECK(pitches(*actual.value.tracks->at(1)) == vec<i32>{48, 52, 55, 48, 51, 55});
}

TEST_CASE("test_extended_inversions", "[abc][accompaniment]") {
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
    vec<i32> inversion;
    vec<i32> added_bass;
    for (const auto& note : *score.tracks->at(2)->notes) {
        auto& pitches = note.time < 1920 ? inversion : added_bass;
        pitches.push_back(note.pitch);
    }
    CHECK(inversion == vec<i32>{62, 64, 67, 70, 72});
    CHECK(added_bass == vec<i32>{48, 52, 55, 58, 62});
}

TEST_CASE("test_multimeasure_backing", "[abc][accompaniment]") {
    const auto opening
        = kStrictHeader + "%%MIDI drum dddd 36 37 38 39 91 92 93 94\n%%MIDI drumon\n";
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

TEST_CASE("test_obsolete_drum_marks_are_ignored", "[abc][accompaniment]") {
    const auto opening = kHeader + "%%MIDI drum dddd 36 37 38 39 91 92 93 94\n";
    for (const auto& [plain, marked] :
         {std::pair{"CDEF | GABc |]", "!nodrum!CDEF | !drum!GABc |]"},
          std::pair{"[CE]DEF | GA [Bc] c |]", "[!nodrum!CE]DEF | GA [B!drum!c] c |]"},
          std::pair{"{[CE]} DEFG | ABcd |]", "{[!nodrum!CE]} DEFG | !drum!ABcd |]"}}) {
        CAPTURE(marked);
        const auto expected = abc::read(opening + "[I:MIDI drumon] " + plain);
        const auto actual   = abc::read(opening + "[I:MIDI drumon] " + marked);
        REQUIRE(expected.warnings.empty());
        REQUIRE(actual.warnings.size() == 2);
        CHECK(actual.warnings[0].find("[ignored-decoration]") != std::string::npos);
        CHECK(actual.warnings[1].find("[ignored-decoration]") != std::string::npos);
        CHECK(actual.value == expected.value);
    }

    const auto mapping  = kHeader + "%%MIDI channel 10\n%%MIDI drummap C 35\n";
    const auto expected = abc::read(mapping + "[C] [I:MIDI drummap C 38] C |]");
    const auto actual   = abc::read(mapping + "[!drum!C] [I:MIDI drummap C 38] C |]");
    REQUIRE(expected.warnings.empty());
    REQUIRE(actual.warnings.size() == 1);
    CHECK(actual.warnings.front().find("[ignored-decoration]") != std::string::npos);
    CHECK(actual.value == expected.value);
}

TEST_CASE("test_overlay_drum_routing", "[abc][accompaniment]") {
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

TEST_CASE("test_drum_map_inheritance", "[abc][accompaniment]") {
    const auto [score, warnings] = abc::read(
        "%abc-2.1\n%%MIDI drummap E 42\nX:1\nT:Drum passes\nM:4/4\nL:1/4\nK:C\n"
        "V:1\n%%MIDI channel 10\nCC [I:MIDI drummap C 35] CC |]\n"
        "V:2\n%%MIDI channel 10\n%%MIDI drum dddd 60 60 60 64\n%%MIDI drumon\n"
        "C [I:MIDI drummap C 38] C [I:MIDI drummap C 40] CE |]"
    );
    REQUIRE(warnings.empty());
    REQUIRE(score.tracks->size() == 3);
    for (const auto& track : *score.tracks) REQUIRE(track->is_drum);
    // The conductor finishes at C=35. The second melody finishes at C=40,
    // which its drum track inherits before replaying the two timed changes.
    CHECK(pitches(*score.tracks->at(0)) == vec<i32>{35, 35, 35, 35});
    CHECK(pitches(*score.tracks->at(1)) == vec<i32>{35, 38, 40, 42});
    CHECK(pitches(*score.tracks->at(2)) == vec<i32>{40, 38, 40, 42});
}

TEST_CASE("test_overlay_drum_gates", "[abc][accompaniment]") {
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

TEST_CASE("test_overlay_drum_clock", "[abc][accompaniment]") {
    const std::string opening = "X:1\nT:Drum clock\nM:4/4\nL:1/4\nK:C\n%%MIDI drum dd 36 42\n";
    using Hits                = pyvec<Note<Tick>>;
    const auto check_part     = [&](std::string_view body, const Hits& expected) {
        const auto [score, warnings] = abc::read(opening + std::string(body));
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

TEST_CASE("test_coincident_drum_restarts", "[abc][accompaniment]") {
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

TEST_CASE("test_overlay_harmony_gate", "[abc][accompaniment]") {
    const auto [score, warnings] = abc::read(
        "X:1\nT:Overlay harmony\nM:4/4\nL:1/4\nK:C\n"
        "\"C\"C4 & [I:MIDI gchordoff] [I:MIDI chordprog 5] [I:MIDI bassvol 31] \"Am\"E4 |\n"
        "[I:MIDI gchordoff] D4 & [I:MIDI gchordon] F4 | B4 |]"
    );
    REQUIRE(warnings.empty());
    REQUIRE(score.tracks->size() == 6);
    using NoteList   = pyvec<Note<Tick>>;
    const auto track = [&](size_t index) -> const auto& { return *score.tracks->at(index); };
    CHECK(*track(1).notes == NoteList{{1, 239, 36, 80}, {961, 239, 36, 80}});
    CHECK(track(2).program == 0);
    REQUIRE(track(2).notes->size() == 6);
    CHECK(track(2).notes->front() == Note<Tick>{481, 239, 48, 75});
    CHECK(*track(3).notes == NoteList{{1, 1919, 64, 90}, {1921, 1919, 65, 90}});
    CHECK(
        *track(4).notes
        == NoteList{
            {1921, 239, 45, 31}, {2881, 239, 45, 31}, {3841, 239, 45, 31}, {4801, 239, 45, 31}
        }
    );
    CHECK(track(5).program == 4);
    REQUIRE(track(5).notes->size() == 12);
    CHECK(track(5).notes->front() == Note<Tick>{2401, 239, 57, 75});
    CHECK(track(5).notes->back() == Note<Tick>{5281, 239, 64, 75});
}

TEST_CASE("test_overlay_drone_gates", "[abc][accompaniment]") {
    const auto [score, warnings] = abc::read(
        "X:1\nT:Overlay drones\nM:4/4\nL:1/4\nK:C\n"
        "%%MIDI drone 40 47 35 61 62\n"
        "C4 & [I:MIDI droneon] E4 & [I:MIDI droneon] G4 |\n"
        "D4 & [I:MIDI droneoff] F4 & A4 | B4 & G4 & [I:MIDI droneoff] F4 |]"
    );
    REQUIRE(warnings.empty());
    REQUIRE(score.tracks->size() == 5);
    using NoteList     = pyvec<Note<Tick>>;
    const auto& first  = *score.tracks->at(2);
    const auto& second = *score.tracks->at(4);
    CHECK(first.program == 40);
    CHECK(second.program == 40);
    CHECK(*first.notes == NoteList{{2, 1921, 47, 61}, {3, 1920, 35, 62}});
    CHECK(*second.notes == NoteList{{2, 3841, 47, 61}, {3, 3840, 35, 62}});
}

TEST_CASE("test_silent_overlay_backing", "[abc][accompaniment]") {
    const std::string opening = kHeader + "C4 & D4 & [I:MIDI bassprog 12] \"C\"E4 | ";
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

TEST_CASE("test_lowercase_bass_quality", "[abc][accompaniment]") {
    const auto accepted = abc::read(kHeader + "\"c7\" C [Q:1/4=96] D E F |]");
    REQUIRE(accepted.warnings.empty());
    REQUIRE(accepted.value.tracks->size() == 2);
    CHECK(accepted.value.tracks->back()->notes->front().pitch == 36);
    CHECK(accepted.value.tempos->back() == Tempo<Tick>{480, 625000});

    const auto rejected = abc::read(kHeader + "\"cunknown\" C [Q:1/4=96] D E F |]");
    REQUIRE(rejected.warnings.size() == 1);
    CHECK_THAT(rejected.warnings.front(), Catch::Matchers::ContainsSubstring("[unknown-chord]"));
    CHECK(rejected.value.tracks->size() == 1);
    CHECK(rejected.value.tempos->back() == Tempo<Tick>{481, 625000});
}

TEST_CASE("test_chord_pitch_range", "[abc][accompaniment]") {
    const auto [score, warnings] = abc::read(
        kHeader + "%%MIDI chordname wide -2147483640 0 4 2147483647\n\"Cwide/C\" CDEF |]"
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

TEST_CASE("test_full_chord_registry", "[abc][accompaniment]") {
    auto abc_text = kHeader;
    for (i32 i = 0; i < 51; ++i) abc_text += fmt::format("%%MIDI chordname q{} 0\n", i);
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

TEST_CASE("test_backing_channel_effects", "[abc][accompaniment]") {
    const auto [score, warnings] = abc::read(
        kHeader
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
    CHECK(chord.program == 24);
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

TEST_CASE("test_backing_sustain_program", "[abc][accompaniment]") {
    const auto [score, warnings] = abc::read(
        kHeader
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

TEST_CASE("test_backing_effect_order_at_program_change", "[abc][accompaniment]") {
    const auto [score, warnings] = abc::read(
        kHeader
        + "%%MIDI bassprog 12\n\"C\"C "
          "[I:MIDI control bass 64 127] [I:MIDI bassprog 16] "
          "[I:MIDI control bass 64 0] [I:MIDI control bass 7 31] "
          "[I:MIDI control bass 7 32] D3 |]"
    );
    REQUIRE(warnings.empty());
    REQUIRE(score.tracks->size() == 4);
    const auto& first = *score.tracks->at(1);
    const auto& next  = *score.tracks->at(2);
    CHECK(first.program == 12);
    CHECK(next.program == 16);
    CHECK(*first.pedals == pyvec<Pedal<Tick>>{{480, 0}});
    CHECK(next.pedals->empty());
    CHECK(*next.controls == pyvec<ControlChange<Tick>>{{480, 64, 0}, {480, 7, 31}, {480, 7, 32}});
}

TEST_CASE("test_backing_effects_after_grace", "[abc][accompaniment]") {
    const auto [score, warnings] = abc::read(
        kHeader
        + "%%MIDI bassprog 12\n\"C\" [I:MIDI control bass 7 10] C "
          "{d} [I:MIDI control bass 7 20] D [I:MIDI control bass 7 30] E "
          "{f} [I:MIDI control bass 7 40] | [I:MIDI control bass 7 50] F |]"
    );
    REQUIRE(warnings.size() == 1);
    CHECK_THAT(warnings.front(), Catch::Matchers::ContainsSubstring("[orphan-grace]"));
    REQUIRE(score.tracks->size() == 3);
    const auto& bass = *score.tracks->at(1);
    REQUIRE(bass.program == 12);
    CHECK(
        *bass.controls
        == pyvec<ControlChange<Tick>>{
            {0, 7, 10}, {600, 7, 20}, {960, 7, 30}, {1440, 7, 40}, {1440, 7, 50}
        }
    );
}

TEST_CASE("test_backing_effects_after_silent_overlay_repeat", "[abc][accompaniment]") {
    const auto [score, warnings] = abc::read(
        kHeader
        + "C4 & E4 & [I:MIDI bassprog 12] [I:MIDI control bass 64 127] \"C\"G4 "
          "|: z4 | (& z4 | z4 & z4 | z4 &) | "
          "z4 & z4 & {d} [I:MIDI control bass 64 0] [I:MIDI control bass 64 127] A4 :|"
    );
    REQUIRE(warnings.empty());
    const auto bass = std::ranges::find_if(*score.tracks, [](const auto& track) {
        return track->program == 12;
    });
    REQUIRE(bass != score.tracks->end());
    CHECK(
        *(*bass)->controls
        == pyvec<ControlChange<Tick>>{
            {0, 64, 127}, {7800, 64, 0}, {7800, 64, 127}, {15480, 64, 0}, {15480, 64, 127}
        }
    );
    CHECK(*(*bass)->pedals == pyvec<Pedal<Tick>>{{0, 7800}, {7800, 7680}});
}

TEST_CASE("test_overlapping_member_backing", "[abc][accompaniment]") {
    const auto opening
        = kStrictHeader + "%%MIDI chordattack 0\n%%MIDI drum dddd 36 37 38 39\n%%MIDI drumon\n";
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

TEST_CASE("test_repeat_guitar_meter", "[abc][accompaniment]") {
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

TEST_CASE("test_midbar_repeat_meter", "[abc][accompaniment]") {
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

TEST_CASE("test_pattern_span_range", "[abc][accompaniment]") {
    const auto check = [](std::string_view command, std::string_view music) {
        const auto read = [&](i32 bars) {
            return abc::read(fmt::format("{}%%MIDI {} {}\n{}", kHeader, command, bars, music));
        };
        const auto single_bar = read(1);
        REQUIRE(single_bar.warnings.empty());
        REQUIRE(single_bar.value.tracks->size() > 1);
        for (const i32 count : {0, -1, 11}) {
            CAPTURE(command, count);
            const auto fallback = read(count);
            CHECK(fallback.warnings.empty());
            CHECK(fallback.value == single_bar.value);
        }
    };
    check("gchordbars", "%%MIDI gchord fghc\n\"C\"CDEF | GABc |]");
    check("drumbars", "%%MIDI drum dddd 36 37 38 39\n%%MIDI drumon\nCDEF | GABc |]");
}

TEST_CASE("test_multibar_chord_slots", "[abc][accompaniment]") {
    const auto [score, warnings] = abc::read(
        kHeader + "%%MIDI gchordbars 10\n%%MIDI gchord fghc\n\"C\"CDEF | GABc | cBAG | FEDC |]"
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

TEST_CASE("test_backing_release_gap", "[abc][accompaniment]") {
    for (const auto& [setup, track] : {
             std::pair{"%%MIDI gchordbars 10\n%%MIDI gchord fghc\n\"C\"", 1u},
             {"%%MIDI drumbars 10\n%%MIDI drum dddd 36 37 38 39\n%%MIDI drumon\n", 1u},
         }) {
        CAPTURE(setup);
        const auto [score, warnings] = abc::read(
            kHeader + "%%MIDI chordattack 4\n" + setup + "CDEF | GABc | cBAG | FEDC |]"
        );
        REQUIRE(warnings.empty());
        REQUIRE(score.tracks->size() > track);
        REQUIRE(!score.tracks->at(track)->notes->empty());
        CHECK(score.tracks->at(track)->notes->front().duration == 4791);
    }
}

TEST_CASE("test_delayed_drone_program", "[abc][accompaniment]") {
    const auto [score, warnings]
        = abc::read(kHeader + "C [I:MIDI droneon] D E [I:MIDI droneoff] F |]");
    REQUIRE(warnings.empty());
    REQUIRE(score.tracks->size() == 2);
    CHECK(
        score.tracks->back()->notes->collect()
        == vec<Note<Tick>>{{962, 1441, 45, 80}, {1443, 960, 33, 80}}
    );
}

}   // namespace symusic::abc_test

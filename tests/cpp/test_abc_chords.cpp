#include <algorithm>

#include <fmt/format.h>

#include "abc_test.h"

namespace symusic::abc_test {

TEST_CASE("test_unison_members", "[abc][chords]") {
    // Section 4.17 gives the two heads one written onset and duration.
    const auto [score, warnings]
        = abc::read(kStrictHeader + "%%MIDI chordattack 0\n[D2D2]2 A |]\nw: one two");
    REQUIRE(warnings.empty());
    CHECK(melody(score) == Notes{{1, 1919, 62}, {1, 1919, 62}, {1921, 479, 69}});
    CHECK(sung(score) == Lyrics{{1, "one"}, {1921, "two"}});
}

TEST_CASE("test_staggered_unison", "[abc][chords]") {
    for (const auto& [setup, chord, gap] : {
             std::tuple{"", "[DED]", 10},
             {"%%MIDI chordattack 4\n", "[DED]", 4},
             {"%%MIDI chordattack 4\n", "!arpeggio![DED]", 12},
         }) {
        CAPTURE(setup, chord);
        const auto [score, warnings] = abc::read(kStrictHeader + setup + chord + " A |]");
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

TEST_CASE("test_staggered_pending_releases", "[abc][chords]") {
    for (const auto version : {"", "%abc-2.1\n"}) {
        for (const auto gap : {0, 4, 10}) {
            CAPTURE(version, gap);
            const auto [score, warnings] = abc::read(
                version + kHeader
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

TEST_CASE("test_expanded_member_release", "[abc][chords]") {
    const auto [score, warnings] = abc::read(
        kStrictHeader + "%%MIDI chordattack 10\n%%MIDI expand 1/24\nC [DFA] {c}E G | z8 |]"
    );
    REQUIRE(warnings.empty());
    const auto& notes = *score.tracks->front()->notes;
    REQUIRE(notes.size() == 7);
    CHECK(notes.at(3).end() == 980);
    CHECK(notes.at(4).time == 961);
    CHECK(notes.at(5).time == 1081);
    CHECK(notes.at(6).time == 1441);
}

TEST_CASE("test_random_chord_spacing", "[abc][chords]") {
    const auto source = kHeader
                        + "%%MIDI chordattack 4\n%%MIDI randomchordattack 10\n"
                          "|: [CEG] [DFA] :| [I:MIDI randomchordattack 0] [CEG] D |]";
    const auto played = abc::read(source);
    REQUIRE(played.warnings.empty());
    REQUIRE(played.value.tracks->size() == 1);
    const auto& notes = *played.value.tracks->front()->notes;
    REQUIRE(notes.size() == 16);
    vec<i32> gaps;
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

TEST_CASE("test_staggered_ties", "[abc][chords]") {
    const auto [score, warnings] = abc::read(kHeader + "%%MIDI chordattack 4\n[CEG-] [FAG] D E |]");
    REQUIRE(warnings.empty());
    const auto&              notes = *score.tracks->front()->notes;
    vec<std::pair<i32, i32>> attacks;
    for (const auto& note : notes) attacks.emplace_back(note.pitch, note.time);
    CHECK(
        attacks
        == vec<std::pair<i32, i32>>{
            {60, 1}, {64, 5}, {67, 5}, {65, 481}, {69, 485}, {62, 961}, {64, 1441}
        }
    );
    CHECK(notes[2].end() == 960);
}

TEST_CASE("test_route_transposition", "[abc][chords]") {
    const auto [score, warnings] = abc::read(
        kHeader
        + "%%MIDI transpose 5\n%%MIDI channel 10\n%%MIDI makechordchannels 1\n"
          "[CE] D [FGA] E |]"
    );
    REQUIRE(warnings.empty());
    REQUIRE(score.tracks->size() == 2);
    using Row = std::tuple<i32, i32, i32>;
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

TEST_CASE("test_tied_member_route", "[abc][chords]") {
    const auto [score, warnings] = abc::read(
        kHeader
        + "%%MIDI transpose 5\nA B [I:MIDI channel 10] "
          "[I:MIDI makechordchannels 1] [CE^F-] [FA_G] |]"
    );
    REQUIRE(warnings.empty());
    const auto extra = std::ranges::find_if(*score.tracks, [](const auto& track) {
        return !track->is_drum && !track->notes->empty() && track->notes->front().time > 1;
    });
    REQUIRE(extra != score.tracks->end());
    using Row = std::tuple<i32, i32, i32>;
    vec<Row> notes;
    for (const auto& note : *(*extra)->notes)
        notes.emplace_back(note.time, note.duration, note.pitch);
    CHECK(notes == vec<Row>{{971, 469, 69}, {971, 949, 71}, {1451, 469, 71}});
}

TEST_CASE("test_short_staggered_members", "[abc][chords]") {
    const auto [score, warnings]
        = abc::read(kStrictHeader + "%%MIDI chordattack 90\n[CE.G]/4 A B c |]");
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

TEST_CASE("test_member_length_factors", "[abc][chords]") {
    for (const auto& version : kVersionLines) {
        CAPTURE(version);
        // ABC 2.1 section 4.17: the first member sets the chord's step duration.
        const auto intro = version + kHeader + "%%MIDI chordattack 0\n";
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
}

TEST_CASE("test_member_rhythm_and_grace", "[abc][chords]") {
    const auto intro = kStrictHeader + "%%MIDI chordattack 0\n";
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

TEST_CASE("test_unequal_member_lengths", "[abc][chords]") {
    const auto [score, warnings] = abc::read(kHeader + "%%MIDI chordattack 0\n[CE3] D |]");
    REQUIRE(warnings.empty());
    const auto& notes = *score.tracks->front()->notes;
    REQUIRE(notes.size() == 3);
    CHECK(notes[0].duration == 479);
    CHECK(notes[1].duration == 1439);
    CHECK(notes[2].time == 481);
}

TEST_CASE("test_overlapping_tied_members", "[abc][chords]") {
    const auto opening = kStrictHeader + "%%MIDI chordattack 0\n";
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

TEST_CASE("test_percussion_member_release", "[abc][chords]") {
    const auto [score, warnings]
        = abc::read(kStrictHeader + "%%MIDI channel 10\n%%MIDI chordattack 0\n[CE8] |]");
    REQUIRE(warnings.empty());
    REQUIRE(score.tracks->size() == 1);
    const auto& track = *score.tracks->front();
    REQUIRE(track.is_drum);
    REQUIRE(track.notes->size() == 2);
    CHECK(track.notes->at(0) == Note<Tick>{1, 479, 60, 90});
    CHECK(track.notes->at(1) == Note<Tick>{1, 3839, 64, 90});
}

TEST_CASE("test_equal_member_factors", "[abc][chords]") {
    const auto [score, warnings] = abc::read(kHeader + "%%MIDI chordattack 0\n[C2E2G2]3/2 D");
    REQUIRE(warnings.empty());
    const auto& notes = *score.tracks->front()->notes;
    REQUIRE(notes.size() == 4);
    for (size_t i = 0; i < 3; ++i) CHECK(notes[i].duration == 1439);
    CHECK(notes.back().time == 1441);
}

TEST_CASE("test_member_fingerings", "[abc][chords]") {
    const auto opening            = kStrictHeader + "%%MIDI chordattack 0\n";
    const auto [marked, warnings] = abc::read(opening + "[!1!C!3!E!5!G] D |]");
    REQUIRE(warnings.empty());
    CHECK(marked == abc::read(opening + "[CEG] D |]").value);
}

TEST_CASE("test_member_staccato", "[abc][chords]") {
    const auto opening = kStrictHeader + "%%MIDI chordattack 0\n";
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
        for (const i32 pitch : {60, 64, 67}) {
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

TEST_CASE("test_member_tenuto", "[abc][chords]") {
    const auto opening = kStrictHeader + "%%MIDI chordattack 0\n%%MIDI trim 1/4\n";
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
        kStrictHeader + "%%MIDI chordattack 0\n%%MIDI expand 1/4\n[CE]-[C!tenuto!E] G z |]"
    );
    REQUIRE(warnings.empty());
    const auto& notes = *tied.tracks->front()->notes;
    REQUIRE(notes.size() == 3);
    CHECK(notes[0].pitch == 60);
    // Both ties keep their joined gate. The following G receives expand.
    CHECK(notes[0].duration == 959);
    CHECK(notes[1].pitch == 64);
    CHECK(notes[1].duration == 959);
    CHECK(notes[2].time == 961);
    CHECK(notes[2].duration == 599);
}

TEST_CASE("test_member_dynamics", "[abc][chords]") {
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

TEST_CASE("test_queued_member_shorthand", "[abc][chords]") {
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

TEST_CASE("test_continued_chord_member_setting", "[abc][chords]") {
    const auto result = abc::read(
        kStrictHeader
        + "%%MIDI chordattack 0\n"
          "[!<(!C\\\nI:MIDI deltaloudness 30\n!<(!E] G"
    );
    CHECK(result.warnings.empty());
    CHECK(velocities(result.value) == vec<i32>{105, 127, 125});
}

TEST_CASE("test_grace_chord_decorations", "[abc][chords]") {
    const auto [score, warnings]
        = abc::read(kStrictHeader + "%%MIDI chordattack 0\n{[C.!p!E]} G D");
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

TEST_CASE("test_terminal_member_tie", "[abc][chords]") {
    const auto [score, warnings] = abc::read(kHeader + "%%MIDI chordattack 0\n[CE]- [C.E] G");
    REQUIRE(warnings.empty());
    const auto& notes = *score.tracks->front()->notes;
    REQUIRE(notes.size() == 3);
    CHECK(notes[0].duration == 959);
    CHECK(notes[1].duration == 719);
    CHECK(notes[2].time == 961);
}

TEST_CASE("test_grace_chord_lengths", "[abc][chords]") {
    const auto [score, warnings]
        = abc::read(kStrictHeader + "%%MIDI chordattack 0\n{[C2E]2 G}A4 B");
    REQUIRE(warnings.empty());
    const auto& notes = *score.tracks->front()->notes;
    REQUIRE(notes.size() == 5);
    CHECK(notes[0].duration == 479);
    CHECK(notes[1].duration == 239);
    CHECK(notes[2].time == 481);
    CHECK(notes[3].time == 601);
    CHECK(notes[4].time == 1921);
}

TEST_CASE("test_track_order_spacing", "[abc][chords]") {
    const auto [score, warnings]
        = abc::read(kHeader + "[V:one] [CEG] [I:MIDI chordattack 4] DEF |]\n[V:two] [GBe] ABc |]");
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

}   // namespace symusic::abc_test

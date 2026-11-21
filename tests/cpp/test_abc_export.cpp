#include <algorithm>
#include <array>
#include <memory>

#include <fmt/format.h>

#include "symusic/conversion.h"
#include "abc_test.h"

namespace symusic::abc_test {

TEST_CASE("test_lossy_events", "[abc][export]") {
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

TEST_CASE("test_floating_duration_validation", "[abc][export]") {
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

TEST_CASE("test_comma53_roundtrip", "[abc][export]") {
    const auto original
        = abc::read(kHeader + "%%MIDI tuningsystem comma53\nC ^/D E F | G A B c |]");
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

TEST_CASE("test_version_and_creator", "[abc][export]") {
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

TEST_CASE("test_opening_free_meter", "[abc][export]") {
    auto score             = abc::read(kHeader + "[V:a] CDEF |]\n[V:b] G2 A2 |]").value;
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

TEST_CASE("test_per_voice_fields", "[abc][export]") {
    auto score = abc::read(kHeader + "[V:a] C D |]\n[V:b] G4 |]\n[V:c] E |]").value;
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

TEST_CASE("test_tempo_range", "[abc][export]") {
    auto score    = abc::read(kHeader + "CDEF |]").value;
    *score.tempos = {{0, 16'777'215}, {480, 7'689'274}, {960, 1}, {1440, 500'000}};

    const auto [text, warnings]        = abc::write(score);
    const auto [loaded, read_warnings] = abc::read(text);
    CHECK(warnings.empty());
    CHECK(read_warnings.empty());
    CHECK(*loaded.tempos == *score.tempos);
    REQUIRE(loaded.tracks->size() == 1);
    CHECK(*loaded.tracks->front()->notes == *score.tracks->front()->notes);
}

TEST_CASE("test_quoted_voice_names", "[abc][export]") {
    auto              score     = abc::read(kHeader + "C").value;
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

TEST_CASE("test_literal_dollars", "[abc][export]") {
    auto        score = abc::read(kHeader + "C").value;
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

TEST_CASE("test_text_controls", "[abc][export]") {
    auto        score = abc::read(kHeader + "C").value;
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

TEST_CASE("test_written_lyrics", "[abc][export]") {
    auto  score = abc::read(
                      kHeader
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

TEST_CASE("test_held_note_lyrics", "[abc][export]") {
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

TEST_CASE("tied release at a same-pitch attack keeps the event clock", "[abc][export]") {
    for (const bool drums : {false, true}) {
        CAPTURE(drums);
        auto  score     = abc::read(kHeader + "C").value;
        auto& track     = *score.tracks->front();
        track.is_drum   = drums;
        *track.notes    = {{1, 480, 60, 90}, {481, 30, 60, 73}, {481, 30, 64, 73}};
        *track.controls = {{110, 7, 99}, {480, 1, 64}, {481, 7, 85}};
        *track.lyrics   = {{221, "held"}, {481, "again"}};

        const auto [loaded, warnings] = abc::read(abc::write(score).value);
        REQUIRE(warnings.empty());
        REQUIRE(loaded.tracks->size() == 1);
        const auto& played = *loaded.tracks->front();
        CHECK(*played.notes == *track.notes);
        CHECK(*played.controls == *track.controls);
        CHECK(*played.lyrics == *track.lyrics);
    }
}

TEST_CASE("test_lyric_split_warning", "[abc][export]") {
    auto  score  = abc::read(kHeader + "C D E |]").value;
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

TEST_CASE("test_same_pitch_reattack", "[abc][export]") {
    for (const i32 duration : {1, 2, 240}) {
        CAPTURE(duration);
        auto  score = abc::read(kHeader + "C").value;
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

TEST_CASE("test_boundary_tie_warning", "[abc][export]") {
    auto  score = abc::read(kHeader + "C").value;
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

TEST_CASE("test_long_hold_warning", "[abc][export]") {
    auto  score   = abc::read(kHeader + "C").value;
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

TEST_CASE("test_constrained_onset_retry", "[abc][export]") {
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

TEST_CASE("test_placement_retry_limit", "[abc][export]") {
    struct LaneRange {
        i32  first;
        i32  last;
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

TEST_CASE("test_voice_capacity", "[abc][export]") {
    for (const auto [tpq, melodic, drums] :
         {std::tuple{480, 16, 0}, std::tuple{960, 16, 0}, std::tuple{480, 15, 16}}) {
        CAPTURE(tpq, melodic, drums);
        Score<Tick> score(tpq);
        for (i32 index = 0; index < melodic + drums; ++index) {
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

TEST_CASE("test_split_voice_lyrics", "[abc][export]") {
    auto  score = abc::read(kHeader + "C4 z2 D").value;
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

}   // namespace symusic::abc_test

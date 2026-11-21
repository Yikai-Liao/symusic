#include <type_traits>
#include <utility>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "abc_test.h"
#include "../../src/io/abc/notation.h"
#include "../../src/io/abc/playback.h"

namespace symusic::abc_test {

TEST_CASE("test_macro_written_settings", "[abc][written]") {
    const auto opening = kStrictHeader + std::string(kStressProfile);
    for (const std::string_view music : {
             "C [I:MIDI C 61] C [I:MIDI C 60] C",
             "H C2 [I:MIDI fermatafixed] H D2 [I:MIDI fermataproportional] H E2",
             "{c}C [I:MIDI gracedivider 8] {d}D [I:MIDI gracedivider 4] {e}E",
             "~C3 [I:MIDI harpmode 1] ~D3 [I:MIDI harpmode 0] ~E3",
             "!<(!C [I:MIDI deltaloudness 30] !<(!D [I:MIDI deltaloudness 5] !<(!E",
             "C [I:MIDI ptstress 2 40 1 80 1] D [I:MIDI ptstress 2 110 1 100 1] E",
             "C [I:MIDI stressmodel 1] D [I:MIDI stressmodel 2] E",
         }) {
        CAPTURE(music);
        const auto actual   = abc::read(opening + "m: ~C2 = " + std::string(music) + "\n~C2");
        const auto expected = abc::read(opening + std::string(music));
        CHECK(actual.warnings.empty());
        CHECK(expected.warnings.empty());
        CHECK(actual.value == expected.value);
    }
}

TEST_CASE("test_written_settings_voice_and_repeat_order", "[abc][written]") {
    const std::string music    = "[V:one] |: ^F f [V:two] [I:propagate-accidentals octave] C "
                                 "[V:one] ^F f :| [V:two] D |]";
    const auto        actual   = abc::read(kStrictHeader + "m: ~C2 = " + music + "\n~C2");
    const auto        expected = abc::read(kStrictHeader + music);
    CHECK(actual.warnings.empty());
    CHECK(expected.warnings.empty());
    CHECK(actual.value == expected.value);
    REQUIRE(actual.value.tracks->size() >= 2);
    CHECK(pitches(*actual.value.tracks->at(0)) == vec<i32>{66, 78, 66, 78, 66, 78, 66, 78});
}

TEST_CASE("test_written_setting_diagnostic_identity", "[abc][written]") {
    const auto result = abc::read(
        kStrictHeader
            + "m: ~C2 = [I:propagate-accidentals invalid] C "
              "[I:propagate-accidentals invalid] D\n~C2 ~C2",
        "identity.abc"
    );
    REQUIRE(result.warnings.size() == 2);
    CHECK(result.warnings[0].starts_with("identity.abc:8:1: ABC [invalid-accidental-scope]"));
    CHECK(result.warnings[1].starts_with("identity.abc:8:5: ABC [invalid-accidental-scope]"));
}

TEST_CASE("test_macro_continued_chord_member_settings", "[abc][written]") {
    const std::string opening = kStrictHeader + "%%MIDI chordattack 0\n";
    const auto        actual
        = abc::read(opening + "m: ~C2 = [!<(!C\n~C2\\\nI:MIDI deltaloudness 30\n!<(!E] G");
    const auto expected = abc::read(opening + "[!<(!C\\\nI:MIDI deltaloudness 30\n!<(!E] G");
    CHECK(actual.warnings.empty());
    CHECK(expected.warnings.empty());
    CHECK(actual.value == expected.value);
    CHECK(velocities(actual.value) == vec<i32>{105, 127, 125});
}

}   // namespace symusic::abc_test

namespace symusic::abc::detail {
namespace {

static_assert(!std::is_copy_constructible_v<Tune>);
static_assert(!std::is_copy_assignable_v<Tune>);
static_assert(std::is_move_constructible_v<Tune>);

TEST_CASE("test_source_text_ownership", "[abc][written]") {
    std::string source = "X:1\nT:Original\nK:C\nC";
    Diagnostics diagnostics("source.abc");
    auto        tune = tokenize(source, {}, diagnostics);
    source.assign(source.size(), 'x');

    const auto title = std::ranges::find_if(tune.header, [](const Token& token) {
        const auto* field = std::get_if<Field>(&token.value);
        return field && field->kind == 'T';
    });
    REQUIRE(title != tune.header.end());
    CHECK(std::get<Field>(title->value).value == "Original");
    CHECK(diagnostics.take().empty());
}

TEST_CASE("test_decoration_spelling", "[abc][written]") {
    CHECK(ornament_flag("T") == &Ornament::trill);
    CHECK(ornament_flag("!T!") == nullptr);
    CHECK(ornament_flag("!>!") == &Ornament::accent);
    CHECK(read_mark("+drum+") == "drum");
}

TEST_CASE("test_beat_string_owns_its_value", "[abc][written]") {
    Diagnostics diagnostics("beat.abc");
    Expression  expression;
    std::string value = "pm";
    REQUIRE(expression.directive("beatstring", value, kTpq, {4, 4}, {}, diagnostics));
    value.assign("ff");
    CHECK(expression.attack_velocity(0, {4, 4}, false, 0).value == 65);
    CHECK(expression.attack_velocity(2 * kTpq, {4, 4}, false, 0).value == 80);
    CHECK(diagnostics.take().empty());
}

TEST_CASE("test_voice_spec_owns_its_text", "[abc][written]") {
    Diagnostics diagnostics("voice.abc");
    std::string text = "part name=Melody";
    const auto  spec = voice_spec(text, diagnostics, {});
    text.assign(text.size(), 'x');
    CHECK(spec.id == "part");
    CHECK(spec.name == "Melody");
    CHECK(diagnostics.take().empty());
}

TEST_CASE("test_macro_stress_alias_invocations", "[abc][written][stress]") {
    const std::string source
        = "%abc-2.1\nX:1\nT:\nM:4/4\nL:1/4\nK:C\n"
          "%%MIDIdef profile ptstress profile.txt\n"
          "m: ~C2 = [I:MIDIx profile profile] C [I:MIDIx profile profile] D\n~C2";
    Diagnostics   diagnostics("alias.abc");
    auto          tune = tokenize(source, {}, diagnostics);
    vec<Location> notes;
    for (const auto& token : tune.body)
        if (std::holds_alternative<Atom>(token.value)) notes.push_back(token.at);
    REQUIRE(notes.size() == 2);
    CHECK(notes[0] == notes[1]);
    CHECK(notes[0].written < notes[1].written);

    size_t           reads = 0;
    const TextLoader load
        = [&](std::string_view name, std::span<const std::string_view>) -> TextRead {
        CHECK(name == "profile.txt");
        ++reads;
        return ExternalText{std::string(name), reads == 1 ? "1 40 1" : "1 100 1"};
    };
    const TuneStress stress(tune, load, diagnostics);
    CHECK(diagnostics.take().empty());
    CHECK(reads == 2);
    CHECK(stress.note(notes[0], 0, kTpq, Meter{4, 4}).velocity == 40);
    CHECK(stress.note(notes[1], 0, kTpq, Meter{4, 4}).velocity == 100);
}

TEST_CASE("test_failed_stress_alias_cached", "[abc][written][stress]") {
    std::string source = "%abc-2.1\nX:1\nT:\nM:4/4\nL:1/4\nK:C\n"
                         "%%MIDIdef bad ptstress missing.txt\n%%MIDIx";
    for (size_t i = 0; i < 128; ++i) source += " bad";
    source += "\n%%MIDI ptstress valid.txt\nC";
    Diagnostics      diagnostics("alias.abc");
    auto             tune  = tokenize(source, {}, diagnostics);
    size_t           reads = 0;
    const TextLoader load
        = [&](std::string_view name, std::span<const std::string_view>) -> TextRead {
        ++reads;
        if (name == "valid.txt" && reads <= 128) return ExternalText{"valid.txt", "1 100 1"};
        return TextReadError::Unavailable;
    };
    const TuneStress stress(tune, load, diagnostics);
    const auto       warnings = diagnostics.take();
    REQUIRE(warnings.size() == 1);
    CHECK(warnings.front().find("[stress-file]") != std::string::npos);
    CHECK(reads == 2);
    CHECK(stress.note(tune.body.back().at, 0, kTpq, Meter{4, 4}).velocity == 100);
}

TEST_CASE("test_macro_stress_direct_invocations", "[abc][written][stress]") {
    const std::string source
        = "%abc-2.1\nX:1\nT:\nM:4/4\nL:1/4\nK:C\n"
          "m: ~C2 = [I:MIDI ptstress profile.txt] C [I:MIDI ptstress profile.txt] D\n~C2";
    Diagnostics   diagnostics("direct.abc");
    auto          tune = tokenize(source, {}, diagnostics);
    vec<Location> notes;
    for (const auto& token : tune.body)
        if (std::holds_alternative<Atom>(token.value)) notes.push_back(token.at);
    REQUIRE(notes.size() == 2);
    size_t           reads = 0;
    const TextLoader load
        = [&](std::string_view name, std::span<const std::string_view>) -> TextRead {
        ++reads;
        return ExternalText{std::string(name), reads == 1 ? "1 40 1" : "1 100 1"};
    };
    const TuneStress stress(tune, load, diagnostics);
    CHECK(diagnostics.take().empty());
    CHECK(reads == 2);
    CHECK(stress.note(notes[0], 0, kTpq, Meter{4, 4}).velocity == 40);
    CHECK(stress.note(notes[1], 0, kTpq, Meter{4, 4}).velocity == 100);
}

TEST_CASE("test_continued_chord_member_written_positions", "[abc][written]") {
    const std::string source = "X:1\nK:C\n[!<(!C\\\nI:MIDI deltaloudness 30\n!<(!E] G";
    Diagnostics       diagnostics("chord.abc");
    auto              tune  = tokenize(source, {}, diagnostics);
    const auto        field = std::ranges::find_if(tune.body, [](const Token& token) {
        return token.information().has_value();
    });
    const auto        chord = std::ranges::find_if(tune.body, [](const Token& token) {
        const auto* atom = std::get_if<Atom>(&token.value);
        return atom && atom->chord;
    });
    REQUIRE(field != tune.body.end());
    REQUIRE(chord != tune.body.end());
    const auto& notes = std::get<Atom>(chord->value).notes;
    REQUIRE(notes.size() == 2);
    REQUIRE(notes[0].decorations.size() == 1);
    REQUIRE(notes[1].decorations.size() == 1);
    CHECK(notes[0].decorations[0].at.written < field->at.written);
    CHECK(field->at.written < notes[1].decorations[0].at.written);
    CHECK(notes[1].decorations[0].at.written < chord->at.written);
    CHECK(diagnostics.take().empty());
}

TEST_CASE("test_written_positions_nested_includes", "[abc][written]") {
    const std::string source = "%abc-2.1\n%%MIDI C 59\nI:abc-include outer.abh\n"
                               "X:1\nT:\nK:C\nC";
    const TextLoader  load
        = [](std::string_view name, std::span<const std::string_view>) -> TextRead {
        if (name == "outer.abh")
            return ExternalText{"outer.abh", "I:abc-include inner.abh\n%%MIDI C 62\n"};
        if (name == "inner.abh") return ExternalText{"inner.abh", "%%MIDI C 61\n"};
        return TextReadError::Unavailable;
    };
    Diagnostics diagnostics("root.abc");
    auto        tune = [&] {
        auto original = tokenize(source, load, diagnostics);
        return Tune(std::move(original));
    }();
    vec<WrittenPosition> changes;
    for (const auto& token : tune.header)
        if (Instruction(token).midi == "C") changes.push_back(token.at.written);
    REQUIRE(changes.size() == 3);
    CHECK(changes[0] < changes[1]);
    CHECK(changes[1] < changes[2]);
    const PitchTuning tuning(tune, diagnostics);
    REQUIRE_FALSE(tune.body.empty());
    CHECK(tuning.pitch('C', 0, {}, tune.body.front().at).note == 62);
    CHECK(diagnostics.take().empty());
}

TEST_CASE("test_written_setting_positions_are_independent", "[abc][written]") {
    WrittenSetting<i32> settings(1);
    settings.record(WrittenPosition{2}, 2);
    settings.record(WrittenPosition{4}, 3);
    CHECK(settings.at(WrittenPosition{1}) == 1);
    CHECK(settings.at(WrittenPosition{2}) == 2);
    CHECK(settings.at(WrittenPosition{3}) == 2);
    CHECK(settings.at(WrittenPosition{4}) == 3);
}

VoicePlan written_plan(vec<Token> voice, std::string_view parts, Diagnostics& diagnostics) {
    const PartItinerary itinerary(parts, {}, diagnostics);
    auto                order = itinerary.arrange(voice);
    return {.source = std::move(voice), .order = std::move(order)};
}

WrittenNavigation compile_parts(const VoicePlan& plan, Diagnostics& diagnostics) {
    const TuneSettings settings{.meter = {4, 4}, .unit = kWhole / 8};
    return written_navigation(
        plan, {settings.meter, settings.key, settings.unit}, settings, diagnostics
    );
}

TEST_CASE("test_voice_plan_owns_written_tokens", "[abc][written]") {
    Diagnostics diagnostics("plan.abc");
    auto        plan = [&] {
        vec<Token> voice{{{}, Atom{.notes = {{{'C', 0, {}, false}}}}}};
        auto       order = PartItinerary("", {}, diagnostics).arrange(voice);
        return plan_voice(std::move(voice), std::move(order), {}, diagnostics);
    }();
    const auto notation = compile_parts(plan, diagnostics);
    REQUIRE(notation.notes.size() == 1);
    CHECK(notation.notes.contains(&plan.source.front()));
    CHECK(diagnostics.take().empty());
}

TEST_CASE("test_written_part_ranges", "[abc][written]") {
    Diagnostics diagnostics("parts.abc");
    // Expanded atoms may all carry one physical location.
    vec<Token> voice(1024, Token{{}, Atom{}});
    voice.front().value = Field{'P', "A"};
    const auto order    = PartItinerary("AA", {}, diagnostics).arrange(voice);
    REQUIRE(order.size() == 4);
    for (size_t pass = 0; pass < 2; ++pass) {
        const auto& entry = order[2 * pass];
        CHECK(entry.flow == Flow::EnterPart);
        CHECK(entry.part_occurrence == pass);
        CHECK(entry.restore_written == (pass == 1));
        const auto& range = order[2 * pass + 1];
        CHECK(range.flow == Flow::Token);
        CHECK_FALSE(range.restore_written);
        const auto tokens = range.tokens(std::span(voice));
        CHECK(tokens.data() == voice.data());
        CHECK(tokens.size() == voice.size());
    }
    // 2,000 compact ranges still dispatch more than a million tokens.
    CHECK_THROWS_AS(PartItinerary("A1000", {}, diagnostics).arrange(voice), std::invalid_argument);
}

TEST_CASE("test_written_ending_entry", "[abc][written]") {
    Diagnostics diagnostics("ending.abc");
    const auto  ending = [](i32 pass) {
        return Bar{.resets_measure = false, .endings = RepeatEndings({{pass, pass}})};
    };
    vec<Token> voice{
        {{}, Bar{.repeat_start = 2}, 1},
        {{}, Field{'L', "1/4"}, 1},
        {{}, ending(1), 1},
        {{}, Atom{}, 1},
        {{}, Bar{.repeat_end = 2}, 1},
        {{}, ending(2), 2},
        {{}, Atom{}, 2}
    };
    // On pass two the first lyric anchor is the selected ending, past the skipped atom.
    for (size_t line : {1, 2}) {
        voice[5].music_line = line;
        const auto order    = PartItinerary("", {}, diagnostics).arrange(voice);
        const auto jump     = std::ranges::find(order, Flow::JumpRepeat, &PlayRange::flow);
        REQUIRE(jump != order.end());
        CHECK_FALSE(jump->restore_written);
        CHECK(jump->continues_lyrics == (line == 1));
        const auto landing = std::ranges::find(order, true, &PlayRange::restore_written);
        REQUIRE(landing != order.end());
        CHECK(landing->flow == Flow::InlineEnding);
        CHECK(landing->first == 5);
        CHECK(landing->last == 6);
        CHECK(std::ranges::count(order, true, &PlayRange::restore_written) == 1);
    }
    const auto plan     = written_plan(std::move(voice), "", diagnostics);
    const auto notation = compile_parts(plan, diagnostics);
    CHECK(diagnostics.take().empty());
    REQUIRE(notation.entries.size() == 1);
    CHECK(notation.entries.contains(&plan.source[5]));
    REQUIRE(notation.notes.size() == 2);
    CHECK(notation.notes.contains(&plan.source[3]));
    CHECK(notation.notes.contains(&plan.source[6]));
}

TEST_CASE("test_written_hornpipe_across_skipped_parts", "[abc][written]") {
    Diagnostics diagnostics("swing.abc");
    const Atom  eighth{.notes = {{{'C', 0, {}, false}}}, .rhythm = {2, 1, true}};
    // B starts on the short half of the first pair and ends on the long half of the next.
    vec<Token> voice{
        {{}, Field{'P', "A"}},
        {{}, eighth},
        {{}, Field{'P', "B"}},
        {{}, eighth},
        {{}, eighth},
        {{}, Field{'P', "C"}},
        {{}, eighth}
    };
    const auto plan     = written_plan(std::move(voice), "BB", diagnostics);
    const auto notation = compile_parts(plan, diagnostics);
    CHECK(diagnostics.take().empty());
    REQUIRE(notation.notes.size() == 2);
    CHECK(notation.notes.at(&plan.source[3]).played_duration() == Catch::Approx(160));
    CHECK(notation.notes.at(&plan.source[4]).played_duration() == Catch::Approx(320));
}

TEST_CASE("test_skipped_rhythm_diagnostics_and_tuplet_grace", "[abc][written]") {
    Diagnostics diagnostics("rhythm.abc");
    const auto tune = tokenize("X:1\nK:C\nP:A\nC2>D> |\n(3 {g}F\nP:B\n{a}GAB |\n", {}, diagnostics);
    const auto plan = written_plan(tune.body, "B", diagnostics);
    const auto notation = compile_parts(plan, diagnostics);
    const auto warnings = diagnostics.take();
    REQUIRE(warnings.size() == 2);
    CHECK(warnings[0].starts_with("rhythm.abc:4:3: ABC [unequal-broken-rhythm]"));
    CHECK(warnings[1].starts_with("rhythm.abc:4:5: ABC [unfinished-broken-rhythm]"));
    // F consumes the first slot in the skipped part. Both grace groups borrow the tuplet unit.
    REQUIRE(notation.grace_units.size() == 1);
    CHECK(notation.grace_units.begin()->second == Catch::Approx(160));
    REQUIRE(notation.notes.size() == 3);
    for (const auto& [token, note] : notation.notes) {
        const auto pitch = note.atom->notes.front().pitch.letter;
        CAPTURE(pitch);
        CHECK(note.played_duration() == Catch::Approx(pitch == 'B' ? 240 : 160));
    }
}

TEST_CASE("test_part_entry_resumes_primary_measure_and_lane_slurs", "[abc][written]") {
    Diagnostics diagnostics("lanes.abc");
    const auto  tune
        = tokenize("X:1\nK:C\nP:A\n(^F & [L:1/16](_F\nP:B\nF F) & F F) | F\n", {}, diagnostics);
    const auto plan     = written_plan(tune.body, "B", diagnostics);
    auto       notation = compile_parts(plan, diagnostics);
    CHECK(diagnostics.take().empty());
    REQUIRE(notation.entries.size() == 1);
    auto& entry = notation.entries.begin()->second;
    CHECK(entry.fields.unit == 240);
    CHECK(
        entry.accidentals.resolve({'F', 0, {}, false}, entry.fields.tone, AccidentalScope::Pitch)
            .numerator
        == 1
    );
    vec<SlurPosition> slurs;
    for (const auto& [token, note] : notation.notes) slurs.push_back(note.slur);
    using enum SlurPosition;
    CHECK(slurs == vec<SlurPosition>{Inside, Closing, Inside, Closing, Outside});
}

}   // namespace
}   // namespace symusic::abc::detail

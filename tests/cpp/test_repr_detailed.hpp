#include <string>
#include <sstream>

#include "catch2/catch_test_macros.hpp"
#include "catch2/matchers/catch_matchers_all.hpp"

#include "symusic/repr.h"
#include "symusic/event.h"
#include "symusic/track.h"
#include "symusic/score.h"

using namespace symusic;

TEST_CASE("Test Note formatting", "[repr][note]") {
    Note<Tick> note(100, 200, 60, 80);
    Note<Quarter> quarter_note(0.5f, 1.0f, 62, 90);
    Note<Second> second_note(1.5f, 0.75f, 64, 100);

    SECTION("Default format (short)") {
        // Test the default formatting (short format)
        std::string str = fmt::format("{}", note);
        REQUIRE(str == "Note(100, 200, 60, 80, 'Tick')");

        str = fmt::format("{}", quarter_note);
        REQUIRE(str == "Note(0.50, 1.00, 62, 90, 'Quarter')");

        str = fmt::format("{}", second_note);
        REQUIRE(str == "Note(1.50, 0.75, 64, 100, 'Second')");
    }

    SECTION("Short format (:s)") {
        // Test explicit short format
        std::string str = fmt::format("{:s}", note);
        REQUIRE(str == "Note(100, 200, 60, 80, 'Tick')");
    }

    SECTION("Detailed format (:d)") {
        // Test detailed format
        std::string str = fmt::format("{:d}", note);
        REQUIRE(str == "Note(time=100, duration=200, pitch=60, velocity=80, ttype='Tick')");

        str = fmt::format("{:d}", quarter_note);
        REQUIRE(str == "Note(time=0.50, duration=1.00, pitch=62, velocity=90, ttype='Quarter')");
    }

    SECTION("Stream operator") {
        // Test ostream operator
        std::ostringstream oss;
        oss << note;
        REQUIRE(oss.str() == "Note(time=100, duration=200, pitch=60, velocity=80, ttype='Tick')");
    }
}

TEST_CASE("Test Pedal formatting", "[repr][pedal]") {
    Pedal<Tick> pedal(100, 200);

    SECTION("Default format") {
        std::string str = fmt::format("{}", pedal);
        REQUIRE(str == "Pedal(100, 200, 'Tick')");
    }

    SECTION("Detailed format") {
        std::string str = fmt::format("{:d}", pedal);
        REQUIRE(str == "Pedal(time=100, duration=200, ttype='Tick')");
    }
}

TEST_CASE("Test ControlChange formatting", "[repr][control]") {
    ControlChange<Tick> cc(100, 7, 127);

    SECTION("Default format") {
        std::string str = fmt::format("{}", cc);
        REQUIRE(str == "ControlChange(100, 7, 127, 'Tick')");
    }

    SECTION("Detailed format") {
        std::string str = fmt::format("{:d}", cc);
        REQUIRE(str == "ControlChange(time=100, number=7, value=127, ttype='Tick')");
    }
}

TEST_CASE("Test TimeSignature formatting", "[repr][timesig]") {
    TimeSignature<Tick> ts(100, 3, 4);

    SECTION("Default format") {
        std::string str = fmt::format("{}", ts);
        REQUIRE(str == "TimeSignature(100, 3, 4, 'Tick')");
    }

    SECTION("Detailed format") {
        std::string str = fmt::format("{:d}", ts);
        REQUIRE(str == "TimeSignature(time=100, numerator=3, denominator=4, ttype='Tick')");
    }
}

TEST_CASE("Test KeySignature formatting", "[repr][keysig]") {
    KeySignature<Tick> ks(100, 2, 0);

    SECTION("Default format") {
        std::string str = fmt::format("{}", ks);
        REQUIRE(str == "KeySignature(100, 2, 0, 10, 'Tick')");
    }

    SECTION("Detailed format") {
        std::string str = fmt::format("{:d}", ks);
        REQUIRE(str == "KeySignature(time=100, key=2, tonality=0, degree=10, ttype='Tick')");
    }
}

TEST_CASE("Test Tempo formatting", "[repr][tempo]") {
    Tempo<Tick> tempo(100, 500000);  // 120 BPM

    SECTION("Default format") {
        std::string str = fmt::format("{}", tempo);
        REQUIRE(str == "Tempo(100, 120, 500000, 'Tick')");
    }

    SECTION("Detailed format") {
        std::string str = fmt::format("{:d}", tempo);
        REQUIRE(str == "Tempo(time=100, qpm=120, mspq=500000, ttype='Tick')");
    }
}

TEST_CASE("Test PitchBend formatting", "[repr][pitchbend]") {
    PitchBend<Tick> pb(100, 8192);

    SECTION("Default format") {
        std::string str = fmt::format("{}", pb);
        REQUIRE(str == "PitchBend(100, 8192, 'Tick')");
    }

    SECTION("Detailed format") {
        std::string str = fmt::format("{:d}", pb);
        REQUIRE(str == "PitchBend(time=100, value=8192, ttype='Tick')");
    }
}

TEST_CASE("Test TextMeta formatting", "[repr][text]") {
    TextMeta<Tick> text(100, "Test Text");

    SECTION("Default format") {
        std::string str = fmt::format("{}", text);
        REQUIRE(str == "Text(100, 'Test Text', 'Tick')");
    }

    SECTION("Detailed format") {
        std::string str = fmt::format("{:d}", text);
        REQUIRE(str == "Text(time=100, text='Test Text', ttype='Tick')");
    }
}

TEST_CASE("Test Track formatting", "[repr][track]") {
    Track<Tick> track("Piano", 0, false);
    track.notes->push_back(Note<Tick>(0, 100, 60, 100));
    track.notes->push_back(Note<Tick>(100, 100, 62, 100));

    SECTION("Basic formatting") {
        std::string str = track.to_string();
        REQUIRE_FALSE(str.empty());
        // Test if the track name and some expected content is present
        REQUIRE(str.find("Piano") != std::string::npos);
        REQUIRE(str.find("notes") != std::string::npos);
    }

    SECTION("Summary method") {
        std::string summary = track.summary();
        REQUIRE_FALSE(summary.empty());
        REQUIRE(summary.find("Piano") != std::string::npos);
    }

    SECTION("Stream operator") {
        std::ostringstream oss;
        oss << track;
        REQUIRE_FALSE(oss.str().empty());
        REQUIRE(oss.str().find("Piano") != std::string::npos);
    }
}

TEST_CASE("Test Score formatting", "[repr][score]") {
    Score<Tick> score(480);
    auto track = std::make_shared<Track<Tick>>("Piano", 0, false);
    track->notes->push_back(Note<Tick>(0, 100, 60, 100));
    score.tracks->push_back(track);

    SECTION("Basic formatting") {
        std::string str = score.to_string();
        REQUIRE_FALSE(str.empty());
        // Check for key components in the formatted string
        REQUIRE(str.find("480") != std::string::npos);
        // Let's check for more general terms rather than specific track name
        REQUIRE(str.find("tracks") != std::string::npos);
    }

    SECTION("Summary method") {
        std::string summary = score.summary();
        REQUIRE_FALSE(summary.empty());
        REQUIRE(summary.find("480") != std::string::npos);
    }

    SECTION("Stream operator") {
        std::ostringstream oss;
        oss << score;
        REQUIRE_FALSE(oss.str().empty());
        REQUIRE(oss.str().find("480") != std::string::npos);
    }
}

TEST_CASE("Test fixed_float formatting function", "[repr][utils]") {
    // Test the fix_float helper function through note formatting
    Note<Tick> integer_note(100, 200, 60, 80);
    Note<Quarter> float_note(0.5123f, 0.9987f, 62, 90);

    std::string int_str = fmt::format("{}", integer_note);
    std::string float_str = fmt::format("{}", float_note);

    // Integer values should be formatted without decimal point
    REQUIRE(int_str == "Note(100, 200, 60, 80, 'Tick')");

    // Float values should be formatted with 2 decimal places
    REQUIRE(float_str == "Note(0.51, 1.00, 62, 90, 'Quarter')");
}

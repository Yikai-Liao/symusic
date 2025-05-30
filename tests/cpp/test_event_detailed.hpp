#include <vector>
#include <string>

#include "catch2/catch_test_macros.hpp"
#include "catch2/matchers/catch_matchers_all.hpp"

#include "symusic/event.h"

using namespace symusic;

// Test all methods and functionality of the Note class
TEST_CASE("Test Note class comprehensive functionality", "[event][note]") {
    SECTION("Test constructors and assignment") {
        // Default constructor
        Note<Tick> default_note;
        REQUIRE(default_note.time == 0);
        REQUIRE(default_note.duration == 0);
        REQUIRE(default_note.pitch == 0);
        REQUIRE(default_note.velocity == 0);

        // Constructor with parameters
        Note<Tick> note(100, 200, 60, 80);
        REQUIRE(note.time == 100);
        REQUIRE(note.duration == 200);
        REQUIRE(note.pitch == 60);
        REQUIRE(note.velocity == 80);

        // Copy constructor
        Note<Tick> copied_note(note);
        REQUIRE(copied_note.time == note.time);
        REQUIRE(copied_note.duration == note.duration);
        REQUIRE(copied_note.pitch == note.pitch);
        REQUIRE(copied_note.velocity == note.velocity);

        // Constructor from other time type
        Note<Quarter> quarter_note(1.0f, 0.5f, 62, 90);
        Note<Tick> converted_note(50, 100, quarter_note);
        REQUIRE(converted_note.time == 50);
        REQUIRE(converted_note.duration == 100);
        REQUIRE(converted_note.pitch == 62);
        REQUIRE(converted_note.velocity == 90);

        // Assignment operator
        Note<Tick> assigned_note;
        assigned_note = note;
        REQUIRE(assigned_note.time == note.time);
        REQUIRE(assigned_note.duration == note.duration);
        REQUIRE(assigned_note.pitch == note.pitch);
        REQUIRE(assigned_note.velocity == note.velocity);
    }

    SECTION("Test time-related methods") {
        Note<Tick> note(100, 200, 60, 80);

        REQUIRE(note.start() == 100);
        REQUIRE(note.end() == 300);  // 100 + 200
    }

    SECTION("Test empty() method") {
        Note<Tick> note1(100, 200, 60, 80);
        REQUIRE_FALSE(note1.empty());

        // Note with zero duration should be empty
        Note<Tick> note2(100, 0, 60, 80);
        REQUIRE(note2.empty());

        // Note with zero velocity should be empty
        Note<Tick> note3(100, 200, 60, 0);
        REQUIRE(note3.empty());

        // Note with both zero duration and velocity should be empty
        Note<Tick> note4(100, 0, 60, 0);
        REQUIRE(note4.empty());
    }

    SECTION("Test equality operators") {
        Note<Tick> note1(100, 200, 60, 80);
        Note<Tick> note2(100, 200, 60, 80);
        Note<Tick> note3(100, 200, 62, 80);  // Different pitch

        REQUIRE(note1 == note2);
        REQUIRE(note1 != note3);
    }

    SECTION("Test default_key() method") {
        Note<Tick> note(100, 200, 60, 80);
        auto key = note.default_key();

        REQUIRE(std::get<0>(key) == 100);
        REQUIRE(std::get<1>(key) == 200);
        REQUIRE(std::get<2>(key) == 60);
        REQUIRE(std::get<3>(key) == 80);
    }

    SECTION("Test shift_time() and shift_time_inplace() methods") {
        Note<Tick> note(100, 200, 60, 80);

        // Test non-inplace time shift
        Note<Tick> shifted = note.shift_time(50);
        REQUIRE(shifted.time == 150);
        REQUIRE(note.time == 100);  // Original note not modified

        // Test inplace time shift
        note.shift_time_inplace(50);
        REQUIRE(note.time == 150);
    }

    SECTION("Test shift_pitch() and shift_pitch_inplace() methods") {
        Note<Tick> note(100, 200, 60, 80);

        // Test non-inplace pitch shift
        Note<Tick> pitched = note.shift_pitch(12);  // Up one octave
        REQUIRE(pitched.pitch == 72);
        REQUIRE(note.pitch == 60);  // Original note not modified

        // Test inplace pitch shift
        note.shift_pitch_inplace(12);
        REQUIRE(note.pitch == 72);
    }

    SECTION("Test shift_velocity() and shift_velocity_inplace() methods") {
        Note<Tick> note(100, 200, 60, 80);

        // Test non-inplace velocity shift
        Note<Tick> velocitied = note.shift_velocity(10);
        REQUIRE(velocitied.velocity == 90);
        REQUIRE(note.velocity == 80);  // Original note not modified

        // Test inplace velocity shift
        note.shift_velocity_inplace(10);
        REQUIRE(note.velocity == 90);
    }

    SECTION("Test copy() and deepcopy() methods") {
        Note<Tick> note(100, 200, 60, 80);

        Note<Tick> copied = note.copy();
        REQUIRE(copied == note);

        Note<Tick> deep_copied = note.deepcopy();
        REQUIRE(deep_copied == note);
    }
}

// Test all methods and functionality of the Pedal class
TEST_CASE("Test Pedal class comprehensive functionality", "[event][pedal]") {
    SECTION("Test constructors and assignment") {
        // Default constructor
        Pedal<Tick> default_pedal;
        REQUIRE(default_pedal.time == 0);
        // The default duration is implementation-dependent, so we don't test its exact value

        // Constructor with parameters
        Pedal<Tick> pedal(100, 200);
        REQUIRE(pedal.time == 100);
        REQUIRE(pedal.duration == 200);

        // Copy constructor
        Pedal<Tick> copied_pedal(pedal);
        REQUIRE(copied_pedal.time == pedal.time);
        REQUIRE(copied_pedal.duration == pedal.duration);

        // Constructor from other time type
        Pedal<Quarter> quarter_pedal(1.0f, 0.5f);
        Pedal<Tick> converted_pedal(50, 100, quarter_pedal);
        REQUIRE(converted_pedal.time == 50);
        REQUIRE(converted_pedal.duration == 100);

        // Assignment operator
        Pedal<Tick> assigned_pedal;
        assigned_pedal = pedal;
        REQUIRE(assigned_pedal.time == pedal.time);
        REQUIRE(assigned_pedal.duration == pedal.duration);
    }

    SECTION("Test time-related methods") {
        Pedal<Tick> pedal(100, 200);

        REQUIRE(pedal.start() == 100);
        REQUIRE(pedal.end() == 300);  // 100 + 200
    }

    SECTION("Test equality operators") {
        Pedal<Tick> pedal1(100, 200);
        Pedal<Tick> pedal2(100, 200);
        Pedal<Tick> pedal3(100, 300);  // Different duration

        REQUIRE(pedal1 == pedal2);
        REQUIRE(pedal1 != pedal3);
    }

    SECTION("Test default_key() method") {
        Pedal<Tick> pedal(100, 200);
        auto key = pedal.default_key();

        REQUIRE(std::get<0>(key) == 100);
        REQUIRE(std::get<1>(key) == 200);
    }

    SECTION("Test shift_time() and shift_time_inplace() methods") {
        Pedal<Tick> pedal(100, 200);

        // Test non-inplace time shift
        Pedal<Tick> shifted = pedal.shift_time(50);
        REQUIRE(shifted.time == 150);
        REQUIRE(pedal.time == 100);  // Original pedal not modified

        // Test inplace time shift
        pedal.shift_time_inplace(50);
        REQUIRE(pedal.time == 150);
    }

    SECTION("Test copy() and deepcopy() methods") {
        Pedal<Tick> pedal(100, 200);

        Pedal<Tick> copied = pedal.copy();
        REQUIRE(copied == pedal);

        Pedal<Tick> deep_copied = pedal.deepcopy();
        REQUIRE(deep_copied == pedal);
    }
}

// Test all methods and functionality of the ControlChange class
TEST_CASE("Test ControlChange class comprehensive functionality", "[event][control]") {
    SECTION("Test constructors and assignment") {
        // Default constructor
        ControlChange<Tick> default_cc;
        REQUIRE(default_cc.time == 0);
        REQUIRE(default_cc.number == 0);
        REQUIRE(default_cc.value == 0);

        // Constructor with parameters
        ControlChange<Tick> cc(100, 7, 127);  // Volume controller set to maximum
        REQUIRE(cc.time == 100);
        REQUIRE(cc.number == 7);
        REQUIRE(cc.value == 127);

        // Copy constructor
        ControlChange<Tick> copied_cc(cc);
        REQUIRE(copied_cc.time == cc.time);
        REQUIRE(copied_cc.number == cc.number);
        REQUIRE(copied_cc.value == cc.value);

        // Constructor from other time type
        ControlChange<Quarter> quarter_cc(1.0f, 10, 64);
        ControlChange<Tick> converted_cc(50, quarter_cc);
        REQUIRE(converted_cc.time == 50);
        REQUIRE(converted_cc.number == 10);
        REQUIRE(converted_cc.value == 64);

        // Assignment operator
        ControlChange<Tick> assigned_cc;
        assigned_cc = cc;
        REQUIRE(assigned_cc.time == cc.time);
        REQUIRE(assigned_cc.number == cc.number);
        REQUIRE(assigned_cc.value == cc.value);
    }

    SECTION("Test equality operators") {
        ControlChange<Tick> cc1(100, 7, 127);
        ControlChange<Tick> cc2(100, 7, 127);
        ControlChange<Tick> cc3(100, 7, 64);  // Different value

        REQUIRE(cc1 == cc2);
        REQUIRE(cc1 != cc3);
    }

    SECTION("Test default_key() method") {
        ControlChange<Tick> cc(100, 7, 127);
        auto key = cc.default_key();

        REQUIRE(std::get<0>(key) == 100);
        REQUIRE(std::get<1>(key) == 7);
        REQUIRE(std::get<2>(key) == 127);
    }

    SECTION("Test shift_time() and shift_time_inplace() methods") {
        ControlChange<Tick> cc(100, 7, 127);

        // Test non-inplace time shift
        ControlChange<Tick> shifted = cc.shift_time(50);
        REQUIRE(shifted.time == 150);
        REQUIRE(cc.time == 100);  // Original controller not modified

        // Test inplace time shift
        cc.shift_time_inplace(50);
        REQUIRE(cc.time == 150);
    }

    SECTION("Test copy() and deepcopy() methods") {
        ControlChange<Tick> cc(100, 7, 127);

        ControlChange<Tick> copied = cc.copy();
        REQUIRE(copied == cc);

        ControlChange<Tick> deep_copied = cc.deepcopy();
        REQUIRE(deep_copied == cc);
    }
}

// Test all methods and functionality of the TimeSignature class
TEST_CASE("Test TimeSignature class comprehensive functionality", "[event][timesig]") {
    SECTION("Test constructors and assignment") {
        // Default constructor
        TimeSignature<Tick> default_ts;
        REQUIRE(default_ts.time == 0);
        REQUIRE(default_ts.numerator == 4);
        REQUIRE(default_ts.denominator == 4);

        // Constructor with parameters
        TimeSignature<Tick> ts(100, 3, 4);  // 3/4 time
        REQUIRE(ts.time == 100);
        REQUIRE(ts.numerator == 3);
        REQUIRE(ts.denominator == 4);

        // Copy constructor
        TimeSignature<Tick> copied_ts(ts);
        REQUIRE(copied_ts.time == ts.time);
        REQUIRE(copied_ts.numerator == ts.numerator);
        REQUIRE(copied_ts.denominator == ts.denominator);

        // Constructor from other time type
        TimeSignature<Quarter> quarter_ts(1.0f, 6, 8);
        TimeSignature<Tick> converted_ts(50, quarter_ts);
        REQUIRE(converted_ts.time == 50);
        REQUIRE(converted_ts.numerator == 6);
        REQUIRE(converted_ts.denominator == 8);

        // Assignment operator
        TimeSignature<Tick> assigned_ts;
        assigned_ts = ts;
        REQUIRE(assigned_ts.time == ts.time);
        REQUIRE(assigned_ts.numerator == ts.numerator);
        REQUIRE(assigned_ts.denominator == ts.denominator);
    }

    SECTION("Test equality operators") {
        TimeSignature<Tick> ts1(100, 3, 4);
        TimeSignature<Tick> ts2(100, 3, 4);
        TimeSignature<Tick> ts3(100, 4, 4);  // Different numerator

        REQUIRE(ts1 == ts2);
        REQUIRE(ts1 != ts3);
    }

    SECTION("Test default_key() method") {
        TimeSignature<Tick> ts(100, 3, 4);
        auto key = ts.default_key();

        REQUIRE(key == 100);  // Returns time
    }

    SECTION("Test shift_time() and shift_time_inplace() methods") {
        TimeSignature<Tick> ts(100, 3, 4);

        // Test non-inplace time shift
        TimeSignature<Tick> shifted = ts.shift_time(50);
        REQUIRE(shifted.time == 150);
        REQUIRE(ts.time == 100);  // Original time signature not modified

        // Test inplace time shift
        ts.shift_time_inplace(50);
        REQUIRE(ts.time == 150);
    }

    SECTION("Test copy() and deepcopy() methods") {
        TimeSignature<Tick> ts(100, 3, 4);

        TimeSignature<Tick> copied = ts.copy();
        REQUIRE(copied == ts);

        TimeSignature<Tick> deep_copied = ts.deepcopy();
        REQUIRE(deep_copied == ts);
    }
}

// Test all methods and functionality of the KeySignature class
TEST_CASE("Test KeySignature class comprehensive functionality", "[event][keysig]") {
    SECTION("Test constructors and assignment") {
        // Default constructor
        KeySignature<Tick> default_ks;
        REQUIRE(default_ks.time == 0);
        REQUIRE(default_ks.key == 0);
        REQUIRE(default_ks.tonality == 0);

        // Constructor with parameters
        KeySignature<Tick> ks(100, 2, 0);  // D major, 2 sharps
        REQUIRE(ks.time == 100);
        REQUIRE(ks.key == 2);
        REQUIRE(ks.tonality == 0);

        // Copy constructor
        KeySignature<Tick> copied_ks(ks);
        REQUIRE(copied_ks.time == ks.time);
        REQUIRE(copied_ks.key == ks.key);
        REQUIRE(copied_ks.tonality == ks.tonality);

        // Constructor from other time type
        KeySignature<Quarter> quarter_ks(1.0f, -3, 1);  // C minor, 3 flats
        KeySignature<Tick> converted_ks(50, quarter_ks);
        REQUIRE(converted_ks.time == 50);
        REQUIRE(converted_ks.key == -3);
        REQUIRE(converted_ks.tonality == 1);

        // Assignment operator
        KeySignature<Tick> assigned_ks;
        assigned_ks = ks;
        REQUIRE(assigned_ks.time == ks.time);
        REQUIRE(assigned_ks.key == ks.key);
        REQUIRE(assigned_ks.tonality == ks.tonality);
    }

    SECTION("Test degree() method") {
        // Test degree() results for different key signatures
        KeySignature<Tick> ks1(0, 0, 0);  // C major
        REQUIRE(ks1.degree() == 0);

        KeySignature<Tick> ks2(0, 2, 0);  // D major
        REQUIRE(ks2.degree() == 10);  // (2*5)%12

        KeySignature<Tick> ks3(0, -1, 1);  // A minor
        REQUIRE(ks3.degree() == 7);  // (-1*5)%12 + 1*12 = 7
    }

    SECTION("Test equality operators") {
        KeySignature<Tick> ks1(100, 2, 0);
        KeySignature<Tick> ks2(100, 2, 0);
        KeySignature<Tick> ks3(100, 1, 0);  // Different key

        REQUIRE(ks1 == ks2);
        REQUIRE(ks1 != ks3);
    }

    SECTION("Test default_key() method") {
        KeySignature<Tick> ks(100, 2, 0);
        auto key = ks.default_key();

        REQUIRE(key == 100);  // Returns time
    }

    SECTION("Test shift_time() and shift_time_inplace() methods") {
        KeySignature<Tick> ks(100, 2, 0);

        // Test non-inplace time shift
        KeySignature<Tick> shifted = ks.shift_time(50);
        REQUIRE(shifted.time == 150);
        REQUIRE(ks.time == 100);  // Original key signature not modified

        // Test inplace time shift
        ks.shift_time_inplace(50);
        REQUIRE(ks.time == 150);
    }

    SECTION("Test copy() and deepcopy() methods") {
        KeySignature<Tick> ks(100, 2, 0);

        KeySignature<Tick> copied = ks.copy();
        REQUIRE(copied == ks);

        KeySignature<Tick> deep_copied = ks.deepcopy();
        REQUIRE(deep_copied == ks);
    }
}

// Test all methods and functionality of the Tempo class
TEST_CASE("Test Tempo class comprehensive functionality", "[event][tempo]") {
    SECTION("Test constructors and assignment") {
        // Default constructor
        Tempo<Tick> default_tempo;
        REQUIRE(default_tempo.time == 0);
        REQUIRE(default_tempo.mspq == 0);

        // Constructor with parameters
        Tempo<Tick> tempo(100, 500000);  // 120 BPM
        REQUIRE(tempo.time == 100);
        REQUIRE(tempo.mspq == 500000);

        // Copy constructor
        Tempo<Tick> copied_tempo(tempo);
        REQUIRE(copied_tempo.time == tempo.time);
        REQUIRE(copied_tempo.mspq == tempo.mspq);

        // Constructor from other time type
        Tempo<Quarter> quarter_tempo(1.0f, 400000);  // 150 BPM
        Tempo<Tick> converted_tempo(50, quarter_tempo);
        REQUIRE(converted_tempo.time == 50);
        REQUIRE(converted_tempo.mspq == 400000);

        // Assignment operator
        Tempo<Tick> assigned_tempo;
        assigned_tempo = tempo;
        REQUIRE(assigned_tempo.time == tempo.time);
        REQUIRE(assigned_tempo.mspq == tempo.mspq);
    }

    SECTION("Test from_qpm() and qpm() methods") {
        // Test creating Tempo from QPM
        Tempo<Tick> tempo = Tempo<Tick>::from_qpm(100, 120.0);
        REQUIRE(tempo.time == 100);
        REQUIRE(tempo.mspq == 500000);  // 120 BPM = 500000 microseconds/quarter note

        // Test reading QPM
        REQUIRE(tempo.qpm() == Catch::Approx(120.0));
    }

    SECTION("Test set_qpm() method") {
        Tempo<Tick> tempo(100, 500000);  // 120 BPM

        // Change tempo to 80 BPM
        tempo.set_qpm(80.0);
        REQUIRE(tempo.mspq == 750000);  // 80 BPM = 750000 microseconds/quarter note
        REQUIRE(tempo.qpm() == Catch::Approx(80.0));
    }

    SECTION("Test static conversion methods") {
        // Test QPM to MSPQ conversion
        i32 mspq = Tempo<Tick>::qpm2mspq(120.0);
        REQUIRE(mspq == 500000);

        // Test MSPQ to QPM conversion
        f64 qpm = Tempo<Tick>::mspq2qpm(500000);
        REQUIRE(qpm == Catch::Approx(120.0));
    }

    SECTION("Test equality operators") {
        Tempo<Tick> tempo1(100, 500000);
        Tempo<Tick> tempo2(100, 500000);
        Tempo<Tick> tempo3(100, 400000);  // Different tempo

        REQUIRE(tempo1 == tempo2);
        REQUIRE(tempo1 != tempo3);
    }

    SECTION("Test default_key() method") {
        Tempo<Tick> tempo(100, 500000);
        auto key = tempo.default_key();

        REQUIRE(std::get<0>(key) == 100);  // Returns time
    }

    SECTION("Test shift_time() and shift_time_inplace() methods") {
        Tempo<Tick> tempo(100, 500000);

        // Test non-inplace time shift
        Tempo<Tick> shifted = tempo.shift_time(50);
        REQUIRE(shifted.time == 150);
        REQUIRE(tempo.time == 100);  // Original tempo not modified

        // Test inplace time shift
        tempo.shift_time_inplace(50);
        REQUIRE(tempo.time == 150);
    }

    SECTION("Test copy() and deepcopy() methods") {
        Tempo<Tick> tempo(100, 500000);

        Tempo<Tick> copied = tempo.copy();
        REQUIRE(copied == tempo);

        Tempo<Tick> deep_copied = tempo.deepcopy();
        REQUIRE(deep_copied == tempo);
    }
}

// Test all methods and functionality of the PitchBend class
TEST_CASE("Test PitchBend class comprehensive functionality", "[event][pitchbend]") {
    SECTION("Test constructors and assignment") {
        // Default constructor
        PitchBend<Tick> default_pb;
        REQUIRE(default_pb.time == 0);
        REQUIRE(default_pb.value == 0);

        // Constructor with parameters
        PitchBend<Tick> pb(100, 8192);  // Center value
        REQUIRE(pb.time == 100);
        REQUIRE(pb.value == 8192);

        // Copy constructor
        PitchBend<Tick> copied_pb(pb);
        REQUIRE(copied_pb.time == pb.time);
        REQUIRE(copied_pb.value == pb.value);

        // Constructor from other time type
        PitchBend<Quarter> quarter_pb(1.0f, 12288);  // Upward bend
        PitchBend<Tick> converted_pb(50, quarter_pb);
        REQUIRE(converted_pb.time == 50);
        REQUIRE(converted_pb.value == 12288);

        // Assignment operator
        PitchBend<Tick> assigned_pb;
        assigned_pb = pb;
        REQUIRE(assigned_pb.time == pb.time);
        REQUIRE(assigned_pb.value == pb.value);
    }

    SECTION("Test equality operators") {
        PitchBend<Tick> pb1(100, 8192);
        PitchBend<Tick> pb2(100, 8192);
        PitchBend<Tick> pb3(100, 10000);  // Different value

        REQUIRE(pb1 == pb2);
        REQUIRE(pb1 != pb3);
    }

    SECTION("Test default_key() method") {
        PitchBend<Tick> pb(100, 8192);
        auto key = pb.default_key();

        REQUIRE(key == 100);  // Returns time
    }

    SECTION("Test shift_time() and shift_time_inplace() methods") {
        PitchBend<Tick> pb(100, 8192);

        // Test non-inplace time shift
        PitchBend<Tick> shifted = pb.shift_time(50);
        REQUIRE(shifted.time == 150);
        REQUIRE(pb.time == 100);  // Original pitch bend not modified

        // Test inplace time shift
        pb.shift_time_inplace(50);
        REQUIRE(pb.time == 150);
    }

    SECTION("Test copy() and deepcopy() methods") {
        PitchBend<Tick> pb(100, 8192);

        PitchBend<Tick> copied = pb.copy();
        REQUIRE(copied == pb);

        PitchBend<Tick> deep_copied = pb.deepcopy();
        REQUIRE(deep_copied == pb);
    }
}

// Test all methods and functionality of the TextMeta class
TEST_CASE("Test TextMeta class comprehensive functionality", "[event][text]") {
    SECTION("Test constructors and assignment") {
        // Default constructor
        TextMeta<Tick> default_text;
        REQUIRE(default_text.time == 0);
        REQUIRE(default_text.text == "");

        // Constructor with parameters
        TextMeta<Tick> text(100, "Test Text");
        REQUIRE(text.time == 100);
        REQUIRE(text.text == "Test Text");

        // Copy constructor
        TextMeta<Tick> copied_text(text);
        REQUIRE(copied_text.time == text.time);
        REQUIRE(copied_text.text == text.text);

        // Constructor from other time type
        TextMeta<Quarter> quarter_text(1.0f, "Quarter Text");
        TextMeta<Tick> converted_text(50, quarter_text);
        REQUIRE(converted_text.time == 50);
        REQUIRE(converted_text.text == "Quarter Text");

        // Assignment operator
        TextMeta<Tick> assigned_text;
        assigned_text = text;
        REQUIRE(assigned_text.time == text.time);
        REQUIRE(assigned_text.text == text.text);
    }

    SECTION("Test equality operators") {
        TextMeta<Tick> text1(100, "Text A");
        TextMeta<Tick> text2(100, "Text A");
        TextMeta<Tick> text3(100, "Text B");  // Different text

        REQUIRE(text1 == text2);
        REQUIRE(text1 != text3);
    }

    SECTION("Test default_key() method") {
        TextMeta<Tick> text(100, "Test Text");
        auto key = text.default_key();

        REQUIRE(key == 100);  // Returns time
    }

    SECTION("Test shift_time() and shift_time_inplace() methods") {
        TextMeta<Tick> text(100, "Test Text");

        // Test non-inplace time shift
        TextMeta<Tick> shifted = text.shift_time(50);
        REQUIRE(shifted.time == 150);
        REQUIRE(text.time == 100);  // Original text not modified

        // Test inplace time shift
        text.shift_time_inplace(50);
        REQUIRE(text.time == 150);
    }

    SECTION("Test copy() and deepcopy() methods") {
        TextMeta<Tick> text(100, "Test Text");

        TextMeta<Tick> copied = text.copy();
        REQUIRE(copied == text);

        TextMeta<Tick> deep_copied = text.deepcopy();
        REQUIRE(deep_copied == text);
    }
}

// Test functionality of the TimeStamp base class
TEST_CASE("Test TimeStamp base class functionality", "[event][timestamp]") {
    SECTION("Test constructors and assignment") {
        // Default constructor
        TimeStamp<Tick> default_ts;
        REQUIRE(default_ts.time == 0);

        // Constructor with parameters
        TimeStamp<Tick> ts(100);
        REQUIRE(ts.time == 100);

        // Copy constructor
        TimeStamp<Tick> copied_ts(ts);
        REQUIRE(copied_ts.time == ts.time);

        // Assignment operator
        TimeStamp<Tick> assigned_ts;
        assigned_ts = ts;
        REQUIRE(assigned_ts.time == ts.time);
    }

    SECTION("Test equality operators") {
        TimeStamp<Tick> ts1(100);
        TimeStamp<Tick> ts2(100);
        TimeStamp<Tick> ts3(200);

        REQUIRE(ts1 == ts2);
        REQUIRE(ts1 != ts3);
    }
}

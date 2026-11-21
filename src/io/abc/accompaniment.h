#pragma once

#include <deque>
#include <map>
#include <optional>
#include <span>
#include <string>

#include "backing_pattern.h"
#include "channel_effect.h"
#include "playback_pass.h"
#include "settings.h"

namespace symusic::abc::detail {

struct BackingTracks {
    bool               chords = false;
    bool               drums  = false;
    bool               drone  = false;
    [[nodiscard]] bool any() const { return chords || drums || drone; }
};

struct BackingOpening {
    i32     chord_shift = 0;
    i32     drone_shift = 0;
    i32     chord_gap   = ChordAttackSettings::kDefaultGap;
    i32     drum_gap    = ChordAttackSettings::kDefaultGap;
    DrumMap drums;   // Routes after the melody pass, before this lane's drum pass.
};

// Collect tune-wide abcMIDI chord names before resolving any written symbols.
// A symbol can use a custom name only if it was known at its written position,
// but plays the final voicing.
class ChordNames {
public:
    bool consume(const Token& token, Diagnostics& diagnostics);
    void resolve(Harmony& harmony, Location at, Diagnostics& diagnostics) const;

private:
    struct Definition {
        std::string                    name;
        std::optional<WrittenPosition> first;   // Built-in qualities are known from the start.
        vec<i32>                       intervals;
    };

    vec<Definition> slots;
};

// Discover MIDI backing tracks in written order, including skipped sections.
void discover_backing(const Token& token, BackingTracks& tracks);

// Generate chord, bass, drum and drone backing on each melody lane's clock.
// Clock arguments are fractional ticks before the note-on offset, except
// apply(), which takes a quantized tick. The caller supplies bar boundaries
// for active and silent measures; advance() alone cannot restart a pattern.
class Accompaniment {
public:
    Accompaniment(const ProgramNumbers& programs, Diagnostics& diagnostics);
    bool directive(
        std::string_view name,
        std::string_view args,
        size_t           lane,
        Meter            meter,
        f64              time,
        Location         at
    );
    // Open overlay lanes in order before advancing or emitting harmony for them.
    void open_lane(size_t lane, Meter meter, f64 bar_origin, const BackingOpening& opening);
    void transpose(size_t lane, MidiTransposition change);
    void chord_gap(size_t lane, i32 gap, f64 time, Location at);
    void declare_drums(size_t lane, const BackingOpening& opening);
    void map_drums(size_t lane, DrumRoute route, f64 time, Location at);
    void apply(const ChannelEffect& effect, size_t lane, i32 time);
    void harmony(const Harmony::Voicing& chord, size_t lane, i32 transpose, Location at);
    void meter(size_t lane, Meter change);
    void bar(size_t lane, f64 time, Location at);
    void advance(size_t lane, f64 end, Location at);
    // Append bass, chord, drum and drone tracks after the melody in each lane's pass.
    // lane_ends covers the lanes opened during playback. Written-only lanes stay empty.
    void finish(std::span<PlaybackPass> passes, std::span<const f64> lane_ends, Location at);

private:
    void drum_gate(size_t lane, bool enabled, f64 time, Location at);
    struct DrumDefinition {
        vec<BackingStep> steps;
        Meter            meter;
    };
    struct DrumBar {};
    struct DrumSpan {
        i32 bars;
    };
    struct DrumGap {
        i32 ticks;
    };
    struct DrumEvent {
        f64                                                                       time;
        Location                                                                  at;
        std::variant<DrumDefinition, DrumBar, DrumSpan, bool, DrumGap, DrumRoute> change;

        void apply(BackingPattern& pattern, DrumMap& routing);
    };
    struct DrumPart {
        std::optional<i32> opening_gap;
        DrumMap            routing;
        vec<DrumEvent>     events;
    };

    struct Instrument {
        i32                              program = 0;
        i32                              octave  = 0;
        i32                              velocity;
        std::map<i32, TrackNative<Tick>> tracks;
        SustainPedal                     pedal;

        void apply(const ChannelEffect& effect, i32 time);
        void play(i64 pitch, i32 start, i32 duration, f64 time, BackingPattern& pattern);
        void finish(vec<TrackNative<Tick>>& output, i32 end);
    };

    struct Chord {
        i64      bass;
        vec<i64> pitches;
        Location at;
    };

    struct DroneSetup {
        i32                program = 70;
        std::array<i32, 2> pitch{45, 33};
        std::array<i32, 2> velocity{80, 80};
    };

    struct Drone {
        std::map<i32, TrackNative<Tick>> tracks;
        // The sounding attacks are the tail of a single program's note list.
        std::optional<i32> sounding_program;
        size_t             first_attack   = 0;
        i32                emitted        = 0;
        i32                program_at     = 0;
        i32                previous_delta = 0;
        i32                transposition  = 0;
    };

    struct HarmonicPart {
        BackingPattern       pattern{BackingPattern::Part::Guitar};
        Instrument           bass{.velocity = 80};
        Instrument           chord{.velocity = 75};
        std::optional<Chord> current_chord;
        Drone                drone;
        i32                  transposition = 0;
    };

    Diagnostics&          diagnostics;
    const ProgramNumbers& programs;
    // Opening an overlay must preserve the track addresses held by pending releases.
    std::deque<HarmonicPart>   harmonic_parts;
    std::map<size_t, DrumPart> drum_parts;
    DroneSetup                 drone_setup;

    static std::string_view default_chords(Meter meter);
    vec<BackingStep>        pattern(std::string_view text, std::string_view allowed, Location at);
    DrumDefinition          drum_pattern(std::string_view args, Meter meter, Location at);
    void                    record_if_declared(size_t lane, DrumEvent event);
    void                    finish_drums(
        size_t lane, BackingPattern& pattern, vec<TrackNative<Tick>>& tracks, f64 end, Location at
    );
    void start_drone(Drone& drone, f64 time, Location at);
    void stop_drone(Drone& drone, f64 time, Location at);
    void drone_settings(Drone& drone, std::string_view args, f64 time, Location at);
    bool instrument_directive(
        HarmonicPart& part, std::string_view name, std::string_view args, Location at
    );
    void chord_step(HarmonicPart& part, char action, f64 time, f64 length, Location at);
    std::pair<i32, i32> backing_times(f64 time, f64 length, Location at);
};

}   // namespace symusic::abc::detail

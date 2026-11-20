#pragma once

#include <map>
#include <optional>
#include <span>
#include <string>

#include "channel_effect.h"
#include "drum_output.h"
#include "settings.h"
#include "symusic/score.h"

namespace symusic::abc::detail {

struct BackingTracks {
    bool               chords = false;
    bool               drums  = false;
    bool               drone  = false;
    [[nodiscard]] bool any() const { return chords || drums || drone; }
};

// These are separate MIDI passes. Bass and chord instruments share one pass.
struct PerformedBacking {
    vec<RenderedTrack> harmony;
    vec<RenderedTrack> drums;
    vec<RenderedTrack> drone;
};

struct BackingOpening {
    int chord_shift = 0;
    int drone_shift = 0;
    int chord_gap   = ChordAttackSettings::default_gap;
    int drum_gap    = ChordAttackSettings::default_gap;
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
        std::optional<Location> first;   // Built-in qualities are known from the start.
        vec<int>                intervals;
    };

    std::map<std::string, Definition, std::less<>> custom;
    // "5" is the final entry in abc2midi's initial registry.
    std::string newest      = "5";
    size_t      extra_slots = 0;
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
        int              lane,
        Meter            meter,
        long double      time,
        Location         at
    );
    // Open overlay lanes in order before advancing or emitting harmony for them.
    void open_lane(int lane, Meter meter, long double bar_origin, const BackingOpening& opening);
    void transpose(int lane, MidiTransposition change);
    void chord_gap(int lane, int gap, long double time, Location at);
    void declare_drums(int lane, int opening_gap);
    // An old drum mark gates an existing drum part; only a written drumon declares one.
    void drum_gate(int lane, bool enabled, long double time, Location at);
    void map_drums(int lane, DrumRoute route, long double time, Location at);
    void apply(const ChannelEffect& effect, int lane, i32 time);
    void harmony(const Harmony::Voicing& chord, int lane, int transpose, Location at);
    void meter(int lane, Meter change);
    void bar(int lane, long double time, Location at);
    void advance(int lane, long double end, Location at);
    // Drone MIDI deltas may put the last emitted event past the written time.
    [[nodiscard]] i32 drone_event_time(int lane) const {
        return harmonic_parts[lane].drone.emitted;
    }
    // Close active gates and return the backing for each lane, in lane order.
    vec<PerformedBacking> finish(std::span<const long double> lane_ends, Location at);

private:
    struct Step {
        char kind;
        int  length;
        int  pitch    = 0;
        int  velocity = 0;
    };

    // MIDI backing slots and releases share a queue. Inserting an earlier
    // slot can shorten outstanding delays, including sounding notes.
    class BackingQueue {
    public:
        void set_gap(int ticks) { gap = ticks; }
        void slot(long double time, size_t index, bool unique = false);
        void release(long double time, TrackNative<Tick>& track, size_t index);
        template<class Play>
        void advance(long double end, Location at, Diagnostics& diagnostics, Play play);
        void finish(i32 stop);

    private:
        struct Slot {
            size_t index;
        };
        struct Release {
            TrackNative<Tick>* track;
            size_t             index;

            void stop(i32 tick) const;
        };
        struct Pending {
            long double                 time;
            std::variant<Slot, Release> action;
        };
        vec<Pending> pending;
        int          gap = ChordAttackSettings::default_gap;

        void enqueue(Pending item);
    };

    // Installing a pattern retains requests already in the queue. A span
    // change affects future installs and restarts playback at the new cursor.
    class ChordPattern {
    public:
        void install(vec<Step> replacement, Meter meter);
        void set_span(int bars, long double now);
        void set_enabled(bool on) { enabled = on; }
        void set_gap(int gap) { queue.set_gap(gap); }
        void bar(long double now);
        template<class Play>
        void advance(long double end, Location at, Diagnostics& diagnostics, Play play);
        void release(long double time, TrackNative<Tick>& track, size_t index) {
            queue.release(time, track, index);
        }
        void finish(i32 stop) { queue.finish(stop); }

    private:
        BackingQueue queue;
        vec<Step>    steps;
        long double  slot_length = 0;
        int          bars        = 1;
        size_t       cursor      = 0;
        bool         enabled     = true;
    };

    struct DrumDefinition {
        vec<Step> steps;
        Meter     meter;
    };
    struct DrumBar {};
    struct DrumSpan {
        int bars;
    };
    struct DrumGate {
        bool enabled;
    };
    struct DrumGap {
        int ticks;
    };
    struct DrumEvent {
        long double                                                                   time;
        Location                                                                      at;
        std::variant<DrumDefinition, DrumBar, DrumSpan, DrumGate, DrumGap, DrumRoute> change;
    };
    struct DrumPart {
        bool           declared    = false;
        int            opening_gap = ChordAttackSettings::default_gap;
        vec<DrumEvent> events;
    };
    struct DrumSequence;

    struct Instrument {
        struct TimedWrite {
            i32                  time;
            int                  program;
            ChannelEffect::Write event;
        };

        int                              program = 0;
        int                              octave  = 0;
        int                              velocity;
        std::map<int, TrackNative<Tick>> tracks;
        vec<TimedWrite>                  writes;

        TrackNative<Tick>& track_for(int patch);
        void play(i64 pitch, i32 start, i32 duration, long double time, ChordPattern& pattern);
        void write_effects();
        void finish(vec<TrackNative<Tick>>& output, i32 end);
    };

    struct Chord {
        i64      bass;
        vec<i64> pitches;
        Location at;

        [[nodiscard]] std::optional<i64> arpeggio(char action) const;
    };

    struct DroneSetup {
        int                program = 70;
        std::array<int, 2> pitch{45, 33};
        std::array<int, 2> velocity{80, 80};
    };

    struct Drone {
        DroneSetup                        setup;
        std::optional<std::array<i32, 2>> attacks;
        std::map<int, TrackNative<Tick>>  tracks;
        i32                               emitted        = 0;
        i32                               program_at     = 0;
        i32                               previous_delta = 0;
        int                               transposition  = 0;
        int                               sounding_shift = 0;
    };

    struct HarmonicPart {
        ChordPattern         pattern;
        Instrument           bass{.velocity = 80};
        Instrument           chord{.velocity = 75};
        std::optional<Chord> current_chord;
        Drone                drone;
        int                  transposition = 0;
    };

    Diagnostics&            diagnostics;
    const ProgramNumbers&   programs;
    vec<HarmonicPart>       harmonic_parts;
    std::map<int, DrumPart> drum_parts;
    DroneSetup              drone_setup;

    static std::string_view default_chords(Meter meter);
    vec<Step>               pattern(std::string_view text, std::string_view allowed, Location at);
    DrumDefinition          drum_pattern(std::string_view args, Meter meter, Location at);
    void                    finish_drums(
        std::span<PerformedBacking> lanes, std::span<const long double> lane_ends, Location at
    );
    void start_drone(Drone& drone, long double time, Location at);
    void stop_drone(Drone& drone, long double time, Location at);
    void close_drone(Drone& drone, i32 release);
    void drone_settings(Drone& drone, std::string_view args, long double time, Location at);
    bool instrument_directive(
        HarmonicPart& part, std::string_view name, std::string_view args, Location at
    );
    bool drone_directive(
        Drone& drone, std::string_view name, std::string_view args, long double time, Location at
    );
    void chord_step(
        HarmonicPart& part, Step step, long double time, long double length, Location at
    );
    std::pair<i32, i32> backing_times(long double time, long double length, Location at);
};

}   // namespace symusic::abc::detail

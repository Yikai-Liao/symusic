#pragma once

#include <map>
#include <span>

#include "settings.h"
#include "symusic/score.h"

namespace symusic::abc::detail {

// Stateful playback of chord symbols, ABC MIDI backing patterns and drones.
// The voice advances this clock before changing harmony or applying directives.
class Accompaniment {
public:
    explicit Accompaniment(Meter meter, Diagnostics& diagnostics);
    bool directive(std::string_view name, std::string_view args, long double time, Location at);
    void harmony(std::string_view name, int transpose, Location at);
    void meter(Meter change, long double time);
    void bar(long double time);
    void advance(long double end, Location at);
    void finish(vec<TrackNative<Tick>>& tracks, long double end, Location at);

private:
    struct Step {
        char kind;
        int  length;
        int  pitch    = 0;
        int  velocity = 0;
    };

    struct Pattern {
        vec<Step>   steps;
        long double next      = 0;
        int         units     = 0;
        int         bars      = 1;
        int         bar_count = 0;
        size_t      index     = 0;
        bool        active    = false;

        void               reset(long double time);
        void               bar(long double time);
        void               assign(vec<Step> replacement, long double time);
        [[nodiscard]] bool ready(long double end) const;
    };

    struct Instrument {
        int                              program = 0;
        int                              octave  = 0;
        int                              velocity;
        std::map<int, TrackNative<Tick>> tracks;

        void play(int pitch, i32 time, i32 duration);
        void finish(vec<TrackNative<Tick>>& output, i32 end);
    };

    struct Chord {
        int      bass;
        vec<int> pitches;

        [[nodiscard]] std::optional<int> arpeggio(char action) const;
    };

    struct Drone {
        int                              program = 70;
        std::array<int, 2>               pitch{45, 33};
        std::array<int, 2>               velocity{80, 80};
        std::optional<long double>       start;
        std::map<int, TrackNative<Tick>> tracks;
    };

    Diagnostics&                         diagnostics;
    Meter                                current_meter;
    Pattern                              chords;
    Pattern                              drums;
    Instrument                           bass{.velocity = 80};
    Instrument                           chord{.velocity = 75};
    std::optional<Chord>                 current_chord;
    TrackNative<Tick>                    drum_track{"", 0, true};
    Drone                                drone;
    std::map<std::string_view, vec<int>> custom_chords;

    static std::string_view default_chords(Meter meter);
    vec<Step>               pattern(std::string_view text, std::string_view allowed, Location at);
    void                    chord_pattern(std::string_view args, long double time, Location at);
    void                    drum_pattern(std::string_view args, long double time, Location at);
    void                    chord_type(std::string_view args, Location at);
    std::span<const int>    chord_intervals(std::string_view name, Location at) const;
    void                    close_drone(long double time, Location at);
    void                    drone_settings(std::string_view args, long double time, Location at);
    bool                    pattern_directive(
        std::string_view name, std::string_view args, long double time, Location at
    );
    bool instrument_directive(std::string_view name, std::string_view args, Location at);
    bool drone_directive(
        std::string_view name, std::string_view args, long double time, Location at
    );
    void chord_step(Step step, i32 time, i32 duration);
    template<class Play>
    void advance(Pattern& pattern, long double end, Location at, Play play);
};

}   // namespace symusic::abc::detail

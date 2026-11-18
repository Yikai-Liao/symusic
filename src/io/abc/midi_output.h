#pragma once

#include <map>

#include "settings.h"
#include "symusic/track.h"

namespace symusic::abc::detail {

struct MidiTarget {
    int lane;
    int channel;
    int program;
};

class VoiceOutput;

// A stable note handle until its voice finishes; note vectors may grow in between.
class NoteAddress {
public:
    [[nodiscard]] int pitch() const;
    void              release_at(i32 end) const;
    void              lyric(std::string text) const;

private:
    friend class VoiceOutput;
    NoteAddress(TrackNative<Tick>& track, size_t index) : track(&track), index(index) {}
    [[nodiscard]] Note<Tick>& note() const { return track->notes[index]; }

    TrackNative<Tick>* track;
    size_t             index;
};

// Collects one notation voice's performed MIDI events. Routes attacks and
// controls by lane/channel/program, but pairs sustain by lane/channel even
// across program changes. Only this object owns track storage and quantizes
// note endpoints. Invalid note ranges fail through the reader's diagnostics.
class VoiceOutput {
public:
    VoiceOutput(std::string name, Diagnostics& diagnostics);
    VoiceOutput(const VoiceOutput&)            = delete;
    VoiceOutput& operator=(const VoiceOutput&) = delete;

    std::optional<NoteAddress> attack(
        MidiTarget  target,
        int         pitch,
        long double start,
        long double duration,
        int         stagger,
        int         velocity,
        Location    at
    );
    void control(MidiTarget target, i32 time, int number, int value, Location at);
    void finish(vec<TrackNative<Tick>>& tracks);

private:
    using TrackKey   = std::tuple<int, int, int>;
    using ChannelKey = std::pair<int, int>;

    std::string                                              name;
    Diagnostics&                                             diagnostics;
    std::map<TrackKey, TrackNative<Tick>>                    output;
    std::map<ChannelKey, std::pair<TrackNative<Tick>*, i32>> pedal_starts;

    TrackNative<Tick>& track(MidiTarget target);
    void sustain(ChannelKey channel, TrackNative<Tick>& track, i32 time, int value, Location at);
};

}   // namespace symusic::abc::detail

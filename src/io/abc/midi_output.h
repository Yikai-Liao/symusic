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

struct MidiPitch {
    int  note;
    int  wheel;
    auto operator<=>(const MidiPitch&) const = default;
};

MidiPitch altered_pitch(int base, Alteration alteration);

class VoiceOutput;

// A stable note handle until its voice finishes; note vectors may grow in between.
class NoteAddress {
public:
    void release_at(i32 end) const;
    void lyric(std::string text) const;

private:
    friend class VoiceOutput;
    NoteAddress(TrackNative<Tick>& track, size_t index, i32& gate) :
        track(&track), index(index), gate(&gate) {}
    [[nodiscard]] Note<Tick>& note() const { return track->notes[index]; }

    TrackNative<Tick>* track;
    size_t             index;
    i32*               gate;
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
        MidiPitch   pitch,
        long double start,
        long double duration,
        int         stagger,
        int         velocity,
        Location    at
    );
    void control(MidiTarget target, i32 time, int number, int value, Location at);
    // Map channel-effect directives onto Score events; leave unrelated MIDI
    // commands to the playback and accompaniment state owners.
    bool effect(
        std::string_view name, std::string_view args, MidiTarget target, i32 time, Location at
    );
    void finish(vec<TrackNative<Tick>>& tracks);

private:
    using TrackKey   = std::tuple<int, int, int>;
    using ChannelKey = std::pair<int, int>;

    struct ChannelOutput {
        ChannelOutput(std::string name, u8 program, bool drums) :
            track(std::move(name), program, drums) {}

        TrackNative<Tick> track;
        i32               sounding_until = 0;
        int               wheel          = 0;
    };

    std::string                                              name;
    Diagnostics&                                             diagnostics;
    std::map<TrackKey, ChannelOutput>                        output;
    std::map<ChannelKey, std::pair<TrackNative<Tick>*, i32>> pedal_starts;

    ChannelOutput& channel(MidiTarget target);
    void tune(ChannelOutput& owner, MidiTarget target, MidiPitch pitch, i32 time, Location at);
    void sustain(ChannelKey channel, TrackNative<Tick>& track, i32 time, int value, Location at);
};

}   // namespace symusic::abc::detail

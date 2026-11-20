#pragma once

#include <array>
#include <optional>
#include <span>
#include <string_view>

#include "symusic/track.h"

namespace symusic::abc::detail {

class Diagnostics;
struct Location;

// One parsed MIDI instruction with its original target and ordered writes.
// Controller numbers and values are MIDI data bytes; wheel values are signed
// offsets from center. Output owners choose the track and apply the writes.
struct ChannelEffect {
    enum class Destination { voice, bass, chord };
    enum class Kind { control, control_default, wheel };
    struct Write {
        Kind kind;
        int  value;
        u8   controller = 0;
    };

    Destination          destination;
    std::array<Write, 3> events;
    size_t               count;

    [[nodiscard]] std::span<const Write> writes() const { return {events.data(), count}; }
};

// An unclaimed name returns no effect. A recognized instruction with invalid
// arguments raises a located diagnostic rather than falling through.
[[nodiscard]] std::optional<ChannelEffect> channel_effect(
    std::string_view name, std::string_view args, Location at, Diagnostics& diagnostics
);

// A pedal belongs to the channel, even if a program change splits its output tracks.
class SustainPedal {
public:
    void control(TrackNative<Tick>& track, i32 time, int value);

private:
    TrackNative<Tick>* start_track = nullptr;
    i32                start       = 0;
};

}   // namespace symusic::abc::detail

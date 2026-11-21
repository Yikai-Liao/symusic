#pragma once

#include <array>
#include <optional>
#include <span>
#include <string_view>

#include "symusic/track.h"

namespace symusic::abc::detail {

class Diagnostics;
struct Location;

// Sustain follows the MIDI channel across program changes. Release publishes
// the interval on the Score track that received the pedal-down message.
class SustainPedal {
public:
    void control(TrackNative<Tick>& track, i32 time, i32 value);

private:
    TrackNative<Tick>* start_track = nullptr;
    i32                start       = 0;
};

// One instruction expands to at most three ordered writes. A controller takes
// a MIDI data byte. A wheel takes a signed offset from center.
struct ChannelEffect {
    enum class Destination { Voice, Bass, Chord };
    struct Write {
        std::optional<u8> controller;
        i32               value;

        void emit(TrackNative<Tick>& track, i32 time, SustainPedal& pedal) const;
    };

    Destination          destination;
    std::array<Write, 3> events;
    size_t               count;
    bool                 sets_default;

    [[nodiscard]] std::span<const Write> writes() const { return {events.data(), count}; }
};

// An unclaimed name returns no effect. A recognized instruction with invalid
// arguments raises a located diagnostic rather than falling through.
[[nodiscard]] std::optional<ChannelEffect> channel_effect(
    std::string_view name, std::string_view args, Location at, Diagnostics& diagnostics
);

}   // namespace symusic::abc::detail

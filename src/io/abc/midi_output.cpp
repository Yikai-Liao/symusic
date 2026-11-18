#include "midi_output.h"

#include <limits>

namespace symusic::abc::detail {

int NoteAddress::pitch() const { return note().pitch; }

void NoteAddress::release_at(i32 end) const {
    auto& event    = note();
    event.duration = std::max(0, end - event.time);
}

void NoteAddress::lyric(std::string text) const {
    track->lyrics.emplace_back(note().time, std::move(text));
}

VoiceOutput::VoiceOutput(std::string name, Diagnostics& diagnostics) :
    name(std::move(name)), diagnostics(diagnostics) {}

TrackNative<Tick>& VoiceOutput::track(MidiTarget target) {
    const auto [lane, channel, program] = target;
    return output
        .try_emplace(
            {lane, channel, program}, lane == 0 ? name : "", static_cast<u8>(program), channel == 10
        )
        .first->second;
}

std::optional<NoteAddress> VoiceOutput::attack(
    MidiTarget  target,
    int         pitch,
    long double start,
    long double duration,
    int         stagger,
    int         velocity,
    Location    at
) {
    if (pitch < 0 || pitch > midi_max) {
        diagnostics.error(
            at, "pitch-range", fmt::format("pitch {} is outside MIDI range 0–127", pitch)
        );
    }
    if (velocity == 0) return {};
    const auto time = tick(start + 1 + stagger, diagnostics, at);
    const auto gate = std::max(0, tick(duration, diagnostics, at) - 1 - stagger);
    if (gate > std::numeric_limits<i32>::max() - time)
        diagnostics.error(at, "time-range", "note end exceeds the score's tick range");
    diagnostics.spend(1, at);
    auto& owner = track(target);
    owner.notes.emplace_back(time, gate, static_cast<i8>(pitch), static_cast<i8>(velocity));
    return NoteAddress(owner, owner.notes.size() - 1);
}

void VoiceOutput::sustain(
    ChannelKey channel, TrackNative<Tick>& owner, i32 time, int value, Location at
) {
    if (value >= 64) {
        pedal_starts.try_emplace(channel, &owner, time);
        return;
    }
    auto pedal = pedal_starts.extract(channel);
    if (pedal.empty()) return;
    const auto [start_track, start] = pedal.mapped();
    if (time >= start)
        start_track->pedals.emplace_back(start, time - start);
    else
        diagnostics.warn(at, "backward-pedal", "ignored pedal release before its press");
}

void VoiceOutput::control(MidiTarget target, i32 time, int number, int value, Location at) {
    auto& owner = track(target);
    owner.controls.emplace_back(time, static_cast<u8>(number), static_cast<u8>(value));
    if (number == 64) sustain({target.lane, target.channel}, owner, time, value, at);
}

void VoiceOutput::finish(vec<TrackNative<Tick>>& tracks) {
    for (auto& [key, track] : output) {
        if (!track.empty()) tracks.push_back(std::move(track));
    }
}

}   // namespace symusic::abc::detail

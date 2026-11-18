#include "midi_output.h"

#include <limits>

namespace symusic::abc::detail {

void NoteAddress::release_at(i32 end) const {
    auto& event    = note();
    event.duration = std::max(0, end - event.time);
    *gate          = std::max(*gate, end);
}

void NoteAddress::lyric(std::string text) const {
    track->lyrics.emplace_back(note().time, std::move(text));
}

VoiceOutput::VoiceOutput(std::string name, Diagnostics& diagnostics) :
    name(std::move(name)), diagnostics(diagnostics) {}

VoiceOutput::Target VoiceOutput::target(int lane) const {
    const auto& [selected, programs] = routing[lane];
    return {lane, selected, programs[selected - 1]};
}

VoiceOutput::ChannelOutput& VoiceOutput::channel(Target target) {
    const auto [lane, channel, program] = target;
    return output
        .try_emplace(
            {lane, channel, program},
            lane == 0 ? name : "",
            static_cast<u8>(program),
            channel == midi_drum_channel
        )
        .first->second;
}

void VoiceOutput::tune(
    ChannelOutput& owner, Target target, MidiPitch pitch, i32 time, Location at
) {
    if (target.channel == midi_drum_channel || owner.accidental_wheel == pitch.wheel) return;
    if (time < owner.sounding_until)
        diagnostics.warn(
            at, "shared-pitch-wheel", "pitch bend also affects overlapping notes on this voice"
        );
    owner.accidental_wheel = pitch.wheel;
    owner.track.pitch_bends.emplace_back(time, pitch.wheel);
}

std::optional<NoteAddress> VoiceOutput::attack(
    int         lane,
    MidiPitch   pitch,
    long double start,
    long double duration,
    int         stagger,
    int         velocity,
    Location    at
) {
    if (pitch.note < 0 || pitch.note > midi_max) {
        diagnostics.error(
            at, "pitch-range", fmt::format("pitch {} is outside MIDI range 0–127", pitch.note)
        );
    }
    if (velocity == 0) return {};
    const auto time = tick(start + PlaybackGrid::attack_offset + stagger, diagnostics, at);
    const auto gate
        = std::max(0, tick(duration, diagnostics, at) - PlaybackGrid::release_trim - stagger);
    if (gate > std::numeric_limits<i32>::max() - time)
        diagnostics.error(at, "time-range", "note end exceeds the score's tick range");
    diagnostics.spend(PlaybackWork::attack, at);
    const auto destination = target(lane);
    auto&      owner       = channel(destination);
    tune(owner, destination, pitch, time, at);
    owner.sounding_until = std::max(owner.sounding_until, time + gate);
    owner.track.notes.emplace_back(
        time, gate, static_cast<i8>(pitch.note), static_cast<i8>(velocity)
    );
    return NoteAddress(owner.track, owner.track.notes.size() - 1, owner.sounding_until);
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

void VoiceOutput::control(int lane, i32 time, int number, int value, Location at) {
    const auto destination = target(lane);
    auto&      owner       = channel(destination).track;
    owner.controls.emplace_back(time, static_cast<u8>(number), static_cast<u8>(value));
    if (number == 64) sustain({lane, destination.channel}, owner, time, value, at);
}

void VoiceOutput::program(int lane, std::string_view args, Location at) {
    const auto first           = midi_integer(args, 0, midi_max, diagnostics, at);
    auto& [selected, programs] = routing[lane];
    if (trim(args).empty()) {
        programs[selected - 1] = first;
        return;
    }
    if (first < 1 || first > 16)
        diagnostics.error(at, "invalid-midi-channel", "program channel must be between 1 and 16");
    programs[first - 1] = midi_integer(args, 0, midi_max, diagnostics, at);
}

bool VoiceOutput::route(std::string_view name, std::string_view args, int lane, Location at) {
    if (name == "program")
        program(lane, args, at);
    else if (name == "channel")
        select_channel(lane, midi_integer(args, 1, 16, diagnostics, at));
    else
        return false;
    return true;
}

bool VoiceOutput::effect(
    std::string_view name, std::string_view args, int lane, i32 time, Location at
) {
    const auto byte = [&] { return midi_integer(args, 0, midi_max, diagnostics, at); };
    const auto cc   = [&](int number, int value) { control(lane, time, number, value, at); };
    if (route(name, args, lane, at)) return true;
    if (name == "control") {
        const int number = byte();
        cc(number, byte());
    } else if (name == "pitchbend") {
        const int low  = byte();
        const int high = trim(args).empty() ? 0 : byte();
        channel(target(lane))
            .track.pitch_bends.emplace_back(time, (high << 7) + low - midi_bend_center);
    } else if (name == "pitchbendrange") {
        const int semitones = byte();
        cc(101, 0);
        cc(6, semitones);
        cc(38, 0);
    } else if (name == "portamento") {
        const int speed = midi_integer(args, 0, 63, diagnostics, at);
        cc(65, 127);
        cc(5, speed);
    } else if (name == "noportamento")
        cc(65, 0);
    else
        return false;
    return true;
}

void VoiceOutput::finish(vec<TrackNative<Tick>>& tracks) {
    for (auto& [key, owner] : output) {
        if (!owner.track.empty()) tracks.push_back(std::move(owner.track));
    }
}

}   // namespace symusic::abc::detail

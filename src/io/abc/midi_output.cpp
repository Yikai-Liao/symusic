#include "midi_output.h"

#include <cmath>
#include <limits>

namespace symusic::abc::detail {

namespace {

constexpr size_t bend_steps = 256;
constexpr int    wheel_min  = -midi_bend_center;
constexpr int    wheel_max  = midi_bend_center - 1;

constexpr std::array<int64_t, 8> default_bend{100, 400, 700, 1000, 1300, 1600, 1900, 2200};

int bend_integer(std::string_view& args, Location at, Diagnostics& diagnostics) {
    const auto value = integer(word(args));
    if (!value) diagnostics.error(at, "invalid-bend-curve", "expected a signed integer bend value");
    return *value;
}

vec<int64_t> bend_string(
    std::string_view args, size_t subdivisions, Location at, Diagnostics& diagnostics
) {
    vec<int64_t> contour;
    while (!trim(args).empty()) {
        if (contour.size() == bend_steps)
            diagnostics.error(
                at, "bend-curve-limit", fmt::format("bend curve exceeds {} segments", bend_steps)
            );
        const auto delta    = bend_integer(args, at, diagnostics);
        int64_t    previous = 0;
        for (size_t step = 1; step <= subdivisions; ++step) {
            const auto cumulative = std::llround(static_cast<double>(delta) * step / subdivisions);
            contour.push_back(cumulative - previous);
            previous = cumulative;
        }
    }
    if (contour.empty())
        diagnostics.error(at, "invalid-bend-curve", "bend curve needs at least one value");
    return contour;
}

vec<int64_t> bend_velocity(std::string_view args, Location at, Diagnostics& diagnostics) {
    int64_t    velocity     = bend_integer(args, at, diagnostics);
    const auto acceleration = bend_integer(args, at, diagnostics);
    int        count        = 8;
    if (!trim(args).empty()) {
        const auto written = bend_integer(args, at, diagnostics);
        if (written >= 1 && written < 100)
            count = written;
        else
            diagnostics.warn(
                at, "invalid-bend-steps", "bendvelocity steps must be 1 to 99; used 8"
            );
    }
    if (!trim(args).empty())
        diagnostics.warn(at, "extra-bend-arguments", "ignored extra bendvelocity arguments");
    vec<int64_t> contour;
    contour.reserve(static_cast<size_t>(count));
    for (int step = 0; step < count; ++step) {
        contour.push_back(velocity);
        velocity += acceleration;
    }
    return contour;
}

}   // namespace

void NoteAddress::release_at(i32 end) const {
    auto& event    = note();
    event.duration = std::max(0, end - event.time);
    *gate          = std::max(*gate, end);
}

VoiceOutput::VoiceOutput(
    std::string           name,
    const DrumMap&        drum_map,
    const ProgramNumbers& programs,
    Diagnostics&          diagnostics
) : name(std::move(name)), diagnostics(diagnostics), drum_map(drum_map), programs(programs) {}

void VoiceOutput::Routing::attack_on(size_t chord_member) {
    if (extra_channels == 0) return;
    selected = chord_member > 0 && chord_member <= extra_channels ?
                   midi_channels + static_cast<int>(chord_member) :
                   chord_base;
}

void VoiceOutput::Routing::allocate_chord_channels(size_t count) {
    chord_base     = selected;
    extra_channels = count;
    std::fill_n(programs.begin() + midi_channels, count, programs[selected - 1]);
}

VoiceOutput::Target VoiceOutput::target(int lane) const {
    const auto& route = routing[lane];
    return {lane, route.selected, route.programs[route.selected - 1]};
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

Attack VoiceOutput::attack(
    int                   lane,
    MidiPitch             pitch,
    long double           start,
    long double           duration,
    int                   stagger,
    int                   velocity,
    Location              at,
    std::optional<size_t> chord_member
) {
    if (pitch.note < 0 || pitch.note > midi_max) {
        diagnostics.error(
            at, "pitch-range", fmt::format("pitch {} is outside MIDI range 0–127", pitch.note)
        );
    }
    if (chord_member) routing[lane].attack_on(*chord_member);
    const auto time        = tick(start + PlaybackGrid::attack_offset + stagger, diagnostics, at);
    const auto destination = target(lane);
    auto&      owner       = channel(destination);
    Attack     attack{LyricAnchor(owner.track, time), {}};
    if (velocity == 0) return attack;
    const auto gate
        = std::max(0, tick(duration, diagnostics, at) - PlaybackGrid::release_trim - stagger);
    if (gate > std::numeric_limits<i32>::max() - time)
        diagnostics.error(at, "time-range", "note end exceeds the score's tick range");
    diagnostics.spend(PlaybackWork::attack, at);
    tune(owner, destination, pitch, time, at);
    if (destination.channel == midi_drum_channel) pitch.note = drum_map.pitch(pitch.note);
    const auto overlaps  = time < owner.sounding_until;
    owner.sounding_until = std::max(owner.sounding_until, time + gate);
    owner.track.notes.emplace_back(
        time, gate, static_cast<i8>(pitch.note), static_cast<i8>(velocity)
    );
    attack.note = NoteAddress(owner.track, owner.track.notes.size() - 1, owner.sounding_until);
    attach_bend(lane, *attack.note, owner.accidental_wheel, overlaps);
    return attack;
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
    auto       remaining = args;
    const auto first     = word(remaining);
    auto&      route     = routing[lane];
    int        channel   = route.selected;
    if (!trim(remaining).empty()) {
        const auto explicit_channel = integer(first);
        if (!explicit_channel || *explicit_channel < 1 || *explicit_channel > midi_channels)
            diagnostics.error(
                at, "invalid-midi-channel", "program channel must be between 1 and 16"
            );
        channel = *explicit_channel;
        args    = remaining;
    }
    route.programs[channel - 1] = programs.read(args, diagnostics, at);
}

bool VoiceOutput::route(std::string_view name, std::string_view args, int lane, Location at) {
    if (name == "program")
        program(lane, args, at);
    else if (name == "channel")
        select_channel(lane, midi_integer(args, 1, midi_channels, diagnostics, at));
    else if (name == "makechordchannels")
        routing[lane].allocate_chord_channels(
            midi_integer(args, 1, max_chord_channels, diagnostics, at)
        );
    else
        return false;
    return true;
}

bool VoiceOutput::effect(
    std::string_view name, std::string_view args, int lane, i32 time, Location at
) {
    if (name == "pitchbend") {
        const int low  = midi_integer(args, 0, midi_max, diagnostics, at);
        const int high = trim(args).empty() ? 0 : midi_integer(args, 0, midi_max, diagnostics, at);
        channel(target(lane))
            .track.pitch_bends.emplace_back(time, (high << 7) + low - midi_bend_center);
        return true;
    }
    return route(name, args, lane, at) || bend_directive(name, args, lane, at)
           || channel_effect(name, args, lane, time, at);
}

bool VoiceOutput::channel_effect(
    std::string_view name, std::string_view args, int lane, i32 time, Location at
) {
    const auto byte = [&] { return midi_integer(args, 0, midi_max, diagnostics, at); };
    const auto cc   = [&](int number, int value) { control(lane, time, number, value, at); };
    if (name == "control") {
        const int number = byte();
        cc(number, byte());
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

bool VoiceOutput::bend_directive(
    std::string_view name, std::string_view args, int lane, Location at
) {
    vec<int64_t> contour;
    if (name == "bendvelocity")
        contour = bend_velocity(args, at, diagnostics);
    else if (name == "bendstring" || name == "bendstringex")
        contour = bend_string(args, name == "bendstring" ? 1 : 4, at, diagnostics);
    else
        return false;
    diagnostics.spend(contour.size(), at);
    bends[lane].curve = std::make_shared<const vec<int64_t>>(std::move(contour));
    return true;
}

void VoiceOutput::bend_next(int lane, Location at) { bends[lane].next = at; }

void VoiceOutput::attach_bend(int lane, NoteAddress note, int base, bool overlapping) {
    auto& setting = bends[lane];
    if (!setting.next) return;
    const auto mark = *std::exchange(setting.next, {});
    if (overlapping)
        diagnostics.warn(
            mark, "shared-pitch-wheel", "bend also affects overlapping notes on this voice"
        );
    diagnostics.spend((setting.curve ? setting.curve->size() : default_bend.size()) + 1, mark);
    bent_notes.push_back({note, setting.curve, base, mark});
}

void VoiceOutput::write_bend(const BentNote& bend) {
    const auto& note    = bend.note.note();
    auto&       events  = bend.note.track->pitch_bends;
    const auto  curve   = bend.curve ? std::span<const int64_t>(*bend.curve) :
                                       std::span<const int64_t>(default_bend);
    const auto  step    = note.duration / static_cast<i32>(curve.size());
    const auto  release = note.time + note.duration;
    auto        segment = bend.curve ? 0 : 1;
    int64_t     value   = bend.base;
    bool        clipped = false;
    for (const auto delta : curve) {
        const auto requested = value + delta;
        value                = std::clamp<int64_t>(requested, wheel_min, wheel_max);
        clipped |= value != requested;
        events.emplace_back(note.time + segment++ * step, static_cast<i32>(value));
    }
    events.emplace_back(release, bend.base);
    if (bend.curve && clipped)
        diagnostics.warn(bend.mark, "bend-range", "clipped the bend curve to the MIDI wheel range");
}

void VoiceOutput::finish(vec<TrackNative<Tick>>& tracks) {
    for (const auto& setting : bends) {
        if (setting.next)
            diagnostics.warn(*setting.next, "orphan-decoration", "bend has no following attack");
    }
    for (const auto& bend : bent_notes) write_bend(bend);
    for (auto& [key, owner] : output) {
        auto& events = owner.track.pitch_bends;
        if (!bent_notes.empty() && !std::ranges::is_sorted(events, {}, &PitchBend<Tick>::time))
            std::ranges::stable_sort(events, {}, &PitchBend<Tick>::time);
        if (!owner.track.empty()) tracks.push_back(std::move(owner.track));
    }
}

}   // namespace symusic::abc::detail

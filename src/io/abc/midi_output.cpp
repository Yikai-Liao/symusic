#include "midi_output.h"

#include <cmath>
#include <limits>
#include <numeric>

namespace symusic::abc::detail {

namespace {

constexpr size_t bend_steps        = 256;
constexpr size_t controller_steps  = 255;
constexpr size_t controller_layers = 3;
constexpr int    wheel_min         = -midi_bend_center;
constexpr int    wheel_max         = midi_bend_center - 1;

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

// Inspect finalized gates, not provisional lengths from before ties resolve.
// Equal starts retain attack order for accidental tuning diagnostics.
struct VoiceOutput::NoteActivity {
    struct Overlap {
        bool prior = false;
        bool later = false;
    };
    struct Attack {
        i32 start;
        i32 open_until;
    };

    explicit NoteActivity(const vec<Note<Tick>>& notes);
    [[nodiscard]] bool intersects(Gate gate) const;

    vec<Overlap> overlaps;
    vec<Attack>  attacks;
};

VoiceOutput::NoteActivity::NoteActivity(const vec<Note<Tick>>& notes) : overlaps(notes.size()) {
    vec<size_t> order(notes.size());
    std::iota(order.begin(), order.end(), size_t{0});
    const auto start = [&](size_t index) { return notes[index].time; };
    if (!std::ranges::is_sorted(order, {}, start)) std::ranges::stable_sort(order, {}, start);
    attacks.reserve(notes.size());
    i32                   open_until = 0;
    std::optional<size_t> last;
    for (const auto index : order) {
        const auto& note      = notes[index];
        overlaps[index].prior = note.time < open_until;
        if (note.duration <= 0) continue;
        if (last) {
            const auto& previous  = notes[*last];
            overlaps[*last].later = note.time < previous.time + previous.duration;
        }
        last       = index;
        open_until = std::max(open_until, note.time + note.duration);
        attacks.push_back({note.time, open_until});
    }
}

bool VoiceOutput::NoteActivity::intersects(Gate gate) const {
    const auto stop = std::ranges::lower_bound(attacks, gate.end(), {}, &Attack::start);
    return stop != attacks.begin() && std::prev(stop)->open_until > gate.time;
}

void NoteAddress::release_at(i32 end) const {
    auto& event    = note();
    event.duration = std::max(0, end - event.time);
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
    Attack     hit{LyricAnchor(owner.track, time), {}};
    diagnostics.spend(PlaybackWork::attack, at);
    const auto gate = note_gate(time, duration, stagger, at);
    if (velocity != 0) hit.note = emit_note(owner, destination, pitch, gate, velocity);
    const auto note = hit.note ? std::optional{hit.note->index} : std::nullopt;
    if (destination.channel != midi_drum_channel)
        owner.tuning.push_back({time, pitch.wheel, event_order++, at, note});
    attach_envelope(
        owner, destination, note ? EnvelopeGate{*note} : EnvelopeGate{gate}, pitch.wheel
    );
    return hit;
}

VoiceOutput::Gate VoiceOutput::note_gate(i32 time, long double duration, int stagger, Location at) {
    const auto length
        = std::max(0, tick(duration, diagnostics, at) - PlaybackGrid::release_trim - stagger);
    if (length > std::numeric_limits<i32>::max() - time)
        diagnostics.error(at, "time-range", "note end exceeds the score's tick range");
    return {time, length};
}

NoteAddress VoiceOutput::emit_note(
    ChannelOutput& owner, Target destination, MidiPitch pitch, Gate gate, int velocity
) {
    if (destination.channel == midi_drum_channel) pitch.note = drum_map.pitch(pitch.note);
    owner.track.notes.emplace_back(
        gate.time, gate.duration, static_cast<i8>(pitch.note), static_cast<i8>(velocity)
    );
    return NoteAddress(owner.track, owner.track.notes.size() - 1);
}

void VoiceOutput::sustain(ChannelKey channel, TrackNative<Tick>& owner, i32 time, int value) {
    if (value >= 64) {
        pedal_starts.try_emplace(channel, &owner, time);
        return;
    }
    auto pedal = pedal_starts.extract(channel);
    if (pedal.empty()) return;
    const auto [start_track, start] = pedal.mapped();
    start_track->pedals.emplace_back(start, time - start);
}

void VoiceOutput::control(int lane, i32 time, int number, int value) {
    writes.push_back({target(lane), time, number, value, event_order++, WireKind::controller});
}

void VoiceOutput::write_events() {
    const auto sequence = [](const WireWrite& event) { return std::pair(event.time, event.order); };
    if (!std::ranges::is_sorted(writes, {}, sequence))
        std::ranges::stable_sort(writes, {}, sequence);
    ControllerDefaults defaults{};
    defaults[7]  = 96;         // Volume.
    defaults[8]  = 64;         // Balance.
    defaults[10] = 64;         // Pan.
    defaults[11] = midi_max;   // Expression.
    for (const auto& event : writes) write_event(event, defaults);
}

void VoiceOutput::write_event(const WireWrite& event, ControllerDefaults& defaults) {
    auto& owner = channel(event.destination);
    switch (event.kind) {
    case WireKind::tuning: owner.restore_wheel = event.value; [[fallthrough]];
    case WireKind::wheel: owner.track.pitch_bends.emplace_back(event.time, event.value); break;
    case WireKind::restore_wheel:
        owner.track.pitch_bends.emplace_back(event.time, owner.restore_wheel);
        break;
    default: write_controller(owner, event, defaults); break;
    }
}

void VoiceOutput::write_controller(
    ChannelOutput& owner, const WireWrite& event, ControllerDefaults& defaults
) {
    const auto [dest, time, number, value, order, kind] = event;
    if (kind == WireKind::controller_default) defaults[number] = value;
    const auto setting = kind == WireKind::restore_controller ? defaults[number] : value;
    owner.track.controls.emplace_back(time, static_cast<u8>(number), static_cast<u8>(setting));
    if (number == 64) sustain({dest.lane, dest.channel}, owner.track, time, setting);
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
        writes.push_back(
            {target(lane),
             time,
             0,
             (high << 7) + low - midi_bend_center,
             event_order++,
             WireKind::wheel}
        );
        return true;
    }
    return route(name, args, lane, at) || envelope_directive(name, args, lane, at)
           || channel_effect(name, args, lane, time, at);
}

bool VoiceOutput::channel_effect(
    std::string_view name, std::string_view args, int lane, i32 time, Location at
) {
    const auto byte = [&] { return midi_integer(args, 0, midi_max, diagnostics, at); };
    const auto cc   = [&](int number, int value) { control(lane, time, number, value); };
    if (name == "control") {
        const int number = byte();
        writes.push_back(
            {target(lane), time, number, byte(), event_order++, WireKind::controller_default}
        );
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

void VoiceOutput::controller_curve(LaneEnvelope& state, std::string_view args, Location at) {
    const auto      byte = [&] { return midi_integer(args, 0, midi_max, diagnostics, at); };
    ControllerCurve curve{byte(), {}};
    while (!trim(args).empty()) {
        if (curve.values.size() == controller_steps)
            diagnostics.error(at, "controller-curve-limit", "controlstring exceeds 255 values");
        curve.values.push_back(static_cast<u8>(byte()));
    }
    if (curve.values.empty())
        diagnostics.error(
            at, "empty-controller-curve", "controlstring needs a controller and at least one value"
        );
    auto layers = std::exchange(state.combine_next, false) && state.controllers ?
                      *state.controllers :
                      vec<shared<const ControllerCurve>>{};
    if (layers.size() == controller_layers)
        diagnostics.error(
            at, "controller-layer-limit", "shape supports at most 3 controller curves"
        );
    diagnostics.spend(curve.values.size(), at);
    layers.push_back(std::make_shared<const ControllerCurve>(std::move(curve)));
    state.controllers = std::make_shared<const decltype(layers)>(std::move(layers));
}

bool VoiceOutput::envelope_directive(
    std::string_view name, std::string_view args, int lane, Location at
) {
    auto& state = envelopes[lane];
    if (name == "controlcombo") {
        state.combine_next = true;
        return true;
    }
    if (name == "controlstring") {
        controller_curve(state, args, at);
        return true;
    }
    vec<int64_t> contour;
    if (name == "bendvelocity")
        contour = bend_velocity(args, at, diagnostics);
    else if (name == "bendstring" || name == "bendstringex")
        contour = bend_string(args, name == "bendstring" ? 1 : 4, at, diagnostics);
    else
        return false;
    diagnostics.spend(contour.size(), at);
    state.bend = std::make_shared<const BendCurve>(BendCurve{std::move(contour)});
    return true;
}

void VoiceOutput::envelope_next(int lane, EnvelopeMark mark, Location at) {
    envelopes[lane].pending = PendingEnvelope{mark, at};
}

VoiceOutput::BendProfile VoiceOutput::bend_for(const LaneEnvelope& state, EnvelopeMark mark) {
    if (state.bend || mark == EnvelopeMark::shape) return state.bend;
    static const auto fallback = std::make_shared<const BendCurve>(
        BendCurve{{100, 400, 700, 1000, 1300, 1600, 1900, 2200}, true}
    );
    return fallback;
}

size_t VoiceOutput::NoteEnvelope::work() const {
    size_t count = bend ? bend->deltas.size() + 1 : 0;
    if (controllers) {
        for (const auto& curve : *controllers) count += curve->values.size() + 1;
    }
    return count;
}

void VoiceOutput::attach_envelope(
    ChannelOutput& owner, Target destination, EnvelopeGate gate, int wheel
) {
    auto& state = envelopes[destination.lane];
    if (!state.pending) return;
    const auto [mark, at] = *std::exchange(state.pending, {});
    if (destination.channel == midi_drum_channel) return;
    NoteEnvelope envelope{
        gate,
        destination,
        bend_for(state, mark),
        mark == EnvelopeMark::shape ? state.controllers : ControllerLayers{},
        wheel,
        event_order++,
        at
    };
    const auto work = envelope.work();
    if (!work) {
        diagnostics.warn(
            at, "empty-note-shape", "shape needs a preceding bend or controller curve"
        );
        return;
    }
    diagnostics.spend(work, at);
    owner.note_envelopes.push_back(std::move(envelope));
}

void VoiceOutput::warn_tuning_overlap(Location at) {
    diagnostics.warn(
        at, "shared-pitch-wheel", "pitch bend also affects overlapping notes on this voice"
    );
}

void VoiceOutput::warn_envelope_overlap(const NoteEnvelope& envelope) {
    diagnostics.warn(
        envelope.at,
        envelope.bend ? "shared-pitch-wheel" : "shared-controller",
        "note envelope also affects overlapping notes on this voice"
    );
}

void VoiceOutput::write_bend(const NoteEnvelope& bend, Gate gate) {
    const auto& curve   = *bend.bend;
    const auto  step    = gate.duration / static_cast<i32>(curve.deltas.size());
    const auto  release = gate.end();
    int         segment = curve.leading_pause ? 1 : 0;
    int64_t     value   = bend.base;
    bool        clipped = false;
    for (const auto delta : curve.deltas) {
        const auto requested = value + delta;
        value                = std::clamp<int64_t>(requested, wheel_min, wheel_max);
        clipped |= value != requested;
        writes.push_back(
            {bend.destination,
             gate.time + segment++ * step,
             0,
             static_cast<int>(value),
             bend.order,
             WireKind::wheel}
        );
    }
    writes.push_back({bend.destination, release, 0, 0, bend.order, WireKind::restore_wheel});
    if (!curve.leading_pause && clipped)
        diagnostics.warn(bend.at, "bend-range", "clipped the bend curve to the MIDI wheel range");
}

void VoiceOutput::write_shape(const NoteEnvelope& envelope, Gate gate) {
    for (const auto& curve : *envelope.controllers) {
        const auto step = gate.duration / static_cast<i32>(curve->values.size());
        auto       time = gate.time;
        for (const int value : curve->values) {
            writes.push_back(
                {envelope.destination,
                 time,
                 curve->number,
                 value,
                 envelope.order,
                 WireKind::controller}
            );
            time += step;
        }
        writes.push_back(
            {envelope.destination,
             gate.end(),
             curve->number,
             0,
             envelope.order,
             WireKind::restore_controller}
        );
    }
}

void VoiceOutput::complete_envelopes() {
    for (const auto& setting : envelopes) {
        if (setting.pending) {
            const auto [mark, at] = *setting.pending;
            diagnostics.warn(
                at,
                "orphan-decoration",
                mark == EnvelopeMark::bend ? "bend has no following attack" :
                                             "shape has no following attack"
            );
        }
    }
    for (auto& [key, owner] : output) {
        const auto [lane, channel, program] = key;
        complete_tuning({lane, channel, program}, owner);
        complete_channel_envelopes(owner);
    }
}

void VoiceOutput::complete_tuning(Target target, ChannelOutput& owner) {
    auto& attacks = owner.tuning;
    if (std::ranges::none_of(attacks, [](const auto& attack) { return attack.wheel != 0; })) return;
    // A delayed chord member can sound after notes that were visited later.
    // Only adjacent requests on the performed timeline can share a wheel write.
    if (!std::ranges::is_sorted(attacks, {}, &TuningAttack::time))
        std::ranges::stable_sort(attacks, {}, &TuningAttack::time);
    int wheel          = 0;
    i32 sounding_until = 0;
    for (const auto& [time, value, order, at, note] : attacks) {
        if (value != wheel) {
            if (time < sounding_until) warn_tuning_overlap(at);
            wheel = value;
            writes.push_back({target, time, 0, value, order, WireKind::tuning});
        }
        if (note) {
            const auto& sounding = owner.track.notes[*note];
            sounding_until       = std::max(sounding_until, sounding.time + sounding.duration);
        }
    }
}

void VoiceOutput::complete_channel_envelopes(ChannelOutput& owner) {
    if (owner.note_envelopes.empty()) return;
    const NoteActivity activity(owner.track.notes);
    for (const auto& envelope : owner.note_envelopes)
        complete_envelope(envelope, owner.track.notes, activity);
}

void VoiceOutput::complete_envelope(
    const NoteEnvelope& envelope, const vec<Note<Tick>>& notes, const NoteActivity& activity
) {
    if (const auto* muted = std::get_if<Gate>(&envelope.gate)) {
        if (activity.intersects(*muted)) warn_envelope_overlap(envelope);
        write_envelope(envelope, *muted);
        return;
    }
    const auto index          = std::get<size_t>(envelope.gate);
    const auto [prior, later] = activity.overlaps[index];
    if (prior || later) warn_envelope_overlap(envelope);
    const auto& note = notes[index];
    write_envelope(envelope, {note.time, note.duration});
}

void VoiceOutput::write_envelope(const NoteEnvelope& envelope, Gate gate) {
    if (envelope.bend) write_bend(envelope, gate);
    if (envelope.controllers) write_shape(envelope, gate);
}

void VoiceOutput::finish(vec<TrackNative<Tick>>& tracks) {
    complete_envelopes();
    write_events();
    for (auto& [key, owner] : output)
        if (!owner.track.empty()) tracks.push_back(std::move(owner.track));
}

}   // namespace symusic::abc::detail

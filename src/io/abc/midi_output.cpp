#include "midi_output.h"

#include <cmath>
#include <limits>
#include <numeric>
#include <utility>

#include "expression.h"
#include "symusic/utils.h"

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

vec<i64> bend_string(
    std::string_view args, size_t subdivisions, Location at, Diagnostics& diagnostics
) {
    vec<i64> contour;
    while (!trim(args).empty()) {
        if (contour.size() == bend_steps)
            diagnostics.error(
                at, "bend-curve-limit", fmt::format("bend curve exceeds {} segments", bend_steps)
            );
        const auto delta    = bend_integer(args, at, diagnostics);
        i64        previous = 0;
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

vec<i64> bend_velocity(std::string_view args, Location at, Diagnostics& diagnostics) {
    i64        velocity     = bend_integer(args, at, diagnostics);
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
    vec<i64> contour;
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
        const auto& note = notes[index];
        if (note.velocity == 0) continue;
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

void AttackGate::release_at(i32 end) const {
    const auto update = [end](auto& note) { note.duration = std::max(0, end - note.time); };
    if (const auto* note = std::get_if<Sounding>(&address))
        update(note->note());
    else
        update(*std::get<Range*>(address));
}

void AttackGate::use_unstressed_velocity() const {
    if (const auto* note = std::get_if<Sounding>(&address); note && unstressed)
        note->note().velocity = *unstressed;
}

AttackGate::Range AttackGate::range() const {
    if (const auto* note = std::get_if<Sounding>(&address))
        return {note->note().time, note->note().duration};
    return *std::get<Range*>(address);
}

std::optional<size_t> AttackGate::note_index() const {
    if (const auto* note = std::get_if<Sounding>(&address)) return note->index;
    return {};
}

std::optional<size_t> AttackGate::sounding_index() const {
    const auto* note = std::get_if<Sounding>(&address);
    return note && note->note().velocity != 0 ? std::optional{note->index} : std::nullopt;
}

VoiceOutput::VoiceOutput(
    std::string name, const ProgramNumbers& programs, Diagnostics& diagnostics
) : name(std::move(name)), diagnostics(diagnostics), programs(programs) {}

void RenderedTrack::append_drum(Note<Tick> note, const DrumMap& changes) {
    if (const auto pitch = changes.destination(note.pitch))
        note.pitch = static_cast<i8>(*pitch);
    else
        inherited_drums.push_back(track.notes.size());
    track.notes.push_back(note);
}

void RenderedTrack::discard_silent_notes() {
    if (inherited_drums.empty()) {
        std::erase_if(track.notes, [](const auto& note) { return note.velocity == 0; });
        return;
    }
    size_t kept = 0, read = 0, written = 0;
    for (size_t i = 0; i < track.notes.size(); ++i) {
        const bool inherited = read < inherited_drums.size() && inherited_drums[read] == i;
        if (inherited) ++read;
        if (track.notes[i].velocity == 0) continue;
        if (inherited) inherited_drums[written++] = kept;
        if (kept != i) track.notes[kept] = track.notes[i];
        ++kept;
    }
    track.notes.resize(kept);
    inherited_drums.resize(written);
}

void RenderedTrack::resolve_drums(const DrumMap& initial) {
    for (const auto index : inherited_drums) {
        auto& note = track.notes[index];
        note.pitch = static_cast<i8>(initial.pitch(note.pitch));
    }
}

void PlaybackPass::publish(vec<TrackNative<Tick>>& output, DrumMap& drum_map) {
    for (auto& rendered : tracks) {
        rendered.resolve_drums(drum_map);
        auto& notes = rendered.track.notes;
        if (!std::ranges::is_sorted(notes, {}, &Note<Tick>::time))
            std::ranges::stable_sort(notes, {}, &Note<Tick>::time);
        output.push_back(std::move(rendered.track));
    }
    drum_map.apply(drum_changes);
}

void VoiceOutput::map_drums(int lane, DrumRoute route) { drum_changes[lane].apply(route); }

bool VoiceOutput::Selection::is_drum() const { return channel == midi_drum_channel; }

MidiPitch VoiceOutput::Selection::transpose(MidiPitch pitch, int semitones) const {
    if (!is_drum()) pitch.note += semitones;
    return pitch;
}

VoiceOutput::Selection VoiceOutput::current_route(int lane) const {
    return Selection(routing[lane].selected);
}

VoiceOutput::Selection VoiceOutput::chord_route(int lane, size_t member) const {
    const auto& channels = routing[lane].chord_channels;
    if (member < channels.size() && channels[member] != 0) return Selection(channels[member]);
    return channels.front() != 0 ? Selection(channels.front()) : current_route(lane);
}

void VoiceOutput::restore_route(int lane, Selection route) {
    routing[lane].selected = route.channel;
}

void VoiceOutput::set_channels(int lane, const MidiTrackChannels& channels) {
    routing[lane].selected = channels.initial;
    routing[lane].unused   = channels.extras;
}

std::optional<int> chord_channel_request(std::string_view name, std::string_view args) {
    if (name != "makechordchannels") return {};
    return std::clamp(integer(word(args)).value_or(0), 0, max_chord_channels);
}

void VoiceOutput::allocate_chord_channels(int lane, int count, Location at) {
    if (count == 0) return;
    auto& route = routing[lane];
    if (static_cast<size_t>(count) > route.unused.size())
        diagnostics.error(at, "midi-channels-exhausted", "all 16 MIDI channels are in use");
    const int program = route.programs[route.selected - 1];
    route.chord_channels.fill(0);
    route.chord_channels.front() = route.selected;
    for (int member = 0; member < count; ++member) {
        const int channel                = route.unused[member];
        route.chord_channels[member + 1] = channel;
        route.programs[channel - 1]      = program;
    }
    route.unused = route.unused.subspan(count);
}

VoiceOutput::Target VoiceOutput::target(int lane) const {
    const auto& route = routing[lane];
    return {lane, route.selected, route.programs[route.selected - 1], voice_bank};
}

VoiceOutput::ChannelOutput& VoiceOutput::channel(Target target) {
    return output
        .try_emplace(
            target,
            target.lane == 0 ? name : "",
            static_cast<u8>(target.program),
            target.channel == midi_drum_channel
        )
        .first->second;
}

Attack VoiceOutput::attack(
    int            lane,
    Selection      route,
    MidiPitch      pitch,
    long double    start,
    long double    duration,
    int            stagger,
    AttackVelocity velocity,
    Location       at
) {
    restore_route(lane, route);
    const auto  time        = tick(start + PlaybackGrid::attack_offset + stagger, diagnostics, at);
    const auto  destination = target(lane);
    const auto* drums = destination.channel == midi_drum_channel ? &drum_changes[lane] : nullptr;
    auto        hit   = emit_attack(
        destination, voice_muted, drums, pitch, time, duration, stagger, velocity, at
    );
    attach_envelope(channel(destination), destination, hit.gate, pitch.wheel);
    return hit;
}

VoiceOutput::RetainedRoute VoiceOutput::retain_route(int lane) const {
    const auto destination = target(lane);
    return {
        destination,
        voice_muted,
        destination.channel == midi_drum_channel ? std::make_unique<DrumMap>(drum_changes[lane]) :
                                                   nullptr
    };
}

Attack VoiceOutput::attack(
    const RetainedRoute& route,
    MidiPitch            pitch,
    long double          start,
    long double          duration,
    i8                   velocity,
    Location             at
) {
    const auto time = tick(start + PlaybackGrid::attack_offset, diagnostics, at);
    return emit_attack(
        route.target, route.muted, route.drums.get(), pitch, time, duration, 0, {velocity}, at
    );
}

void VoiceOutput::revise_attack(
    const AttackGate& attack, long double duration, i8 velocity, Location at
) {
    attack.release_at(note_gate(attack.range().time, duration, 0, at).end());
    if (const auto* sounding = std::get_if<AttackGate::Sounding>(&attack.address))
        sounding->note().velocity = velocity;
}

Attack VoiceOutput::emit_attack(
    Target         destination,
    bool           silent,
    const DrumMap* drums,
    MidiPitch      pitch,
    i32            time,
    long double    duration,
    int            stagger,
    AttackVelocity velocity,
    Location       at
) {
    if (pitch.note < 0 || pitch.note > midi_max) {
        diagnostics.error(
            at, "pitch-range", fmt::format("pitch {} is outside MIDI range 0–127", pitch.note)
        );
    }
    auto& owner = channel(destination);
    diagnostics.spend(PlaybackWork::attack, at);
    const auto gate = store_attack(
        owner, silent, drums, pitch, note_gate(time, duration, stagger, at), velocity
    );
    if (destination.channel != midi_drum_channel)
        owner.tuning.push_back({time, pitch.wheel, event_order++, at, gate.note_index()});
    return {LyricAnchor(owner.rendered.track, time), gate};
}

LyricAnchor VoiceOutput::lyric_anchor(int lane, long double start, Location at) {
    const auto time = tick(start + PlaybackGrid::attack_offset, diagnostics, at);
    return LyricAnchor(channel(target(lane)).rendered.track, time);
}

VoiceOutput::Gate VoiceOutput::note_gate(i32 time, long double duration, int stagger, Location at) {
    const auto length
        = std::max(0, tick(duration, diagnostics, at) - PlaybackGrid::release_trim - stagger);
    if (length > std::numeric_limits<i32>::max() - time)
        diagnostics.error(at, "time-range", "note end exceeds the score's tick range");
    return {time, length};
}

AttackGate VoiceOutput::store_attack(
    ChannelOutput& owner,
    bool           silent,
    const DrumMap* drums,
    MidiPitch      pitch,
    Gate           gate,
    AttackVelocity velocity
) {
    if (silent || (velocity.value == 0 && velocity.without_stress.value_or(0) == 0))
        return AttackGate(muted.emplace_back(gate));
    const Note<Tick> note{gate.time, gate.duration, static_cast<i8>(pitch.note), velocity.value};
    auto&            track = owner.rendered.track;
    if (drums)
        owner.rendered.append_drum(note, *drums);
    else
        track.notes.push_back(note);
    return AttackGate(track, track.notes.size() - 1, velocity.without_stress);
}

void SustainPedal::control(TrackNative<Tick>& owner, i32 time, int value) {
    if (value >= 64) {
        if (!start_track) {
            start_track = &owner;
            start       = time;
        }
    } else if (auto* held = std::exchange(start_track, nullptr)) {
        held->pedals.emplace_back(start, details::sub_time(time, start));
    }
}

void VoiceOutput::control(int lane, i32 time, int number, int value) {
    writes.push_back({target(lane), time, number, value, event_order++, WireKind::controller});
}

void VoiceOutput::set_instrument(const VoiceInstrument& change) {
    if (change.patch) {
        voice_bank = change.patch->bank;
        for (auto& lane : routing) lane.programs.fill(change.patch->program);
    }
    voice_muted = voice_muted || change.mute;
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
    auto& track = owner.rendered.track;
    switch (event.kind) {
    case WireKind::tuning: owner.restore_wheel = event.value; [[fallthrough]];
    case WireKind::wheel: track.pitch_bends.emplace_back(event.time, event.value); break;
    case WireKind::restore_wheel:
        track.pitch_bends.emplace_back(event.time, owner.restore_wheel);
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
    auto&      track   = owner.rendered.track;
    track.controls.emplace_back(time, static_cast<u8>(number), static_cast<u8>(setting));
    if (number == 64) pedals[{dest.lane, dest.channel}].control(track, time, setting);
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
        restore_route(lane, Selection(midi_integer(args, 1, midi_channels, diagnostics, at)));
    else if (const auto count = chord_channel_request(name, args))
        allocate_chord_channels(lane, *count, at);
    else
        return false;
    return true;
}

bool VoiceOutput::directive(std::string_view name, std::string_view args, int lane, Location at) {
    return route(name, args, lane, at) || envelope_directive(name, args, lane, at);
}

std::optional<ChannelEffect> channel_effect(
    std::string_view name, std::string_view args, Location at, Diagnostics& diagnostics
) {
    if (name != "control" && name != "pitchbend" && name != "pitchbendrange" && name != "portamento"
        && name != "noportamento")
        return {};
    using Destination = ChannelEffect::Destination;
    using Kind        = ChannelEffect::Kind;
    using Write       = ChannelEffect::Write;
    auto destination  = Destination::voice;
    if (name != "pitchbendrange") {
        auto       remaining = args;
        const auto part      = word(remaining);
        if (part == "bass") destination = Destination::bass;
        if (part == "chord") destination = Destination::chord;
        if (destination != Destination::voice) args = remaining;
    }
    const auto byte = [&] { return midi_integer(args, 0, midi_max, diagnostics, at); };
    const auto cc   = [](u8 number, int value) { return Write{Kind::control, value, number}; };
    if (name == "control") {
        const auto number = static_cast<u8>(byte());   // Validated MIDI data byte.
        return ChannelEffect{destination, {Write{Kind::control_default, byte(), number}}, 1};
    }
    if (name == "pitchbend") {
        const int low  = byte();
        const int high = trim(args).empty() ? 0 : byte();
        return ChannelEffect{
            destination, {Write{Kind::wheel, (high << 7) + low - midi_bend_center}}, 1
        };
    }
    if (name == "pitchbendrange") {
        const int semitones = byte();
        return ChannelEffect{destination, {cc(101, 0), cc(6, semitones), cc(38, 0)}, 3};
    }
    if (name == "portamento") {
        const int speed = midi_integer(args, 0, 63, diagnostics, at);
        return ChannelEffect{destination, {cc(65, midi_max), cc(5, speed)}, 2};
    }
    return ChannelEffect{destination, {cc(65, 0)}, 1};
}

void VoiceOutput::apply(const ChannelEffect& effect, int lane, i32 time) {
    using Kind = ChannelEffect::Kind;
    for (const auto& [kind, value, controller] : effect.writes()) {
        auto wire = WireKind::controller;
        if (kind == Kind::wheel) wire = WireKind::wheel;
        if (kind == Kind::control_default) wire = WireKind::controller_default;
        writes.push_back({target(lane), time, controller, value, event_order++, wire});
    }
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
    vec<i64> contour;
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
    ChannelOutput& owner, Target destination, AttackGate gate, int wheel
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
    i64         value   = bend.base;
    bool        clipped = false;
    for (const auto delta : curve.deltas) {
        const auto requested = value + delta;
        value                = std::clamp<i64>(requested, wheel_min, wheel_max);
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
    for (auto& [target, owner] : output) {
        complete_tuning(target, owner);
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
            const auto& sounding = owner.rendered.track.notes[*note];
            if (sounding.velocity != 0)
                sounding_until = std::max(sounding_until, sounding.time + sounding.duration);
        }
    }
}

void VoiceOutput::complete_channel_envelopes(ChannelOutput& owner) {
    if (owner.note_envelopes.empty()) return;
    const NoteActivity activity(owner.rendered.track.notes);
    for (const auto& envelope : owner.note_envelopes) complete_envelope(envelope, activity);
}

void VoiceOutput::complete_envelope(const NoteEnvelope& envelope, const NoteActivity& activity) {
    const auto gate = envelope.gate.range();
    if (const auto index = envelope.gate.sounding_index()) {
        const auto [prior, later] = activity.overlaps[*index];
        if (prior || later) warn_envelope_overlap(envelope);
    } else if (activity.intersects(gate)) {
        warn_envelope_overlap(envelope);
    }
    write_envelope(envelope, gate);
}

void VoiceOutput::write_envelope(const NoteEnvelope& envelope, Gate gate) {
    if (envelope.bend) write_bend(envelope, gate);
    if (envelope.controllers) write_shape(envelope, gate);
}

void VoiceOutput::finish(std::span<PlaybackPass> passes) {
    complete_envelopes();
    write_events();
    for (size_t lane = 0; lane < passes.size(); ++lane)
        passes[lane].drum_changes = drum_changes[lane];
    for (auto& [target, owner] : output) {
        auto& track = owner.rendered.track;
        // Keep indices stable until all ties and envelopes have read the final attacks.
        owner.rendered.discard_silent_notes();
        if (track.empty()) continue;
        if (const auto bank = target.bank; bank != 0) {
            const std::array selection{
                ControlChange<Tick>{0, 0, static_cast<u8>(bank >> 7)},
                ControlChange<Tick>{0, 32, static_cast<u8>(bank & midi_max)},
            };
            track.controls.insert(track.controls.begin(), selection.begin(), selection.end());
        }
        passes[target.lane].tracks.push_back(std::move(owner.rendered));
    }
}

}   // namespace symusic::abc::detail

#include "midi_output.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <deque>
#include <limits>
#include <map>
#include <numeric>
#include <span>
#include <utility>

#include "expression.h"
#include "symusic/utils.h"

namespace symusic::abc::detail {

namespace {

constexpr size_t kBendSteps        = 256;
constexpr size_t kControllerSteps  = 255;
constexpr size_t kControllerLayers = 3;
constexpr i32    kWheelMin         = -kMidiBendCenter;
constexpr i32    kWheelMax         = kMidiBendCenter - 1;

i32 bend_integer(std::string_view& args, Location at, Diagnostics& diagnostics) {
    const auto value = integer(word(args));
    if (!value) diagnostics.error(at, "invalid-bend-curve", "expected a signed integer bend value");
    return *value;
}

vec<i64> bend_string(
    std::string_view args, size_t subdivisions, Location at, Diagnostics& diagnostics
) {
    vec<i64> contour;
    while (!trim(args).empty()) {
        if (contour.size() == kBendSteps)
            diagnostics.error(
                at, "bend-curve-limit", fmt::format("bend curve exceeds {} segments", kBendSteps)
            );
        const auto delta    = bend_integer(args, at, diagnostics);
        i64        previous = 0;
        for (size_t step = 1; step <= subdivisions; ++step) {
            const auto cumulative = std::llround(
                static_cast<f64>(delta) * static_cast<f64>(step) / static_cast<f64>(subdivisions)
            );
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
    i32        count        = 8;
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
    for (i32 step = 0; step < count; ++step) {
        contour.push_back(velocity);
        velocity += acceleration;
    }
    return contour;
}

using Message = ChannelEffect::Write;
using Mark    = VoiceOutput::EnvelopeMark;

struct BendCurve {
    vec<i64> deltas;
    bool     leading_pause = false;
};

struct ControllerCurve {
    u8      number;
    vec<u8> values;
};

// A note keeps the curves in effect at its attack. Durations are read after ties resolve.
struct Curves {
    shared<const BendCurve>            bend;
    shared<const vec<ControllerCurve>> controllers;

    [[nodiscard]] size_t work() const {
        size_t count = bend ? bend->deltas.size() + 1 : 0;
        if (controllers) {
            for (const auto& curve : *controllers) count += curve.values.size() + 1;
        }
        return count;
    }
};

struct NoteEnvelope {
    size_t   note;
    Location at;
    Curves   curves;
};

struct LaneEnvelopes {
    Curves                                   curves;
    bool                                     combine_next = false;
    std::optional<std::pair<Mark, Location>> pending;

    bool directive(
        std::string_view name, std::string_view args, Location at, Diagnostics& diagnostics
    );
    void read_controller(std::string_view args, Location at, Diagnostics& diagnostics);
    std::optional<NoteEnvelope> take(size_t note, bool drums, Diagnostics& diagnostics);
};

void LaneEnvelopes::read_controller(std::string_view args, Location at, Diagnostics& diagnostics) {
    const auto byte
        = [&] { return static_cast<u8>(midi_integer(args, 0, kMidiMax, diagnostics, at)); };
    ControllerCurve curve{byte(), {}};
    while (!trim(args).empty()) {
        if (curve.values.size() == kControllerSteps)
            diagnostics.error(at, "controller-curve-limit", "controlstring exceeds 255 values");
        curve.values.push_back(byte());
    }
    if (curve.values.empty())
        diagnostics.error(
            at, "empty-controller-curve", "controlstring needs a controller and at least one value"
        );

    vec<ControllerCurve> layers;
    if (std::exchange(combine_next, false) && curves.controllers) layers = *curves.controllers;
    if (layers.size() == kControllerLayers)
        diagnostics.error(
            at, "controller-layer-limit", "shape supports at most 3 controller curves"
        );
    diagnostics.spend(curve.values.size(), at);
    layers.push_back(std::move(curve));
    curves.controllers = std::make_shared<const vec<ControllerCurve>>(std::move(layers));
}

bool LaneEnvelopes::directive(
    std::string_view name, std::string_view args, Location at, Diagnostics& diagnostics
) {
    if (name == "controlcombo") {
        combine_next = true;
    } else if (name == "controlstring") {
        read_controller(args, at, diagnostics);
    } else {
        vec<i64> deltas;
        if (name == "bendvelocity")
            deltas = bend_velocity(args, at, diagnostics);
        else if (name == "bendstring" || name == "bendstringex")
            deltas = bend_string(args, name == "bendstring" ? 1 : 4, at, diagnostics);
        else
            return false;
        diagnostics.spend(deltas.size(), at);
        curves.bend = std::make_shared<const BendCurve>(BendCurve{std::move(deltas)});
    }
    return true;
}

std::optional<NoteEnvelope> LaneEnvelopes::take(size_t note, bool drums, Diagnostics& diagnostics) {
    if (!pending) return {};
    const auto [mark, at] = *pending;
    pending.reset();
    if (drums) return {};
    auto shape = curves;
    if (mark == Mark::Bend) {
        static const auto fallback = std::make_shared<const BendCurve>(
            BendCurve{{100, 400, 700, 1000, 1300, 1600, 1900, 2200}, true}
        );
        if (!shape.bend) shape.bend = fallback;
        shape.controllers.reset();
    }
    if (const auto work = shape.work()) {
        diagnostics.spend(work, at);
        return NoteEnvelope{note, at, std::move(shape)};
    }
    diagnostics.warn(at, "empty-note-shape", "shape needs a preceding bend or controller curve");
    return {};
}

struct Lane {
    Lane(MidiTrackChannels plan, const DrumMap& opening_drums) :
        selected(plan.initial), extras(std::move(plan.extras)), drums(opening_drums) {}

    i32      selected;
    vec<i32> extras;
    size_t   next_extra  = 0;
    size_t   chord_start = 0;
    i32      chord_root  = 1;
    struct Channel {
        // Chord-channel allocation copies the last program command on its source.
        u8           declared_program = 0;
        u8           program          = 0;
        SustainPedal pedal;
    };
    std::array<Channel, kMidiChannels> channels;
    DrumMap                            drums;
    LaneEnvelopes                      envelopes;

    [[nodiscard]] i32 chord_channel(size_t member) const {
        if (chord_start == next_extra) return selected;
        return member > 0 && member <= next_extra - chord_start ? extras[chord_start + member - 1] :
                                                                  chord_root;
    }
    void allocate(size_t count, Location at, Diagnostics& diagnostics) {
        if (count == 0) return;
        if (count > extras.size() - next_extra)
            diagnostics.error(at, "midi-channels-exhausted", "all 16 MIDI channels are in use");
        chord_root  = selected;
        chord_start = next_extra;
        next_extra += count;
        for (size_t i = chord_start; i < next_extra; ++i) {
            const auto channel            = extras[i];
            channels[channel - 1].program = channels[selected - 1].declared_program;
        }
    }
    void program(
        std::string_view args, const ProgramNumbers& programs, Location at, Diagnostics& diagnostics
    ) {
        auto       remaining = args;
        const auto first     = word(remaining);
        i32        channel   = selected;
        if (!trim(remaining).empty()) {
            const auto explicit_channel = integer(first);
            if (!explicit_channel || *explicit_channel < 1 || *explicit_channel > kMidiChannels)
                diagnostics.error(
                    at, "invalid-midi-channel", "program channel must be between 1 and 16"
                );
            channel = *explicit_channel;
            args    = remaining;
        }
        // ProgramNumbers validates the program as a MIDI data byte.
        auto& selected_channel   = channels[channel - 1];
        selected_channel.program = selected_channel.declared_program
            = static_cast<u8>(programs.read(args, diagnostics, at));
    }
};

struct TrackKey {
    size_t lane;
    i32    channel;
    u8     program;
    i32    bank;
    auto   operator<=>(const TrackKey&) const = default;
};

// Curves and immediate instructions share one timeline. Samples from one
// action retain their insertion order even when they fall on the same tick.
enum class Setting { Direct, Remember, Restore };
struct Timeline {
    struct Event {
        RoutedTrack* track;
        i32          time;
        size_t       order;
        Message      message;
        Setting      setting;
    };
    vec<Event> events;
    size_t     next_order = 0;

    void render();
};

struct Action {
    Timeline&    timeline;
    RoutedTrack& track;
    size_t       order;

    void write(i32 time, Message message, Setting setting = Setting::Direct) const {
        timeline.events.push_back({&track, time, order, message, setting});
    }
};

void sample_bend(
    const BendCurve&  curve,
    const Note<Tick>& note,
    i32               wheel,
    const Action&     action,
    Location          at,
    Diagnostics&      diagnostics
) {
    // The default bend leaves one segment before its first write.
    const auto step    = note.duration / static_cast<i32>(curve.deltas.size());
    i32        segment = curve.leading_pause ? 1 : 0;
    bool       clipped = false;
    for (const i64 delta : curve.deltas) {
        const i64 requested = wheel + delta;
        wheel               = static_cast<i32>(std::clamp<i64>(requested, kWheelMin, kWheelMax));
        clipped |= wheel != requested;
        action.write(note.time + segment++ * step, {std::nullopt, wheel});
    }
    action.write(note.end(), {std::nullopt, 0}, Setting::Restore);
    if (!curve.leading_pause && clipped)
        diagnostics.warn(at, "bend-range", "clipped the bend curve to the MIDI wheel range");
}

void sample_controllers(
    const vec<ControllerCurve>& curves, const Note<Tick>& note, const Action& action
) {
    for (const auto& curve : curves) {
        const auto step = note.duration / static_cast<i32>(curve.values.size());
        auto       time = note.time;
        for (const u8 value : curve.values) {
            action.write(time, {curve.number, value});
            time += step;
        }
        action.write(note.end(), {curve.number, 0}, Setting::Restore);
    }
}

// Each sounding onset carries the furthest release up to that onset. An
// envelope only needs to check that release and the next sounding onset.
struct SoundingNotes {
    using Onset = std::pair<i32, size_t>;
    struct Interval {
        Onset onset;
        i32   open_until;
    };
    vec<Interval> intervals;

    [[nodiscard]] bool overlaps(const Note<Tick>& note, size_t index) const {
        if (note.velocity == 0) {
            // A silent gate intersects any note that starts before its release
            // and is still open at its onset.
            const auto stop
                = std::ranges::lower_bound(intervals, Onset{note.end(), 0}, {}, &Interval::onset);
            return stop != intervals.begin() && std::prev(stop)->open_until > note.time;
        }
        // Sounding attacks at a shared tick follow their attack order.
        auto next
            = std::ranges::lower_bound(intervals, Onset{note.time, index}, {}, &Interval::onset);
        if (next != intervals.begin() && std::prev(next)->open_until > note.time) return true;
        if (!note.empty()) ++next;   // Skip the envelope's own sounding interval.
        return next != intervals.end() && next->onset.first < note.end();
    }
};

}   // namespace

// A Score track is split by program and bank. Its melodic attacks request
// tuning even when muted. Their indices stay valid through finalization.
struct RoutedTrack {
    RoutedTrack(std::string_view name, u8 program, bool drums, SustainPedal& pedal) :
        track(std::string(name), program, drums), pedal(pedal) {}

    TrackNative<Tick> track;
    SustainPedal&     pedal;
    i32               restore_wheel = 0;
    struct Tuning {
        i32      wheel;
        size_t   order;
        Location at;
    };
    vec<Tuning>       tuning;   // One per melodic note, in track.notes order.
    vec<NoteEnvelope> envelopes;

    void schedule(Timeline& timeline, Diagnostics& diagnostics);
};

void RoutedTrack::schedule(Timeline& timeline, Diagnostics& diagnostics) {
    if (envelopes.empty() && std::ranges::none_of(tuning, &Tuning::wheel)) return;
    const auto& notes = track.notes;
    vec<size_t> order(notes.size());
    std::iota(order.begin(), order.end(), size_t{0});
    const auto start = [&](size_t i) { return notes[i].time; };
    if (!std::ranges::is_sorted(order, {}, start)) std::ranges::stable_sort(order, {}, start);

    SoundingNotes sounding;
    i32           wheel      = 0;
    i32           open_until = 0;
    for (const size_t i : order) {
        const auto& note    = notes[i];
        const auto& request = tuning[i];
        if (request.wheel != wheel) {
            if (note.time < open_until)
                diagnostics.warn(
                    request.at,
                    "shared-pitch-wheel",
                    "pitch bend also affects overlapping notes on this voice"
                );
            wheel = request.wheel;
            Action{timeline, *this, request.order}.write(
                note.time, {std::nullopt, wheel}, Setting::Remember
            );
        }
        if (note.empty()) continue;
        open_until = std::max(open_until, note.end());
        if (!envelopes.empty()) sounding.intervals.push_back({{note.time, i}, open_until});
    }
    // A note's tuning and envelope share an order. At its onset the stable
    // timeline sort keeps the tuning write ahead of the first curve sample.
    for (const auto& [i, at, curves] : envelopes) {
        const auto& note = notes[i];
        if (sounding.overlaps(note, i))
            diagnostics.warn(
                at,
                curves.bend ? "shared-pitch-wheel" : "shared-controller",
                "note envelope also affects overlapping notes on this voice"
            );
        const Action action{timeline, *this, tuning[i].order};
        if (curves.bend) sample_bend(*curves.bend, note, tuning[i].wheel, action, at, diagnostics);
        if (curves.controllers) sample_controllers(*curves.controllers, note, action);
    }
}

void Timeline::render() {
    const auto sequence = [](const Event& event) { return std::pair(event.time, event.order); };
    if (!std::ranges::is_sorted(events, {}, sequence))
        std::ranges::stable_sort(events, {}, sequence);
    std::array<i32, kMidiMax + 1> defaults{};
    defaults[7]  = 96;         // Volume.
    defaults[8]  = 64;         // Balance.
    defaults[10] = 64;         // Pan.
    defaults[11] = kMidiMax;   // Expression.
    for (const auto& event : events) {
        auto& owner   = *event.track;
        auto  message = event.message;
        if (event.setting != Setting::Direct) {
            auto& baseline
                = message.controller ? defaults[*message.controller] : owner.restore_wheel;
            if (event.setting == Setting::Remember)
                baseline = message.value;
            else
                message.value = baseline;
        }
        message.emit(owner.track, event.time, owner.pedal);
    }
}

struct VoiceOutput::Storage {
    std::deque<Lane>                lanes;   // Tracks borrow their channel's pedal.
    std::map<TrackKey, RoutedTrack> tracks;
    Timeline                        timeline;
};

VoiceOutput::VoiceOutput(
    std::string name, const ProgramNumbers& programs, Diagnostics& diagnostics
) :
    storage(std::make_unique<Storage>()), name(std::move(name)), programs(programs),
    diagnostics(diagnostics) {}
VoiceOutput::~VoiceOutput() = default;

Note<Tick>& AttackGate::note() const { return owner->track.notes[index]; }

void AttackGate::release_at(i32 end) const {
    auto& attack    = note();
    attack.duration = end > attack.time ? end - attack.time : 0;
}

void AttackGate::use_unstressed_velocity() const {
    if (unstressed) note().velocity = *unstressed;
}

void VoiceOutput::add_lane(MidiTrackChannels channels, const DrumMap& opening_drums) {
    storage->lanes.emplace_back(std::move(channels), opening_drums);
}

void VoiceOutput::allocate_channels(size_t lane, size_t count, Location at) {
    storage->lanes[lane].allocate(count, at, diagnostics);
}

void VoiceOutput::map_drums(size_t lane, DrumRoute route) {
    storage->lanes[lane].drums.apply(route);
}

bool VoiceOutput::Selection::is_drum() const { return channel == kMidiDrumChannel; }

MidiPitch VoiceOutput::sounding_pitch(
    size_t lane, Selection route, MidiPitch pitch, i32 shift
) const {
    if (!route.is_drum())
        pitch.note += shift;
    else if (pitch.note >= 0 && pitch.note <= kMidiMax)
        pitch.note = storage->lanes[lane].drums.pitch(pitch.note);
    return pitch;
}

VoiceOutput::Selection VoiceOutput::current_route(size_t lane) const {
    return Selection(storage->lanes[lane].selected);
}

VoiceOutput::Selection VoiceOutput::chord_route(size_t lane, size_t member) const {
    return Selection(storage->lanes[lane].chord_channel(member));
}

void VoiceOutput::restore_route(size_t lane, Selection route) {
    storage->lanes[lane].selected = route.channel;
}

RoutedTrack& VoiceOutput::selected_track(size_t lane_id) {
    auto&          lane    = storage->lanes[lane_id];
    auto&          channel = lane.channels[lane.selected - 1];
    const TrackKey key{lane_id, lane.selected, channel.program, bank};
    return storage->tracks
        .try_emplace(
            key,
            lane_id == 0 ? std::string_view(name) : "",
            channel.program,
            lane.selected == kMidiDrumChannel,
            channel.pedal
        )
        .first->second;
}

void VoiceOutput::set_instrument(const VoiceInstrument& change) {
    if (change.patch) {
        bank = change.patch->bank;
        for (auto& lane : storage->lanes) {
            for (auto& channel : lane.channels)
                channel.program = channel.declared_program = static_cast<u8>(change.patch->program);
        }
    }
    if (change.mute) muted = true;
}

bool VoiceOutput::directive(
    std::string_view name, std::string_view args, size_t lane_id, Location at
) {
    auto& lane = storage->lanes[lane_id];
    if (name == "program")
        lane.program(args, programs, at, diagnostics);
    else if (name == "channel")
        lane.selected = midi_integer(args, 1, kMidiChannels, diagnostics, at);
    else
        return lane.envelopes.directive(name, args, at, diagnostics);
    return true;
}

void VoiceOutput::envelope_next(size_t lane, EnvelopeMark mark, Location at) {
    storage->lanes[lane].envelopes.pending.emplace(mark, at);
}

Attack VoiceOutput::attack(
    size_t         lane_id,
    Selection      route,
    MidiPitch      pitch,
    f64            start,
    f64            duration,
    i32            stagger,
    AttackVelocity velocity,
    Location       at
) {
    restore_route(lane_id, route);
    auto& lane  = storage->lanes[lane_id];
    auto& track = selected_track(lane_id);
    auto  hit   = emit_attack(
        track, pitch, start, duration, stagger, muted ? AttackVelocity{} : velocity, at
    );
    if (auto envelope = lane.envelopes.take(hit.gate.index, route.is_drum(), diagnostics))
        track.envelopes.push_back(std::move(*envelope));
    return hit;
}

AttackGate VoiceOutput::ornament_pulse(
    const AttackGate& first, MidiPitch pitch, f64 start, f64 duration, i8 velocity, Location at
) {
    return emit_attack(
               *first.owner, pitch, start, duration, 0, {first.can_sound ? velocity : i8{0}}, at
    )
        .gate;
}

i32 VoiceOutput::duration_ticks(i32 time, f64 duration, i32 stagger, Location at) const {
    const auto length
        = std::max(0, tick(duration, diagnostics, at) - PlaybackGrid::kReleaseTrim - stagger);
    if (length > std::numeric_limits<i32>::max() - time)
        diagnostics.error(at, "time-range", "note end exceeds the score's tick range");
    return length;
}

Attack VoiceOutput::emit_attack(
    RoutedTrack&   owner,
    MidiPitch      pitch,
    f64            start,
    f64            duration,
    i32            stagger,
    AttackVelocity velocity,
    Location       at
) {
    const auto time = tick(start + PlaybackGrid::kAttackOffset + stagger, diagnostics, at);
    if (pitch.note < 0 || pitch.note > kMidiMax)
        diagnostics.error(
            at, "pitch-range", fmt::format("pitch {} is outside MIDI range 0–127", pitch.note)
        );
    diagnostics.spend(PlaybackWork::kAttack, at);
    auto&        track = owner.track;
    const size_t index = track.notes.size();
    track.notes.emplace_back(
        time,
        duration_ticks(time, duration, stagger, at),
        static_cast<i8>(pitch.note),
        velocity.value
    );
    if (!track.is_drum) owner.tuning.push_back({pitch.wheel, storage->timeline.next_order++, at});
    const bool can_sound = velocity.value != 0 || velocity.without_stress.value_or(0) != 0;
    return {LyricAnchor(track, time), AttackGate(owner, index, can_sound, velocity.without_stress)};
}

void VoiceOutput::revise_attack(const AttackGate& attack, f64 duration, i8 velocity, Location at) {
    auto& note    = attack.note();
    note.duration = duration_ticks(note.time, duration, 0, at);
    if (attack.can_sound) note.velocity = velocity;
}

LyricAnchor VoiceOutput::lyric_anchor(size_t lane, f64 start, Location at) {
    const auto time = tick(start + PlaybackGrid::kAttackOffset, diagnostics, at);
    return LyricAnchor(selected_track(lane).track, time);
}

void SustainPedal::control(TrackNative<Tick>& owner, i32 time, i32 value) {
    if (value >= 64) {
        if (!start_track) {
            start_track = &owner;
            start       = time;
        }
    } else if (auto* held = std::exchange(start_track, nullptr)) {
        held->pedals.emplace_back(start, details::sub_time(time, start));
    }
}

void ChannelEffect::Write::emit(TrackNative<Tick>& track, i32 time, SustainPedal& pedal) const {
    if (controller) {
        // Control values have been validated as MIDI data bytes.
        track.controls.emplace_back(time, *controller, static_cast<u8>(value));
        if (*controller == 64) pedal.control(track, time, value);
    } else {
        track.pitch_bends.emplace_back(time, value);
    }
}

std::optional<ChannelEffect> channel_effect(
    std::string_view name, std::string_view args, Location at, Diagnostics& diagnostics
) {
    using Destination = ChannelEffect::Destination;
    auto destination  = Destination::Voice;
    if (name != "pitchbendrange") {
        auto       remaining = args;
        const auto part      = word(remaining);
        if (part == "bass") destination = Destination::Bass;
        if (part == "chord") destination = Destination::Chord;
        if (destination != Destination::Voice) args = remaining;
    }
    const auto byte
        = [&] { return static_cast<u8>(midi_integer(args, 0, kMidiMax, diagnostics, at)); };
    const auto effect = [&](auto... writes) {
        return ChannelEffect{destination, {writes...}, sizeof...(writes), name == "control"};
    };
    if (name == "control") {
        const u8 number = byte();
        return effect(Message{number, byte()});
    }
    if (name == "pitchbend") {
        const i32 low  = byte();
        const i32 high = trim(args).empty() ? 0 : byte();
        return effect(Message{std::nullopt, high * 128 + low - kMidiBendCenter});
    }
    if (name == "pitchbendrange")
        return effect(Message{101, 0}, Message{6, byte()}, Message{38, 0});
    if (name == "portamento") {
        const i32 speed = midi_integer(args, 0, 63, diagnostics, at);
        return effect(Message{65, kMidiMax}, Message{5, speed});
    }
    if (name == "noportamento") return effect(Message{65, 0});
    return {};
}

void VoiceOutput::control(size_t lane, i32 time, u8 number, i32 value) {
    auto& timeline = storage->timeline;
    Action{timeline, selected_track(lane), timeline.next_order++}.write(time, {number, value});
}

void VoiceOutput::apply(const ChannelEffect& effect, size_t lane, i32 time) {
    auto&        timeline = storage->timeline;
    const Action action{timeline, selected_track(lane), timeline.next_order++};
    const auto   setting = effect.sets_default ? Setting::Remember : Setting::Direct;
    for (const auto message : effect.writes()) action.write(time, message, setting);
}

vec<vec<TrackNative<Tick>>> VoiceOutput::finish() {
    for (const auto& lane : storage->lanes) {
        if (lane.envelopes.pending) {
            const auto [mark, at] = *lane.envelopes.pending;
            diagnostics.warn(
                at,
                "orphan-decoration",
                mark == EnvelopeMark::Bend ? "bend has no following attack" :
                                             "shape has no following attack"
            );
        }
    }
    for (auto& [key, track] : storage->tracks) track.schedule(storage->timeline, diagnostics);
    storage->timeline.render();
    vec<vec<TrackNative<Tick>>> lanes(storage->lanes.size());
    for (auto& [key, owner] : storage->tracks) {
        auto& track = owner.track;
        // Ties, tuning and envelopes have finished reading attack indices.
        std::erase_if(track.notes, [](const auto& note) { return note.velocity == 0; });
        if (track.empty()) continue;
        if (!std::ranges::is_sorted(track.notes, {}, &Note<Tick>::time))
            std::ranges::stable_sort(track.notes, {}, &Note<Tick>::time);
        if (key.bank != 0) {
            const std::array selection{
                ControlChange<Tick>{0, 0, static_cast<u8>(key.bank / 128)},
                ControlChange<Tick>{0, 32, static_cast<u8>(key.bank % 128)},
            };
            track.controls.insert(track.controls.begin(), selection.begin(), selection.end());
        }
        lanes[key.lane].push_back(std::move(track));
    }
    return lanes;
}

}   // namespace symusic::abc::detail

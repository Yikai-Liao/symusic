#pragma once

#include <deque>
#include <map>
#include <memory>
#include <utility>

#include "channel_effect.h"
#include "drum_output.h"
#include "pitch_tuning.h"
#include "symusic/track.h"

namespace symusic::abc::detail {

class VoiceOutput;
struct AttackVelocity;

struct MidiTrackChannels {
    int      initial = 1;
    vec<int> extras;
};

// A recognized command can request zero channels (no change). Both the
// track planner and playback use the same bounded count.
inline constexpr int max_chord_channels = 9;
std::optional<int>   chord_channel_request(std::string_view name, std::string_view args);

// A borrowed gate for a sounding or muted attack. Ties may extend its release
// at an absolute tick or restore its unstressed velocity. The handle is valid
// until VoiceOutput::finish compacts and moves the track data.
class AttackGate {
public:
    void              release_at(i32 end) const;
    void              use_unstressed_velocity() const;
    [[nodiscard]] i32 end() const { return range().end(); }

private:
    friend class VoiceOutput;
    struct Range {
        i32               time;
        i32               duration;
        [[nodiscard]] i32 end() const { return time + duration; }
    };
    struct Sounding {
        TrackNative<Tick>*        track;
        size_t                    index;
        [[nodiscard]] Note<Tick>& note() const { return track->notes[index]; }
    };
    AttackGate(TrackNative<Tick>& track, size_t index, std::optional<i8> unstressed) :
        address(Sounding{&track, index}), unstressed(unstressed) {}
    explicit AttackGate(Range& muted) : address(&muted) {}
    [[nodiscard]] Range                 range() const;
    [[nodiscard]] std::optional<size_t> note_index() const;
    [[nodiscard]] std::optional<size_t> sounding_index() const;
    std::variant<Sounding, Range*>      address;
    std::optional<i8>                   unstressed;
};

// An onset and its destination track, borrowed until VoiceOutput::finish.
// Multiple syllables can use one anchor without creating new note attacks.
class LyricAnchor {
public:
    void append(std::string text) const { track->lyrics.emplace_back(time, std::move(text)); }

private:
    friend class VoiceOutput;
    LyricAnchor(TrackNative<Tick>& track, i32 time) : track(&track), time(time) {}
    TrackNative<Tick>* track;
    i32                time;
};

struct Attack {
    LyricAnchor lyric;
    AttackGate  gate;
};

// Collects one notation voice's performed MIDI events. Routes attacks and
// controls by lane/channel/program, but pairs sustain by lane/channel even
// across program changes. Only this object owns track storage and quantizes
// note endpoints. Invalid note ranges fail through the reader's diagnostics.
class VoiceOutput {
    struct Target {
        int  lane;
        int  channel;
        int  program;
        int  bank;
        auto operator<=>(const Target&) const = default;
    };

public:
    VoiceOutput(std::string name, const ProgramNumbers& programs, Diagnostics& diagnostics);
    VoiceOutput(const VoiceOutput&)            = delete;
    VoiceOutput& operator=(const VoiceOutput&) = delete;

    // A repeat restores the active channel, not the programs or allocations.
    class Selection {
    public:
        [[nodiscard]] bool      is_drum() const;
        [[nodiscard]] MidiPitch transpose(MidiPitch pitch, int semitones) const;

    private:
        friend class VoiceOutput;
        explicit Selection(int channel) : channel(channel) {}
        int channel;
    };
    [[nodiscard]] Selection current_route(int lane) const;
    [[nodiscard]] Selection chord_route(int lane, size_t member) const;
    void                    restore_route(int lane, Selection route);
    // Supply the lane's assigned channels before playback. The allocation
    // must outlive this output, and a repeat does not replenish it.
    void set_channels(int lane, const MidiTrackChannels& channels);

    // start and duration are fractional ticks, before the global note-on
    // offset and release trim. stagger delays the attack without moving the
    // intended end. The caller supplies a route and its resolved MIDI pitch.
    // Even a muted attack has a gate and lyric anchor for later playback work.
    Attack attack(
        int            lane,
        Selection      route,
        MidiPitch      pitch,
        long double    start,
        long double    duration,
        int            stagger,
        AttackVelocity velocity,
        Location       at
    );
    // Save the destination of an ornament whose later attacks need the final
    // tied duration. Subsequent routing, muting and drum commands can run first.
    class RetainedRoute {
        friend class VoiceOutput;
        RetainedRoute(Target target, bool muted, std::unique_ptr<DrumMap> drums) :
            target(target), muted(muted), drums(std::move(drums)) {}
        Target                   target;
        bool                     muted;
        std::unique_ptr<DrumMap> drums;
    };
    [[nodiscard]] RetainedRoute retain_route(int lane) const;
    Attack                      attack(
        const RetainedRoute& route,
        MidiPitch            pitch,
        long double          start,
        long double          duration,
        i8                   velocity,
        Location             at
    );
    // Finalize a provisional first attack before envelope gates are materialized.
    void revise_attack(const AttackGate& attack, long double duration, i8 velocity, Location at);
    // Route lyrics for a written note that did not produce a new attack.
    LyricAnchor lyric_anchor(int lane, long double start, Location at);
    void        set_instrument(const VoiceInstrument& change);
    void        map_drums(int lane, DrumRoute route);
    void        control(int lane, i32 time, int number, int value);
    // Mark the next attack, including a muted attack. Percussion consumes
    // the mark without emitting an envelope. Curves use the final tied gate.
    enum class EnvelopeMark { bend, shape };
    void envelope_next(int lane, EnvelopeMark mark, Location at);
    bool directive(std::string_view name, std::string_view args, int lane, Location at);
    void apply(const ChannelEffect& effect, int lane, i32 time);
    // Materialize timed channel events, discard silent attacks and move tracks
    // into the passes indexed by lane. Do not use borrowed handles afterward.
    void finish(std::span<PlaybackPass> passes);

private:
    using ChannelKey = std::pair<int, int>;

    struct Routing {
        int                                     selected = 1;
        std::span<const int>                    unused;
        std::array<int, max_chord_channels + 1> chord_channels{};
        std::array<int, midi_channels>          programs{};
    };
    std::array<Routing, max_voice_lanes> routing;
    int                                  voice_bank  = 0;
    bool                                 voice_muted = false;

    struct BendCurve {
        vec<i64> deltas;
        bool     leading_pause = false;
    };
    using BendProfile = shared<const BendCurve>;
    struct ControllerCurve {
        int     number;
        vec<u8> values;
    };
    using ControllerLayers = shared<const vec<shared<const ControllerCurve>>>;
    struct PendingEnvelope {
        EnvelopeMark mark;
        Location     at;
    };
    struct LaneEnvelope {
        BendProfile                    bend;
        ControllerLayers               controllers;
        bool                           combine_next = false;
        std::optional<PendingEnvelope> pending;
    };
    using Gate = AttackGate::Range;
    struct NoteEnvelope {
        AttackGate           gate;
        Target               destination;
        BendProfile          bend;
        ControllerLayers     controllers;
        int                  base;
        size_t               order;
        Location             at;
        [[nodiscard]] size_t work() const;
    };
    std::array<LaneEnvelope, max_voice_lanes> envelopes;
    std::deque<Gate>                          muted;

    struct TuningAttack {
        i32                   time;
        int                   wheel;
        size_t                order;
        Location              at;
        std::optional<size_t> note;
    };

    struct ChannelOutput {
        ChannelOutput(std::string name, u8 program, bool drums) :
            rendered{TrackNative<Tick>(std::move(name), program, drums), {}} {}

        RenderedTrack     rendered;
        vec<NoteEnvelope> note_envelopes;
        vec<TuningAttack> tuning;
        int               restore_wheel = 0;
    };

    // Deferred envelope samples share the direct events' timeline. At the
    // same tick, finish an earlier action before starting the next one.
    // Score stores event kinds separately; only within-kind order survives.
    enum class WireKind {
        controller,
        controller_default,
        restore_controller,
        wheel,
        tuning,
        restore_wheel
    };
    struct WireWrite {
        Target   destination;
        i32      time;
        int      number;
        int      value;
        size_t   order;
        WireKind kind;
    };
    using ControllerDefaults   = std::array<int, midi_max + 1>;
    size_t         event_order = 0;
    vec<WireWrite> writes;

    std::string                          name;
    Diagnostics&                         diagnostics;
    std::array<DrumMap, max_voice_lanes> drum_changes;
    const ProgramNumbers&                programs;
    std::map<Target, ChannelOutput>      output;
    std::map<ChannelKey, SustainPedal>   pedals;

    [[nodiscard]] Target target(int lane) const;
    ChannelOutput&       channel(Target target);
    bool                 route(std::string_view name, std::string_view args, int lane, Location at);
    void                 program(int lane, std::string_view args, Location at);
    void                 allocate_chord_channels(int lane, int count, Location at);
    void                 complete_tuning(Target target, ChannelOutput& owner);
    bool envelope_directive(std::string_view name, std::string_view args, int lane, Location at);
    void controller_curve(LaneEnvelope& state, std::string_view args, Location at);
    void attach_envelope(ChannelOutput& owner, Target destination, AttackGate gate, int wheel);
    void write_bend(const NoteEnvelope& envelope, Gate gate);
    void write_shape(const NoteEnvelope& envelope, Gate gate);
    void complete_envelopes();
    void complete_channel_envelopes(ChannelOutput& owner);
    struct NoteActivity;
    void   complete_envelope(const NoteEnvelope& envelope, const NoteActivity& activity);
    Gate   note_gate(i32 time, long double duration, int stagger, Location at);
    Attack emit_attack(
        Target         destination,
        bool           muted,
        const DrumMap* drums,
        MidiPitch      pitch,
        i32            time,
        long double    duration,
        int            stagger,
        AttackVelocity velocity,
        Location       at
    );
    AttackGate store_attack(
        ChannelOutput& owner,
        bool           silent,
        const DrumMap* drums,
        MidiPitch      pitch,
        Gate           gate,
        AttackVelocity velocity
    );
    void               warn_envelope_overlap(const NoteEnvelope& envelope);
    void               warn_tuning_overlap(Location at);
    void               write_envelope(const NoteEnvelope& envelope, Gate gate);
    void               write_events();
    static BendProfile bend_for(const LaneEnvelope& state, EnvelopeMark mark);
    void               write_event(const WireWrite& event, ControllerDefaults& defaults);
    void               write_controller(
        ChannelOutput& owner, const WireWrite& event, ControllerDefaults& defaults
    );
};

}   // namespace symusic::abc::detail

#pragma once

#include <map>

#include "pitch_tuning.h"
#include "symusic/track.h"

namespace symusic::abc::detail {

class VoiceOutput;

// A stable note handle until its voice finishes; note vectors may grow in between.
class NoteAddress {
public:
    void release_at(i32 end) const;

private:
    friend class VoiceOutput;
    NoteAddress(TrackNative<Tick>& track, size_t index) : track(&track), index(index) {}
    [[nodiscard]] Note<Tick>& note() const { return track->notes[index]; }

    TrackNative<Tick>* track;
    size_t             index;
};

// Unlike a note address, this anchor also exists at a muted attack.
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
    LyricAnchor                lyric;
    std::optional<NoteAddress> note;
};

// Collects one notation voice's performed MIDI events. Routes attacks and
// controls by lane/channel/program, but pairs sustain by lane/channel even
// across program changes. Only this object owns track storage and quantizes
// note endpoints. Invalid note ranges fail through the reader's diagnostics.
class VoiceOutput {
public:
    VoiceOutput(
        std::string           name,
        const DrumMap&        drum_map,
        const ProgramNumbers& programs,
        Diagnostics&          diagnostics
    );
    VoiceOutput(const VoiceOutput&)            = delete;
    VoiceOutput& operator=(const VoiceOutput&) = delete;

    Attack attack(
        int                   lane,
        MidiPitch             pitch,
        long double           start,
        long double           duration,
        int                   stagger,
        int                   velocity,
        Location              at,
        std::optional<size_t> chord_member = 0
    );
    int  selected_channel(int lane) const { return routing[lane].selected; }
    void select_channel(int lane, int channel) { routing[lane].selected = channel; }
    void control(int lane, i32 time, int number, int value);
    // Mark the next attack, including a muted attack. Percussion consumes
    // the mark without emitting an envelope. Curves use the final tied gate.
    enum class EnvelopeMark { bend, shape };
    void envelope_next(int lane, EnvelopeMark mark, Location at);
    // Map channel-effect directives onto Score events; leave unrelated MIDI
    // commands to the playback and accompaniment state owners.
    bool effect(std::string_view name, std::string_view args, int lane, i32 time, Location at);
    void finish(vec<TrackNative<Tick>>& tracks);

private:
    using TrackKey   = std::tuple<int, int, int>;
    using ChannelKey = std::pair<int, int>;

    struct Target {
        int lane;
        int channel;
        int program;
    };

    static constexpr int max_chord_channels = 9;
    struct Routing {
        int    selected       = 1;
        int    chord_base     = 1;
        size_t extra_channels = 0;
        // The extra slots have independent wheels and programs, but their
        // real MIDI channel numbers are not part of the Score contract.
        std::array<int, midi_channels + max_chord_channels> programs{};

        void attack_on(size_t chord_member);
        void allocate_chord_channels(size_t count);
    };
    std::array<Routing, max_voice_lanes> routing;

    struct BendCurve {
        vec<int64_t> deltas;
        bool         leading_pause = false;
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
    struct Gate {
        i32               time;
        i32               duration;
        [[nodiscard]] i32 end() const { return time + duration; }
    };
    using EnvelopeGate = std::variant<size_t, Gate>;
    struct NoteEnvelope {
        EnvelopeGate         gate;
        Target               destination;
        BendProfile          bend;
        ControllerLayers     controllers;
        int                  base;
        size_t               order;
        Location             at;
        [[nodiscard]] size_t work() const;
    };
    std::array<LaneEnvelope, max_voice_lanes> envelopes;

    struct TuningAttack {
        i32                   time;
        int                   wheel;
        size_t                order;
        Location              at;
        std::optional<size_t> note;
    };

    struct ChannelOutput {
        ChannelOutput(std::string name, u8 program, bool drums) :
            track(std::move(name), program, drums) {}

        TrackNative<Tick> track;
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

    std::string                                              name;
    Diagnostics&                                             diagnostics;
    const DrumMap&                                           drum_map;
    const ProgramNumbers&                                    programs;
    std::map<TrackKey, ChannelOutput>                        output;
    std::map<ChannelKey, std::pair<TrackNative<Tick>*, i32>> pedal_starts;

    [[nodiscard]] Target target(int lane) const;
    ChannelOutput&       channel(Target target);
    bool                 route(std::string_view name, std::string_view args, int lane, Location at);
    void                 program(int lane, std::string_view args, Location at);
    void                 complete_tuning(Target target, ChannelOutput& owner);
    bool                 channel_effect(
        std::string_view name, std::string_view args, int lane, i32 time, Location at
    );
    bool envelope_directive(std::string_view name, std::string_view args, int lane, Location at);
    void controller_curve(LaneEnvelope& state, std::string_view args, Location at);
    void attach_envelope(ChannelOutput& owner, Target destination, EnvelopeGate gate, int wheel);
    void write_bend(const NoteEnvelope& envelope, Gate gate);
    void write_shape(const NoteEnvelope& envelope, Gate gate);
    void complete_envelopes();
    void complete_channel_envelopes(ChannelOutput& owner);
    struct NoteActivity;
    void complete_envelope(
        const NoteEnvelope& envelope, const vec<Note<Tick>>& notes, const NoteActivity& activity
    );
    Gate        note_gate(i32 time, long double duration, int stagger, Location at);
    NoteAddress emit_note(
        ChannelOutput& owner, Target destination, MidiPitch pitch, Gate gate, int velocity
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
    void sustain(ChannelKey channel, TrackNative<Tick>& track, i32 time, int value);
};

}   // namespace symusic::abc::detail

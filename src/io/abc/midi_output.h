#pragma once

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include "channel_effect.h"
#include "pitch_tuning.h"
#include "symusic/track.h"

namespace symusic::abc::detail {

class VoiceOutput;
struct RoutedTrack;
struct AttackVelocity;

struct MidiTrackChannels {
    i32      initial = 1;
    vec<i32> extras;
};

// A borrowed gate for a sounding or muted attack. Ties may extend its release
// at an absolute tick or restore its unstressed velocity. The handle is valid
// until VoiceOutput::finish compacts and moves the track data.
class AttackGate {
public:
    void              release_at(i32 end) const;
    void              use_unstressed_velocity() const;
    [[nodiscard]] i32 end() const { return note().end(); }

private:
    friend class VoiceOutput;
    AttackGate(RoutedTrack& owner, size_t index, bool can_sound, std::optional<i8> unstressed) :
        owner(&owner), index(index), can_sound(can_sound), unstressed(unstressed) {}
    [[nodiscard]] Note<Tick>& note() const;

    RoutedTrack*      owner;
    size_t            index;
    bool              can_sound;
    std::optional<i8> unstressed;
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
    struct Storage;

public:
    VoiceOutput(std::string name, const ProgramNumbers& programs, Diagnostics& diagnostics);
    ~VoiceOutput();
    VoiceOutput(const VoiceOutput&)            = delete;
    VoiceOutput& operator=(const VoiceOutput&) = delete;
    VoiceOutput(VoiceOutput&&)                 = delete;
    VoiceOutput& operator=(VoiceOutput&&)      = delete;

    // A repeat restores the active channel, not the programs or allocations.
    class Selection {
    public:
        [[nodiscard]] bool is_drum() const;


    private:
        friend class VoiceOutput;
        explicit Selection(i32 channel) : channel(channel) {}
        i32 channel;
    };
    [[nodiscard]] Selection current_route(size_t lane) const;
    [[nodiscard]] Selection chord_route(size_t lane, size_t member) const;
    void                    restore_route(size_t lane, Selection route);
    // Resolve at the attack's opening so every pulse of a tied ornament keeps its pitches.
    [[nodiscard]] MidiPitch sounding_pitch(
        size_t lane, Selection route, MidiPitch pitch, i32 shift
    ) const;
    // Add lanes in pass order before playback. Each lane owns its channel
    // allocation and copies its opening drum map.
    void allocate_channels(size_t lane, size_t count, Location at);
    void add_lane(MidiTrackChannels channels, const DrumMap& opening_drums);

    // start and duration are fractional ticks, before the global note-on
    // offset and release trim. stagger delays the attack without moving the
    // intended end. The caller supplies a route and its resolved MIDI pitch.
    // Even a muted attack has a gate and lyric anchor for later playback work.
    Attack attack(
        size_t         lane,
        Selection      route,
        MidiPitch      pitch,
        f64            start,
        f64            duration,
        i32            stagger,
        AttackVelocity velocity,
        Location       at
    );
    // A later ornament pulse inherits the first pulse's track and mute state.
    AttackGate ornament_pulse(
        const AttackGate& first, MidiPitch pitch, f64 start, f64 duration, i8 velocity, Location at
    );
    // Finalize a provisional first attack before envelope gates are materialized.
    void revise_attack(const AttackGate& attack, f64 duration, i8 velocity, Location at);
    // Route lyrics for a written note that did not produce a new attack.
    LyricAnchor lyric_anchor(size_t lane, f64 start, Location at);
    void        set_instrument(const VoiceInstrument& change);
    void        map_drums(size_t lane, DrumRoute route);
    void        control(size_t lane, i32 time, u8 number, i32 value);
    // Mark the next attack, including a muted attack. Percussion consumes
    // the mark without emitting an envelope. Curves use the final tied gate.
    enum class EnvelopeMark { Bend, Shape };
    void envelope_next(size_t lane, EnvelopeMark mark, Location at);
    bool directive(std::string_view name, std::string_view args, size_t lane, Location at);
    void apply(const ChannelEffect& effect, size_t lane, i32 time);
    // Return one group of Score tracks per lane, with notes in stable onset
    // order. Finishing consumes the tracks and expires all borrowed handles.
    [[nodiscard]] vec<vec<TrackNative<Tick>>> finish();

private:
    std::unique_ptr<Storage> storage;
    std::string              name;
    const ProgramNumbers&    programs;
    Diagnostics&             diagnostics;
    i32                      bank  = 0;
    bool                     muted = false;

    RoutedTrack&      selected_track(size_t lane);
    [[nodiscard]] i32 duration_ticks(i32 time, f64 duration, i32 stagger, Location at) const;
    Attack            emit_attack(
        RoutedTrack&   track,
        MidiPitch      pitch,
        f64            start,
        f64            duration,
        i32            stagger,
        AttackVelocity velocity,
        Location       at
    );
};

}   // namespace symusic::abc::detail

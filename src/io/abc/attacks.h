#pragma once

#include <list>
#include <string>
#include <string_view>

#include "attack_releases.h"
#include "lanes.h"

namespace symusic::abc::detail {

// The stress map supplies the performed duration. Written lengths are in ticks
// on its input grid, after any preceding grace or cut has borrowed its time.
struct AttackTiming {
    StressNote full{};
    f64        written_start    = 0;
    f64        written_length   = 0;
    f64        written_staccato = 0;
};

inline f64 staccato_length(f64 duration) { return duration / 2; }

// Emit attacks on the active lane. Call finish before publishing the voice:
// continued ornaments and spread chords use the final gates of earlier notes.
class NoteEmitter {
public:
    NoteEmitter(
        const TuneSettings&  settings,
        OverlayLanes&        lanes,
        VoiceOutput&         output,
        ChordAttackSequence& chord_attacks,
        Diagnostics&         diagnostics,
        const VoicePlan&     plan
    );
    ~NoteEmitter();
    NoteEmitter(const NoteEmitter&)            = delete;
    NoteEmitter& operator=(const NoteEmitter&) = delete;
    NoteEmitter(NoteEmitter&&)                 = delete;
    NoteEmitter& operator=(NoteEmitter&&)      = delete;

    void play_grace_attack(const Atom& atom, Location at, AttackTiming timing, f64 unit);
    void play_notes(
        const Prepared& atom, AttackTiming timing, f64 borrowed, const std::string* syllable
    );
    void play_harp_lead(const Prepared& atom, StressNote first, StressNote second);
    void play_cut(const Prepared& atom, StressNote timing, i32 neighbor);
    void apply_decoration(
        std::string_view spelling, Location at, const std::optional<TieNote>& continuation = {}
    );
    void finish();

private:
    struct Chord;
    const TuneSettings&     settings;
    OverlayLanes&           lanes;
    VoiceOutput&            output;
    ChordAttackSequence&    chord_attacks;
    Diagnostics&            diagnostics;
    ExpandedReleaseSchedule releases;
    // A tie can borrow its ornament across new attacks in other lanes.
    std::list<TiedOrnament> ornaments;

    [[nodiscard]] PlaybackLane& lane() const { return lanes.active(); }
    [[nodiscard]] i32           shift() const {
        return lane().midi.shift + lanes.notation().fields.tone.transpose.value_or(0);
    }
    [[nodiscard]] auto           pitches(Location at) const;
    [[nodiscard]] AttackVelocity velocity(f64 start, bool accented, i32 stressed) const;
    void lead(MidiPitch pitch, f64 start, StressNote timing, f64 gate, const Prepared& atom);
};

}   // namespace symusic::abc::detail

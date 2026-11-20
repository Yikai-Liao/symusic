#pragma once

#ifndef LIBSYMUSIC_TRACK_HSCORE_H
#define LIBSYMUSIC_TRACK_HSCORE_H

#include <optional>

#include "symusic/event.h"
#include "symusic/track.h"

namespace symusic {

template<TType T>
struct Score {
    typedef T                ttype;
    typedef typename T::unit unit;

    i32                             ticks_per_quarter;
    shared<vec<shared<Track<T>>>>   tracks;   // track is not small object, use shared_ptr
    shared<pyvec<TimeSignature<T>>> time_signatures;
    shared<pyvec<KeySignature<T>>>  key_signatures;
    shared<pyvec<Tempo<T>>>         tempos;
    shared<pyvec<TextMeta<T>>>      markers;
    shared<pyvec<NoteTuning<T>>>    note_tunings = std::make_shared<pyvec<NoteTuning<T>>>();
    vec<std::string>                copyright_notices;
    // An explicit ending can extend playback through silence after all events.
    std::optional<unit> explicit_end;

    Score() : ticks_per_quarter{0} {
        tracks          = std::make_shared<vec<shared<Track<T>>>>();
        time_signatures = std::make_shared<pyvec<TimeSignature<T>>>();
        key_signatures  = std::make_shared<pyvec<KeySignature<T>>>();
        tempos          = std::make_shared<pyvec<Tempo<T>>>();
        markers         = std::make_shared<pyvec<TextMeta<T>>>();
    }

    Score(const Score&) = default;

    Score(Score&& other) noexcept :
        note_tunings{std::move(other.note_tunings)},
        copyright_notices{std::move(other.copyright_notices)}, explicit_end{other.explicit_end} {
        // move constructor
        ticks_per_quarter = other.ticks_per_quarter;
        tracks            = std::move(other.tracks);
        time_signatures   = std::move(other.time_signatures);
        key_signatures    = std::move(other.key_signatures);
        tempos            = std::move(other.tempos);
        markers           = std::move(other.markers);
    }

    Score(
        const i32                     tpq,
        shared<vec<shared<Track<T>>>> tracks,
        pyvec<TimeSignature<T>>&&     time_signatures,
        pyvec<KeySignature<T>>&&      key_signatures,
        pyvec<Tempo<T>>&&             tempos,
        pyvec<TextMeta<T>>&&          markers
    ) : ticks_per_quarter{tpq}, tracks{std::move(tracks)} {
        this->time_signatures
            = std::make_shared<pyvec<TimeSignature<T>>>(std::move(time_signatures));
        this->key_signatures = std::make_shared<pyvec<KeySignature<T>>>(std::move(key_signatures));
        this->tempos         = std::make_shared<pyvec<Tempo<T>>>(std::move(tempos));
        this->markers        = std::make_shared<pyvec<TextMeta<T>>>(std::move(markers));
    }


    Score(
        const i32                       tpq,
        shared<vec<shared<Track<T>>>>   tracks,
        shared<pyvec<TimeSignature<T>>> time_signatures,
        shared<pyvec<KeySignature<T>>>  key_signatures,
        shared<pyvec<Tempo<T>>>         tempos,
        shared<pyvec<TextMeta<T>>>      markers
    ) :
        ticks_per_quarter{tpq}, tracks{std::move(tracks)},
        time_signatures{std::move(time_signatures)}, key_signatures{std::move(key_signatures)},
        tempos{std::move(tempos)}, markers{std::move(markers)} {}

    Score& operator=(const Score&) = default;

    bool operator==(const Score& other) const {

        auto tracks_equal
            = [](const shared<vec<shared<Track<T>>>>& a, const shared<vec<shared<Track<T>>>>& b) {
                  if (a == b) return true;
                  if (a->size() != b->size()) return false;
                  for (size_t i = 0; i < a->size(); i++) {
                      const auto& track1 = a->operator[](i);
                      const auto& track2 = b->operator[](i);
                      if (track1 != track2 && *track1 != *track2) return false;
                  }
                  return true;
              };
        // clang-format off
        return ticks_per_quarter == other.ticks_per_quarter
               && tracks_equal(tracks, other.tracks)
               && *time_signatures == *other.time_signatures
               && *key_signatures == *other.key_signatures
               && *tempos == *other.tempos
               && *markers == *other.markers
               && *note_tunings == *other.note_tunings
               && copyright_notices == other.copyright_notices
               && explicit_end == other.explicit_end;
        // clang-format on
    }

    bool operator!=(const Score& other) const { return !(*this == other); }

    [[nodiscard]] Score copy() const { return {*this}; }

    [[nodiscard]] Score deepcopy() const {
        auto new_tracks = std::make_shared<vec<shared<Track<T>>>>();
        new_tracks->reserve(tracks->size());
        for (const auto& track : *tracks) {
            new_tracks->push_back(std::make_shared<Track<T>>(std::move(track->deepcopy())));
        }
        Score copy{
            ticks_per_quarter,
            std::move(new_tracks),
            std::move(time_signatures->deepcopy()),
            std::move(key_signatures->deepcopy()),
            std::move(tempos->deepcopy()),
            std::move(markers->deepcopy())
        };
        copy.note_tunings      = std::make_shared<pyvec<NoteTuning<T>>>(note_tunings->deepcopy());
        copy.copyright_notices = copyright_notices;
        copy.explicit_end      = explicit_end;
        return copy;
    }

    explicit Score(const i32 tpq) : Score() { ticks_per_quarter = tpq; }

    template<DataFormat F>
    [[nodiscard]] static Score parse(std::span<const u8> bytes);

    template<DataFormat F>
    [[nodiscard]] vec<u8> dumps() const;

    // return the start time of the score
    [[nodiscard]] unit start() const;

    // return the end time of the score
    [[nodiscard]] unit end() const;

    // return the number of notes in the score
    [[nodiscard]] size_t note_num() const;

    // return the number of tracks in the score
    [[nodiscard]] size_t track_num() const;

    // return true if the score is empty
    [[nodiscard]] bool empty() const;

    // return beat locations in the current time unit
    [[nodiscard]] vec<unit> get_beats(unit start_time = 0) const;

    // return downbeat locations in the current time unit
    [[nodiscard]] vec<unit> get_downbeats(unit start_time = 0) const;

    // return a string representation of the score, same as summary
    [[nodiscard]] std::string to_string() const;

    // summary info of the score
    [[nodiscard]] std::string summary() const;

    // non-inplace sort, return a new score
    [[nodiscard]] Score sort(bool reverse = false) const;

    // inplace sort, and return self reference
    void sort_inplace(bool reverse = false);

    // Clip all the events in the score, non-inplace, return a new Score
    // For events with duration, clip_end is used to determine whether to clip based on end time.
    [[nodiscard]] Score clip(unit start, unit end, bool clip_end = false) const;
    void                clip_inplace(unit start, unit end, bool clip_end = false);

    [[nodiscard]] Score trim(
        unit               start,
        unit               end,
        unit               min_overlap = 0,
        const std::string& start_mode  = "remove",
        const std::string& end_mode    = "remove"
    ) const;
    void trim_inplace(
        unit               start,
        unit               end,
        unit               min_overlap = 0,
        const std::string& start_mode  = "remove",
        const std::string& end_mode    = "remove"
    );

    // shift the time of all the events in the score, non-inplace, return a new Score
    [[nodiscard]] Score shift_time(unit offset) const;

    // shift the time of all the events in the score, inplace, return self reference
    void shift_time_inplace(unit offset);

    // shift the pitch of all notes in the score, non-inplace, return a new Score
    [[nodiscard]] Score shift_pitch(i8 offset) const;

    // shift the pitch of all notes in the score, inplace, return self reference
    void shift_pitch_inplace(i8 offset);

    // shift the velocity of all notes in the score, non-inplace, return a new Score
    [[nodiscard]] Score shift_velocity(i8 offset) const;

    // shift the velocity of all notes in the score, inplace, return self reference
    void shift_velocity_inplace(i8 offset);
};

/*
 *  ScoreNative is an original version of Score, which doesn't use shared_ptr
 */

template<typename T>
struct ScoreNative {
    i32                             ticks_per_quarter;
    vec<TimeSignature<T>>           time_signatures;
    vec<KeySignature<T>>            key_signatures;
    vec<Tempo<T>>                   tempos;
    vec<TextMeta<T>>                markers;
    vec<NoteTuning<T>>              note_tunings;
    vec<std::string>                copyright_notices;
    std::optional<typename T::unit> explicit_end;
    vec<TrackNative<T>>             tracks;

    ScoreNative() : ticks_per_quarter(0) {}

    explicit ScoreNative(const i32 ticks_per_quarter) : ticks_per_quarter(ticks_per_quarter) {}

    [[nodiscard]] bool empty() const {
        return time_signatures.empty() && key_signatures.empty() && tempos.empty()
               && markers.empty() && note_tunings.empty() && tracks.empty() && !explicit_end;
    }

    template<DataFormat F>
    [[nodiscard]] static ScoreNative parse(std::span<const u8> bytes);

    template<DataFormat F>
    [[nodiscard]] vec<u8> dumps() const;
};
}   // namespace symusic

#endif   // LIBSYMUSIC_TRACK_HSCORE_H

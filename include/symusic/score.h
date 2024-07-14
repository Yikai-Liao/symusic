//
// Created by lyk on 2023/12/24.
//
#pragma once

#ifndef LIBSYMUSIC_TRACK_HSCORE_H
#define LIBSYMUSIC_TRACK_HSCORE_H

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

    Score() : ticks_per_quarter{0} {
        tracks          = std::make_shared<vec<shared<Track<T>>>>();
        time_signatures = std::make_shared<pyvec<TimeSignature<T>>>();
        key_signatures  = std::make_shared<pyvec<KeySignature<T>>>();
        tempos          = std::make_shared<pyvec<Tempo<T>>>();
        markers         = std::make_shared<pyvec<TextMeta<T>>>();
    }

    Score(const Score&) = default;

    Score(Score&& other) noexcept {
        // move constructor
        ticks_per_quarter = other.ticks_per_quarter;
        tracks            = std::move(other.tracks);
        time_signatures   = std::move(other.time_signatures);
        key_signatures    = std::move(other.key_signatures);
        tempos            = std::move(other.tempos);
        // lyrics            = std::move(other.lyrics);
        markers = std::move(other.markers);
    }

    Score(
        const i32                     tpq,
        shared<vec<shared<Track<T>>>> tracks,
        pyvec<TimeSignature<T>>&&     time_signatures,
        pyvec<KeySignature<T>>&&      key_signatures,
        pyvec<Tempo<T>>&&             tempos,
        // pyvec<TextMeta<T>>&&          lyrics,
        pyvec<TextMeta<T>>&& markers
    ) : ticks_per_quarter{tpq}, tracks{std::move(tracks)} {
        this->time_signatures
            = std::make_shared<pyvec<TimeSignature<T>>>(std::move(time_signatures));
        this->key_signatures = std::make_shared<pyvec<KeySignature<T>>>(std::move(key_signatures));
        this->tempos         = std::make_shared<pyvec<Tempo<T>>>(std::move(tempos));
        // this->lyrics         = std::make_shared<pyvec<TextMeta<T>>>(std::move(lyrics));
        this->markers = std::make_shared<pyvec<TextMeta<T>>>(std::move(markers));
    }


    Score(
        const i32                       tpq,
        shared<vec<shared<Track<T>>>>   tracks,
        shared<pyvec<TimeSignature<T>>> time_signatures,
        shared<pyvec<KeySignature<T>>>  key_signatures,
        shared<pyvec<Tempo<T>>>         tempos,
        // shared<pyvec<TextMeta<T>>>      lyrics,
        shared<pyvec<TextMeta<T>>> markers
    ) :
        ticks_per_quarter{tpq}, tracks{std::move(tracks)},
        time_signatures{std::move(time_signatures)}, key_signatures{std::move(key_signatures)},
        tempos{std::move(tempos)},
        // lyrics{std::move(lyrics)},
        markers{std::move(markers)} {}

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
               // && *lyrics == *other.lyrics
               && *markers == *other.markers;
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
        return {
            ticks_per_quarter,
            std::move(new_tracks),
            std::move(time_signatures->deepcopy()),
            std::move(key_signatures->deepcopy()),
            std::move(tempos->deepcopy()),
            // std::move(lyrics->deepcopy()),
            std::move(markers->deepcopy())
        };
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
    i32                   ticks_per_quarter;
    vec<TimeSignature<T>> time_signatures;
    vec<KeySignature<T>>  key_signatures;
    vec<Tempo<T>>         tempos;
    // vec<TextMeta<T>>      lyrics;
    vec<TextMeta<T>>      markers;
    vec<TrackNative<T>>   tracks;

    ScoreNative() : ticks_per_quarter(0) {}

    explicit ScoreNative(const i32 ticks_per_quarter) : ticks_per_quarter(ticks_per_quarter) {}

    [[nodiscard]] bool empty() const {
        return time_signatures.empty()
            && key_signatures.empty()
            && tempos.empty()
            // && lyrics.empty()
            && markers.empty()
            && tracks.empty();
    }

    template<DataFormat F>
    [[nodiscard]] static ScoreNative parse(std::span<const u8> bytes);

    template<DataFormat F>
    [[nodiscard]] vec<u8> dumps() const;
};
}   // namespace symusic

#endif   // LIBSYMUSIC_TRACK_HSCORE_H

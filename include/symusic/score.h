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

    i32                                   ticks_per_quarter{};
    shared<vec<shared<Track<T>>>>         tracks{};
    shared<vec<shared<TimeSignature<T>>>> time_signatures{};
    shared<vec<shared<KeySignature<T>>>>  key_signatures{};
    shared<vec<shared<Tempo<T>>>>         tempos{};
    shared<vec<shared<TextMeta<T>>>>      lyrics, markers{};

    Score() : ticks_per_quarter{0} {
        tracks          = std::make_shared<vec<shared<Track<T>>>>();
        time_signatures = std::make_shared<vec<shared<TimeSignature<T>>>>();
        key_signatures  = std::make_shared<vec<shared<KeySignature<T>>>>();
        tempos          = std::make_shared<vec<shared<Tempo<T>>>>();
        lyrics          = std::make_shared<vec<shared<TextMeta<T>>>>();
        markers         = std::make_shared<vec<shared<TextMeta<T>>>>();
    }

    Score(const Score&) = default;

    Score(Score&& other) noexcept {
        // move constructor
        ticks_per_quarter = other.ticks_per_quarter;
        tracks            = std::move(other.tracks);
        time_signatures   = std::move(other.time_signatures);
        key_signatures    = std::move(other.key_signatures);
        tempos            = std::move(other.tempos);
        lyrics            = std::move(other.lyrics);
        markers           = std::move(other.markers);
    }

    Score(
        const i32                             tpq,
        shared<vec<shared<Track<T>>>>         tracks,
        shared<vec<shared<TimeSignature<T>>>> time_signatures,
        shared<vec<shared<KeySignature<T>>>>  key_signatures,
        shared<vec<shared<Tempo<T>>>>         tempos,
        shared<vec<shared<TextMeta<T>>>>      lyrics,
        shared<vec<shared<TextMeta<T>>>>      markers
    ) :
        ticks_per_quarter{tpq}, tracks{std::move(tracks)},
        time_signatures{std::move(time_signatures)}, key_signatures{std::move(key_signatures)},
        tempos{std::move(tempos)}, lyrics{std::move(lyrics)}, markers{std::move(markers)} {}

    Score& operator=(const Score&)              = default;
    bool   operator==(const Score& other) const = default;
    bool   operator!=(const Score& other) const = default;

    [[nodiscard]] Score copy() const { return {*this}; }

    [[nodiscard]] Score deepcopy() const {
        return {
            ticks_per_quarter,
            std::move(details::deepcopy(tracks)),
            std::move(details::deepcopy(time_signatures)),
            std::move(details::deepcopy(key_signatures)),
            std::move(details::deepcopy(tempos)),
            std::move(details::deepcopy(lyrics)),
            std::move(details::deepcopy(markers))
        };
    }

    explicit Score(const i32 tpq) : ticks_per_quarter{tpq} {}

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
    vec<TextMeta<T>>      lyrics;
    vec<TextMeta<T>>      markers;
    vec<TrackNative<T>>   tracks;

    ScoreNative() : ticks_per_quarter(0) {}

    explicit ScoreNative(const i32 ticks_per_quarter) : ticks_per_quarter(ticks_per_quarter) {}

    [[nodiscard]] bool empty() const {
        return time_signatures.empty() && key_signatures.empty() && tempos.empty() &&
               lyrics.empty() && markers.empty() && tracks.empty();
    }

    template<DataFormat F>
    [[nodiscard]] static ScoreNative parse(std::span<const u8> bytes);

    template<DataFormat F>
    [[nodiscard]] vec<u8> dumps() const;
};
}   // namespace symusic

#endif   // LIBSYMUSIC_TRACK_HSCORE_H

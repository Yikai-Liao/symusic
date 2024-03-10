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
    typedef T ttype;
    typedef typename T::unit unit;

    i32 ticks_per_quarter{};
    // pyvec<U> = shared<vec<shared<U>>>
    pyvec<Track<T>> tracks{};
    pyvec<TimeSignature<T>> time_signatures{};
    pyvec<KeySignature<T>> key_signatures{};
    pyvec<Tempo<T>> tempos{};
    pyvec<TextMeta<T>> lyrics, markers{};

    Score() {
        ticks_per_quarter = 0;
        tracks = std::make_shared<vec<Track<T>>>();
        time_signatures = std::make_shared<vec<TimeSignature<T>>>();
        key_signatures = std::make_shared<vec<KeySignature<T>>>();
        tempos = std::make_shared<vec<Tempo<T>>>();
        lyrics = std::make_shared<vec<TextMeta<T>>>();
        markers = std::make_shared<vec<TextMeta<T>>>();
    }

    Score(const Score&) = default;

    Score(Score && other) noexcept {
        // move constructor
        ticks_per_quarter = other.ticks_per_quarter;
        tracks = std::move(other.tracks);
        time_signatures = std::move(other.time_signatures);
        key_signatures = std::move(other.key_signatures);
        tempos = std::move(other.tempos);
        lyrics = std::move(other.lyrics);
        markers = std::move(other.markers);
    }

    explicit Score(const i32 tpq): Score() {
        ticks_per_quarter = tpq;
    }

    Score& operator=(const Score&) = default;
    bool operator==(const Score & other) const = default;
    bool operator!=(const Score & other) const = default;

    [[nodiscard]] Score copy() const { return {*this}; }

    [[nodiscard]] Score deepcopy() const {
        Score ret{ticks_per_quarter};
        ret.tracks = std::make_shared<vec<Track<T>>>();
        for(const shared<Track<T>> & track: *tracks) {
            ret.tracks->push_back(track->deepcopy());
        }
        ret.time_signatures = std::make_shared<vec<TimeSignature<T>>>(*time_signatures);
        ret.key_signatures = std::make_shared<vec<KeySignature<T>>>(*key_signatures);
        ret.tempos = std::make_shared<vec<Tempo<T>>>(*tempos);
        ret.lyrics = std::make_shared<vec<TextMeta<T>>>(*lyrics);
        ret.markers = std::make_shared<vec<TextMeta<T>>>(*markers);
        return ret;
    }


    Score(
        const i32 tpq, const pyvec<Track<T>> & tracks,
        const pyvec<TimeSignature<T>> & time_signatures,
        const pyvec<KeySignature<T>> & key_signatures,
        const pyvec<Tempo<T>> & tempos,
        const pyvec<TextMeta<T>> & lyrics,
        const pyvec<TextMeta<T>> & markers
    ):  ticks_per_quarter{tpq}, tracks{tracks}, time_signatures{time_signatures},
        key_signatures{key_signatures}, tempos{tempos}, lyrics{lyrics}, markers{markers} {}

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
    Score& sort_inplace(bool reverse = false);

    // Clip all the events in the score, non-inplace, return a new Score
    // For events with duration, clip_end is used to determine whether to clip based on end time.
    [[nodiscard]] Score clip(unit start, unit end, bool clip_end = false) const;

    // shift the time of all the events in the score, non-inplace, return a new Score
    [[nodiscard]] Score shift_time(unit offset) const;

    // shift the time of all the events in the score, inplace, return self reference
    Score& shift_time_inplace(unit offset);

    // shift the pitch of all notes in the score, non-inplace, return a new Score
    [[nodiscard]] Score shift_pitch(i8 offset) const;

    // shift the pitch of all notes in the score, inplace, return self reference
    Score& shift_pitch_inplace(i8 offset);

    // shift the velocity of all notes in the score, non-inplace, return a new Score
    [[nodiscard]] Score shift_velocity(i8 offset) const;

    // shift the velocity of all notes in the score, inplace, return self reference
    Score& shift_velocity_inplace(i8 offset);
};
} // namespace symusic

#endif //LIBSYMUSIC_TRACK_HSCORE_H

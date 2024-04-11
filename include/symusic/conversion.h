//
// Created by lyk on 23-12-17.
//
#pragma once

#ifndef LIBSYMUSIC_CONVERSION_H
#define LIBSYMUSIC_CONVERSION_H

#include "symusic/time_unit.h"
#include "symusic/score.h"

namespace symusic {

template<TType To, TType From>
Score<To> convert(
    const Score<From>& score, typename To::unit min_dur = static_cast<typename To::unit>(0)
);

template<TType T>
Score<Tick> resample(const Score<T>& score, i32 tpq, Tick::unit min_dur = 0);

template<TType T>
Score<T> to_shared(ScoreNative<T>&& score);

template<TType T>
Track<T> to_shared(TrackNative<T>&& track);

template<TType T>
ScoreNative<T> to_native(const Score<T>& score);

template<TType T>
TrackNative<T> to_native(const Track<T>& track);

/*
 *  Conversion between shared and native
 */

template<TType T>
Track<T> to_shared(TrackNative<T>&& track) {
    Track<T> new_track{std::move(track.name), track.program, track.is_drum};
    new_track.notes       = details::to_shared_vec(std::move(track.notes));
    new_track.controls    = details::to_shared_vec(std::move(track.controls));
    new_track.pitch_bends = details::to_shared_vec(std::move(track.pitch_bends));
    new_track.pedals      = details::to_shared_vec(std::move(track.pedals));
    return new_track;
}

template<TType T>
Score<T> to_shared(ScoreNative<T>&& score) {
    Score<T> new_score{score.ticks_per_quarter};
    new_score.time_signatures = details::to_shared_vec(std::move(score.time_signatures));
    new_score.key_signatures  = details::to_shared_vec(std::move(score.key_signatures));
    new_score.tempos          = details::to_shared_vec(std::move(score.tempos));
    new_score.lyrics          = details::to_shared_vec(std::move(score.lyrics));
    new_score.markers         = details::to_shared_vec(std::move(score.markers));
    new_score.tracks->reserve(score.tracks.size());
    for (auto & track : score.tracks) {
        shared<Track<T>> new_track = std::make_shared<Track<T>>(std::move(to_shared(std::move(track))));
        new_score.tracks->push_back(std::move(new_track));
    }
    return new_score;
}

template<TType T>
TrackNative<T> to_native(const Track<T>& track) {
    TrackNative<T> new_track{track.name, track.program, track.is_drum};
    new_track.notes       = details::to_native_vec(track.notes);
    new_track.controls    = details::to_native_vec(track.controls);
    new_track.pitch_bends = details::to_native_vec(track.pitch_bends);
    new_track.pedals      = details::to_native_vec(track.pedals);
    return new_track;
}

template<TType T>
ScoreNative<T> to_native(const Score<T>& score) {
    ScoreNative<T> new_score{score.ticks_per_quarter};
    new_score.time_signatures = details::to_native_vec(score.time_signatures);
    new_score.key_signatures  = details::to_native_vec(score.key_signatures);
    new_score.tempos          = details::to_native_vec(score.tempos);
    new_score.lyrics          = details::to_native_vec(score.lyrics);
    new_score.markers         = details::to_native_vec(score.markers);
    new_score.tracks.reserve(score.tracks->size());
    for (const shared<Track<T>>& track : *score.tracks) {
        new_score.tracks.push_back(std::move(to_native(*track)));
    }
    return new_score;
}

}   // namespace symusic



#endif   // LIBSYMUSIC_CONVERSION_H

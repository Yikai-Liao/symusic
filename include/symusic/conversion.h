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
    new_track.notes       = std::make_shared<pyvec<Note<T>>>(std::move(track.notes));
    new_track.controls    = std::make_shared<pyvec<ControlChange<T>>>(std::move(track.controls));
    new_track.pedals      = std::make_shared<pyvec<Pedal<T>>>(std::move(track.pedals));
    new_track.pitch_bends = std::make_shared<pyvec<PitchBend<T>>>(std::move(track.pitch_bends));
    new_track.lyrics      = std::make_shared<pyvec<TextMeta<T>>>(std::move(track.lyrics));
    return new_track;
}

template<TType T>
Score<T> to_shared(ScoreNative<T>&& score) {
    Score<T> new_score{score.ticks_per_quarter};
    new_score.tempos  = std::make_shared<pyvec<Tempo<T>>>(std::move(score.tempos));
    // new_score.lyrics  = std::make_shared<pyvec<TextMeta<T>>>(std::move(score.lyrics));
    new_score.markers = std::make_shared<pyvec<TextMeta<T>>>(std::move(score.markers));
    new_score.time_signatures
        = std::make_shared<pyvec<TimeSignature<T>>>(std::move(score.time_signatures));
    new_score.key_signatures
        = std::make_shared<pyvec<KeySignature<T>>>(std::move(score.key_signatures));
    new_score.tracks->reserve(score.tracks.size());
    // clang-format off
    for (auto& track : score.tracks) {
        new_score.tracks->push_back(
            std::make_shared<Track<T>>(std::move(to_shared(std::move(track))
        )));
    }
    // clang-format on
    return new_score;
}

template<TType T>
TrackNative<T> to_native(const Track<T>& track) {
    TrackNative<T> new_track{track.name, track.program, track.is_drum};
    new_track.notes       = std::move(track.notes->collect());
    new_track.controls    = std::move(track.controls->collect());
    new_track.pedals      = std::move(track.pedals->collect());
    new_track.pitch_bends = std::move(track.pitch_bends->collect());
    new_track.lyrics      = std::move(track.lyrics->collect());
    return new_track;
}

template<TType T>
ScoreNative<T> to_native(const Score<T>& score) {
    ScoreNative<T> new_score{score.ticks_per_quarter};
    new_score.time_signatures = std::move(score.time_signatures->collect());
    new_score.key_signatures  = std::move(score.key_signatures->collect());
    new_score.tempos          = std::move(score.tempos->collect());
    // new_score.lyrics          = std::move(score.lyrics->collect());
    new_score.markers         = std::move(score.markers->collect());

    new_score.tracks.reserve(score.tracks->size());
    for (const shared<Track<T>>& track : *score.tracks) {
        new_score.tracks.emplace_back(std::move(to_native(*track)));
    }
    return new_score;
}

}   // namespace symusic



#endif   // LIBSYMUSIC_CONVERSION_H

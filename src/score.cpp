//
// Created by lyk on 23-12-25.
//
#include "symusic/score.h"
#include "symusic/ops.h"

namespace symusic {

// extern to_string and summary
#define EXTERN_REPR(__COUNT, T) \
extern template std::string Score<T>::to_string() const; \
extern template std::string Score<T>::summary() const;

REPEAT_ON(EXTERN_REPR, Tick, Quarter, Second)
#undef EXTERN_REPR

template<TType T>
typename T::unit Score<T>::start() const {
    if(this->empty()) return 0;

    typename T::unit ans = std::numeric_limits<typename T::unit>::max();
    for(const auto & track: tracks) {
        ans = std::min(ans, track.start());
    }
    ans = std::min(ans, ops::start(time_signatures));
    ans = std::min(ans, ops::start(key_signatures));
    ans = std::min(ans, ops::start(tempos));
    ans = std::min(ans, ops::start(lyrics));
    ans = std::min(ans, ops::start(markers));
    return ans;
}

template<TType T>
typename T::unit Score<T>::end() const {
    if(this->empty()) return 0;

    typename T::unit ans = std::numeric_limits<typename T::unit>::min();
    for (const auto & track: tracks) {
        ans = std::max(ans, track.end());
    }
    ans = std::max(ans, ops::end(time_signatures));
    ans = std::max(ans, ops::end(key_signatures));
    ans = std::max(ans, ops::end(tempos));
    ans = std::max(ans, ops::end(lyrics));
    ans = std::max(ans, ops::end(markers));
    return ans;
}

template<TType T>
size_t Score<T>::note_num() const {
    size_t ans = 0;
    for (const auto & track: tracks) {
        ans += track.note_num();
    }   return ans;
}

template<TType T>
bool Score<T>::empty() const {
    return tracks.empty()
        && time_signatures.empty()
        && key_signatures.empty()
        && tempos.empty()
        && lyrics.empty()
        && markers.empty();
}

template<TType T>
size_t Score<T>::track_num() const {
    return tracks.size();
}

template<TType T>
Score<T>& Score<T>::sort_inplace(const bool reverse) {

    for(auto &track: tracks) track.sort_inplace(reverse);

    ops::sort_by_time(time_signatures, reverse);
    ops::sort_by_time(key_signatures, reverse);
    ops::sort_by_time(tempos, reverse);
    ops::sort_by_time(lyrics, reverse);
    ops::sort_by_time(markers, reverse);

    return *this;
}

template<TType T>
Score<T> Score<T>::sort(const bool reverse) const {
    return copy().sort_inplace(reverse);
}

template<TType T>
Score<T> Score<T>::clip(unit start, unit end, bool clip_end) const {
    vec<Track<T>> new_tracks;
    new_tracks.reserve(track_num());
    for(const auto & track: tracks) {
        new_tracks.emplace_back(track.clip(start, end, clip_end));
    }
    return {
        ticks_per_quarter,
        std::move(new_tracks),
        std::move(ops::clip_with_sentinel(time_signatures, start, end)),
        std::move(ops::clip_with_sentinel(key_signatures, start, end)),
        std::move(ops::clip_with_sentinel(tempos, start, end)),
        std::move(ops::clip(lyrics, start, end)),
        std::move(ops::clip(markers, start, end))
    };
}

// time shift
template<TType T>
Score<T>& Score<T>::shift_time_inplace(const unit offset) {
    for(auto &track: tracks) track.shift_time_inplace(offset);
    for(auto &time_signature: time_signatures) time_signature.shift_time_inplace(offset);
    for(auto &key_signature: key_signatures) key_signature.shift_time_inplace(offset);
    for(auto &tempo: tempos) tempo.shift_time_inplace(offset);
    for(auto &lyric: lyrics) lyric.shift_time_inplace(offset);
    for(auto &marker: markers) marker.shift_time_inplace(offset);
    return *this;
}

template<TType T>
Score<T> Score<T>::shift_time(const unit offset) const {
    return copy().shift_time_inplace(offset);
}

// pitch shift
template<TType T>
Score<T>& Score<T>::shift_pitch_inplace(const i8 offset) {
    for(auto &track: tracks) track.shift_pitch_inplace(offset);
    return *this;
}

template<TType T>
Score<T> Score<T>::shift_pitch(const i8 offset) const {
    return copy().shift_pitch_inplace(offset);
}

// velocity shift
template<TType T>
Score<T>& Score<T>::shift_velocity_inplace(const i8 offset) {
    for(auto &track: tracks) track.shift_velocity_inplace(offset);
    return *this;
}

template<TType T>
Score<T> Score<T>::shift_velocity(const i8 offset) const {
    return copy().shift_velocity_inplace(offset);
}

#define INSTANTIATE_SCORE(__COUNT, T) template struct Score<T>;
REPEAT_ON(INSTANTIATE_SCORE, Tick, Quarter, Second)
#undef INSTANTIATE_SCORE

}

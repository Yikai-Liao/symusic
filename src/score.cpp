//
// Created by lyk on 23-12-25.
//
#include "MetaMacro.h"
#include "symusic/score.h"
#include "symusic/ops.h"

namespace symusic {

// extern to_string and summary
#define EXTERN_REPR(__COUNT, T)                              \
    extern template std::string Score<T>::to_string() const; \
    extern template std::string Score<T>::summary() const;

REPEAT_ON(EXTERN_REPR, Tick, Quarter, Second)
#undef EXTERN_REPR

template<TType T>
typename T::unit Score<T>::start() const {
    if (this->empty()) return 0;

    typename T::unit ans = std::numeric_limits<typename T::unit>::max();
    for (const shared<Track<T>>& track : *tracks) { ans = std::min(ans, track->start()); }
    ans = std::min(ans, ops::start(*time_signatures));
    ans = std::min(ans, ops::start(*key_signatures));
    ans = std::min(ans, ops::start(*tempos));
    // ans = std::min(ans, ops::start(*lyrics));
    ans = std::min(ans, ops::start(*markers));
    return ans;
}

template<TType T>
typename T::unit Score<T>::end() const {
    if (this->empty()) return 0;

    typename T::unit ans = std::numeric_limits<typename T::unit>::min();
    for (const shared<Track<T>>& track : *tracks) { ans = std::max(ans, track->end()); }
    ans = std::max(ans, ops::end(*time_signatures));
    ans = std::max(ans, ops::end(*key_signatures));
    ans = std::max(ans, ops::end(*tempos));
    // ans = std::max(ans, ops::end(*lyrics));
    ans = std::max(ans, ops::end(*markers));
    return ans;
}

template<TType T>
size_t Score<T>::note_num() const {
    size_t ans = 0;
    for (const shared<Track<T>>& track : *tracks) { ans += track->note_num(); }
    return ans;
}

template<TType T>
bool Score<T>::empty() const {
    return tracks->empty()
        && time_signatures->empty()
        && key_signatures->empty()
        && tempos->empty()
        // && lyrics->empty()
        && markers->empty();
}

template<TType T>
size_t Score<T>::track_num() const {
    return tracks->size();
}

template<TType T>
void Score<T>::sort_inplace(const bool reverse) {

    for (auto& track : *tracks) track->sort_inplace(reverse);
    auto key = [](const auto& event) { return event.default_key(); };
    time_signatures->sort(key, reverse);
    key_signatures->sort(key, reverse);
    tempos->sort(key, reverse);
    // lyrics->sort(key, reverse);
    markers->sort(key, reverse);
}

template<TType T>
Score<T> Score<T>::sort(const bool reverse) const {
    auto ans = deepcopy();
    ans.sort_inplace(reverse);
    return ans;
}

template<TType T>
void Score<T>::clip_inplace(unit start, unit end, bool clip_end) {
    for (auto& track : *tracks) track->clip_inplace(start, end, clip_end);
    ops::clip_inplace(*time_signatures, start, end);
    ops::clip_inplace(*key_signatures, start, end);
    ops::clip_inplace(*tempos, start, end);
    // ops::clip_inplace(*lyrics, start, end);
    ops::clip_inplace(*markers, start, end);
}


template<TType T>
Score<T> Score<T>::clip(unit start, unit end, bool clip_end) const {
    auto ans = deepcopy();
    ans.clip_inplace(start, end, clip_end);
    return ans;
}

// time shift
template<TType T>
void Score<T>::shift_time_inplace(const unit offset) {
    for (auto& track : *tracks) track->shift_time_inplace(offset);
    for (auto& time_signature : *time_signatures) time_signature->shift_time_inplace(offset);
    for (auto& key_signature : *key_signatures) key_signature->shift_time_inplace(offset);
    for (auto& tempo : *tempos) tempo->shift_time_inplace(offset);
    // for (auto& lyric : *lyrics) lyric->shift_time_inplace(offset);
    for (auto& marker : *markers) marker->shift_time_inplace(offset);
}

template<TType T>
Score<T> Score<T>::shift_time(const unit offset) const {
    auto ans = deepcopy();
    ans.shift_time_inplace(offset);
    return ans;
}

// pitch shift
template<TType T>
void Score<T>::shift_pitch_inplace(const i8 offset) {
    for (auto& track : *tracks) track->shift_pitch_inplace(offset);
}

template<TType T>
Score<T> Score<T>::shift_pitch(const i8 offset) const {
    auto ans = deepcopy();
    ans.shift_pitch_inplace(offset);
    return ans;
}

// velocity shift
template<TType T>
void Score<T>::shift_velocity_inplace(const i8 offset) {
    for (auto& track : *tracks) track->shift_velocity_inplace(offset);
}

template<TType T>
Score<T> Score<T>::shift_velocity(const i8 offset) const {
    auto ans = deepcopy();
    ans.shift_velocity_inplace(offset);
    return ans;
}

#define INSTANTIATE_SCORE(__COUNT, T) template struct Score<T>;
REPEAT_ON(INSTANTIATE_SCORE, Tick, Quarter, Second)
#undef INSTANTIATE_SCORE

}   // namespace symusic

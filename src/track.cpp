//
// Created by lyk on 23-12-25.
//
#include "symusic/track.h"
#include "symusic/ops.h"
#include "MetaMacro.h"

namespace symusic {

// extern to_string and summary
#define EXTERN_REPR(__COUNT, T) \
    extern template std::string Track<T>::to_string() const; \
    extern template std::string Track<T>::summary() const;

REPEAT_ON(EXTERN_REPR, Tick, Quarter, Second)
#undef EXTERN_REPR

template<TType T>
typename T::unit Track<T>::start() const {
    if(this->empty()) return 0;
    typename T::unit ans = std::numeric_limits<typename T::unit>::max();
    ans = std::min(ans, ops::start(*notes));
    ans = std::min(ans, ops::start(*controls));
    ans = std::min(ans, ops::start(*pitch_bends));
    ans = std::min(ans, ops::start(*pedals));
    ans = std::min(ans, ops::start(*lyrics));
    return ans;
}

template<TType T>
typename T::unit Track<T>::end() const {
    if(this->empty()) return 0;
    typename T::unit ans = std::numeric_limits<typename T::unit>::min();
    ans = std::max(ans, ops::end(*notes));
    ans = std::max(ans, ops::end(*controls));
    ans = std::max(ans, ops::end(*pitch_bends));
    ans = std::max(ans, ops::end(*pedals));
    ans = std::max(ans, ops::end(*lyrics));
    return ans;
}

template<TType T>
size_t Track<T>::note_num() const {
    return notes->size();
}

template<TType T>
bool Track<T>::empty() const {
    return notes->empty() && controls->empty() && pitch_bends->empty() && pedals->empty() && lyrics->empty();
}

template<TType T>
void Track<T>::sort_inplace(const bool reverse) {
    auto key = [](const auto & event) { return event.default_key(); };
    notes->sort(key, reverse);
    controls->sort(key, reverse);
    pitch_bends->sort(key, reverse);
    pedals->sort(key, reverse);
    lyrics->sort(key, reverse);
}

template<TType T>
Track<T> Track<T>::sort(const bool reverse) {
    auto ans = deepcopy();
    ans.sort_inplace(reverse);
    return ans;
}

template<TType T>
void Track<T>::clip_inplace(const unit start, const unit end, const bool clip_end) {
    ops::clip_inplace(*notes, start, end, clip_end);
    ops::clip_inplace(*controls, start, end);
    ops::clip_inplace(*pitch_bends, start, end);
    ops::clip_inplace(*pedals, start, end, clip_end);
    ops::clip_inplace(*lyrics, start, end, clip_end);
}


template<TType T>
Track<T> Track<T>::clip(const unit start, const unit end, const bool clip_end) const {
    auto ans = deepcopy();
    ans.clip_inplace(start, end, clip_end);
    return ans;
}

template<TType T>
void Track<T>::shift_time_inplace(const unit offset) {
    for(auto& note: *notes) note->shift_time_inplace(offset);
    for(auto& control: *controls) control->shift_time_inplace(offset);
    for(auto& pitch_bend: *pitch_bends) pitch_bend->shift_time_inplace(offset);
    for(auto& pedal: *pedals) pedal->shift_time_inplace(offset);
    for(auto& lyric: *lyrics) lyric->shift_time_inplace(offset);
}

template<TType T>
Track<T> Track<T>::shift_time(const unit offset) const {
    auto ans = deepcopy();
    ans.shift_time_inplace(offset);
    return ans;
}

template<TType T>
void Track<T>::shift_pitch_inplace(const i8 offset) {
    for(auto& note: *notes) note->shift_pitch_inplace(offset);
}

template<TType T>
Track<T> Track<T>::shift_pitch(const i8 offset) const {
    auto ans = deepcopy();
    ans.shift_pitch_inplace(offset);
    return ans;
}

template<TType T>
void Track<T>::shift_velocity_inplace(const i8 offset) {
    for(auto& note: *notes) note->shift_velocity_inplace(offset);
}

template<TType T>
Track<T> Track<T>::shift_velocity(const i8 offset) const {
    auto ans = deepcopy();
    ans.shift_velocity_inplace(offset);
    return ans;
}

#define INSTANTIATE_TRACK(__COUNT, T) template struct Track<T>;

REPEAT_ON(INSTANTIATE_TRACK, Tick, Quarter, Second)

#undef INSTANTIATE_TRACK

}

//
// Created by lyk on 23-12-25.
//
#include "symusic/track.h"
#include "symusic/ops.h"

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
    ans = std::min(ans, ops::start(notes));
    ans = std::min(ans, ops::start(controls));
    ans = std::min(ans, ops::start(pitch_bends));
    ans = std::min(ans, ops::start(pedals));
    return ans;
}

template<TType T>
typename T::unit Track<T>::end() const {
    if(this->empty()) return 0;
    typename T::unit ans = std::numeric_limits<typename T::unit>::min();
    ans = std::max(ans, ops::end(notes));
    ans = std::max(ans, ops::end(controls));
    ans = std::max(ans, ops::end(pitch_bends));
    ans = std::max(ans, ops::end(pedals));
    return ans;
}

template<TType T>
size_t Track<T>::note_num() const {
    return notes.size();
}

template<TType T>
bool Track<T>::empty() const {
    return notes.empty() && controls.empty() && pitch_bends.empty() && pedals.empty();
}

template<TType T>
Track<T>& Track<T>::sort_inplace(const bool reverse) {
    ops::sort_by_time(notes, reverse);
    ops::sort_by_time(controls, reverse);
    ops::sort_by_time(pitch_bends, reverse);
    ops::sort_by_time(pedals, reverse);
    return *this;
}

template<TType T>
Track<T> Track<T>::sort(const bool reverse) {
    return copy().sort_inplace(reverse);
}

template<TType T>
Track<T> Track<T>::clip(unit start, unit end, bool clip_end) const {
    return {
        name, program, is_drum,
        std::move(ops::clip(notes, start, end, clip_end)),
        std::move(ops::clip(controls, start, end)),
        std::move(ops::clip(pitch_bends, start, end)),
        std::move(ops::clip(pedals, start, end, clip_end))
    };
}

template<TType T>
Track<T>& Track<T>::shift_time_inplace(const unit offset) {
    for(auto& note: notes) note.shift_time_inplace(offset);
    for(auto& control: controls) control.shift_time_inplace(offset);
    for(auto& pitch_bend: pitch_bends) pitch_bend.shift_time_inplace(offset);
    for(auto& pedal: pedals) pedal.shift_time_inplace(offset);
    return *this;
}

template<TType T>
Track<T> Track<T>::shift_time(const unit offset) const {
    return copy().shift_time_inplace(offset);
}

template<TType T>
Track<T>& Track<T>::shift_pitch_inplace(const i8 offset) {
    for(auto& note: notes) note.shift_pitch_inplace(offset);
    return *this;
}

template<TType T>
Track<T> Track<T>::shift_pitch(const i8 offset) const {
    return copy().shift_pitch_inplace(offset);
}

template<TType T>
Track<T>& Track<T>::shift_velocity_inplace(const i8 offset) {
    for(auto& note: notes) note.shift_velocity_inplace(offset);
    return *this;
}

template<TType T>
Track<T> Track<T>::shift_velocity(const i8 offset) const {
    return copy().shift_velocity_inplace(offset);
}

#define INSTANTIATE_TRACK(__COUNT, T) template struct Track<T>;

REPEAT_ON(INSTANTIATE_TRACK, Tick, Quarter, Second)

#undef INSTANTIATE_TRACK

}

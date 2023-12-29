//
// Created by lyk on 23-12-25.
//

#include <stdexcept>
#include <cassert>
#include <functional>

#include "pdqsort.h"

#include "symusic/note_arr.h"
#include "symusic/track.h"
#include "symusic/conversion.h"

namespace symusic {

// extern to_string and summary
#define EXTERN_REPR(__COUNT, T) \
    extern template std::string NoteArr<T>::to_string() const; \
    extern template std::string NoteArr<T>::summary() const;

REPEAT_ON(EXTERN_REPR, Tick, Quarter, Second)
#undef EXTERN_REPR

template<TType T>
typename T::unit NoteArr<T>::start() const {
    if (time.empty()) {
        throw std::runtime_error("NoteArr is empty");
    }
    typename T::unit ans = std::numeric_limits<typename T::unit>::max();
    for (const auto & t: time) {
        ans = std::min(ans, t);
    }   return ans;
}

template<TType T>
typename T::unit NoteArr<T>::end() const {
    if (time.empty()) throw std::runtime_error("NoteArr is empty");
    if (time.size() != duration.size()) throw std::runtime_error("NoteArr: size of time and duration are not equal");
    typename T::unit ans = std::numeric_limits<typename T::unit>::min();
    for(size_t i = 0; i < time.size(); ++i) {
        ans = std::max(ans, time[i] + duration[i]);
    }   return ans;
}

template<TType T>
bool NoteArr<T>::empty() const {
    return time.empty() && duration.empty() && pitch.empty() && velocity.empty();
}

template<TType T>
bool NoteArr<T>::valid() const {
    const size_t note_num = time.size();
    return note_num == duration.size() && note_num == pitch.size() && note_num == velocity.size();
}

template<TType T>
size_t NoteArr<T>::note_num() const {
    if (!valid()) throw std::runtime_error("NoteArr is not valid");
    return time.size();
}

template<TType T>
NoteArr<T> NoteArr<T>::sort(const bool reverse) {
    // Maybe there is a better way to do this
    Track<T> tmp_track = to_track(*this);
    tmp_track.sort_inplace(reverse);
    return to_note_arr(tmp_track);
}

template<TType T>
void NoteArr<T>::reserve(size_t size) {
    time.reserve(size);
    duration.reserve(size);
    pitch.reserve(size);
    velocity.reserve(size);
}

template<TType T>
void NoteArr<T>::emplace_back(unit time, unit duration, i8 pitch, i8 velocity) {
    this->time.push_back(time);
    this->duration.push_back(duration);
    this->pitch.emplace_back(pitch);
    this->velocity.emplace_back(velocity);
}

template<TType T>
void NoteArr<T>::push_back(const Note<T> & note) {
    this->time.push_back(note.time);
    this->duration.push_back(note.duration);
    this->pitch.emplace_back(note.pitch);
    this->velocity.emplace_back(note.velocity);
}

template<TType T>
NoteArr<T> NoteArr<T>::clip(const unit start, const unit end, const bool clip_end) const {
    if (!valid()) throw std::runtime_error("NoteArr is not valid");
    if (start > end) throw std::runtime_error("start time is larger than end time");
    if (start < 0) throw std::runtime_error("start time is negative");
    if (end < 0) throw std::runtime_error("end time is negative");
    if (empty()) return {};

    std::function<bool(size_t)> check_end;
    if (clip_end) check_end = [end, this](size_t i) {return time[i] + duration[i] <= end;};
    else check_end = [end, this](size_t i) {return time[i] <= end;};

    NoteArr new_arr;
    new_arr.reserve(time.size());
    for (size_t i = 0; i < note_num(); ++i) {
        if (time[i] >= start && check_end(i)) {
            new_arr.emplace_back(time[i], duration[i], pitch[i], velocity[i]);
        }
    }   return new_arr;
}

#define INSTANTIATE(__COUNT, T) template struct NoteArr<T>;
REPEAT_ON(INSTANTIATE, Tick, Quarter, Second)
#undef INSTANTIATE

} // namespace symusic
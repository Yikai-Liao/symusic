//
// Created by lyk on 2023/12/26.
//
#include <stdexcept>

#include "symusic/event.h"
namespace symusic {

// extern to_string for simple events
#define EVENT_TO_STRING(__COUNT, NAME)                      \
    extern template std::string NAME<Tick>::to_string() const;     \
    extern template std::string NAME<Quarter>::to_string() const;  \
    extern template std::string NAME<Second>::to_string() const;

REPEAT_ON(
    EVENT_TO_STRING,
    Note, Pedal, ControlChange, TimeSignature,
    KeySignature, Tempo, PitchBend, TextMeta
)

#undef EVENT_TO_STRING

inline i8 safe_add(const i8 a, const i8 b) {
    const int ans = a + b;
    if (ans > 127 || ans < 0)
        throw std::range_error("Overflow while adding " + std::to_string(a) + " and " + std::to_string(b));
    return static_cast<i8>(ans);
}

#define SHIFT_TIME(__COUNT, NAME)                                                   \
    template<TType T>                                                               \
    inline NAME<T>& NAME<T>::shift_time_inplace(const typename T::unit offset) {    \
        this->time += offset;                                                       \
        return *this;                                                               \
    }                                                                               \
    template<TType T>                                                               \
    inline NAME<T> NAME<T>::shift_time(const typename T::unit offset) const {       \
        return NAME<T>(*this).shift_time_inplace(offset);                           \
    }

REPEAT_ON(
    SHIFT_TIME,
    Note, Pedal, ControlChange, TimeSignature,
    KeySignature, Tempo, PitchBend, TextMeta
)

#undef SHIFT_TIME

template<TType T>
Note<T> Note<T>::shift_pitch(const i8 offset) const {
    return Note<T>(*this).shift_pitch_inplace(offset);
}

template<TType T>
Note<T>& Note<T>::shift_pitch_inplace(const i8 offset) {
    this->pitch = safe_add(this->pitch, offset);
    return *this;
}

template<TType T>
Note<T> Note<T>::shift_velocity(const i8 offset) const {
    return Note<T>(*this).shift_velocity_inplace(offset);
}

template<TType T>
Note<T>& Note<T>::shift_velocity_inplace(const i8 offset) {
    this->velocity = safe_add(this->velocity, offset);
    return *this;
}

#define INSTANTIATE_EVENT(__COUNT, NAME) \
    template struct NAME<Tick>;          \
    template struct NAME<Quarter>;       \
    template struct NAME<Second>;

REPEAT_ON(
    INSTANTIATE_EVENT,
    Note, Pedal, ControlChange, TimeSignature,
    KeySignature, Tempo, PitchBend, TextMeta
)

#undef INSTANTIATE_EVENT

}

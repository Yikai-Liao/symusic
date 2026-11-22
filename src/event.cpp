#include <stdexcept>
#include "symusic/event.h"
namespace symusic {

inline i8 safe_add(const i8 a, const i8 b) {
    const int ans = a + b;
    if (ans > 127 || ans < 0)
        throw std::range_error(
            "Overflow while adding " + std::to_string(a) + " and " + std::to_string(b)
        );
    // The range check bounds the result to a MIDI data byte.
    return static_cast<i8>(ans);
}

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

template struct Note<Tick>;
template struct Note<Quarter>;
template struct Note<Second>;
template struct Pedal<Tick>;
template struct Pedal<Quarter>;
template struct Pedal<Second>;
template struct ControlChange<Tick>;
template struct ControlChange<Quarter>;
template struct ControlChange<Second>;
template struct TimeSignature<Tick>;
template struct TimeSignature<Quarter>;
template struct TimeSignature<Second>;
template struct KeySignature<Tick>;
template struct KeySignature<Quarter>;
template struct KeySignature<Second>;
template struct Tempo<Tick>;
template struct Tempo<Quarter>;
template struct Tempo<Second>;
template struct PitchBend<Tick>;
template struct PitchBend<Quarter>;
template struct PitchBend<Second>;
template struct TextMeta<Tick>;
template struct TextMeta<Quarter>;
template struct TextMeta<Second>;

}  // namespace symusic

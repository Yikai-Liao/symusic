#include <stdexcept>
#include "symusic/detail/event_fields.h"
namespace symusic {

template<TType T>
void Note<T>::shift_pitch(const i32 offset) {
    pitch = detail::checked_event_field<Note<T>, 2>(static_cast<i64>(pitch) + offset);
}

template<TType T>
void Note<T>::shift_velocity(const i32 offset) {
    velocity = detail::checked_event_field<Note<T>, 3>(static_cast<i64>(velocity) + offset);
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

}   // namespace symusic

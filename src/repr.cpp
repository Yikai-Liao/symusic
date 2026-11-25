#include "symusic/repr.h"

namespace symusic {

template<TType T>
std::string Note<T>::to_string() const {
    return fmt::format("{:d}", *this);
}

template std::string Note<Tick>::to_string() const;
template std::string Note<Quarter>::to_string() const;
template std::string Note<Second>::to_string() const;

template<TType T>
std::string Pedal<T>::to_string() const {
    return fmt::format("{:d}", *this);
}

template std::string Pedal<Tick>::to_string() const;
template std::string Pedal<Quarter>::to_string() const;
template std::string Pedal<Second>::to_string() const;

template<TType T>
std::string ControlChange<T>::to_string() const {
    return fmt::format("{:d}", *this);
}

template std::string ControlChange<Tick>::to_string() const;
template std::string ControlChange<Quarter>::to_string() const;
template std::string ControlChange<Second>::to_string() const;

template<TType T>
std::string TimeSignature<T>::to_string() const {
    return fmt::format("{:d}", *this);
}

template std::string TimeSignature<Tick>::to_string() const;
template std::string TimeSignature<Quarter>::to_string() const;
template std::string TimeSignature<Second>::to_string() const;

template<TType T>
std::string KeySignature<T>::to_string() const {
    return fmt::format("{:d}", *this);
}

template std::string KeySignature<Tick>::to_string() const;
template std::string KeySignature<Quarter>::to_string() const;
template std::string KeySignature<Second>::to_string() const;

template<TType T>
std::string Tempo<T>::to_string() const {
    return fmt::format("{:d}", *this);
}

template std::string Tempo<Tick>::to_string() const;
template std::string Tempo<Quarter>::to_string() const;
template std::string Tempo<Second>::to_string() const;

template<TType T>
std::string PitchBend<T>::to_string() const {
    return fmt::format("{:d}", *this);
}

template std::string PitchBend<Tick>::to_string() const;
template std::string PitchBend<Quarter>::to_string() const;
template std::string PitchBend<Second>::to_string() const;

template<TType T>
std::string TextMeta<T>::to_string() const {
    return fmt::format("{:d}", *this);
}

template std::string TextMeta<Tick>::to_string() const;
template std::string TextMeta<Quarter>::to_string() const;
template std::string TextMeta<Second>::to_string() const;

}   // namespace symusic

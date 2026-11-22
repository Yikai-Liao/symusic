#pragma once

#include <stdexcept>
#include <string>
#include <span>
#include <cmath>

#include "symusic/mtype.h"
#include "symusic/io/iodef.h"
#include "symusic/time_unit.h"
#include "symusic/utils.h"
#include <tuple>
#include <utility>

namespace symusic {

// Define TimeStamp for events like Note
template<TType T>
struct TimeStamp {
    using ttype = T;
    using unit = typename T::unit;
    unit                     time = 0;

    bool operator==(const TimeStamp&) const = default;
};

// Define TimeEvent concept, representing events with time
template<typename T>
concept TimeEvent = std::is_base_of_v<TimeStamp<typename T::ttype>, T>;

template<typename T>
concept HashDuration = requires(T t) {
    { t.duration } -> std::convertible_to<typename T::unit>;
};

namespace detail {
template<class E, size_t I>
auto checked_event_field(auto&& input);
}

template<TType T>
struct Note : TimeStamp<T> {
    Note() = default;
    Note(auto time, auto duration, auto pitch, auto velocity) :
        TimeStamp<T>{detail::checked_event_field<Note, 0>(time)},
        duration(detail::checked_event_field<Note, 1>(std::move(duration))),
        pitch(detail::checked_event_field<Note, 2>(std::move(pitch))),
        velocity(detail::checked_event_field<Note, 3>(std::move(velocity))) {}

    [[nodiscard]] Note copy() const { return *this; }
    bool operator==(const Note&) const = default;
    [[nodiscard]] std::string to_string() const;

    [[nodiscard]] static Note deserialize(std::span<const u8> bytes);
    [[nodiscard]] vec<u8> serialize() const;

    typename T::unit duration{0};
    i8   pitch{0}, velocity{0};

    [[nodiscard]] typename T::unit start() const { return this->time; }
    [[nodiscard]] typename T::unit end() const { return detail::add_time(this->time, duration); }

    [[nodiscard]] bool empty() const { return duration <= 0 || velocity <= 0; }

    auto default_key() const { return std::tie(this->time, duration, pitch, velocity); }

    void shift_pitch(i32 offset);

    void shift_velocity(i32 offset);
};

template<TType T>
struct Pedal : TimeStamp<T> {
    Pedal() = default;
    Pedal(auto time, auto duration) :
        TimeStamp<T>{detail::checked_event_field<Pedal, 0>(time)},
        duration(detail::checked_event_field<Pedal, 1>(std::move(duration))) {}

    [[nodiscard]] Pedal copy() const { return *this; }
    bool operator==(const Pedal&) const = default;
    [[nodiscard]] std::string to_string() const;

    [[nodiscard]] static Pedal deserialize(std::span<const u8> bytes);
    [[nodiscard]] vec<u8> serialize() const;

    typename T::unit duration{};

    [[nodiscard]] typename T::unit start() const { return this->time; }
    [[nodiscard]] typename T::unit end() const { return detail::add_time(this->time, duration); }

    auto default_key() const { return std::tie(this->time, duration); }
};

template<TType T>
struct ControlChange : TimeStamp<T> {
    ControlChange() = default;
    ControlChange(auto time, auto number, auto value) :
        TimeStamp<T>{detail::checked_event_field<ControlChange, 0>(time)},
        number(detail::checked_event_field<ControlChange, 1>(std::move(number))),
        value(detail::checked_event_field<ControlChange, 2>(std::move(value))) {}

    [[nodiscard]] ControlChange copy() const { return *this; }
    bool operator==(const ControlChange&) const = default;
    [[nodiscard]] std::string to_string() const;

    [[nodiscard]] static ControlChange deserialize(std::span<const u8> bytes);
    [[nodiscard]] vec<u8> serialize() const;

    u8 number{0}, value{0};

    auto default_key() const { return std::tie(this->time, number, value); }
};

template<TType T>
struct TimeSignature : TimeStamp<T> {
    TimeSignature() = default;
    TimeSignature(auto time, auto numerator, auto denominator) :
        TimeStamp<T>{detail::checked_event_field<TimeSignature, 0>(time)},
        numerator(detail::checked_event_field<TimeSignature, 1>(std::move(numerator))),
        denominator(detail::checked_event_field<TimeSignature, 2>(std::move(denominator))) {}

    [[nodiscard]] TimeSignature copy() const { return *this; }
    bool operator==(const TimeSignature&) const = default;
    [[nodiscard]] std::string to_string() const;

    [[nodiscard]] static TimeSignature deserialize(std::span<const u8> bytes);
    [[nodiscard]] vec<u8> serialize() const;

    u8 numerator{4}, denominator{4};

    auto default_key() const { return this->time; }
};

template<TType T>
struct KeySignature : TimeStamp<T> {
    KeySignature() = default;
    KeySignature(auto time, auto key, auto minor) :
        TimeStamp<T>{detail::checked_event_field<KeySignature, 0>(time)},
        key(detail::checked_event_field<KeySignature, 1>(std::move(key))),
        minor(detail::checked_event_field<KeySignature, 2>(std::move(minor))) {}

    [[nodiscard]] KeySignature copy() const { return *this; }
    bool operator==(const KeySignature&) const = default;
    [[nodiscard]] std::string to_string() const;

    [[nodiscard]] static KeySignature deserialize(std::span<const u8> bytes);
    [[nodiscard]] vec<u8> serialize() const;

    i8 key{0};
    bool minor{false};

    [[nodiscard]] u8 degree() const {
        const int pitch_class = ((key * 5) % 12 + 12) % 12;
        // The pitch class is in [0, 11], with an offset of 12 for minor.
        return static_cast<u8>(pitch_class + (minor ? 12 : 0));
    }

    auto default_key() const { return this->time; }
};

template<TType T>
struct Tempo : TimeStamp<T> {
    Tempo() = default;
    Tempo(auto time, auto microseconds_per_quarter) :
        TimeStamp<T>{detail::checked_event_field<Tempo, 0>(time)},
        microseconds_per_quarter(detail::checked_event_field<Tempo, 1>(std::move(microseconds_per_quarter))) {}

    [[nodiscard]] Tempo copy() const { return *this; }
    bool operator==(const Tempo&) const = default;
    [[nodiscard]] std::string to_string() const;

    [[nodiscard]] static Tempo deserialize(std::span<const u8> bytes);
    [[nodiscard]] vec<u8> serialize() const;
    // store microseconds_per_quarter (i32) instead of qpm (f64) to avoid floating point error
    i32 microseconds_per_quarter{500000};   // microseconds per quarter note

    // qpm must be positive and finite, and the rounded microseconds_per_quarter must be in [1, INT32_MAX].
    static i32 qpm_to_microseconds_per_quarter(const f64 qpm) {
        if (!(qpm > 0.) || std::isinf(qpm)) {
            throw std::invalid_argument("qpm must be positive and finite");
        }
        const i32 microseconds_per_quarter = detail::round_to_i32(60000000. / qpm);
        if (microseconds_per_quarter < 1) { throw std::invalid_argument("qpm is too large: microseconds_per_quarter would be zero"); }
        return microseconds_per_quarter;
    }

    static constexpr f64 microseconds_per_quarter_to_qpm(const i32 microseconds_per_quarter) { return 60000000. / static_cast<f64>(microseconds_per_quarter); }

    // create a tempo event from qpm
    [[nodiscard]] static Tempo from_qpm(const typename T::unit time, const f64 qpm) {
        return Tempo{time, qpm_to_microseconds_per_quarter(qpm)};
    }

    // return qpm based on microseconds_per_quarter
    [[nodiscard]] f64 qpm() const { return microseconds_per_quarter_to_qpm(microseconds_per_quarter); }

    // set microseconds_per_quarter based on qpm
    Tempo& set_qpm(const f64 t_qpm) {
        microseconds_per_quarter = qpm_to_microseconds_per_quarter(t_qpm);
        return *this;
    }

    auto default_key() const { return std::tie(this->time); }
};

template<TType T>
struct PitchBend : TimeStamp<T> {
    PitchBend() = default;
    PitchBend(auto time, auto value) :
        TimeStamp<T>{detail::checked_event_field<PitchBend, 0>(time)},
        value(detail::checked_event_field<PitchBend, 1>(std::move(value))) {}

    [[nodiscard]] PitchBend copy() const { return *this; }
    bool operator==(const PitchBend&) const = default;
    [[nodiscard]] std::string to_string() const;

    [[nodiscard]] static PitchBend deserialize(std::span<const u8> bytes);
    [[nodiscard]] vec<u8> serialize() const;

    i32 value{0};

    auto default_key() const { return this->time; }
};

template<TType T>
struct TextMeta : TimeStamp<T> {
    TextMeta() = default;
    TextMeta(auto time, auto text) :
        TimeStamp<T>{detail::checked_event_field<TextMeta, 0>(time)},
        text(detail::checked_event_field<TextMeta, 1>(std::move(text))) {}

    [[nodiscard]] TextMeta copy() const { return *this; }
    bool operator==(const TextMeta&) const = default;
    [[nodiscard]] std::string to_string() const;

    [[nodiscard]] static TextMeta deserialize(std::span<const u8> bytes);
    [[nodiscard]] vec<u8> serialize() const;

    std::string text{};

    auto default_key() const { return this->time; }
};

// Shift one event in its own time unit. Tick overflow leaves the event unchanged.
template<TimeEvent E>
void shift_time(E& event, typename E::unit offset) {
    event.time = detail::add_time(event.time, offset);
}

namespace detail {

template<TimeEvent E, typename... Fields>
[[nodiscard]] E make_event(auto time, Fields&&... fields) {
    return E(time, std::forward<Fields>(fields)...);
}

}  // namespace detail

}  // namespace symusic

#include "symusic/detail/event_fields.h"

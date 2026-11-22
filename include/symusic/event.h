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
    using unit  = typename T::unit;
    unit time   = 0;

    bool operator==(const TimeStamp&) const = default;
};

// Define TimeEvent concept, representing events with time
template<typename T>
concept TimeEvent = std::is_base_of_v<TimeStamp<typename T::ttype>, T>;

template<typename T>
concept HashDuration = requires(T t) {
    { t.duration } -> std::convertible_to<typename T::unit>;
};

template<TType T>
struct Note : TimeStamp<T> {
    bool                      operator==(const Note&) const = default;
    [[nodiscard]] std::string to_string() const;

    template<DataFormat F>
    [[nodiscard]] static Note parse(std::span<const u8> bytes);
    template<DataFormat F>
    [[nodiscard]] vec<u8> dumps() const;

    typename T::unit duration{0};
    i8               pitch{0}, velocity{0};

    [[nodiscard]] typename T::unit start() const { return this->time; }
    [[nodiscard]] typename T::unit end() const { return detail::add_time(this->time, duration); }

    [[nodiscard]] bool empty() const { return duration <= 0 || velocity <= 0; }

    auto default_key() const { return std::tie(this->time, duration, pitch, velocity); }

    Note  shift_pitch(i8 offset) const;
    Note& shift_pitch_inplace(i8 offset);

    Note  shift_velocity(i8 offset) const;
    Note& shift_velocity_inplace(i8 offset);
};

template<TType T>
struct Pedal : TimeStamp<T> {
    bool                      operator==(const Pedal&) const = default;
    [[nodiscard]] std::string to_string() const;

    template<DataFormat F>
    [[nodiscard]] static Pedal parse(std::span<const u8> bytes);
    template<DataFormat F>
    [[nodiscard]] vec<u8> dumps() const;

    typename T::unit duration{};

    [[nodiscard]] typename T::unit start() const { return this->time; }
    [[nodiscard]] typename T::unit end() const { return detail::add_time(this->time, duration); }

    auto default_key() const { return std::tie(this->time, duration); }
};

template<TType T>
struct ControlChange : TimeStamp<T> {
    bool                      operator==(const ControlChange&) const = default;
    [[nodiscard]] std::string to_string() const;

    template<DataFormat F>
    [[nodiscard]] static ControlChange parse(std::span<const u8> bytes);
    template<DataFormat F>
    [[nodiscard]] vec<u8> dumps() const;

    u8 number{0}, value{0};

    auto default_key() const { return std::tie(this->time, number, value); }
};

template<TType T>
struct TimeSignature : TimeStamp<T> {
    bool                      operator==(const TimeSignature&) const = default;
    [[nodiscard]] std::string to_string() const;

    template<DataFormat F>
    [[nodiscard]] static TimeSignature parse(std::span<const u8> bytes);
    template<DataFormat F>
    [[nodiscard]] vec<u8> dumps() const;

    u8 numerator{4}, denominator{4};

    auto default_key() const { return this->time; }
};

template<TType T>
struct KeySignature : TimeStamp<T> {
    bool                      operator==(const KeySignature&) const = default;
    [[nodiscard]] std::string to_string() const;

    template<DataFormat F>
    [[nodiscard]] static KeySignature parse(std::span<const u8> bytes);
    template<DataFormat F>
    [[nodiscard]] vec<u8> dumps() const;

    i8 key{0}, tonality{0};

    [[nodiscard]] u8 degree() const {
        const int pitch_class = ((key * 5) % 12 + 12) % 12;
        // The pitch class is in [0, 11], with an offset of 12 for minor.
        return static_cast<u8>(pitch_class + (tonality != 0 ? 12 : 0));
    }

    auto default_key() const { return this->time; }
};

template<TType T>
struct Tempo : TimeStamp<T> {
    bool                      operator==(const Tempo&) const = default;
    [[nodiscard]] std::string to_string() const;

    template<DataFormat F>
    [[nodiscard]] static Tempo parse(std::span<const u8> bytes);
    template<DataFormat F>
    [[nodiscard]] vec<u8> dumps() const;
    // store mspq (i32) instead of qpm (f64) to avoid floating point error
    i32 mspq{};   // microseconds per quarter note

    // qpm must be positive and finite, and the rounded mspq must be in [1, INT32_MAX].
    static i32 qpm2mspq(const f64 qpm) {
        if (!(qpm > 0.) || std::isinf(qpm)) {
            throw std::invalid_argument("qpm must be positive and finite");
        }
        const i32 mspq = detail::round_to_i32(60000000. / qpm);
        if (mspq < 1) { throw std::invalid_argument("qpm is too large: mspq would be zero"); }
        return mspq;
    }

    static constexpr f64 mspq2qpm(const i32 mspq) { return 60000000. / static_cast<f64>(mspq); }

    // create a tempo event from qpm
    [[nodiscard]] static Tempo from_qpm(const typename T::unit time, const f64 qpm) {
        return Tempo{{time}, qpm2mspq(qpm)};
    }

    // return qpm based on mspq
    [[nodiscard]] f64 qpm() const { return mspq2qpm(mspq); }

    // set mspq based on qpm
    Tempo& set_qpm(const f64 t_qpm) {
        mspq = qpm2mspq(t_qpm);
        return *this;
    }

    auto default_key() const { return std::tie(this->time); }
};

template<TType T>
struct PitchBend : TimeStamp<T> {
    bool                      operator==(const PitchBend&) const = default;
    [[nodiscard]] std::string to_string() const;

    template<DataFormat F>
    [[nodiscard]] static PitchBend parse(std::span<const u8> bytes);
    template<DataFormat F>
    [[nodiscard]] vec<u8> dumps() const;

    i32 value{0};

    auto default_key() const { return this->time; }
};

template<TType T>
struct TextMeta : TimeStamp<T> {
    bool                      operator==(const TextMeta&) const = default;
    [[nodiscard]] std::string to_string() const;

    template<DataFormat F>
    [[nodiscard]] static TextMeta parse(std::span<const u8> bytes);
    template<DataFormat F>
    [[nodiscard]] vec<u8> dumps() const;

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
[[nodiscard]] E make_event(typename E::unit time, Fields&&... fields) {
    return E(TimeStamp<typename E::ttype>{time}, std::forward<Fields>(fields)...);
}

}   // namespace detail

}   // namespace symusic

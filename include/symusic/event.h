#pragma once

#ifndef LIBSYMUSIC_EVENT_H
#define LIBSYMUSIC_EVENT_H

#include <string>
#include <span>
#include <cmath>

#include "MetaMacro.h"

#include "symusic/mtype.h"
#include "symusic/io/iodef.h"
#include "symusic/time_unit.h"

namespace symusic {

// Default Methods Macro
#define COMPILER_DEFAULT_METHODS(T)                             \
    T() = default;                                              \
    T(const T&) = default;                                      \
    T copy() const { return {*this}; }                          \
    T& operator=(const T&) = default;                           \
    bool operator==(const T& other) const = default;            \
    bool operator!=(const T& other) const = default;

// Define Basic Declarations for TimeStamp
#define BASIC_TIMESTAMP_METHODS(EVENT, T)                       \
    COMPILER_DEFAULT_METHODS(EVENT)                             \
    typedef T ttype;                                            \
    typedef typename T::unit unit;                              \
    [[nodiscard]] std::string to_string() const;                \
    template<DataFormat F>                                      \
    [[nodiscard]] static EVENT parse(std::span<const u8> bytes);\
    template<DataFormat F>                                      \
    [[nodiscard]] vec<u8> dumps() const;                        \
    EVENT shift_time(unit offset) const;                        \
    EVENT& shift_time_inplace(unit offset);


/*
 *  List of all the events (based on TimeStamp):
 *  - Note(duration: unit, pitch: i8, velocity: i8)
 *  - Pedal(duration: unit)
 *  - ControlChange(number: u8, value: u8)
 *  - TimeSignature(numerator: u8, denominator: u8)
 *  - KeySignature(key: i8, tonality: u8)
 *  - Tempo(mspq: i32)
 *  - PitchBend(value: i32)
 *  - TextMeta(text: string)
 */

// Define TimeStamp for events like Note
template<TType T>
struct TimeStamp {
    typedef T ttype;
    typedef typename T::unit unit;
    unit time;

    COMPILER_DEFAULT_METHODS(TimeStamp)
    explicit TimeStamp(const unit &time) : time(time) {};
};

// Define TimeEvent concept, representing events with time
template<typename T>
concept TimeEvent = std::is_base_of_v<TimeStamp<typename T::ttype>, T>;

template<typename T>
concept HashDuration = requires(T t) {
    { t.duration } -> std::convertible_to<typename T::unit>;
};

template<TType T>
struct Note: TimeStamp<T> {
    BASIC_TIMESTAMP_METHODS(Note, T);
    unit duration{0};
    i8 pitch{0}, velocity{0};

    Note(const unit time, const unit duration, const i8 pitch, const i8 velocity):
        TimeStamp<T>{time}, duration{duration}, pitch{pitch}, velocity{velocity} {}

    template<TType U>
    Note(const unit time, const unit duration, const Note<U> & other):
        TimeStamp<T>{time}, duration{duration}, pitch{other.pitch}, velocity{other.velocity} {}

    [[nodiscard]] unit start() const { return this->time; }
    [[nodiscard]] unit end() const { return this->time + duration; }

    [[nodiscard]] bool empty() const { return duration <= 0 || velocity <= 0; }

    Note shift_pitch(i8 offset) const;
    Note& shift_pitch_inplace(i8 offset);

    Note shift_velocity(i8 offset) const;
    Note& shift_velocity_inplace(i8 offset);
};

template<TType T>
struct Pedal: TimeStamp<T> {
    BASIC_TIMESTAMP_METHODS(Pedal, T);
    unit duration;

    Pedal(const unit time, const unit duration):
        TimeStamp<T>{time}, duration{duration} {}

    template<class U>
    Pedal(const unit time, const unit duration, const Pedal<U> & other):
        TimeStamp<T>{time}, duration{duration} {}

    [[nodiscard]] unit start() const { return this->time; }
    [[nodiscard]] unit end() const { return this->time + duration; }

};

template<TType T>
struct ControlChange: TimeStamp<T> {
    BASIC_TIMESTAMP_METHODS(ControlChange, T);
    u8 number{0}, value{0};

    ControlChange(const unit time, const u8 number, const u8 value):
        TimeStamp<T>{time}, number{number}, value{value} {}

    template<class U>
    ControlChange(const unit time, const ControlChange<U> & other):
        TimeStamp<T>{time}, number{other.number}, value{other.value} {}
};

template<TType T>
struct TimeSignature: TimeStamp<T> {
    BASIC_TIMESTAMP_METHODS(TimeSignature, T);
    u8 numerator{4}, denominator{4};

    TimeSignature(const unit time, const u8 numerator, const u8 denominator):
        TimeStamp<T>{time}, numerator{numerator}, denominator{denominator} {}

    template<class U>
    TimeSignature(const unit time, const TimeSignature<U> & other):
        TimeStamp<T>{time}, numerator{other.numerator}, denominator{other.denominator} {}
};

template<TType T>
struct KeySignature: TimeStamp<T> {
    BASIC_TIMESTAMP_METHODS(KeySignature, T);
    i8 key{0}, tonality{0};

    KeySignature(const unit time, const i8 key, const i8 tonality):
        TimeStamp<T>{time}, key{key}, tonality{tonality} {}

    template<class U>
    KeySignature(const unit time, const KeySignature<U> & other):
        TimeStamp<T>{time}, key{other.key}, tonality{other.tonality} {}

    [[nodiscard]] u8 degree() const { return (key * 5) % 12 + tonality * 12; }
};

template<TType T>
struct Tempo: TimeStamp<T> {
    BASIC_TIMESTAMP_METHODS(Tempo, T);
    // store mspq (i32) instead of qpm (f64) to avoid floating point error
    i32 mspq{}; // microseconds per quarter note

    static constexpr i32 qpm2mspq(const f64 qpm) {
        return static_cast<i32>(std::round(60000000. / qpm));
    }

    static constexpr f64 mspq2qpm(const i32 mspq) {
        return 60000000. / static_cast<f64>(mspq);
    }

    Tempo(const unit time, const i32 mspq): TimeStamp<T>{time}, mspq{mspq} {}

    template<class U>
    Tempo(const unit time, const Tempo<U> & other): TimeStamp<T>{time}, mspq{other.mspq} {}

    // create a tempo event from qpm
    [[nodiscard]] static Tempo from_qpm(const unit time, const f64 qpm) {
        return Tempo(time, qpm2mspq(qpm));
    }

    // return qpm based on mspq
    [[nodiscard]] f64 qpm() const { return mspq2qpm(mspq); }

    // set mspq based on qpm
    Tempo& set_qpm(const f64 t_qpm) {
        mspq = qpm2mspq(t_qpm); return *this;
    }
};

template<TType T>
struct PitchBend: TimeStamp<T> {
    BASIC_TIMESTAMP_METHODS(PitchBend, T);
    i32 value{0};

    PitchBend(const unit time, const i32 value): TimeStamp<T>{time}, value{value} {}

    template<class U>
    PitchBend(const unit time, const PitchBend<U> & other): TimeStamp<T>{time}, value{other.value} {}
};

template<TType T>
struct TextMeta: TimeStamp<T> {
    BASIC_TIMESTAMP_METHODS(TextMeta, T);
    std::string text{};

    TextMeta(const unit time, std::string  text): TimeStamp<T>{time}, text{std::move(text)} {}

    template<class U>
    TextMeta(const unit time, const TextMeta<U> & other): TimeStamp<T>{time}, text{other.text} {}
};

#undef COMPILER_DEFAULT_METHODS
#undef BASIC_TIMESTAMP_METHODS

#define ERROR_PARSE_DUMPS(__COUNT, EVENT)                   \
    template<TType T> template<DataFormat>                  \
    EVENT<T> EVENT<T>::parse(std::span<const u8>) {         \
        static_assert(true, "Not Implemented"); return {};  \
    }                                                       \
    template<TType T> template<DataFormat>                  \
    vec<u8> EVENT<T>::dumps() const {                       \
        static_assert(true, "Not Implemented"); return {};  \
    }

// "Not Implemented" Error at compile time for parse and dumps
// include the header files corresponding to the specializations to avoid these errors
// REPEAT_ON(
//     ERROR_PARSE_DUMPS,
//     Note, Pedal, ControlChange, TimeSignature, KeySignature, Tempo, PitchBend, TextMeta
// )

#undef ERROR_PARSE_DUMPS

} // namespace symusic

#endif //LIBSYMUSIC_EVENT_H

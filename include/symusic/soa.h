//
// Created by nhy on 2024/1/8.
//

#ifndef LIBSYMUSIC_SOA_H
#define LIBSYMUSIC_SOA_H

#include "symusic/event.h"
#include "MetaMacro.h"
namespace symusic {
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

// Now, define the Struct of Array for all kinds of events

#define RESERVE(__COUNT, NAME) NAME.reserve(_size);

template<TType T>
struct NoteArr {
    typedef typename T::unit unit;
    typedef T ttype;

    vec<unit> time, duration;
    vec<i8> pitch, velocity;

    void reserve(const size_t _size) {
        REPEAT_ON(RESERVE, time, duration, pitch, velocity)
    }

    void emplace_back(const unit _time, const unit _duration, const i8 _pitch, const i8 _velocity) {
        time.emplace_back(_time);
        duration.emplace_back(_duration);
        pitch.emplace_back(_pitch);
        velocity.emplace_back(_velocity);
    }

    void emplace_back(const Note<T> &note) {
        emplace_back(note.time, note.duration, note.pitch, note.velocity);
    }

    explicit NoteArr(std::span<const Note<T>> data) {
        reserve(data.size());
        for (const auto &note: data) emplace_back(note);
    }

    vec<Note<T>> to_vec() const {
        vec<Note<T>> ans;
        ans.reserve(time.size());
        for (size_t i = 0; i < time.size(); ++i) {
            ans.emplace_back(time[i], duration[i], pitch[i], velocity[i]);
        }
        return ans;
    }

    [[nodiscard]] size_t size() const {
        return time.size();
    }
};

template<TType T>
struct PedalArr {
    typedef typename T::unit unit;
    typedef T ttype;

    vec<unit> time, duration;

    void reserve(const size_t _size) {
        REPEAT_ON(RESERVE, time, duration)
    }

    void emplace_back(const unit _time, const unit _duration) {
        time.emplace_back(_time);
        duration.emplace_back(_duration);
    }

    void emplace_back(const Pedal<T> &pedal) {
        emplace_back(pedal.time, pedal.duration);
    }

    explicit PedalArr(std::span<const Pedal<T>> data) {
        reserve(data.size());
        for (const auto &pedal: data) emplace_back(pedal);
    }

    vec<Pedal<T>> to_vec() const {
        vec<Pedal<T>> ans;
        ans.reserve(time.size());
        for (size_t i = 0; i < time.size(); ++i) {
            ans.emplace_back(time[i], duration[i]);
        }
        return ans;
    }

    [[nodiscard]] size_t size() const {
        return time.size();
    }
};

template<TType T>
struct ControlChangeArr {
    typedef typename T::unit unit;
    typedef T ttype;

    vec<unit> time;
    vec<u8> number, value;

    void reserve(const size_t _size) {
        REPEAT_ON(RESERVE, time, number, value)
    }

    void emplace_back(const unit _time, const u8 _number, const u8 _value) {
        time.emplace_back(_time);
        number.emplace_back(_number);
        value.emplace_back(_value);
    }

    void emplace_back(const ControlChange<T> &control_change) {
        emplace_back(control_change.time, control_change.number, control_change.value);
    }

    explicit ControlChangeArr(std::span<const ControlChange<T>> data) {
        reserve(data.size());
        for (const auto &control_change: data) emplace_back(control_change);
    }

    vec<ControlChange<T>> to_vec() const {
        vec<ControlChange<T>> ans;
        ans.reserve(time.size());
        for (size_t i = 0; i < time.size(); ++i) {
            ans.emplace_back(time[i], number[i], value[i]);
        }
        return ans;
    }

    [[nodiscard]] size_t size() const {
        return time.size();
    }
};

template<TType T>
struct TimeSignatureArr {
    typedef typename T::unit unit;
    typedef T ttype;

    vec<unit> time;
    vec<u8> numerator, denominator;

    void reserve(const size_t _size) {
        REPEAT_ON(RESERVE, time, numerator, denominator)
    }

    void emplace_back(const unit _time, const u8 _numerator, const u8 _denominator) {
        time.emplace_back(_time);
        numerator.emplace_back(_numerator);
        denominator.emplace_back(_denominator);
    }

    void emplace_back(const TimeSignature<T> &time_signature) {
        emplace_back(time_signature.time, time_signature.numerator, time_signature.denominator);
    }

    explicit TimeSignatureArr(std::span<const TimeSignature<T>> data) {
        reserve(data.size());
        for (const auto &time_signature: data) emplace_back(time_signature);
    }

    vec<TimeSignature<T>> to_vec() const {
        vec<TimeSignature<T>> ans;
        ans.reserve(time.size());
        for (size_t i = 0; i < time.size(); ++i) {
            ans.emplace_back(time[i], numerator[i], denominator[i]);
        }
        return ans;
    }

    [[nodiscard]] size_t size() const {
        return time.size();
    }
};

template<TType T>
struct KeySignatureArr {
    typedef typename T::unit unit;
    typedef T ttype;

    vec<unit> time;
    vec<i8> key;
    vec<u8> tonality;

    void reserve(const size_t _size) {
        REPEAT_ON(RESERVE, time, key, tonality)
    }

    void emplace_back(const unit _time, const i8 _key, const u8 _tonality) {
        time.emplace_back(_time);
        key.emplace_back(_key);
        tonality.emplace_back(_tonality);
    }

    void emplace_back(const KeySignature<T> &key_signature) {
        emplace_back(key_signature.time, key_signature.key, key_signature.tonality);
    }

    explicit KeySignatureArr(std::span<const KeySignature<T>> data) {
        reserve(data.size());
        for (const auto &key_signature: data) emplace_back(key_signature);
    }

    vec<KeySignature<T>> to_vec() const {
        vec<KeySignature<T>> ans;
        ans.reserve(time.size());
        for (size_t i = 0; i < time.size(); ++i) {
            ans.emplace_back(time[i], key[i], tonality[i]);
        }
        return ans;
    }

    [[nodiscard]] size_t size() const {
        return time.size();
    }
};

template<TType T>
struct TempoArr {
    typedef typename T::unit unit;
    typedef T ttype;

    vec<unit> time;
    vec<i32> mspq;

    void reserve(const size_t _size) {
        REPEAT_ON(RESERVE, time, mspq)
    }

    void emplace_back(const unit _time, const i32 _mspq) {
        time.emplace_back(_time);
        mspq.emplace_back(_mspq);
    }

    void emplace_back(const Tempo<T> &tempo) {
        emplace_back(tempo.time, tempo.mspq);
    }

    explicit TempoArr(std::span<const Tempo<T>> data) {
        reserve(data.size());
        for (const auto &tempo: data) emplace_back(tempo);
    }

    vec<Tempo<T>> to_vec() const {
        vec<Tempo<T>> ans;
        ans.reserve(time.size());
        for (size_t i = 0; i < time.size(); ++i) {
            ans.emplace_back(time[i], mspq[i]);
        }
        return ans;
    }

    [[nodiscard]] size_t size() const {
        return time.size();
    }
};

template<TType T>
struct PitchBendArr {
    typedef typename T::unit unit;
    typedef T ttype;

    vec<unit> time;
    vec<i32> value;

    void reserve(const size_t _size) {
        REPEAT_ON(RESERVE, time, value)
    }

    void emplace_back(const unit _time, const i32 _value) {
        time.emplace_back(_time);
        value.emplace_back(_value);
    }

    void emplace_back(const PitchBend<T> &pitch_bend) {
        emplace_back(pitch_bend.time, pitch_bend.value);
    }

    explicit PitchBendArr(std::span<const PitchBend<T>> data) {
        reserve(data.size());
        for (const auto &pitch_bend: data) emplace_back(pitch_bend);
    }

    vec<PitchBend<T>> to_vec() const {
        vec<PitchBend<T>> ans;
        ans.reserve(time.size());
        for (size_t i = 0; i < time.size(); ++i) {
            ans.emplace_back(time[i], value[i]);
        }
        return ans;
    }

    [[nodiscard]] size_t size() const {
        return time.size();
    }
};

template<TType T>
struct TextMetaArr {
    typedef typename T::unit unit;
    typedef T ttype;

    vec<unit> time;
    vec<std::string> text;

    void reserve(const size_t _size) {
        REPEAT_ON(RESERVE, time, text)
    }

    void emplace_back(const unit _time, const std::string &_text) {
        time.emplace_back(_time);
        text.emplace_back(_text);
    }

    void emplace_back(const TextMeta<T> &text_meta) {
        emplace_back(text_meta.time, text_meta.text);
    }

    explicit TextMetaArr(std::span<const TextMeta<T>> data) {
        reserve(data.size());
        for (const auto &text_meta: data) emplace_back(text_meta);
    }

    vec<TextMeta<T>> to_vec() const {
        vec<TextMeta<T>> ans;
        ans.reserve(time.size());
        for (size_t i = 0; i < time.size(); ++i) {
            ans.emplace_back(time[i], text[i]);
        }
        return ans;
    }

    [[nodiscard]] size_t size() const {
        return time.size();
    }
};

#undef RESERVE
}
#endif //LIBSYMUSIC_SOA_H

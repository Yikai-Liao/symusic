//
// Created by lyk on 2023/12/24.
//

#ifndef LIBSYMUSIC_NOTE_ARR_H
#define LIBSYMUSIC_NOTE_ARR_H

#include <string>
#include <span>

#include "symusic/mtype.h"
#include "symusic/io/iodef.h"
#include "symusic/time_unit.h"
#include "symusic/event.h"

namespace symusic {

template<TType T>
struct NoteArr {
    // NoteArr is a "Struct of Arrays" (SoA) data structure
    // It contains note part information in Track

    typedef T ttype;
    typedef typename T::unit unit;

    std::string name;
    u8 program = 0;
    bool is_drum = false;

    vec<unit> time, duration;
    vec<i8> pitch, velocity;

    NoteArr() = default;

    NoteArr(const NoteArr &) = default;

    [[nodiscard]] NoteArr copy() const { return {*this}; }

    NoteArr &operator=(const NoteArr &) = default;

    NoteArr(
        std::string name, const u8 program, const bool is_drum, const size_t note_num = 0
    ) : name{std::move(name)}, program{program}, is_drum{is_drum},
        time(note_num), duration(note_num), pitch(note_num), velocity(note_num) {}

    NoteArr(
        std::string name, const u8 program, const bool is_drum,
        const vec<unit> &time, const vec<unit> &duration,
        const vec<i8> &pitch, const vec<i8> &velocity
    ) : name{std::move(name)}, program{program}, is_drum{is_drum},
        time{time}, duration{duration}, pitch{pitch}, velocity{velocity} {}

    template<DataFormat F>
    [[nodiscard]] static NoteArr parse(std::span<const u8> bytes);

    template<DataFormat F>
    [[nodiscard]] vec<u8> dumps() const;

    // return the start time of the track
    [[nodiscard]] unit start() const;

    // return the end time of the track
    [[nodiscard]] unit end() const;

    // return the number of notes in the NoteArr
    [[nodiscard]] size_t note_num() const;

    // return true if the NoteArr is empty
    // Note that empty does not guarantee valid
    [[nodiscard]] bool empty() const;

    // return true if the NoteArr is valid, i.e. get the same size for all the arrays
    [[nodiscard]] bool valid() const;

    // return a string representation of the NoteArr, same as summary
    [[nodiscard]] std::string to_string() const;

    // summary info of the NoteArr
    [[nodiscard]] std::string summary() const;

    // non-inplace sort, return a new NoteArr
    [[nodiscard]] NoteArr sort(bool reverse = false);

    // Clip all the events in the NoteArr, non-inplace, return a new NoteArr
    // clip_end is used to determine whether to clip based on end time.
    [[nodiscard]] NoteArr clip(unit start, unit end, bool clip_end = false) const;

    void reserve(size_t size);

    void emplace_back(unit time, unit duration, i8 pitch, i8 velocity);

    void push_back(const Note<T> & note);
};

// template<TType T> template<DataFormat>
// NoteArr<T> NoteArr<T>::parse(std::span<const u8>) {
//     static_assert(true, "Not implemented"); return {};
// }
//
// template<TType T> template<DataFormat>
// vec<u8> NoteArr<T>::dumps() const {
//     static_assert(true, "Not implemented"); return {};
// }

}

#endif //LIBSYMUSIC_NOTE_ARR_H

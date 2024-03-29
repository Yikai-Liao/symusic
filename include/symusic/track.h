//
// Created by lyk on 2023/12/24.
//
#pragma once

#ifndef LIBSYMUSIC_TRACK_H
#define LIBSYMUSIC_TRACK_H

#include <span>
#include "symusic/mtype.h"
#include "symusic/io/iodef.h"
#include "symusic/event.h"


namespace symusic {
template<TType T>
struct Track {
    typedef T ttype;
    typedef typename T::unit unit;

    std::string name;
    u8 program = 0;
    bool is_drum = false;
    shared<vec<shared<Note<T>>>>            notes;
    shared<vec<shared<ControlChange<T>>>>   controls;
    shared<vec<shared<PitchBend<T>>>>       pitch_bends;
    shared<vec<shared<Pedal<T>>>>           pedals;

    POINTER_METHODS(Track)

    Track(): name{""}, program{0}, is_drum{false} {
        notes = std::make_shared<vec<shared<Note<T>>>>();
        controls = std::make_shared<vec<shared<ControlChange<T>>>>();
        pitch_bends = std::make_shared<vec<shared<PitchBend<T>>>>();
        pedals = std::make_shared<vec<shared<Pedal<T>>>>();
    }

    Track(const Track &) = default;

    Track(std::string name, const u8 program, const bool is_drum):
        name{std::move(name)}, program{program}, is_drum{is_drum} {
        notes = std::make_shared<vec<shared<Note<T>>>>();
        controls = std::make_shared<vec<shared<ControlChange<T>>>>();
        pitch_bends = std::make_shared<vec<shared<PitchBend<T>>>>();
        pedals = std::make_shared<vec<shared<Pedal<T>>>>();
    }

    void move_other(Track && other) {
        name = std::move(other.name);
        program = other.program;
        is_drum = other.is_drum;
        notes = std::move(other.notes);
        controls = std::move(other.controls);
        pitch_bends = std::move(other.pitch_bends);
        pedals = std::move(other.pedals);
    }

    Track(Track && other) noexcept { move_other(std::move(other));}

    Track(
        std::string name,const u8 program, const bool is_drum,
        const vec<Note<T>> & notes, const vec<ControlChange<T>> & controls,
        const vec<PitchBend<T>> & pitch_bends, const vec<Pedal<T>> & pedals
    ): name{std::move(name)}, program{program}, is_drum{is_drum} {
        this->notes = std::make_shared<vec<shared<Note<T>>>>(notes);
        this->controls = std::make_shared<vec<shared<ControlChange<T>>>>(controls);
        this->pitch_bends = std::make_shared<vec<shared<PitchBend<T>>>>(pitch_bends);
        this->pedals = std::make_shared<vec<shared<Pedal<T>>>>(pedals);
    }

    Track(
        std::string name, const u8 program, const bool is_drum,
        shared<vec<shared<Note<T>>>> notes, shared<vec<shared<ControlChange<T>>> > controls,
        shared<vec<shared<PitchBend<T>>>> pitch_bends, shared<vec<shared<Pedal<T>>>> pedals
    ): name{std::move(name)}, program{program}, is_drum{is_drum},
       notes{std::move(notes)}, controls{std::move(controls)},
       pitch_bends{std::move(pitch_bends)}, pedals{std::move(pedals)} {}

    // shallow copy
    [[nodiscard]] Track copy() const { return {*this}; }

    [[nodiscard]] Track deepcopy() const {
        return {
            name, program, is_drum,
            *notes, *controls, *pitch_bends, *pedals
        };
    }

    Track& operator=(const Track &) = default;
    Track& operator=(Track && other) noexcept {
        move_other(std::move(other)); return *this;
    }

    bool operator==(const Track & other) const {
        return name == other.name && program == other.program && is_drum == other.is_drum &&
            *notes == *other.notes && *controls == *other.controls &&
            *pitch_bends == *other.pitch_bends && *pedals == *other.pedals;
    };

    bool operator!=(const Track & other) const { return !(*this == other); }

    template<DataFormat F>
    [[nodiscard]] static Track parse(std::span<const u8> bytes);

    template<DataFormat F>
    [[nodiscard]] vec<u8> dumps() const;


    // return the start time of the track
    [[nodiscard]] unit start() const;

    // return the end time of the track
    [[nodiscard]] unit end() const;

    // return the number of notes in the track
    [[nodiscard]] size_t note_num() const;

    // return true if the track is empty
    [[nodiscard]] bool empty() const;

    // return a string representation of the track, same as summary
    [[nodiscard]] std::string to_string() const;

    // summary info of the track
    [[nodiscard]] std::string summary() const;

    // non-inplace sort, return a new Track
    [[nodiscard]] Track sort(bool reverse = false);

    // inplace sort, and return self reference
    void sort_inplace(bool reverse = false);

    // Clip all the events in the track, non-inplace, return a new Track
    // For events with duration, clip_end is used to determine whether to clip based on end time.
    void clip_inplace(unit start, unit end, bool clip_end = false);
    [[nodiscard]] Track clip(unit start, unit end, bool clip_end = false) const;

    // shift the time of all the events in the track, non-inplace, return a new Track
    [[nodiscard]] Track shift_time(unit offset) const;

    // shift the time of all the events in the track, inplace, return self reference
    void shift_time_inplace(unit offset);

    // shift the pitch of all notes in the track, non-inplace, return a new Track
    [[nodiscard]] Track shift_pitch(i8 offset) const;

    // shift the pitch of all notes in the track, inplace, return self reference
    void shift_pitch_inplace(i8 offset);

    // shift the velocity of all notes in the track, non-inplace, return a new Track
    [[nodiscard]] Track shift_velocity(i8 offset) const;

    // shift the velocity of all notes in the track, inplace, return self reference
    void shift_velocity_inplace(i8 offset);
};


}

#endif //LIBSYMUSIC_TRACK_H

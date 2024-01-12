//
// Created by lyk on 23-12-13.
//
#pragma once

#ifndef LIBSYMUSIC_BATCH_OPS_H
#define LIBSYMUSIC_BATCH_OPS_H

#include "symusic/event.h"
#include "pdqsort.h"
// #include "algorithm"

namespace symusic::ops {

template<typename T>
void sort_by_time(vec<T> & data, const bool reverse = false) {
    if (reverse) {
        pdqsort_branchless(data.begin(), data.end(), [](const T & a, const T & b) {return (a.time) > (b.time);});
    } else {
        pdqsort_branchless(data.begin(), data.end(), [](const T & a, const T & b) {return (a.time) < (b.time);});
    }
}

template<TType T>
void sort_notes(vec<Note<T>> & notes, const bool reverse = false) {
    #define KEY(NOTE) std::tie(NOTE.time, NOTE.duration, NOTE.pitch, NOTE.velocity)
    if (reverse) {
        pdqsort_branchless(notes.begin(), notes.end(), [](const Note<T> & a, const Note<T> & b) {return KEY(a) > KEY(b);});
    } else {
        pdqsort_branchless(notes.begin(), notes.end(), [](const Note<T> & a, const Note<T> & b) {return KEY(a) < KEY(b);});
    }
    #undef KEY
}

template<TType T>
void sort_pedals(vec<Pedal<T>> & pedals, const bool reverse = false) {
    #define KEY(PEDAL) std::tie(PEDAL.time, PEDAL.duration)
    if (reverse) {
        pdqsort_branchless(pedals.begin(), pedals.end(), [](const Pedal<T> & a, const Pedal<T> & b) {return KEY(a) > KEY(b);});
    } else {
        pdqsort_branchless(pedals.begin(), pedals.end(), [](const Pedal<T> & a, const Pedal<T> & b) {return KEY(a) < KEY(b);});
    }
    #undef KEY
}

template<TType T>
void sort_tracks(vec<Track<T>> & tracks, const bool reverse = false) {
    #define KEY(TRACK) std::make_tuple(TRACK.is_drum, TRACK.program, TRACK.name, TRACK.note_num())
    if (reverse) {
        pdqsort_branchless(tracks.begin(), tracks.end(), [](const Track<T> & a, const Track<T> & b) {return KEY(a) > KEY(b);});
    } else {
        pdqsort_branchless(tracks.begin(), tracks.end(), [](const Track<T> & a, const Track<T> & b) {return KEY(a) < KEY(b);});
    }
    #undef KEY
}


template<class Iter, class Compare>
void sort(Iter begin, Iter end, Compare cmp) {
    pdqsort_branchless(begin, end, cmp);
}

template<typename T, class Compare>
void sort(vec<T> & data, Compare cmp){
    pdqsort_branchless(data.begin(), data.end(), cmp);
}

template<class T, class FILTER>
vec<T> filter(const vec<T>& data, FILTER t_fiter) {
    if (data.empty()) return {};
    vec<T> new_data;
    new_data.reserve(data.size());
    std::copy_if(
        data.begin(), data.end(),
        std::back_inserter(new_data), t_fiter
    );
    new_data.shrink_to_fit();
    return new_data;
}

template<TimeEvent T> // used for general events, with time
vec<T> clip(const vec<T>& events, typename T::unit start, typename T::unit end) {
    auto func = [start, end](const T &event) {
        return ((event.time) >= start) && ((event.time) < end);
    };  return filter(events, func);
}

template<TimeEvent T> // used for events with duration (e.g. Note and Pedal)
vec<T> clip(const vec<T>& events, typename T::unit start, typename T::unit end, const bool clip_end) {
    if (clip_end) {
        auto func = [start, end](const T &event) {
            return (event.time >= start) && (event.time + event.duration <= end);
        };  return filter(events, func);
    }   return clip(events, start, end);
}

// make sure T has time and duration fields using requires
template<TimeEvent T>
requires requires(T t) {{ t.duration } -> std::convertible_to<typename T::unit>;}
vec<T>& clamp_dur_inplace(vec<T>& events, typename T::unit min_dur, typename T::unit max_dur) {
    for(auto &event : events) {
        event.duration = std::clamp(event.duration, min_dur, max_dur);
    }   return events;
}

template<TimeEvent T>
requires requires(T t) {{ t.duration } -> std::convertible_to<typename T::unit>;}
vec<T> clamp_dur(const vec<T>& events, typename T::unit min_dur, typename T::unit max_dur) {
    vec<T> new_events(events);
    return clamp_dur_inplace(new_events, min_dur, max_dur);
}

} // namespace symusic::ops

#endif //LIBSYMUSIC_BATCH_OPS_H

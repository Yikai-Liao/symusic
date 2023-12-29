//
// Created by lyk on 23-12-13.
//

#ifndef LIBSYMUSIC_BATCH_OPS_H
#define LIBSYMUSIC_BATCH_OPS_H

#include <functional>

#include "symusic/event.h"
#include "pdqsort.h"
// #include "algorithm"

namespace symusic::ops {

template<typename T>
void sort_by_time(vec<T> & data, const bool reverse = false) {
    std::function<bool(const T&, const T &)> cmp;
    if (reverse) {
        cmp = [](const T & a, const T & b) {return (a.time) > (b.time);};
    } else {
        cmp = [](const T & a, const T & b) {return (a.time) < (b.time);};
    }
    pdqsort_branchless(data.begin(), data.end(), cmp);
}

template<TType T>
void sort_notes(vec<Note<T>> & notes, const bool reverse = false) {
    std::function<bool(const Note<T>&, const Note<T> &)> cmp;
    #define KEY(NOTE) std::tie(NOTE.time, NOTE.duration, NOTE.pitch, NOTE.velocity)
    if (reverse) {
        cmp = [](const Note<T> & a, const Note<T> & b) {return KEY(a) > KEY(b);};
    } else {
        cmp = [](const Note<T> & a, const Note<T> & b) {return KEY(a) < KEY(b);};
    }
    #undef KEY
    pdqsort_branchless(notes.begin(), notes.end(), cmp);
}

template<TType T>
void sort_pedals(vec<Pedal<T>> & pedals, const bool reverse = false) {
    std::function<bool(const Pedal<T>&, const Pedal<T> &)> cmp;
    #define KEY(PEDAL) std::tie(PEDAL.time, PEDAL.duration)
    if (reverse) {
        cmp = [](const Pedal<T> & a, const Pedal<T> & b) {return KEY(a) > KEY(b);};
    } else {
        cmp = [](const Pedal<T> & a, const Pedal<T> & b) {return KEY(a) < KEY(b);};
    }
    #undef KEY
    pdqsort_branchless(pedals.begin(), pedals.end(), cmp);
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

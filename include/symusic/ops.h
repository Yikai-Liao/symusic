//
// Created by lyk on 23-12-13.
//
#pragma once

#ifndef LIBSYMUSIC_BATCH_OPS_H
#define LIBSYMUSIC_BATCH_OPS_H

#include "symusic/event.h"
#include "symusic/score.h"
#include "pdqsort.h"
#include "pyvec.hpp"

#include <stdexcept>

namespace symusic::ops {

template<typename T, class Comp>
void sort(vec<T>& data, const bool reverse, Comp cmp) {
    if (reverse) {
        pdqsort_branchless(data.rpbegin(), data.rpend(), cmp);
    } else {
        pdqsort_branchless(data.pbegin(), data.pend(), cmp);
    }
}

// template<typename T>
// void sort_by_time(pyvec<T> & data, const bool reverse = false) {
//     auto cmp = [](const T & a, const T & b) {return (a->time) < (b->time);};
//     sort(data, reverse, cmp);
// }
//
// template<TType T>
// void sort_notes(vec<shared<Note<T>>> & notes, const bool reverse = false) {
//     #define KEY(NOTE) std::tie((NOTE)->time, (NOTE)->duration, (NOTE)->pitch, (NOTE)->velocity)
//     sort(notes, reverse, [](const auto & a, const auto & b) {return KEY(a) < KEY(b);});
//     #undef KEY
// }
//
// template<TType T>
// void sort_pedals(vec<shared<Pedal<T>>> & pedals, const bool reverse = false) {
//     #define KEY(PEDAL) std::tie((PEDAL)->time, (PEDAL)->duration)
//     sort(pedals, reverse, [](const auto & a, const auto & b) {return KEY(a) < KEY(b);});
//     #undef KEY
// }

template<TType T>
void sort_tracks(vec<shared<Track<T>>>& tracks, const bool reverse = false) {
#define KEY(TRACK) \
    std::make_tuple((TRACK)->is_drum, (TRACK)->program, (TRACK)->name, (TRACK)->note_num())
    sort(tracks, reverse, [](const auto& a, const auto& b) { return KEY(&a) < KEY(&b); });
}


// template<class Iter, class Compare>
// void sort(Iter begin, Iter end, Compare cmp) {
//     pdqsort_branchless(begin, end, cmp);
// }
//
// template<typename T, class Compare>
// void sort(vec<T> & data, Compare cmp){
//     pdqsort_branchless(data.begin(), data.end(), cmp);
// }

// deep copy filter
template<class T, class Filter>
vec<shared<T>> filter(const vec<shared<T>>& data, Filter t_fiter) {
    if (data.empty()) return {};
    vec<T> new_data;
    new_data.reserve(data.size());
    for (const auto& event : data) {
        if (t_fiter(event)) { new_data.push_back(*event); }
    }
    if (new_data.capacity() > new_data.size() / 2) { new_data.shrink_to_fit(); }
    // bound all the events' lifetime to the new_data

    auto           capsule = std::make_shared<vec<T>>(std::move(new_data));
    vec<shared<T>> ans;
    ans.reserve(capsule->size());
    for (auto& event : *capsule) { ans.emplace_back(capsule, &event); }
    return ans;
}

// inplace filter
template<class T, class Filter>
void filter_inplace(vec<shared<T>>& data, Filter t_fiter) {
    if (data.empty()) return;
    size_t i = 0;
    for (size_t j = 0; j < data.size(); j++) {
        if (t_fiter(data[j])) {
            if (i != j) { std::swap(data[i], data[j]); }
            i++;
        }
    }
    data.resize(i);
}

template<TimeEvent T>
void clip_inplace(
    pyvec<T>& events, typename T::unit start, typename T::unit end, const bool clip_end = false
) {
    if constexpr (HashDuration<T>) {
        if (clip_end) {
            events.filter([start, end](const T& event) {
                return ((event.time) >= start) && ((event.end()) <= end);
            });
            return;
        }
    }
    events.filter([start, end](const T& event) {
        return ((event.time) >= start) && ((event.time) < end);
    });
}

template<TimeEvent T>   // used for events with duration (e.g. Note and Pedal)
pyvec<T> clip(
    const pyvec<T>&  events,
    typename T::unit start,
    typename T::unit end,
    const bool       clip_end = false
) {
    auto ans = events.copy();   // shallow copy
    clip_inplace(ans, start, end, clip_end);
    return ans;
}

template<TimeEvent T>
void clip_with_sentinel_inplace(pyvec<T>& events, typename T::unit start, typename T::unit end) {
    if (events.empty()) return;

    T sentinel{};
    sentinel.time = std::numeric_limits<typename T::unit>::min();
    events.filter([start, end, &sentinel](const shared<T>& event) {
        if (event.time <= start) {
            if ((sentinel.time) < event.time) { sentinel = *event; }
        } else if ((event.time) < end) {
            return true;
        }
        return false;
    });

    if (sentinel.time != std::numeric_limits<typename T::unit>::min()) {
        sentinel.time = start;
        events.insert(events.begin(), sentinel);
    }
}


template<TimeEvent T>
vec<shared<T>> clip_with_sentinel(
    const vec<shared<T>>& events, typename T::unit start, typename T::unit end
) {
    auto ans = events.copy();   // shallow copy
    clip_with_sentinel_inplace(ans, start, end);
    return ans;
}


// make sure T has time and duration fields using requires
template<TimeEvent T>
    requires requires(T t) {
        { t.duration } -> std::convertible_to<typename T::unit>;
    }
void clamp_dur_inplace(pyvec<T>& events, typename T::unit min_dur, typename T::unit max_dur) {
    for (auto& event : events) { event.duration = std::clamp(event.duration, min_dur, max_dur); }
}

template<TimeEvent T>
    requires requires(T t) {
        { t.duration } -> std::convertible_to<typename T::unit>;
    }
vec<shared<T>> clamp_dur(
    const vec<shared<T>>& events, typename T::unit min_dur, typename T::unit max_dur
) {
    auto new_events = events.deepcopy();
    clamp_dur_inplace(new_events, min_dur, max_dur);
    return new_events;
}

namespace details {
template<TimeEvent T>
void adjust_time_inplace_inner(
    pyvec<T>&                    events,
    const vec<typename T::unit>& original_times,
    const vec<typename T::unit>& new_times
) {
    using unit = typename T::unit;
    // check if the events have duration
    constexpr bool has_dur = HashDuration<T>;
    auto           get_end = [has_dur](const T& event) {
        if constexpr (has_dur) { return event.time + event.duration; }
        return event.time;
    };
    // return empty vector if events is empty
    if (events.empty()) return;
    auto get_factor = [&original_times, &new_times](const size_t x) {
        f64 factor = static_cast<f64>(new_times[x] - new_times[x - 1])
               / static_cast<f64>(original_times[x] - original_times[x - 1]);
        return factor;
    };

    const auto range = std::make_pair(original_times.front(), original_times.back());

    auto valid = [get_end, range](const T& event) {
        return (event.time >= range.first) & (get_end(event) <= range.second);
    };

    auto cur_range  = std::make_pair(original_times[0], original_times[1]);
    auto cur_factor = get_factor(1);
    auto pivot_new  = new_times[0];

    size_t i = 0;
    // clang-format off
    for (size_t j = 0; j < events.size(); j++) {
        T& event = events[j];
        if (!valid(event)) continue;
        if ((event.time) < cur_range.first | (event.time) > cur_range.second) {
            auto idx = std::lower_bound(original_times.begin() + 1, original_times.end(), event.time)
                - original_times.begin();
            cur_factor = get_factor(idx);
            cur_range  = std::make_pair(original_times[idx - 1], original_times[idx]);
            pivot_new  = new_times[idx - 1];
        }
        unit start = pivot_new
            + static_cast<typename T::unit>(cur_factor * static_cast<f64>(event.time - cur_range.first));
        if constexpr (has_dur) {
            auto original_end = get_end(event);
            if (original_end > cur_range.second) {
                auto idx = std::lower_bound(original_times.begin() + 1, original_times.end(), original_end)
                    - original_times.begin();
                cur_factor = get_factor(idx);
                cur_range  = std::make_pair(original_times[idx - 1], original_times[idx]);
                pivot_new  = new_times[idx - 1];
            }
            unit end = pivot_new
                + static_cast<typename T::unit>(cur_factor * static_cast<f64>(original_end - cur_range.first));
            event.duration = end - start;
        }
        event.time = start;
        *(events.pbegin() + i) = *(events.pbegin() + j);
        ++i;
    }
    events.resize(i);
    // clang-format on
}

template<typename T>
void check_times(const vec<T>& original_times, const vec<T>& new_times) {
    if (original_times.size() != new_times.size()) {
        throw std::invalid_argument(
            "symusic::ops::adjust_time: original_times and new_times should have the same size"
        );
    }
    if (original_times.size() < 2) {
        throw std::invalid_argument("symusic::ops::adjust_time: original_times and new_times "
                                    "should have at least 2 elements");
    }
    if (!std::is_sorted(original_times.begin(), original_times.end())) {
        throw std::invalid_argument("symusic::ops::adjust_time: original_times should be sorted");
    }
    if (!std::is_sorted(new_times.begin(), new_times.end())) {
        throw std::invalid_argument("symusic::ops::adjust_time: new_times should be sorted");
    }
}

}   // namespace details

template<bool check_times = true, TimeEvent T>
void adjust_time_inplace(
    pyvec<T>&                    events,
    const vec<typename T::unit>& original_times,
    const vec<typename T::unit>& new_times
) {
    if constexpr (check_times) { details::check_times(original_times, new_times); }
    return details::adjust_time_inplace_inner(events, original_times, new_times);
}

template<bool check_times = true, TType T>
void adjust_time_inplace(
    Track<T>&                    track,
    const vec<typename T::unit>& original_times,
    const vec<typename T::unit>& new_times
) {
    if constexpr (check_times) { details::check_times(original_times, new_times); }
    adjust_time_inplace<false>(*(track.notes), original_times, new_times);
    adjust_time_inplace<false>(*(track.controls), original_times, new_times);
    adjust_time_inplace<false>(*(track.pitch_bends), original_times, new_times);
    adjust_time_inplace<false>(*(track.pedals), original_times, new_times);
    adjust_time_inplace<false>(*(track.lyrics), original_times, new_times);
}

template<bool check_times = true, TType T>
void adjust_time_inplace(
    Score<T>&                    score,
    const vec<typename T::unit>& original_times,
    const vec<typename T::unit>& new_times
) {
    if constexpr (check_times) { details::check_times(original_times, new_times); }
    for (const shared<Track<T>>& track : *(score.tracks)) {
        adjust_time_inplace<false>(*track, original_times, new_times);
    }
    adjust_time_inplace<false>(*(score.time_signatures), original_times, new_times);
    adjust_time_inplace<false>(*(score.key_signatures), original_times, new_times);
    adjust_time_inplace<false>(*(score.tempos), original_times, new_times);
    // adjust_time_inplace<false>(*(score.lyrics), original_times, new_times);
    adjust_time_inplace<false>(*(score.markers), original_times, new_times);
}

template<typename T>
T adjust_time(
    const T&                     data,
    const vec<typename T::unit>& original_times,
    const vec<typename T::unit>& new_times
) {
    T new_data = data.deepcopy();
    adjust_time_inplace<false>(new_data, original_times, new_times);
    return new_data;
}

template<TimeEvent T>
typename T::unit start(const pyvec<T>& events) {
    if (events.empty()) return 0;
    typename T::unit ans = std::numeric_limits<typename T::unit>::max();
    for (const T& event : events) { ans = std::min(ans, event.time); }
    return ans;
}

template<TimeEvent T>
typename T::unit end(const pyvec<T>& events) {
    if (events.empty()) return 0;
    typename T::unit ans     = std::numeric_limits<typename T::unit>::min();
    auto             get_end = [](const T& event) {
        if constexpr (HashDuration<T>) { return event.end(); }
        return event.time;
    };
    for (const T& event : events) { ans = std::max(ans, get_end(event)); }
    return ans;
}

}   // namespace symusic::ops

#endif   // LIBSYMUSIC_BATCH_OPS_H

#pragma once

#ifndef LIBSYMUSIC_BATCH_OPS_H
#define LIBSYMUSIC_BATCH_OPS_H

#include "symusic/event.h"
#include "symusic/score.h"
#include "pdqsort.h"
#include "pyvec.hpp"

#include <stdexcept>

namespace symusic::ops {

namespace details {
// Comparisons on event ends use a wide intermediate (i64 for Tick) so that an event whose end
// exceeds the tick range compares correctly instead of throwing or overflowing. Only values
// written back to an event go through narrow_time.
template<TimeEvent T>
using wide_t = symusic::details::wide_time_t<typename T::unit>;

template<TimeEvent T>
wide_t<T> wide_end(const T& event) {
    return static_cast<wide_t<T>>(event.time) + static_cast<wide_t<T>>(event.duration);
}

// Truncating conversion of a scaled offset; rejects NaN and out-of-range values for Tick.
template<typename U>
U trunc_time(const f64 value) {
    if constexpr (std::integral<U>) {
        return symusic::details::f64_to_i32(value);
    } else {
        return static_cast<U>(value);
    }
}
inline void validate_trim_modes(const std::string& start_mode, const std::string& end_mode) {
    const auto validate = [](const std::string& mode, const char* name) {
        if (mode != "remove" && mode != "truncate" && mode != "retain") {
            throw std::invalid_argument(
                std::string(name) + " must be remove, truncate, or retain, got " + mode
            );
        }
    };
    validate(start_mode, "start_mode");
    validate(end_mode, "end_mode");
}
}   // namespace details

template<typename T, class Comp>
void sort(vec<T>& data, const bool reverse, Comp cmp) {
    if (reverse) {
        pdqsort_branchless(data.rpbegin(), data.rpend(), cmp);
    } else {
        pdqsort_branchless(data.pbegin(), data.pend(), cmp);
    }
}


template<TType T>
void sort_tracks(vec<shared<Track<T>>>& tracks, const bool reverse = false) {
#define KEY(TRACK) \
    std::make_tuple((TRACK)->is_drum, (TRACK)->program, (TRACK)->name, (TRACK)->note_num())
    sort(tracks, reverse, [](const auto& a, const auto& b) { return KEY(&a) < KEY(&b); });
}


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
                return ((event.time) >= start) && (details::wide_end(event) <= end);
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
void trim_inplace(
    pyvec<T>&          events,
    typename T::unit   start,
    typename T::unit   end,
    typename T::unit   min_overlap = 0,
    const std::string& start_mode  = "remove",
    const std::string& end_mode    = "remove"
) {
    details::validate_trim_modes(start_mode, end_mode);
    if constexpr (HashDuration<T>) {
        using wide = details::wide_t<T>;
        using unit = typename T::unit;
        if (start_mode == std::string("remove") && end_mode == std::string("remove")) {
            events.filter([start, end](const T& event) {
                return (event.time >= start) && (details::wide_end(event) <= end);
            });
        } else if (start_mode == std::string("remove")) {
            events.filter([start, end, min_overlap](const T& event) {
                return (event.time >= start)
                       && (static_cast<wide>(end) - event.time >= min_overlap);
            });
        } else if (end_mode == std::string("remove")) {
            events.filter([start, end, min_overlap](const T& event) {
                const wide event_end = details::wide_end(event);
                return (event_end - start >= min_overlap) && (event_end <= end);
            });
        } else {
            events.filter([start, end, min_overlap](const T& event) {
                return (details::wide_end(event) - start >= min_overlap)
                       && (static_cast<wide>(end) - event.time >= min_overlap);
            });
        }

        for (auto& event : events) {
            if (start_mode == "truncate" && event.time < start) {
                event.duration = symusic::details::narrow_time<unit>(
                    details::wide_end(event) - static_cast<wide>(start)
                );
                event.time = start;
            }
            if (end_mode == "truncate" && details::wide_end(event) > end) {
                event.duration = symusic::details::narrow_time<unit>(
                    static_cast<wide>(event.duration)
                    - (details::wide_end(event) - static_cast<wide>(end))
                );
            }
        }
    } else {
        clip_inplace(events, start, end);
    }
}

template<TimeEvent T>
void trim(
    const pyvec<T>&    events,
    typename T::unit   start,
    typename T::unit   end,
    typename T::unit   min_overlap = 0,
    const std::string& start_mode  = "remove",
    const std::string& end_mode    = "remove"
) {
    auto ans = events.copy();
    trim_inplace(ans, start, end, min_overlap, start_mode, end_mode);
    return ans;
}

template<TimeEvent T>
void clip_with_sentinel_inplace(pyvec<T>& events, typename T::unit start, typename T::unit end) {
    if (events.empty()) return;

    T sentinel{};
    sentinel.time = std::numeric_limits<typename T::unit>::lowest();
    events.filter([start, end, &sentinel](const T& event) {
        if (event.time <= start) {
            if ((sentinel.time) < event.time) { sentinel = *event; }
        } else if ((event.time) < end) {
            return true;
        }
        return false;
    });

    if (sentinel.time != std::numeric_limits<typename T::unit>::lowest()) {
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
    // Difference of two times as f64, exact for Tick (i64) and in unit precision for floats.
    auto diff = [](const unit lhs, const unit rhs) {
        return static_cast<f64>(static_cast<wide_t<T>>(lhs) - static_cast<wide_t<T>>(rhs));
    };
    // Wide end so that an event ending beyond the tick range is rejected by `valid`, not wrapped.
    auto get_end = [has_dur](const T& event) {
        if constexpr (has_dur) { return wide_end(event); }
        return static_cast<wide_t<T>>(event.time);
    };
    // return empty vector if events is empty
    if (events.empty()) return;
    auto get_factor = [&original_times, &new_times, diff](const size_t x) {
        f64 factor
            = diff(new_times[x], new_times[x - 1]) / diff(original_times[x], original_times[x - 1]);
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
        unit start = symusic::details::add_time<unit>(
            pivot_new,
            trunc_time<unit>(cur_factor * diff(event.time, cur_range.first))
        );
        if constexpr (has_dur) {
            // `valid` guarantees original_end <= original_times.back(), so it fits in `unit`.
            const auto original_end = static_cast<unit>(get_end(event));
            if (original_end > cur_range.second) {
                auto idx = std::lower_bound(original_times.begin() + 1, original_times.end(), original_end)
                    - original_times.begin();
                cur_factor = get_factor(idx);
                cur_range  = std::make_pair(original_times[idx - 1], original_times[idx]);
                pivot_new  = new_times[idx - 1];
            }
            unit end = symusic::details::add_time<unit>(
                pivot_new,
                trunc_time<unit>(cur_factor * diff(original_end, cur_range.first))
            );
            event.duration = symusic::details::sub_time<unit>(end, start);
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
        throw std::invalid_argument(
            "symusic::ops::adjust_time: original_times and new_times "
            "should have at least 2 elements"
        );
    }
    const auto invalid = std::adjacent_find(
        original_times.begin(), original_times.end(),
        [](const T left, const T right) { return !(left < right); }
    );
    if (invalid != original_times.end()) {
        throw std::invalid_argument(
            "symusic::ops::adjust_time: original_times must be strictly increasing"
        );
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
    adjust_time_inplace<false>(*(score.markers), original_times, new_times);
}

template<typename T>
T adjust_time(
    const T&                     data,
    const vec<typename T::unit>& original_times,
    const vec<typename T::unit>& new_times
) {
    details::check_times(original_times, new_times);
    T new_data = data.deepcopy();
    adjust_time_inplace<false>(new_data, original_times, new_times);
    return new_data;
}

// get the start time of the first event
template<TimeEvent T>
inline typename T::unit start(const pyvec<T>& events) {
    if (events.empty()) { return std::numeric_limits<typename T::unit>::max(); }
    typename T::unit ans = std::numeric_limits<typename T::unit>::max();
    for (const T& event : events) { ans = std::min(ans, event.time); }
    return ans;
}

// get the end time of the last event
template<TimeEvent T>
inline typename T::unit end(const pyvec<T>& events) {
    if (events.empty()) { return std::numeric_limits<typename T::unit>::min(); }
    typename T::unit ans = std::numeric_limits<typename T::unit>::min();
    if constexpr (HashDuration<T>) {
        for (const T& event : events) { ans = std::max(ans, event.end()); }
    } else {
        for (const T& event : events) { ans = std::max(ans, event.time); }
    }
    return ans;
}

}   // namespace symusic::ops

#endif   // LIBSYMUSIC_BATCH_OPS_H

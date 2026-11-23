#pragma once

#ifndef LIBSYMUSIC_BATCH_OPS_H
#define LIBSYMUSIC_BATCH_OPS_H

#include "symusic/event.h"
#include "symusic/score.h"
#include "fmt/core.h"
#include "pdqsort.h"
#include "pyvec.hpp"

#include <algorithm>
#include <functional>
#include <stdexcept>
#include <unordered_set>

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
inline void validate_trim_options(
    auto min_overlap, const std::string& start_mode, const std::string& end_mode
) {
    if (min_overlap < 0) { throw std::invalid_argument("min_overlap must be non-negative"); }
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
    details::validate_trim_options(min_overlap, start_mode, end_mode);
    if constexpr (HashDuration<T>) {
        using wide                 = details::wide_t<T>;
        using unit                 = typename T::unit;
        const bool remove_at_start = start_mode == "remove";
        const bool remove_at_end   = end_mode == "remove";
        events.filter([=](const T& event) {
            const wide event_start = event.time;
            const wide event_end   = details::wide_end(event);
            if ((remove_at_start && event_start < start) || (remove_at_end && event_end > end)) {
                return false;
            }
            const wide overlap_start = std::max<wide>(event_start, start);
            const wide overlap_end   = std::min<wide>(event_end, end);
            return overlap_end - overlap_start >= min_overlap;
        });

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
template<std::floating_point U>
U checked_adjusted_time(f64 value, const char* field) {
    if (!(value >= std::numeric_limits<U>::lowest() && value <= std::numeric_limits<U>::max())) {
        throw std::invalid_argument(
            fmt::format("adjusted {} must be finite and in range, got {}", field, value)
        );
    }
    return static_cast<U>(value);
}

struct TimeAdjustments {
    std::unordered_set<const void*> lists;
    vec<std::function<void()>>      actions;

    void apply() const {
        for (const auto& action : actions) action();
    }
};

template<TimeEvent E>
void plan_time_adjustment(
    pyvec<E>&                    events,
    const vec<typename E::unit>& original_times,
    const vec<typename E::unit>& new_times,
    TimeAdjustments&             plan
) {
    if (events.empty() || !plan.lists.insert(&events).second) return;
    using unit = typename E::unit;
    using wide = wide_t<E>;
    struct Change {
        size_t index;
        unit   start;
        unit   duration;
    };
    vec<Change> retained;
    retained.reserve(events.size());
    const auto diff
        = [](unit left, unit right) { return static_cast<f64>(left) - static_cast<f64>(right); };
    const auto map_time = [&](unit time) {
        const auto right = std::lower_bound(original_times.begin(), original_times.end(), time);
        const auto index = static_cast<size_t>(right - original_times.begin());
        if (*right == time) {
            if constexpr (std::integral<unit>) {
                return new_times[index];
            } else {
                return checked_adjusted_time<unit>(new_times[index], "time");
            }
        }
        const auto left   = index - 1;
        const auto scale  = diff(new_times[index], new_times[left])
                            / diff(original_times[index], original_times[left]);
        const auto offset = scale * diff(time, original_times[left]);
        if constexpr (std::integral<unit>) {
            return symusic::details::add_time(new_times[left], trunc_time<unit>(offset));
        } else {
            return checked_adjusted_time<unit>(static_cast<f64>(new_times[left]) + offset, "time");
        }
    };
    for (size_t index = 0; const E& event : events) {
        const auto end = [&]() -> wide {
            if constexpr (HashDuration<E>) return wide_end(event);
            return event.time;
        }();
        if (event.time >= original_times.front() && event.time <= original_times.back()
            && end >= original_times.front() && end <= original_times.back()) {
            const auto start    = map_time(event.time);
            unit       duration = 0;
            if constexpr (HashDuration<E>) {
                const auto mapped_end = map_time(symusic::details::narrow_time<unit>(end));
                if constexpr (std::integral<unit>) {
                    duration = symusic::details::sub_time(mapped_end, start);
                } else {
                    duration = checked_adjusted_time<unit>(diff(mapped_end, start), "duration");
                }
            }
            retained.push_back({index, start, duration});
        }
        ++index;
    }
    plan.actions.emplace_back([&events, retained = std::move(retained)] {
        for (size_t out = 0; const auto& change : retained) {
            auto& event = events[change.index];
            event.time  = change.start;
            if constexpr (HashDuration<E>) event.duration = change.duration;
            *(events.pbegin() + out++) = *(events.pbegin() + change.index);
        }
        events.resize(retained.size());
    });
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
        original_times.begin(), original_times.end(), [](const T left, const T right) {
            return !(left < right);
        }
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

template<TType T>
void plan_time_adjustment(
    Track<T>&                    track,
    const vec<typename T::unit>& original_times,
    const vec<typename T::unit>& new_times,
    TimeAdjustments&             plan
) {
    plan_time_adjustment(*track.notes, original_times, new_times, plan);
    plan_time_adjustment(*track.controls, original_times, new_times, plan);
    plan_time_adjustment(*track.pitch_bends, original_times, new_times, plan);
    plan_time_adjustment(*track.pedals, original_times, new_times, plan);
    plan_time_adjustment(*track.lyrics, original_times, new_times, plan);
}

template<TType T>
void plan_time_adjustment(
    Score<T>&                    score,
    const vec<typename T::unit>& original_times,
    const vec<typename T::unit>& new_times,
    TimeAdjustments&             plan
) {
    for (auto& track : *score.tracks) {
        plan_time_adjustment(*track, original_times, new_times, plan);
    }
    plan_time_adjustment(*score.time_signatures, original_times, new_times, plan);
    plan_time_adjustment(*score.key_signatures, original_times, new_times, plan);
    plan_time_adjustment(*score.tempos, original_times, new_times, plan);
    plan_time_adjustment(*score.markers, original_times, new_times, plan);
}

}   // namespace details

template<bool check_times = true, TimeEvent T>
void adjust_time_inplace(
    pyvec<T>&                    events,
    const vec<typename T::unit>& original_times,
    const vec<typename T::unit>& new_times
) {
    if constexpr (check_times) { details::check_times(original_times, new_times); }
    details::TimeAdjustments plan;
    details::plan_time_adjustment(events, original_times, new_times, plan);
    plan.apply();
}

template<bool check_times = true, TType T>
void adjust_time_inplace(
    Track<T>&                    track,
    const vec<typename T::unit>& original_times,
    const vec<typename T::unit>& new_times
) {
    if constexpr (check_times) { details::check_times(original_times, new_times); }
    details::TimeAdjustments plan;
    details::plan_time_adjustment(track, original_times, new_times, plan);
    plan.apply();
}

template<bool check_times = true, TType T>
void adjust_time_inplace(
    Score<T>&                    score,
    const vec<typename T::unit>& original_times,
    const vec<typename T::unit>& new_times
) {
    if constexpr (check_times) { details::check_times(original_times, new_times); }
    details::TimeAdjustments plan;
    details::plan_time_adjustment(score, original_times, new_times, plan);
    plan.apply();
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

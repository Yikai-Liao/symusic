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

// Cache the active segment for events that arrive in time order. A segment
// maps both ends of an interval in the same units as the original anchors.
template<class Unit>
class TimeAdjustment {
    using Wide = symusic::details::wide_time_t<Unit>;

    const vec<Unit>& original_;
    const vec<Unit>& targets_;
    Unit             start_;
    Unit             end_;
    Unit             target_;
    Unit             target_end_;
    f64              factor_;

    static f64 difference(Unit time, Unit anchor) {
        return static_cast<f64>(static_cast<Wide>(time) - static_cast<Wide>(anchor));
    }

    void select(size_t next) {
        start_      = original_[next - 1];
        end_        = original_[next];
        target_     = targets_[next - 1];
        target_end_ = targets_[next];
        factor_     = difference(target_end_, target_) / difference(end_, start_);
    }

public:
    TimeAdjustment(const vec<Unit>& original, const vec<Unit>& targets) :
        original_{original}, targets_{targets} {
        select(1);
    }

    bool contains(Unit start, Wide end) const {
        // The release is wide so an out-of-range integer time is discarded before narrowing.
        return start >= original_.front() && start <= original_.back() && end >= original_.front()
               && end <= original_.back();
    }

    Unit project(Unit time) {
        if (time < start_ || time > end_) {
            const auto next = std::lower_bound(original_.begin() + 1, original_.end(), time);
            select(static_cast<size_t>(next - original_.begin()));
        }
        // Use the exact target at the right anchor, retaining the checked tick displacement.
        if (time == end_ && end_ != start_) {
            if constexpr (std::integral<Unit>)
                (void)trunc_time<Unit>(difference(target_end_, target_));
            return target_end_;
        }
        const auto offset = trunc_time<Unit>(factor_ * difference(time, start_));
        return symusic::details::add_time<Unit>(target_, offset);
    }

    // The endpoint bounds an interval of silence. The source window can
    // shorten that interval or exclude it entirely.
    std::optional<Unit> project_endpoint(Unit time) {
        const auto clipped = std::min(time, original_.back());
        if (clipped <= original_.front()) return std::nullopt;
        return project(clipped);
    }
};

template<TimeEvent T>
void adjust_time_inplace_inner(
    pyvec<T>&                    events,
    const vec<typename T::unit>& original_times,
    const vec<typename T::unit>& new_times
) {
    using unit             = typename T::unit;
    constexpr bool has_dur = HashDuration<T>;
    const auto     get_end = [has_dur](const T& event) {
        if constexpr (has_dur) { return wide_end(event); }
        return static_cast<wide_t<T>>(event.time);
    };
    if (events.empty()) return;
    TimeAdjustment adjustment(original_times, new_times);
    auto           output = events.pbegin();
    for (auto input = events.pbegin(); input != events.pend(); ++input) {
        auto&      event        = **input;
        const auto original_end = get_end(event);
        if (!adjustment.contains(event.time, original_end)) continue;
        const auto start = adjustment.project(event.time);
        if constexpr (has_dur) {
            const auto end = adjustment.project(static_cast<unit>(original_end));
            event.duration = symusic::details::sub_time<unit>(end, start);
        }
        event.time = start;
        *output++  = *input;
    }
    events.resize(static_cast<size_t>(output - events.pbegin()));
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
    adjust_time_inplace<false>(*(score.markers), original_times, new_times);
    adjust_time_inplace<false>(*score.note_tunings, original_times, new_times);
    if (score.explicit_end) {
        details::TimeAdjustment adjustment(original_times, new_times);
        score.explicit_end = adjustment.project_endpoint(*score.explicit_end);
    }
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

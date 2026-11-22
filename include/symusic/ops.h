#pragma once

#ifndef LIBSYMUSIC_BATCH_OPS_H
#define LIBSYMUSIC_BATCH_OPS_H

#include "symusic/event.h"
#include "symusic/score.h"
#include "pdqsort.h"

#include <algorithm>
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

template<TType T, typename S = ValueStorage>
void sort_tracks(typename S::template Tracks<Track<T, S>>& tracks, bool reverse = false) {
    auto projection = [](const auto& handle) {
        const auto& track = S::get(handle);
        return std::tuple(track.is_drum, track.program, std::string_view(track.name), track.note_num());
    };
    auto& values = S::get(tracks);
    if (reverse) {
        std::ranges::sort(values, std::ranges::greater{}, projection);
    } else {
        std::ranges::sort(values, std::ranges::less{}, projection);
    }
}

template<typename S = ValueStorage, typename Range, TimeEvent T = typename Range::value_type>
void clip_inplace(
    Range& events, typename T::unit start, typename T::unit end, const bool clip_end = false
) {
    if constexpr (HashDuration<T>) {
        if (clip_end) {
            S::erase_if(events, [start, end](const T& event) {
                return !(event.time >= start && details::wide_end(event) <= end);
            });
            return;
        }
    }
    S::erase_if(events, [start, end](const T& event) {
        return !(event.time >= start && event.time < end);
    });
}

template<typename S = ValueStorage, typename Range, TimeEvent T = typename Range::value_type>
void trim_inplace(
    Range&          events,
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
        const bool remove_at_start = start_mode == "remove";
        const bool remove_at_end = end_mode == "remove";
        S::erase_if(events, [=](const T& event) {
            const wide event_start = event.time;
            const wide event_end = details::wide_end(event);
            if ((remove_at_start && event_start < start) || (remove_at_end && event_end > end)) {
                return true;
            }
            const wide overlap_start = std::max<wide>(event_start, start);
            const wide overlap_end = std::min<wide>(event_end, end);
            return !(overlap_end - overlap_start >= min_overlap);
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
        clip_inplace<S>(events, start, end);
    }
}

template<typename S = ValueStorage, typename Range, TimeEvent T = typename Range::value_type>
void clip_with_sentinel_inplace(Range& events, typename T::unit start, typename T::unit end) {
    if (events.empty()) return;

    T sentinel{};
    sentinel.time = std::numeric_limits<typename T::unit>::lowest();
    S::erase_if(events, [start, end, &sentinel](const T& event) {
        if (event.time <= start) {
            if ((sentinel.time) < event.time) { sentinel = event; }
        } else if (event.time < end) {
            return false;
        }
        return true;
    });

    if (sentinel.time != std::numeric_limits<typename T::unit>::lowest()) {
        sentinel.time = start;
        events.insert(events.begin(), sentinel);
    }
}


namespace details {
template<typename S = ValueStorage, typename Range, TimeEvent T = typename Range::value_type>
void adjust_time_inplace_inner(
    Range&                    events,
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
        // Segment indices start at one, leaving a preceding anchor at x - 1.
        f64 factor
            = diff(new_times[x], new_times[x - 1]) / diff(original_times[x], original_times[x - 1]);
        return factor;
    };

    const auto range = std::make_pair(original_times.front(), original_times.back());

    auto valid = [get_end, range](const T& event) {
        const auto end = get_end(event);
        return event.time >= range.first && event.time <= range.second
               && end >= range.first && end <= range.second;
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
            const auto original_end = symusic::details::narrow_time<unit>(get_end(event));
            if (original_end < cur_range.first || original_end > cur_range.second) {
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
        S::keep(events, i, j);
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

template<bool check_times = true, typename S = ValueStorage, typename Range, TimeEvent T = typename Range::value_type>
void adjust_time_inplace(
    Range&                    events,
    const vec<typename T::unit>& original_times,
    const vec<typename T::unit>& new_times
) {
    if constexpr (check_times) { details::check_times(original_times, new_times); }
    return details::adjust_time_inplace_inner<S>(events, original_times, new_times);
}

template<bool check_times = true, TType T, typename S>
void adjust_time_inplace(
    Track<T, S>&                    track,
    const vec<typename T::unit>& original_times,
    const vec<typename T::unit>& new_times
) {
    if constexpr (check_times) { details::check_times(original_times, new_times); }
    adjust_time_inplace<false, S>(S::get(track.notes), original_times, new_times);
    adjust_time_inplace<false, S>(S::get(track.controls), original_times, new_times);
    adjust_time_inplace<false, S>(S::get(track.pitch_bends), original_times, new_times);
    adjust_time_inplace<false, S>(S::get(track.pedals), original_times, new_times);
    adjust_time_inplace<false, S>(S::get(track.lyrics), original_times, new_times);
}

template<bool check_times = true, TType T, typename S>
void adjust_time_inplace(
    Score<T, S>&                    score,
    const vec<typename T::unit>& original_times,
    const vec<typename T::unit>& new_times
) {
    if constexpr (check_times) { details::check_times(original_times, new_times); }
    for (auto& track : S::get(score.tracks)) {
        adjust_time_inplace<false>(S::get(track), original_times, new_times);
    }
    adjust_time_inplace<false, S>(S::get(score.time_signatures), original_times, new_times);
    adjust_time_inplace<false, S>(S::get(score.key_signatures), original_times, new_times);
    adjust_time_inplace<false, S>(S::get(score.tempos), original_times, new_times);
    adjust_time_inplace<false, S>(S::get(score.markers), original_times, new_times);
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
template<typename Range, TimeEvent T = typename Range::value_type>
inline typename T::unit start(const Range& events) {
    if (events.empty()) { return std::numeric_limits<typename T::unit>::max(); }
    typename T::unit ans = std::numeric_limits<typename T::unit>::max();
    for (const T& event : events) { ans = std::min(ans, event.time); }
    return ans;
}

// get the end time of the last event
template<typename Range, TimeEvent T = typename Range::value_type>
inline typename T::unit end(const Range& events) {
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

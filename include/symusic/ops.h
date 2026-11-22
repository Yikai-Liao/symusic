#pragma once

#include "symusic/event.h"
#include "symusic/score.h"

#include "fmt/core.h"
#include "pdqsort.h"

#include <algorithm>
#include <cmath>
#include <functional>
#include <stdexcept>
#include <tuple>
#include <unordered_set>

namespace symusic::detail {
template<TType T, typename S>
auto track_sort_key(const Track<T, S>& track) {
    return std::tuple(track.is_drum, track.program, track.name, S::get(track.notes).size());
}

// Comparisons on event ends use a wide intermediate (i64 for Tick) so that an event whose end
// exceeds the tick range compares correctly instead of throwing or overflowing. Only values
// written back to an event go through narrow_time.
template<TimeEvent T>
using wide_t = symusic::detail::wide_time_t<typename T::unit>;

template<TimeEvent T>
wide_t<T> wide_end(const T& event) {
    return static_cast<wide_t<T>>(event.time) + static_cast<wide_t<T>>(event.duration);
}

inline void validate_trim_options(
    auto               start,
    auto               end,
    auto               min_overlap,
    const std::string& start_mode,
    const std::string& end_mode
) {
    if (!(start <= end)) {
        throw std::invalid_argument(
            fmt::format("trim requires start <= end, got start={}, end={}", start, end)
        );
    }
    if (!(min_overlap >= 0)) {
        throw std::invalid_argument(
            fmt::format("min_overlap must be non-negative, got {}", min_overlap)
        );
    }
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
}   // namespace symusic::detail

namespace symusic::ops {

template<TType T, typename S = ValueStorage>
void sort_tracks(typename S::template Tracks<Track<T, S>>& tracks, bool reverse = false) {
    auto  projection = [](const auto& handle) { return detail::track_sort_key(S::get(handle)); };
    auto& values     = S::get(tracks);
    if (reverse) {
        std::ranges::sort(values, std::ranges::greater{}, projection);
    } else {
        std::ranges::sort(values, std::ranges::less{}, projection);
    }
}

template<typename S = ValueStorage, typename Range, TimeEvent T = typename Range::value_type>
void clip(
    Range& events, typename T::unit start, typename T::unit end, const bool clip_end = false
) {
    if constexpr (HashDuration<T>) {
        if (clip_end) {
            S::erase_if(events, [start, end](const T& event) {
                return !(event.time >= start && detail::wide_end(event) <= end);
            });
            return;
        }
    }
    S::erase_if(events, [start, end](const T& event) {
        return !(event.time >= start && event.time < end);
    });
}

template<typename S = ValueStorage, typename Range, TimeEvent T = typename Range::value_type>
void trim(
    Range&             events,
    typename T::unit   start,
    typename T::unit   end,
    typename T::unit   min_overlap = 0,
    const std::string& start_mode  = "remove",
    const std::string& end_mode    = "remove"
) {
    detail::validate_trim_options(start, end, min_overlap, start_mode, end_mode);
    if constexpr (HashDuration<T>) {
        using wide                 = detail::wide_t<T>;
        using unit                 = typename T::unit;
        const bool remove_at_start = start_mode == "remove";
        const bool remove_at_end   = end_mode == "remove";
        S::erase_if(events, [=](const T& event) {
            const wide event_start = event.time;
            const wide event_end   = detail::wide_end(event);
            if ((remove_at_start && event_start < start) || (remove_at_end && event_end > end)) {
                return true;
            }
            const wide overlap_start = std::max<wide>(event_start, start);
            const wide overlap_end   = std::min<wide>(event_end, end);
            return !(overlap_end - overlap_start >= min_overlap);
        });

        for (auto& event : events) {
            if (start_mode == "truncate" && event.time < start) {
                event.duration = symusic::detail::narrow_time<unit>(
                    detail::wide_end(event) - static_cast<wide>(start)
                );
                event.time = start;
            }
            if (end_mode == "truncate" && detail::wide_end(event) > end) {
                event.duration = symusic::detail::narrow_time<unit>(
                    static_cast<wide>(event.duration)
                    - (detail::wide_end(event) - static_cast<wide>(end))
                );
            }
        }
    } else {
        clip<S>(events, start, end);
    }
}

template<typename S = ValueStorage, typename Range, TimeEvent T = typename Range::value_type>
void clip_with_sentinel(Range& events, typename T::unit start, typename T::unit end) {
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

}   // namespace symusic::ops

namespace symusic::detail {
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

template<typename S = ValueStorage, typename Range, TimeEvent E = typename Range::value_type>
void plan_time_adjustment(
    Range&                       events,
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
            // Compute in f64 before narrowing so a valid target across a >INT32_MAX span does not
            // falsely overflow on the intermediate offset.
            return symusic::detail::f64_to_i32(
                static_cast<f64>(new_times[left]) + std::trunc(offset)
            );
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
                const auto mapped_end = map_time(narrow_time<unit>(end));
                if constexpr (std::integral<unit>) {
                    duration = sub_time(mapped_end, start);
                } else {
                    duration = checked_adjusted_time<unit>(diff(mapped_end, start), "duration");
                }
            }
            retained.push_back({index, start, duration});
        }
        ++index;
    }
    // Shared storage moves handles when compacting, so the surviving Python
    // references keep pointing to the same events.
    plan.actions.emplace_back([&events, retained = std::move(retained)] {
        for (size_t out = 0; const auto& change : retained) {
            auto& event = events[change.index];
            event.time  = change.start;
            if constexpr (HashDuration<E>) event.duration = change.duration;
            S::keep(events, out++, change.index);
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
    if constexpr (std::floating_point<T>) {
        const auto check_finite = [](const vec<T>& times, const char* parameter) {
            const auto invalid = std::find_if(times.begin(), times.end(), [](const T value) {
                return !std::isfinite(value);
            });
            if (invalid != times.end()) {
                throw std::invalid_argument(
                    fmt::format(
                        "symusic::ops::adjust_time: {} must be finite, got {}", parameter, *invalid
                    )
                );
            }
        };
        check_finite(original_times, "original_times");
        check_finite(new_times, "new_times");
    }
    const auto invalid = std::adjacent_find(
        original_times.begin(), original_times.end(), [](const T left, const T right) {
            return !(left < right);
        }
    );
    if (invalid != original_times.end()) {
        throw std::invalid_argument(
            fmt::format(
                "symusic::ops::adjust_time: original_times must be strictly increasing, got {}, {}",
                *invalid,
                *(invalid + 1)
            )
        );
    }
    const auto unsorted
        = std::adjacent_find(new_times.begin(), new_times.end(), [](const T left, const T right) {
              return !(left <= right);
          });
    if (unsorted != new_times.end()) {
        throw std::invalid_argument(
            fmt::format(
                "symusic::ops::adjust_time: new_times should be sorted, got {}, {}",
                *unsorted,
                *(unsorted + 1)
            )
        );
    }
}

template<TType T, typename S>
void plan_time_adjustment(
    Track<T, S>&                 track,
    const vec<typename T::unit>& original_times,
    const vec<typename T::unit>& new_times,
    TimeAdjustments&             plan
) {
    plan_time_adjustment<S>(S::get(track.notes), original_times, new_times, plan);
    plan_time_adjustment<S>(S::get(track.controls), original_times, new_times, plan);
    plan_time_adjustment<S>(S::get(track.pitch_bends), original_times, new_times, plan);
    plan_time_adjustment<S>(S::get(track.lyrics), original_times, new_times, plan);
}

template<TType T, typename S>
void adjust_track_times(
    vec<shared<Track<T, S>>>&    tracks,
    const vec<typename T::unit>& original_times,
    const vec<typename T::unit>& new_times
) {
    check_times(original_times, new_times);
    if (tracks.empty()) return;
    TimeAdjustments plan;
    for (auto& track : tracks) { plan_time_adjustment(*track, original_times, new_times, plan); }
    plan.apply();
}

template<TType T, typename S>
void plan_time_adjustment(
    Score<T, S>&                 score,
    const vec<typename T::unit>& original_times,
    const vec<typename T::unit>& new_times,
    TimeAdjustments&             plan
) {
    for (auto& track : S::get(score.tracks)) {
        plan_time_adjustment(S::get(track), original_times, new_times, plan);
    }
    plan_time_adjustment<S>(S::get(score.time_signatures), original_times, new_times, plan);
    plan_time_adjustment<S>(S::get(score.key_signatures), original_times, new_times, plan);
    plan_time_adjustment<S>(S::get(score.tempos), original_times, new_times, plan);
    plan_time_adjustment<S>(S::get(score.markers), original_times, new_times, plan);
}

// Keep the update separate from validation so a rejected offset leaves all
// existing event references and values intact.
template<class V>
class FieldUpdates {
    vec<std::pair<V*, V>> updates;

public:
    void add(V& field, V value) { updates.emplace_back(&field, value); }

    void apply() const {
        for (const auto& [field, value] : updates) *field = value;
    }
};

template<TType T, class S>
void collect_time_shifts(
    Track<T, S>& track, typename T::unit offset, FieldUpdates<typename T::unit>& changes
) {
    const auto collect = [&](auto& list) {
        for (auto& event : S::get(list)) changes.add(event.time, add_time(event.time, offset));
    };
    collect(track.notes);
    collect(track.controls);
    collect(track.pitch_bends);
    collect(track.lyrics);
}

template<TType T, class S>
void collect_time_shifts(
    Score<T, S>& score, typename T::unit offset, FieldUpdates<typename T::unit>& changes
) {
    for (auto& track : S::get(score.tracks)) collect_time_shifts(S::get(track), offset, changes);
    const auto collect = [&](auto& list) {
        for (auto& event : S::get(list)) changes.add(event.time, add_time(event.time, offset));
    };
    collect(score.time_signatures);
    collect(score.key_signatures);
    collect(score.tempos);
    collect(score.markers);
}

template<size_t I, TType T, class S>
void collect_note_shifts(Track<T, S>& track, i32 offset, FieldUpdates<i8>& changes) {
    for (auto& note : S::get(track.notes)) {
        auto& field = note.*std::get<I>(event_fields<Note<T>>).second;
        changes.add(field, checked_event_field<Note<T>, I>(static_cast<i64>(field) + offset));
    }
}

template<size_t I, TType T, class S>
void collect_note_shifts(Score<T, S>& score, i32 offset, FieldUpdates<i8>& changes) {
    for (auto& track : S::get(score.tracks)) collect_note_shifts<I>(S::get(track), offset, changes);
}

}   // namespace symusic::detail

namespace symusic::ops {

template<typename S = ValueStorage, typename Range, TimeEvent E = typename Range::value_type>
void adjust_time(
    Range&                       events,
    const vec<typename E::unit>& original_times,
    const vec<typename E::unit>& new_times
) {
    detail::check_times(original_times, new_times);
    detail::TimeAdjustments plan;
    detail::plan_time_adjustment<S>(events, original_times, new_times, plan);
    plan.apply();
}

template<class Object>
    requires requires { typename Object::storage_type; }
void adjust_time(
    Object&                           object,
    const vec<typename Object::unit>& original_times,
    const vec<typename Object::unit>& new_times
) {
    detail::check_times(original_times, new_times);
    detail::TimeAdjustments plan;
    detail::plan_time_adjustment(object, original_times, new_times, plan);
    plan.apply();
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

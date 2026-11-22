#pragma once

#include <span>

#include "symusic/time_unit.h"
#include "symusic/score.h"
#include "symusic/detail/time_conversion.h"

namespace symusic {

template<TType To, TType From, typename S>
Score<To, S> convert(
    const Score<From, S>& score, typename To::unit min_duration = static_cast<typename To::unit>(0)
);

template<TType To, TType From, typename S>
[[nodiscard]] typename To::unit convert_time(
    typename From::unit time, const Score<From, S>& context
) {
    const auto converter = detail::make_time_converter<To>(context);
    return converter.time_value(time);
}

template<TType To, TType From, typename S>
[[nodiscard]] vec<typename To::unit> convert_times(
    std::span<const typename From::unit> times, const Score<From, S>& context
) {
    const auto converter = detail::make_time_converter<To>(context);
    return converter.time_values(times);
}

template<TType To, template<class> class Event, TType From, typename S, typename Range>
[[nodiscard]] vec<Event<To>> convert_time_events(const Range& data, const Score<From, S>& context) {
    const auto converter = detail::make_time_converter<To>(context);
    return converter.template time_vec<Event>(data);
}

template<TType To, template<class> class Event, TType From, typename S, typename Range>
[[nodiscard]] vec<Event<To>> convert_duration_events(
    const Range& data, const Score<From, S>& context, typename To::unit min_duration = 0
) {
    const auto converter = detail::make_time_converter<To>(context);
    return converter.template duration_vec<Event>(data, min_duration);
}

template<TType T, typename S>
Score<Tick, S> resample(
    const Score<T, S>& score, i32 ticks_per_quarter, Tick::unit min_duration = 0
);

namespace detail {

template<typename S, TType T>
[[nodiscard]] Track<T, S> adopt_storage(Track<T>&& track) {
    return {
        std::move(track.name),
        track.program,
        track.is_drum,
        S::adopt(std::move(track.notes)),
        S::adopt(std::move(track.controls)),
        S::adopt(std::move(track.pitch_bends)),
        S::adopt(std::move(track.lyrics))
    };
}

template<typename S, TType T>
[[nodiscard]] Score<T, S> adopt_storage(Score<T>&& score) {
    if constexpr (std::is_same_v<S, ValueStorage>) {
        return std::move(score);
    } else {
        vec<typename Score<T, S>::track_handle> tracks;
        tracks.reserve(score.tracks.size());
        for (auto& track : score.tracks) {
            tracks.push_back(S::adopt_track(adopt_storage<S>(std::move(track))));
        }
        return {
            score.ticks_per_quarter,
            S::adopt_tracks(std::move(tracks)),
            S::adopt(std::move(score.time_signatures)),
            S::adopt(std::move(score.key_signatures)),
            S::adopt(std::move(score.tempos)),
            S::adopt(std::move(score.markers))
        };
    }
}

}   // namespace detail
}   // namespace symusic

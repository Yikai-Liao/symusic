#pragma once

#ifndef LIBSYMUSIC_DETAIL_TIME_CONVERSION_H
#define LIBSYMUSIC_DETAIL_TIME_CONVERSION_H

#include <algorithm>
#include <cstddef>
#include <cmath>
#include <iterator>
#include <limits>
#include <span>
#include <type_traits>
#include <utility>

#include "symusic/score.h"
#include "symusic/utils.h"

namespace symusic::details {

template<typename Converter, TType To, TType From>
struct SimpleTimeConverter {
    using from_unit = typename From::unit;
    using to_unit   = typename To::unit;

    template<typename S>
    explicit SimpleTimeConverter(const Score<From, S>&) {}

    [[nodiscard]] to_unit time_value(const from_unit time) const {
        const auto self = static_cast<const Converter*>(this);
        return self->time(time);
    }

    [[nodiscard]] vec<to_unit> time_values(std::span<const from_unit> times) const {
        vec<to_unit> converted;
        converted.reserve(times.size());
        for (const auto time : times) { converted.push_back(time_value(time)); }
        return converted;
    }

    template<template<class> class Event, typename Range>
        requires TimeEvent<Event<From>>
    [[nodiscard]] vec<Event<To>> time_vec(const Range& data) const {
        vec<Event<To>> converted;
        converted.reserve(data.size());
        for (const auto& event : data) { converted.emplace_back(time_value(event.time), event); }
        return converted;
    }

    template<template<class> class Event, typename Range>
        requires HashDuration<Event<From>>
    [[nodiscard]] vec<Event<To>> duration_vec(
        const Range& data, typename To::unit min_dur
    ) const {
        min_dur = std::max(min_dur, static_cast<typename To::unit>(0));

        vec<Event<To>> converted;
        converted.reserve(data.size());
        for (const auto& event : data) {
            converted.emplace_back(
                time_value(event.time), std::max(min_dur, time_value(event.duration)), event
            );
        }
        return converted;
    }
};

template<typename Converter, TType To, TType From>
struct SecondTimeConverter {
    using from_unit = typename From::unit;
    using to_unit   = typename To::unit;

    struct RangeState {
        std::pair<from_unit, from_unit> range;
        to_unit                         pivot_to;
        f64                             factor;
    };

    f64            tpq;
    vec<to_unit>   to_times;
    vec<from_unit> from_times;
    vec<f64>       factors;
    f64            default_factor;

    template<typename S>
    explicit SecondTimeConverter(const Score<From, S>& score) :
        tpq(static_cast<f64>(score.ticks_per_quarter)) {
        constexpr i32 default_mspq = 500000;
        const auto& source_tempos = S::get(score.tempos);
        vec<Tempo<From>> tempos(source_tempos.begin(), source_tempos.end());
        std::stable_sort(tempos.begin(), tempos.end(), [](const auto& lhs, const auto& rhs) {
            return lhs.time < rhs.time;
        });
        const auto zero = std::lower_bound(
            tempos.begin(), tempos.end(), from_unit{0},
            [](const auto& tempo, const auto time) { return tempo.time < time; }
        );
        const auto origin = zero - tempos.begin();
        if (zero == tempos.end() || zero->time != from_unit{0}) {
            // Carry an active negative-time tempo across the origin.
            const auto mspq = zero == tempos.begin() ? default_mspq : std::prev(zero)->mspq;
            tempos.emplace(zero, from_unit{0}, mspq);
        }

        const auto self = static_cast<const Converter*>(this);
        default_factor = self->get_factor(Tempo<From>(from_unit{0}, default_mspq));
        to_times.resize(tempos.size());
        from_times.reserve(tempos.size() + 1);
        factors.reserve(tempos.size());
        for (const auto& tempo : tempos) {
            from_times.push_back(tempo.time);
            factors.push_back(self->get_factor(tempo));
        }
        // The sentinel bounds the final segment without requiring a converted pivot.
        from_times.push_back(std::numeric_limits<from_unit>::max());

        // Keep zero fixed and integrate each segment using the tempo at its left boundary.
        const auto count = tempos.end() - tempos.begin();
        for (auto index = origin + 1; index < count; ++index) {
            to_times[index] = self->get_time(
                from_times[index], to_times[index - 1], from_times[index - 1], factors[index - 1]
            );
        }
        for (auto index = origin; index > 0; --index) {
            to_times[index - 1] = self->get_time(
                from_times[index - 1], to_times[index], from_times[index], factors[index - 1]
            );
        }
    }

    [[nodiscard]] to_unit time_value(const from_unit time) const {
        const auto self  = static_cast<const Converter*>(this);
        if (time < from_times.front()) {
            return self->get_time(time, to_times.front(), from_times.front(), default_factor);
        }
        const auto index = range_index(time);
        return self->get_time(time, to_times[index], from_times[index], factors[index]);
    }

    [[nodiscard]] vec<to_unit> time_values(std::span<const from_unit> times) const {
        vec<to_unit> converted;
        converted.reserve(times.size());
        if (times.empty()) { return converted; }

        const auto self  = static_cast<const Converter*>(this);
        auto       state = initial_state();
        for (const auto time : times) {
            update_state(state, time);
            converted.push_back(
                self->get_time(time, state.pivot_to, state.range.first, state.factor)
            );
        }
        return converted;
    }

    template<template<class> class Event, typename Range>
        requires TimeEvent<Event<From>>
    [[nodiscard]] vec<Event<To>> time_vec(const Range& data) const {
        vec<Event<To>> converted;
        converted.reserve(data.size());
        if (data.empty()) { return converted; }

        const auto self  = static_cast<const Converter*>(this);
        auto       state = initial_state();
        for (const auto& event : data) {
            update_state(state, event.time);
            converted.emplace_back(
                self->get_time(event.time, state.pivot_to, state.range.first, state.factor), event
            );
        }
        return converted;
    }

    template<template<class> class Event, typename Range>
        requires HashDuration<Event<From>>
    [[nodiscard]] vec<Event<To>> duration_vec(
        const Range& data, typename To::unit min_dur
    ) const {
        min_dur = std::max(min_dur, static_cast<typename To::unit>(0));

        vec<Event<To>> converted;
        converted.reserve(data.size());
        if (data.empty()) { return converted; }

        const auto self  = static_cast<const Converter*>(this);
        auto       state = initial_state();
        for (const auto& event : data) {
            update_state(state, event.time);
            const auto start
                = self->get_time(event.time, state.pivot_to, state.range.first, state.factor);

            const auto end_time = event.end();
            update_state(state, end_time);
            const auto end
                = self->get_time(end_time, state.pivot_to, state.range.first, state.factor);
            converted.emplace_back(start, std::max(min_dur, sub_time<to_unit>(end, start)), event);
        }
        return converted;
    }

private:
    [[nodiscard]] size_t range_index(const from_unit time) const {
        // Segment i spans [from_times[i], from_times[i + 1]). The last boundary is
        // the maximum time itself, so a time equal to it belongs to the last segment.
        const auto pos
            = std::upper_bound(from_times.begin(), from_times.end(), time) - from_times.begin();
        const auto last = static_cast<std::ptrdiff_t>(from_times.size()) - 2;
        return static_cast<size_t>(std::clamp<std::ptrdiff_t>(pos - 1, 0, last));
    }

    [[nodiscard]] RangeState initial_state() const {
        return {{from_times[0], from_times[1]}, to_times[0], factors[0]};
    }

    void update_state(RangeState& state, const from_unit time) const {
        if (time < state.range.first || time >= state.range.second) {
            if (time < from_times.front()) {
                // Use an empty cached range with its pivot at the first boundary.
                state = {
                    {from_times.front(), from_times.front()}, to_times.front(), default_factor
                };
                return;
            }
            const auto index = range_index(time);
            state.range      = {from_times[index], from_times[index + 1]};
            state.pivot_to   = to_times[index];
            state.factor     = factors[index];
        }
    }
};

struct Tick2TickConverter : SimpleTimeConverter<Tick2TickConverter, Tick, Tick> {
    using SimpleTimeConverter<Tick2TickConverter, Tick, Tick>::SimpleTimeConverter;

    [[nodiscard]] static Tick::unit time(const Tick::unit value) { return value; }
};

struct Quarter2QuarterConverter : SimpleTimeConverter<Quarter2QuarterConverter, Quarter, Quarter> {
    using SimpleTimeConverter<Quarter2QuarterConverter, Quarter, Quarter>::SimpleTimeConverter;

    [[nodiscard]] static Quarter::unit time(const Quarter::unit value) { return value; }
};

struct Second2SecondConverter : SimpleTimeConverter<Second2SecondConverter, Second, Second> {
    using SimpleTimeConverter<Second2SecondConverter, Second, Second>::SimpleTimeConverter;

    [[nodiscard]] static Second::unit time(const Second::unit value) { return value; }
};

struct Tick2QuarterConverter : SimpleTimeConverter<Tick2QuarterConverter, Quarter, Tick> {
    f32 tpq;

    template<typename S>
    explicit Tick2QuarterConverter(const Score<Tick, S>& score) :
        SimpleTimeConverter(score), tpq(static_cast<f32>(score.ticks_per_quarter)) {}

    [[nodiscard]] Quarter::unit time(const Tick::unit value) const {
        return static_cast<Quarter::unit>(value) / tpq;
    }
};

struct Quarter2TickConverter : SimpleTimeConverter<Quarter2TickConverter, Tick, Quarter> {
    f32 tpq;

    template<typename S>
    explicit Quarter2TickConverter(const Score<Quarter, S>& score) :
        SimpleTimeConverter(score), tpq(static_cast<f32>(score.ticks_per_quarter)) {}

    [[nodiscard]] Tick::unit time(const Quarter::unit value) const {
        return round_to_i32(value * tpq);
    }
};

struct Tick2SecondConverter : SecondTimeConverter<Tick2SecondConverter, Second, Tick> {
    using SecondTimeConverter<Tick2SecondConverter, Second, Tick>::SecondTimeConverter;

    [[nodiscard]] f64 get_factor(const Tempo<Tick>& tempo) const {
        return static_cast<f64>(tempo.mspq) / 1000000. / tpq;
    }

    [[nodiscard]] static Second::unit get_time(
        const Tick::unit   time,
        const Second::unit pivot_to,
        const Tick::unit   pivot_from,
        const f64          factor
    ) {
        return pivot_to + static_cast<Second::unit>(factor * (static_cast<i64>(time) - pivot_from));
    }
};

struct Second2TickConverter : SecondTimeConverter<Second2TickConverter, Tick, Second> {
    using SecondTimeConverter<Second2TickConverter, Tick, Second>::SecondTimeConverter;

    [[nodiscard]] f64 get_factor(const Tempo<Second>& tempo) const {
        return 1000000. * tpq / static_cast<f64>(tempo.mspq);
    }

    [[nodiscard]] static Tick::unit get_time(
        const Second::unit time,
        const Tick::unit   pivot_to,
        const Second::unit pivot_from,
        const f64          factor
    ) {
        return add_time(pivot_to, round_to_i32(factor * (time - pivot_from)));
    }
};

struct Quarter2SecondConverter : SecondTimeConverter<Quarter2SecondConverter, Second, Quarter> {
    using SecondTimeConverter<Quarter2SecondConverter, Second, Quarter>::SecondTimeConverter;

    [[nodiscard]] static f64 get_factor(const Tempo<Quarter>& tempo) {
        return static_cast<f64>(tempo.mspq) / 1000000.;
    }

    [[nodiscard]] static Second::unit get_time(
        const Quarter::unit time,
        const Second::unit  pivot_to,
        const Quarter::unit pivot_from,
        const f64           factor
    ) {
        return pivot_to + static_cast<Second::unit>(factor * (time - pivot_from));
    }
};

struct Second2QuarterConverter : SecondTimeConverter<Second2QuarterConverter, Quarter, Second> {
    using SecondTimeConverter<Second2QuarterConverter, Quarter, Second>::SecondTimeConverter;

    [[nodiscard]] static f64 get_factor(const Tempo<Second>& tempo) {
        return 1000000. / static_cast<f64>(tempo.mspq);
    }

    [[nodiscard]] static Quarter::unit get_time(
        const Second::unit  time,
        const Quarter::unit pivot_to,
        const Second::unit  pivot_from,
        const f64           factor
    ) {
        return pivot_to + static_cast<Quarter::unit>(factor * (time - pivot_from));
    }
};

template<TType To, TType From, typename S>
[[nodiscard]] auto make_time_converter(const Score<From, S>& score) {
    if constexpr (std::is_same_v<To, Tick> && std::is_same_v<From, Tick>) {
        return Tick2TickConverter(score);
    } else if constexpr (std::is_same_v<To, Quarter> && std::is_same_v<From, Quarter>) {
        return Quarter2QuarterConverter(score);
    } else if constexpr (std::is_same_v<To, Second> && std::is_same_v<From, Second>) {
        return Second2SecondConverter(score);
    } else if constexpr (std::is_same_v<To, Quarter> && std::is_same_v<From, Tick>) {
        return Tick2QuarterConverter(score);
    } else if constexpr (std::is_same_v<To, Tick> && std::is_same_v<From, Quarter>) {
        return Quarter2TickConverter(score);
    } else if constexpr (std::is_same_v<To, Second> && std::is_same_v<From, Tick>) {
        return Tick2SecondConverter(score);
    } else if constexpr (std::is_same_v<To, Tick> && std::is_same_v<From, Second>) {
        return Second2TickConverter(score);
    } else if constexpr (std::is_same_v<To, Second> && std::is_same_v<From, Quarter>) {
        return Quarter2SecondConverter(score);
    } else if constexpr (std::is_same_v<To, Quarter> && std::is_same_v<From, Second>) {
        return Second2QuarterConverter(score);
    } else {
        static_assert(std::is_same_v<To, void>, "Unsupported time unit conversion");
    }
}

}   // namespace symusic::details

#endif   // LIBSYMUSIC_DETAIL_TIME_CONVERSION_H

#pragma once

#ifndef LIBSYMUSIC_DETAIL_TIME_CONVERSION_H
#define LIBSYMUSIC_DETAIL_TIME_CONVERSION_H

#include <algorithm>
#include <cmath>
#include <limits>
#include <span>
#include <type_traits>
#include <utility>

#include "symusic/score.h"

namespace symusic::details {

template<typename Converter, TType To, TType From>
struct SimpleTimeConverter {
    using from_unit = typename From::unit;
    using to_unit   = typename To::unit;

    explicit SimpleTimeConverter(const Score<From>&) {}

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

    template<template<class> class Event>
        requires TimeEvent<Event<From>>
    [[nodiscard]] pyvec<Event<To>> time_vec(const pyvec<Event<From>>& data) const {
        vec<Event<To>> converted;
        converted.reserve(data.size());
        for (const auto& event : data) { converted.emplace_back(time_value(event.time), event); }
        return pyvec<Event<To>>(std::move(converted));
    }

    template<template<class> class Event>
        requires HashDuration<Event<From>>
    [[nodiscard]] pyvec<Event<To>> duration_vec(
        const pyvec<Event<From>>& data, typename To::unit min_dur
    ) const {
        min_dur = std::max(min_dur, static_cast<typename To::unit>(0));

        vec<Event<To>> converted;
        converted.reserve(data.size());
        for (const auto& event : data) {
            converted.emplace_back(
                time_value(event.time),
                std::max(min_dur, time_value(event.duration)),
                event
            );
        }
        return pyvec<Event<To>>(std::move(converted));
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

    f64                    tpq;
    vec<to_unit>           to_times;
    vec<from_unit>         from_times;
    vec<f64>               factors;

    explicit SecondTimeConverter(const Score<From>& score) :
        tpq(static_cast<f64>(score.ticks_per_quarter)) {
        vec<Tempo<From>> tempos;
        if (score.tempos->empty()) {
            tempos = {
                Tempo<From>(static_cast<from_unit>(0), 500000),
                Tempo<From>(std::numeric_limits<from_unit>::max(), 500000)
            };
        } else {
            tempos.reserve(score.tempos->size() + 2);
            tempos.assign(score.tempos->cbegin(), score.tempos->cend());
            std::sort(tempos.begin(), tempos.end(), [](const auto& lhs, const auto& rhs) {
                return lhs.time < rhs.time;
            });
            if (tempos.front().time != static_cast<from_unit>(0)) {
                tempos.insert(tempos.begin(), Tempo<From>(static_cast<from_unit>(0), 500000));
            }
            tempos.emplace_back(std::numeric_limits<from_unit>::max(), tempos.back().mspq);
        }

        const auto self = static_cast<const Converter*>(this);

        auto pivot_to   = static_cast<to_unit>(0);
        auto pivot_from = static_cast<from_unit>(0);
        auto cur_factor = self->get_factor(tempos.front());

        to_times.reserve(tempos.size());
        from_times.reserve(tempos.size());
        factors.reserve(tempos.size());

        to_times.push_back(pivot_to);
        from_times.push_back(pivot_from);
        factors.push_back(cur_factor);

        for (size_t index = 1; index < tempos.size(); ++index) {
            pivot_to   = self->get_time(tempos[index].time, pivot_to, pivot_from, cur_factor);
            pivot_from = tempos[index].time;
            cur_factor = self->get_factor(tempos[index]);
            to_times.push_back(pivot_to);
            from_times.push_back(pivot_from);
            factors.push_back(cur_factor);
        }
    }

    [[nodiscard]] to_unit time_value(const from_unit time) const {
        const auto self  = static_cast<const Converter*>(this);
        const auto index = range_index(time);
        return self->get_time(time, to_times[index], from_times[index], factors[index]);
    }

    [[nodiscard]] vec<to_unit> time_values(std::span<const from_unit> times) const {
        vec<to_unit> converted;
        converted.reserve(times.size());
        if (times.empty()) { return converted; }

        const auto self = static_cast<const Converter*>(this);
        auto       state = initial_state();
        for (const auto time : times) {
            update_state(state, time);
            converted.push_back(self->get_time(time, state.pivot_to, state.range.first, state.factor));
        }
        return converted;
    }

    template<template<class> class Event>
        requires TimeEvent<Event<From>>
    [[nodiscard]] pyvec<Event<To>> time_vec(const pyvec<Event<From>>& data) const {
        vec<Event<To>> converted;
        converted.reserve(data.size());
        if (data.empty()) { return pyvec<Event<To>>(std::move(converted)); }

        const auto self = static_cast<const Converter*>(this);
        auto       state = initial_state();
        for (const auto& event : data) {
            update_state(state, event.time);
            converted.emplace_back(
                self->get_time(event.time, state.pivot_to, state.range.first, state.factor),
                event
            );
        }
        return pyvec<Event<To>>(std::move(converted));
    }

    template<template<class> class Event>
        requires HashDuration<Event<From>>
    [[nodiscard]] pyvec<Event<To>> duration_vec(
        const pyvec<Event<From>>& data, typename To::unit min_dur
    ) const {
        min_dur = std::max(min_dur, static_cast<typename To::unit>(0));

        vec<Event<To>> converted;
        converted.reserve(data.size());
        if (data.empty()) { return pyvec<Event<To>>(std::move(converted)); }

        const auto self = static_cast<const Converter*>(this);
        auto       state = initial_state();
        for (const auto& event : data) {
            update_state(state, event.time);
            const auto start = self->get_time(
                event.time, state.pivot_to, state.range.first, state.factor
            );

            const auto end_time = event.end();
            update_state(state, end_time);
            const auto end = self->get_time(
                end_time, state.pivot_to, state.range.first, state.factor
            );
            converted.emplace_back(start, std::max(min_dur, end - start), event);
        }
        return pyvec<Event<To>>(std::move(converted));
    }

private:
    [[nodiscard]] size_t range_index(const from_unit time) const {
        return static_cast<size_t>(
            std::upper_bound(from_times.begin(), from_times.end(), time) - from_times.begin() - 1
        );
    }

    [[nodiscard]] RangeState initial_state() const {
        return {{from_times[0], from_times[1]}, to_times[0], factors[0]};
    }

    void update_state(RangeState& state, const from_unit time) const {
        if (time < state.range.first || time >= state.range.second) {
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

    explicit Tick2QuarterConverter(const Score<Tick>& score) :
        SimpleTimeConverter(score), tpq(static_cast<f32>(score.ticks_per_quarter)) {}

    [[nodiscard]] Quarter::unit time(const Tick::unit value) const {
        return static_cast<Quarter::unit>(value) / tpq;
    }
};

struct Quarter2TickConverter : SimpleTimeConverter<Quarter2TickConverter, Tick, Quarter> {
    f32 tpq;

    explicit Quarter2TickConverter(const Score<Quarter>& score) :
        SimpleTimeConverter(score), tpq(static_cast<f32>(score.ticks_per_quarter)) {}

    [[nodiscard]] Tick::unit time(const Quarter::unit value) const {
        return static_cast<Tick::unit>(std::round(value * tpq));
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
        return pivot_to + static_cast<Second::unit>(factor * (time - pivot_from));
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
        return pivot_to + static_cast<Tick::unit>(std::round(factor * (time - pivot_from)));
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

template<TType To, TType From>
[[nodiscard]] auto make_time_converter(const Score<From>& score) {
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

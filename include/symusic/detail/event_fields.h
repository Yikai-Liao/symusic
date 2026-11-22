#pragma once

#include <bit>
#include <limits>
#include <string_view>
#include <tuple>
#include <utility>

#include "symusic/event.h"

namespace symusic::detail {

// The order is the field order exposed by the structure of arrays.
template<TimeEvent E>
inline constexpr auto event_fields = std::tuple{};

template<TType T>
inline constexpr auto event_fields<Note<T>> = std::tuple{
    std::pair{"time", &Note<T>::time},
    std::pair{"duration", &Note<T>::duration},
    std::pair{"pitch", &Note<T>::pitch},
    std::pair{"velocity", &Note<T>::velocity}
};

template<TType T>
inline constexpr auto event_fields<Pedal<T>> = std::tuple{
    std::pair{"time", &Pedal<T>::time},
    std::pair{"duration", &Pedal<T>::duration}
};

template<TType T>
inline constexpr auto event_fields<ControlChange<T>> = std::tuple{
    std::pair{"time", &ControlChange<T>::time},
    std::pair{"number", &ControlChange<T>::number},
    std::pair{"value", &ControlChange<T>::value}
};

template<TType T>
inline constexpr auto event_fields<TimeSignature<T>> = std::tuple{
    std::pair{"time", &TimeSignature<T>::time},
    std::pair{"numerator", &TimeSignature<T>::numerator},
    std::pair{"denominator", &TimeSignature<T>::denominator}
};

template<TType T>
inline constexpr auto event_fields<KeySignature<T>> = std::tuple{
    std::pair{"time", &KeySignature<T>::time},
    std::pair{"key", &KeySignature<T>::key},
    std::pair{"minor", &KeySignature<T>::minor}
};

template<TType T>
inline constexpr auto event_fields<Tempo<T>> = std::tuple{
    std::pair{"time", &Tempo<T>::time},
    std::pair{"microseconds_per_quarter", &Tempo<T>::microseconds_per_quarter}
};

template<TType T>
inline constexpr auto event_fields<PitchBend<T>> = std::tuple{
    std::pair{"time", &PitchBend<T>::time},
    std::pair{"value", &PitchBend<T>::value}
};

template<TType T>
inline constexpr auto event_fields<TextMeta<T>> = std::tuple{
    std::pair{"time", &TextMeta<T>::time},
    std::pair{"text", &TextMeta<T>::text}
};

template<TimeEvent E>
inline constexpr std::string_view event_name{};

template<TType T>
inline constexpr std::string_view event_name<Note<T>> = "Note";
template<TType T>
inline constexpr std::string_view event_name<Pedal<T>> = "Pedal";
template<TType T>
inline constexpr std::string_view event_name<ControlChange<T>> = "ControlChange";
template<TType T>
inline constexpr std::string_view event_name<TimeSignature<T>> = "TimeSignature";
template<TType T>
inline constexpr std::string_view event_name<KeySignature<T>> = "KeySignature";
template<TType T>
inline constexpr std::string_view event_name<Tempo<T>> = "Tempo";
template<TType T>
inline constexpr std::string_view event_name<PitchBend<T>> = "PitchBend";
template<TType T>
inline constexpr std::string_view event_name<TextMeta<T>> = "Text";

template<TimeEvent E>
constexpr void for_event_field(auto&& visit) {
    std::apply([&](const auto&... fields) { (visit(fields.first, fields.second), ...); },
               event_fields<E>);
}

template<TimeEvent E, size_t I>
using EventField = std::remove_cvref_t<decltype(
    std::declval<E>().*std::get<I>(event_fields<E>).second
)>;

// Field constraints apply to each time unit and to every input representation.
template<TimeEvent E, size_t I>
constexpr auto field_range() {
    using T = typename E::ttype;
    using F = EventField<E, I>;
    if constexpr (std::same_as<F, std::string> || std::same_as<F, bool>) {
        return std::pair{0, 0};
    } else if constexpr (I == 0) {
        return std::pair{std::numeric_limits<F>::lowest(), std::numeric_limits<F>::max()};
    } else if constexpr (HashDuration<E> && I == 1) {
        return std::pair{F{0}, std::numeric_limits<F>::max()};
    } else if constexpr (std::same_as<E, Note<T>> || std::same_as<E, ControlChange<T>>) {
        return std::pair{0, 127};
    } else if constexpr (std::same_as<E, KeySignature<T>>) {
        return std::pair{-7, 7};
    } else if constexpr (std::same_as<E, PitchBend<T>>) {
        return std::pair{-8192, 8191};
    } else if constexpr (std::same_as<E, TimeSignature<T>>) {
        return std::pair{1, I == 1 ? 255 : 128};
    } else {
        return std::pair{1, std::numeric_limits<i32>::max()};
    }
}

template<TimeEvent E, size_t I>
constexpr bool power_of_two_field = std::same_as<E, TimeSignature<typename E::ttype>> && I == 2;

template<class E, size_t I>
auto checked_event_field(auto&& input) {
    using F = EventField<E, I>;
    if constexpr (!std::is_arithmetic_v<F> || std::same_as<F, bool>) {
        return F(std::forward<decltype(input)>(input));
    } else {
        const auto [low, high] = field_range<E, I>();
        const auto value = static_cast<f64>(input);
        const auto name = std::get<I>(event_fields<E>).first;
        if (!(value >= low && value <= high)) {
            throw std::invalid_argument(std::string(name) + " must be in "
                + std::to_string(low) + ".." + std::to_string(high)
                + ", got " + std::to_string(input));
        }
        if constexpr (std::integral<F> && std::floating_point<std::remove_cvref_t<decltype(input)>>) {
            if (std::trunc(value) != value) {
                throw std::invalid_argument(std::string(name) + " must be an integer");
            }
        }
        if constexpr (power_of_two_field<E, I>) {
            if (!std::has_single_bit(static_cast<unsigned>(input))) {
                throw std::invalid_argument(std::string(name) + " must be a power of two, got "
                    + std::to_string(input));
            }
        }
        return static_cast<F>(input);
    }
}

template<TimeEvent E, TimeEvent Source>
[[nodiscard]] E retime_event(typename E::unit time, const Source& source) {
    return [&]<size_t... I>(std::index_sequence<I...>) {
        return make_event<E>(time, (source.*std::get<I + 1>(event_fields<Source>).second)...);
    }(std::make_index_sequence<std::tuple_size_v<decltype(event_fields<Source>)> - 1>{});
}

template<TimeEvent E, TimeEvent Source>
    requires HashDuration<E>
[[nodiscard]] E retime_event(typename E::unit time, typename E::unit duration, const Source& source) {
    return [&]<size_t... I>(std::index_sequence<I...>) {
        return make_event<E>(time, duration, (source.*std::get<I + 2>(event_fields<Source>).second)...);
    }(std::make_index_sequence<std::tuple_size_v<decltype(event_fields<Source>)> - 2>{});
}

}  // namespace symusic::detail

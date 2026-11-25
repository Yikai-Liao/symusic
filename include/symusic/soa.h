#pragma once

#include <tuple>
#include <type_traits>
#include <utility>

#include "symusic/detail/event_fields.h"

namespace symusic {

namespace detail {

// The NumPy dtype of a MIDI key-signature tonality is unsigned.
template<TimeEvent E, size_t I>
using ColumnValue = std::
    conditional_t<std::same_as<E, KeySignature<typename E::ttype>> && I == 2, u8, EventField<E, I>>;

}   // namespace detail

// Owns one contiguous array for every event field.
template<TimeEvent E>
class EventColumns {
    static constexpr auto kIndices
        = std::make_index_sequence<std::tuple_size_v<decltype(detail::event_fields<E>)>>{};

    template<size_t... I>
    static auto make_columns(std::index_sequence<I...>) {
        return std::tuple<vec<detail::ColumnValue<E, I>>...>{};
    }

    decltype(make_columns(kIndices)) columns = make_columns(kIndices);

public:
    explicit EventColumns(const auto& events) {
        std::apply([&](auto&... values) { (values.reserve(events.size()), ...); }, columns);
        for (const auto& event : events) {
            [&]<size_t... I>(std::index_sequence<I...>) {
                (std::get<I>(columns).push_back(
                     static_cast<detail::ColumnValue<E, I>>(
                         event.*std::get<I>(detail::event_fields<E>).second
                     )
                 ),
                 ...);
            }(kIndices);
        }
    }

    void for_each(auto&& visit) {
        [&]<size_t... I>(std::index_sequence<I...>) {
            (visit(std::get<I>(detail::event_fields<E>).first, std::get<I>(columns)), ...);
        }(kIndices);
    }
};

}   // namespace symusic

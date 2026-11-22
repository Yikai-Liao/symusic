//
// Per-unit text shared by the docstrings of the Tick, Quarter and Second classes.
//

#pragma once

#include <string_view>

#include "symusic/time_unit.h"

namespace symusic::py {

struct TimeFlavorInfo {
    std::string_view suffix;         // class name suffix
    std::string_view scalar_label;   // Python type of time values
    std::string_view units;          // plural unit name, used in sentences
};

template<typename T>
constexpr TimeFlavorInfo time_flavor();

template<>
constexpr TimeFlavorInfo time_flavor<Tick>() {
    return {.suffix = "Tick", .scalar_label = "int", .units = "ticks"};
}

template<>
constexpr TimeFlavorInfo time_flavor<Quarter>() {
    return {.suffix = "Quarter", .scalar_label = "float", .units = "quarter notes"};
}

template<>
constexpr TimeFlavorInfo time_flavor<Second>() {
    return {.suffix = "Second", .scalar_label = "float", .units = "seconds"};
}

}   // namespace symusic::py

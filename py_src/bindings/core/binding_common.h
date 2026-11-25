//
// Helpers shared by multiple binding translation units.
//

#pragma once

#include <utility>

#include "symusic.h"

namespace symusic::py {

/// Validate and clamp the pitch range tuple provided from Python.
inline std::pair<u8, u8> validate_pitch_range(const std::pair<i64, i64>& range) {
    if (range.first < 0 || range.first > 128 || range.second < 0 || range.second > 128
        || range.first >= range.second) {
        throw std::invalid_argument(
            "Pitch range [" + std::to_string(range.first) + ", " + std::to_string(range.second)
            + ") is invalid"
        );
    }
    // Both endpoints were checked against [0, 128].
    return {static_cast<u8>(range.first), static_cast<u8>(range.second)};
}

template<typename Binder>
void bind_units(nanobind::module_& module, Binder bind) {
    bind.template operator()<Tick>(module, "Tick");
    bind.template operator()<Quarter>(module, "Quarter");
    bind.template operator()<Second>(module, "Second");
}

}   // namespace symusic::py

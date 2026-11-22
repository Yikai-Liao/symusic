#pragma once

#include <cmath>
#include <concepts>
#include <string>
#include <utility>

#include "symusic/mtype.h"

namespace symusic::detail {
std::string strip_non_utf_8(const std::string& str);

// Checked time arithmetic. Only integer time (Tick) can overflow; the float helpers below are
// no-ops for Quarter and Second so that every unit shares one code path. Failures raise
// std::overflow_error (Python OverflowError) for out-of-range values and std::invalid_argument
// (Python ValueError) for NaN. The throwing functions live in utils.cpp and stay out of line so
// the inline fast paths remain a compare and a branch.

#if defined(__GNUC__) || defined(__clang__)
#define SYMUSIC_COLD __attribute__((cold, noinline))
#else
#define SYMUSIC_COLD
#endif

[[noreturn]] SYMUSIC_COLD void throw_i32_overflow(i64 value);
[[noreturn]] SYMUSIC_COLD void throw_f64_not_i32(f64 value);

// Narrow a 64-bit intermediate to i32.
inline i32 narrow_i32(const i64 value) {
    if (!std::in_range<i32>(value)) [[unlikely]] { throw_i32_overflow(value); }
    return static_cast<i32>(value);
}

// Convert a float whose value lies in [INT32_MIN, INT32_MAX] to i32 (truncating); rejects NaN,
// infinities and out-of-range values.
inline i32 f64_to_i32(const f64 value) {
    if (!(value >= -2147483648.0 && value <= 2147483647.0)) [[unlikely]] {
        throw_f64_not_i32(value);
    }
    return static_cast<i32>(value);
}

inline i32 round_to_i32(const f64 value) { return f64_to_i32(std::round(value)); }

// Single-precision overload: [-2^31, 2^31) is exact in f32, so no widening is needed.
inline i32 round_to_i32(const f32 value) {
    const f32 rounded = std::roundf(value);
    if (!(rounded >= -2147483648.0f && rounded < 2147483648.0f)) [[unlikely]] {
        throw_f64_not_i32(static_cast<f64>(rounded));
    }
    return static_cast<i32>(rounded);
}

// Unit-generic helpers: checked for integer units, plain arithmetic for float units. The
// compiler builtins reduce the check to one add and one overflow-flag branch.
template<typename U>
inline U add_time(const U lhs, const U rhs) {
    if constexpr (std::integral<U>) {
#if defined(__GNUC__) || defined(__clang__)
        U result;
        if (__builtin_add_overflow(lhs, rhs, &result)) [[unlikely]] {
            throw_i32_overflow(static_cast<i64>(lhs) + static_cast<i64>(rhs));
        }
        return result;
#else
        return narrow_i32(static_cast<i64>(lhs) + static_cast<i64>(rhs));
#endif
    } else {
        return lhs + rhs;
    }
}

template<typename U>
inline U sub_time(const U lhs, const U rhs) {
    if constexpr (std::integral<U>) {
#if defined(__GNUC__) || defined(__clang__)
        U result;
        if (__builtin_sub_overflow(lhs, rhs, &result)) [[unlikely]] {
            throw_i32_overflow(static_cast<i64>(lhs) - static_cast<i64>(rhs));
        }
        return result;
#else
        return narrow_i32(static_cast<i64>(lhs) - static_cast<i64>(rhs));
#endif
    } else {
        return lhs - rhs;
    }
}

// Wide intermediate for comparisons that must not overflow (i64 for integer units).
template<typename U>
using wide_time_t = std::conditional_t<std::integral<U>, i64, U>;

// Narrow a wide intermediate back to the unit, checking integer units.
template<typename U>
inline U narrow_time(const wide_time_t<U> value) {
    if constexpr (std::integral<U>) {
        return narrow_i32(value);
    } else {
        return value;
    }
}
}   // namespace symusic::detail

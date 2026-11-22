#pragma once

#include "symusic/mtype.h"

namespace symusic {

template<typename T>
    requires std::is_arithmetic_v<T>
struct TimeUnit {
    using unit = T;
};

// Specific Time Units
struct Tick : TimeUnit<i32> {};

struct Quarter : TimeUnit<f32> {};

struct Second : TimeUnit<f32> {};

template<typename T>
concept TType = std::is_base_of_v<TimeUnit<typename T::unit>, T>;

}   // namespace symusic

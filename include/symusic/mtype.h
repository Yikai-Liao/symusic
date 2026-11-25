#pragma once

#include <cstdint>
#include <vector>
#include <memory>

namespace symusic {
// Simple Type Definitions Begin
using u8     = uint8_t;
using i8     = int8_t;
using u16    = uint16_t;
using i16    = int16_t;
using u32    = uint32_t;
using i32    = int32_t;
using u64    = uint64_t;
using i64    = int64_t;
using f32    = float;
using f64    = double;
using size_t = std::size_t;

template<class Value, class Allocator = std::allocator<Value>>
using vec = std::vector<Value, Allocator>;

template<class Value>
using shared = std::shared_ptr<Value>;

}   // namespace symusic

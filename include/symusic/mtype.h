//
// Created by lyk on 23-12-13.
//
#pragma once

#ifndef LIBSYMUSIC_MTYPE_H
#define LIBSYMUSIC_MTYPE_H

#include <cstdint>
#include <vector>
#include <memory>
#include "pyvec.hpp"

namespace symusic {
// Simple Type Definitions Begin
typedef uint8_t  u8;
typedef int8_t   i8;
typedef uint16_t u16;
typedef int16_t  i16;
typedef uint32_t u32;
typedef int32_t  i32;
typedef uint64_t u64;
typedef int64_t  i64;
typedef float    f32;
typedef double   f64;
using size_t = std::size_t;

template<class _Ty, class _Alloc = std::allocator<_Ty>>
using vec = std::vector<_Ty, _Alloc>;

template<class _Ty>
using shared = std::shared_ptr<_Ty>;

using pycontainer::pyvec;
}   // namespace symusic


#endif   // LIBSYMUSIC_MTYPE_H

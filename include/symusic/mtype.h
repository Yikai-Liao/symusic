//
// Created by lyk on 23-12-13.
//
#pragma once

#ifndef LIBSYMUSIC_MTYPE_H
#define LIBSYMUSIC_MTYPE_H

#include <cstdint>
#include <vector>
#include <memory>

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

namespace details {

template<typename T>
shared<vec<shared<T>>> to_shared_vec(vec<T>&& data) {
    shared<vec<T>> capsule = std::make_shared<vec<T>>(std::move(data));
    auto           ans     = std::make_shared<vec<shared<T>>>();
    ans->reserve(capsule->size());
    for (T& item : *capsule) { ans->emplace_back(capsule, &item); }
    return ans;
}

template<typename T>
vec<T> to_native_vec(const shared<vec<shared<T>>>& data) {
    vec<T> ans;
    ans.reserve(data->size());
    for (const shared<T>& item : *data) { ans.push_back(*item); }
    return ans;
}

// concept HasDeepcopy
template<typename T>
concept HasDeepcopy = requires(const T& t) {
    { t.deepcopy() } -> std::same_as<T>;
};

template<typename T>
vec<shared<T>> deepcopy(const vec<shared<T>>& data) {
    auto capsule = std::make_shared<vec<T>>();
    capsule->reserve(data.size());
    vec<shared<T>> ans;
    ans.reserve(data.size());
    for(const auto& event : data) {
        if constexpr (HasDeepcopy<T>) {
            capsule->push_back(std::move(event->deepcopy()));
        } else {
            capsule->push_back(*event);
        }
        ans.emplace_back(capsule, &capsule->back());
    }   return ans;
}

template<typename T>
shared<vec<shared<T>>> deepcopy(const shared<vec<shared<T>>>& data) {
    return std::make_shared<vec<shared<T>>>(std::move(deepcopy(*data)));
}

}   // namespace details

}   // namespace symusic


#endif   // LIBSYMUSIC_MTYPE_H

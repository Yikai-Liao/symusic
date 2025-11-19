//
// Common helpers shared by the nanobind bindings.
//

#pragma once

#include <span>
#include <string>
#include <string_view>

#include "symusic.h"
#include <nanobind/nanobind.h>
#include <nanobind/ndarray.h>
#include <nanobind/operators.h>
#include <pyvec.hpp>

namespace pyutils {

using namespace symusic;
namespace nb = nanobind;

/// Helper alias for describing CPU contiguous ndarrays.
#define NDARR(DTYPE, DIM) nb::ndarray<DTYPE, nb::ndim<DIM>, nb::c_contig, nb::device::cpu>

/// Helper for binding read/write properties on shared pointer based classes.
#define RW_COPY(type, PYNAME, NAME)                        \
    PYNAME, [](const self_t& self) { return self->NAME; }, \
        [](self_t& self, const type& value) { self->NAME = value; }

/// Construct a shared_ptr-wrapped C++ object in-place for nanobind.
template<typename T, typename... Args>
void pyinit(std::shared_ptr<T>* self, Args&&... args) {
    new (self) std::shared_ptr<T>(std::make_shared<T>(std::forward<Args>(args)...));
}

/// Serialize a shared object to bytes using the ZPP format.
template<typename T>
nb::bytes to_bytes(const shared<T>& self) {
    const vec<unsigned char> data = dumps<DataFormat::ZPP>(*self);
    return nb::bytes(reinterpret_cast<const char*>(data.data()), data.size());
}

/// Serialize a python vector wrapper to bytes.
template<TimeEvent T>
nb::bytes vec_to_bytes(const pyvec<T>& self) {
    const vec<unsigned char> data = dumps<DataFormat::ZPP>(self.collect());
    return nb::bytes(reinterpret_cast<const char*>(data.data()), data.size());
}

/// Restore a shared object from previously serialized bytes.
template<typename T>
void from_bytes(shared<T>& self, const nb::bytes& bytes) {
    const auto      data = std::string_view(bytes.c_str(), bytes.size());
    const std::span span(reinterpret_cast<const unsigned char*>(data.data()), data.size());
    auto            ans = std::make_shared<T>(parse<DataFormat::ZPP, T>(span));
    new (&self) shared<T>(std::move(ans));
}

/// Restore a python vector wrapper from bytes.
template<TimeEvent T>
void vec_from_bytes(shared<pyvec<T>>& self, const nb::bytes& bytes) {
    const auto      data = std::string_view(bytes.c_str(), bytes.size());
    const std::span span(reinterpret_cast<const unsigned char*>(data.data()), data.size());
    auto            native_ans = parse<DataFormat::ZPP, vec<T>>(span);
    new (&self) shared<pyvec<T>>(std::make_shared<pyvec<T>>(std::move(native_ans)));
}

namespace details {
template<typename T>
std::shared_ptr<pyvec<T>> to_shared_vec(vec<T>&& vec_) {
    return std::make_shared<pyvec<T>>(std::move(vec_));
}
}   // namespace details

/// Conversion helpers used by the python API to ingest numpy arrays.
template<TType T, typename unit = typename T::unit>
auto note_from_numpy(
    NDARR(unit, 1) time, NDARR(unit, 1) duration, NDARR(i8, 1) pitch, NDARR(i8, 1) velocity
) {
    const auto size = time.size();
    if (size != duration.size() || size != pitch.size() || size != velocity.size()) {
        throw std::invalid_argument("time, duration, pitch, velocity must have the same size");
    }
    vec<Note<T>> ans;
    ans.reserve(size);
    for (size_t i = 0; i < size; ++i) {
        ans.emplace_back(time(i), duration(i), pitch(i), velocity(i));
    }
    return details::to_shared_vec(std::move(ans));
}

template<TType T, typename unit = typename T::unit>
auto pedal_from_numpy(NDARR(unit, 1) time, NDARR(unit, 1) duration) {
    const auto size = time.size();
    if (size != duration.size()) { throw std::invalid_argument("time, duration size mismatch"); }
    vec<Pedal<T>> ans;
    ans.reserve(size);
    for (size_t i = 0; i < size; ++i) { ans.emplace_back(time(i), duration(i)); }
    return details::to_shared_vec(std::move(ans));
}

template<TType T, typename unit = typename T::unit>
auto controlchange_from_numpy(NDARR(unit, 1) time, NDARR(u8, 1) number, NDARR(u8, 1) value) {
    const auto size = time.size();
    if (size != number.size() || size != value.size()) {
        throw std::invalid_argument("time, number, value must have the same size");
    }
    vec<ControlChange<T>> ans;
    ans.reserve(size);
    for (size_t i = 0; i < size; ++i) { ans.emplace_back(time(i), number(i), value(i)); }
    return details::to_shared_vec(std::move(ans));
}

template<TType T, typename unit = typename T::unit>
auto timesig_from_numpy(NDARR(unit, 1) time, NDARR(u8, 1) numerator, NDARR(u8, 1) denominator) {
    const auto size = time.size();
    if (size != numerator.size() || size != denominator.size()) {
        throw std::invalid_argument("time, numerator, denominator must have the same size");
    }
    vec<TimeSignature<T>> ans;
    ans.reserve(size);
    for (size_t i = 0; i < size; ++i) { ans.emplace_back(time(i), numerator(i), denominator(i)); }
    return details::to_shared_vec(std::move(ans));
}

template<TType T, typename unit = typename T::unit>
auto keysig_from_numpy(NDARR(unit, 1) time, NDARR(i8, 1) key, NDARR(u8, 1) tonality) {
    const auto size = time.size();
    if (size != key.size() || size != tonality.size()) {
        throw std::invalid_argument("time, key, tonality must have the same size");
    }
    vec<KeySignature<T>> ans;
    ans.reserve(size);
    for (size_t i = 0; i < size; ++i) { ans.emplace_back(time(i), key(i), tonality(i)); }
    return details::to_shared_vec(std::move(ans));
}

template<TType T, typename unit = typename T::unit>
auto tempo_from_numpy(NDARR(unit, 1) time, NDARR(i32, 1) mspq) {
    const auto size = time.size();
    if (size != mspq.size()) { throw std::invalid_argument("time, mspq must have the same size"); }
    vec<Tempo<T>> ans;
    ans.reserve(size);
    for (size_t i = 0; i < size; ++i) { ans.emplace_back(time(i), mspq(i)); }
    return details::to_shared_vec(std::move(ans));
}

template<TType T, typename unit = typename T::unit>
auto pitchbend_from_numpy(NDARR(unit, 1) time, NDARR(i32, 1) value) {
    const auto size = time.size();
    if (size != value.size()) { throw std::invalid_argument("time, value must have the same size"); }
    vec<PitchBend<T>> ans;
    ans.reserve(size);
    for (size_t i = 0; i < size; ++i) { ans.emplace_back(time(i), value(i)); }
    return details::to_shared_vec(std::move(ans));
}

}   // namespace pyutils

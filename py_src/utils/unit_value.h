#pragma once

#include <variant>

#include <nanobind/nanobind.h>
#include <nanobind/stl/variant.h>

#include "symusic/utils.h"
#include "utils/storage.h"

namespace symusic::py {

namespace nb = nanobind;
using namespace nb::literals;

// All Python wrappers share the same unit dispatch and preserve the ownership of
// the underlying handle. A same-unit lookup checks the unit before mutation.
template<template<class> class Data>
class UnitValue {
    using Storage = std::variant<shared<Data<Tick>>, shared<Data<Quarter>>, shared<Data<Second>>>;
    Storage value_;

public:
    template<class Pointer>
    explicit UnitValue(Pointer ptr) : value_(std::move(ptr)) {}

    template<class Function>
    decltype(auto) visit(Function&& function) const {
        return std::visit(std::forward<Function>(function), value_);
    }

    template<TType T>
    const shared<Data<T>>* find() const {
        return std::get_if<shared<Data<T>>>(&value_);
    }

    template<TType T>
    const shared<Data<T>>& get() const {
        if (const auto* ptr = find<T>()) return *ptr;
        throw std::invalid_argument("Time units must match");
    }
};

template<class Native>
struct NativeUnit {
    using type = typename Native::ttype;
};
template<TimeEvent E>
struct NativeUnit<pyvec<E>> : NativeUnit<E> {};
template<class E>
struct NativeUnit<vec<shared<E>>> : NativeUnit<E> {};

template<class Pointer>
using UnitOf = typename NativeUnit<typename std::remove_cvref_t<Pointer>::element_type>::type;

template<class Function>
decltype(auto) with_unit(nb::handle marker, Function&& function) {
    if (marker.is(nb::type<Tick>())) return function(Tick{});
    if (marker.is(nb::type<Quarter>())) return function(Quarter{});
    if (marker.is(nb::type<Second>())) return function(Second{});
    throw std::invalid_argument("ttype must be Tick, Quarter or Second");
}

template<TType T>
typename T::unit read_time(nb::handle value) {
    if constexpr (std::same_as<T, Tick>) {
        auto index = nb::steal<nb::object>(PyNumber_Index(value.ptr()));
        if (!index.is_valid()) nb::raise_python_error();
        const auto ticks = PyLong_AsLongLong(index.ptr());
        if (ticks == -1 && PyErr_Occurred()) nb::raise_python_error();
        return detail::narrow_i32(ticks);
    } else {
        typename T::unit time;
        if (!nb::try_cast(value, time)) throw nb::type_error("Expected a numeric time");
        return time;
    }
}

using TimeNumber = std::variant<i32, f32>;

template<class Value>
auto unit_class(nb::module_& module, const char* name, const char* doc) {
    const auto signature = std::string("class ") + name + "(typing.Generic[_Unit])";
    auto type = nb::class_<Value>(module, name, nb::is_generic(), nb::sig(signature.c_str()), doc);
    type.def_prop_ro(
        "ttype",
        [](const Value& self) {
            return self.visit([](const auto& ptr) {
                return nb::borrow<nb::type_object>(nb::type<UnitOf<decltype(ptr)>>());
            });
        },
        nb::sig("def ttype(self, /) -> type[Tick] | type[Quarter] | type[Second]"),
        "The marker class for this object's time unit."
    );
    return type;
}

template<class Value, class Function>
nb::object transform(nb::handle python_self, bool inplace, Function&& function) {
    const auto& self   = nb::cast<const Value&>(python_self);
    Value       result = self.visit(std::forward<Function>(function));
    return inplace ? nb::borrow(python_self) : nb::cast(std::move(result));
}

}   // namespace symusic::py

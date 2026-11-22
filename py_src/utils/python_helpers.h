#pragma once

#include <span>

#include <nanobind/ndarray.h>
#include <nanobind/stl/tuple.h>

#include "symusic.h"
#include "utils/unit_value.h"

namespace symusic::py {

inline std::span<const u8> bytes_span(const nb::bytes& bytes) {
    return {static_cast<const u8*>(bytes.data()), bytes.size()};
}

inline nb::bytes python_bytes(std::span<const u8> bytes) {
    return nb::bytes(bytes.data(), bytes.size());
}

// Read an integer without nanobind narrowing it before the field is validated.
inline i64 read_integer(nb::handle input, const char* field) {
    auto index = nb::steal<nb::object>(PyNumber_Index(input.ptr()));
    if (!index.is_valid()) nb::raise_python_error();
    int overflow = 0;
    const auto value = PyLong_AsLongLongAndOverflow(index.ptr(), &overflow);
    if (PyErr_Occurred()) nb::raise_python_error();
    if (overflow) throw std::invalid_argument(std::string(field) + " is out of range");
    return value;
}

inline i32 read_int32(nb::handle input, const char* field) {
    const auto value = read_integer(input, field);
    if (!std::in_range<i32>(value))
        throw std::invalid_argument(std::string(field) + " is out of range");
    return static_cast<i32>(value);
}

template<class Value, class Dump, class Load>
void bind_pickle(nb::class_<Value>& type, Dump dump, Load load) {
    type.def(
            "__getstate__",
            [dump](const Value& self) {
                return self.visit([&](const auto& ptr) {
                    return std::make_tuple(
                        nb::borrow<nb::type_object>(nb::type<UnitOf<decltype(ptr)>>()),
                        python_bytes(dump(ptr))
                    );
                });
            }
    )
        .def(
            "__setstate__",
            [load](Value* self, const std::tuple<nb::type_object, nb::bytes>& state) {
                const auto& [marker, data] = state;
                new (self) Value(with_unit(marker, [&](auto unit) {
                    return Value(load(unit, bytes_span(data)));
                }));
            },
            "state"_a
        );
}

template<template<class> class Data>
void bind_value_pickle(nb::class_<UnitValue<Data>>& type) {
    bind_pickle(
        type,
        [](const auto& ptr) { return serialize(*ptr); },
        [](auto unit, std::span<const u8> bytes) {
            using Native = Data<decltype(unit)>;
            return std::make_shared<Native>(deserialize<Native>(bytes));
        }
    );
}

template<class Owner>
auto owned_array(std::unique_ptr<Owner> owner, auto make_array) {
    nb::capsule capsule(owner.get(), [](void* ptr) noexcept { delete static_cast<Owner*>(ptr); });
    auto*       data = owner.release();
    return make_array(*data, capsule);
}

template<class Scalar>
auto to_numpy_array(vec<Scalar> values) {
    return owned_array(
        std::make_unique<vec<Scalar>>(std::move(values)), [](auto& data, const nb::capsule& owner) {
            // A null data pointer does not represent an empty NumPy array.
            static Scalar empty = 0;
            auto*         ptr   = data.empty() ? &empty : data.data();
            return nb::ndarray<nb::numpy, Scalar>(ptr, {data.size()}, owner);
        }
    );
}

template<TType T>
auto read_times(const nb::iterable& times) {
    vec<typename T::unit> result;
    for (nb::handle time : times) result.push_back(read_time<T>(time));
    return result;
}

template<class Value, class DeepCopy>
void bind_object_ops(nb::class_<Value>& type, DeepCopy deepcopy) {
    const auto shallow = [](const Value& self) {
        return self.visit([](const auto& ptr) {
            using Native = typename std::remove_cvref_t<decltype(ptr)>::element_type;
            return Value(std::make_shared<Native>(*ptr));
        });
    };
    const auto deep = [deepcopy](const Value& self) {
        return self.visit([&](const auto& ptr) { return Value(deepcopy(*ptr)); });
    };
    type.def(
            "copy",
            deep,
            "Return an independent copy of this object and its contents."
    )
        .def("__copy__", shallow)
        .def(
            "__deepcopy__",
            [deep](const Value& self, nb::handle) { return deep(self); },
            "memo"_a.none()
        )
        .def(
            "__repr__",
            [](const Value& self) {
                return self.visit([](const auto& ptr) { return ptr->to_string(); });
            }
        )
        .def(
            "__eq__",
            [](const Value& self, nb::handle other) {
                if (!nb::isinstance<Value>(other)) return false;
                const auto& rhs = nb::cast<const Value&>(other);
                return self.visit([&](const auto& ptr) {
                    const auto* match = rhs.template find<UnitOf<decltype(ptr)>>();
                    return match && (ptr == *match || *ptr == **match);
                });
            },
            "other"_a.none()
        );
    type.attr("__hash__") = nb::none();
    bind_value_pickle(type);
}

}   // namespace symusic::py

//
// Created by lyk on 2024/3/31.
//

#ifndef PY_UTILS_H
#define PY_UTILS_H

#include "symusic.h"
#include "bind_vector_copy.h"
#include "nanobind/nanobind.h"
#include "nanobind/operators.h"
#include <nanobind/ndarray.h>

namespace pyutils {

using symusic::shared;
using symusic::vec;

namespace nb = nanobind;
using namespace symusic;

#define NDARR(DTYPE, DIM) nb::ndarray<DTYPE, nb::ndim<DIM>, nb::c_contig, nb::device::cpu>

#define RW_COPY(type, PYNAME, NAME)                        \
    PYNAME, [](const self_t& self) { return self->NAME; }, \
        [](self_t& self, const type& value) { self->NAME = value; }

template<typename T, typename... Args>
void pyinit(std::shared_ptr<T>* self, Args&&... args) {
    new (self) std::shared_ptr<T>(std::move(std::make_shared<T>(std::forward<Args>(args)...)));
}

template<typename T>
nb::bytes to_bytes(const shared<T>& self) {
    const vec<unsigned char> data = dumps<DataFormat::ZPP>(*self);
    return nb::bytes(reinterpret_cast<const char*>(data.data()), data.size());
}

template<TimeEvent T>
nb::bytes vec_to_bytes(const pyvec<T>& self) {
    const vec<unsigned char> data = dumps<DataFormat::ZPP>(self.collect());
    return nb::bytes(reinterpret_cast<const char*>(data.data()), data.size());
}

template<typename T>
void from_bytes(shared<T>& self, const nb::bytes& bytes) {
    const auto      data = std::string_view(bytes.c_str(), bytes.size());
    const std::span span(reinterpret_cast<const unsigned char*>(data.data()), data.size());
    auto ans = std::make_shared<T>(std::move(symusic::parse<symusic::DataFormat::ZPP, T>(span)));
    new (&self) shared<T>(std::move(ans));
}

template<TimeEvent T>
void vec_from_bytes(shared<pyvec<T>>& self, const nb::bytes& bytes) {
    const auto      data = std::string_view(bytes.c_str(), bytes.size());
    const std::span span(reinterpret_cast<const unsigned char*>(data.data()), data.size());
    auto            native_ans = symusic::parse<symusic::DataFormat::ZPP, vec<T>>(span);
    new (&self) shared<pyvec<T>>(std::make_shared<pyvec<T>>(std::move(native_ans)));
}

template<typename T>
bool operator==(const shared<T>& a, const shared<T>& b) {
    return *a == *b;
}

template<typename T>
bool operator!=(const shared<T>& a, const shared<T>& b) {
    return *a != *b;
}

/*
 *  List of all the events (based on TimeStamp):
 *  - Note(duration: unit, pitch: i8, velocity: i8)
 *  - Pedal(duration: unit)
 *  - ControlChange(number: u8, value: u8)
 *  - TimeSignature(numerator: u8, denominator: u8)
 *  - KeySignature(key: i8, tonality: u8)
 *  - Tempo(mspq: i32)
 *  - PitchBend(value: i32)
 *  - TextMeta(text: string)
 */

namespace details {
template<typename T>
std::shared_ptr<pyvec<T>> to_shared_vec(vec<T>&& vec) {
    return std::make_shared<pyvec<T>>(std::move(vec));
}
}   // namespace details

template<TType T, typename unit = typename T::unit>
auto note_from_numpy(
    NDARR(unit, 1) time, NDARR(unit, 1) duration, NDARR(i8, 1) pitch, NDARR(i8, 1) velocity
) {
    auto size = time.size();
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
    auto size = time.size();
    if (size != duration.size()) {
        throw std::invalid_argument("time, duration must have the same size");
    }
    vec<Pedal<T>> ans;
    ans.reserve(size);
    for (size_t i = 0; i < size; ++i) { ans.emplace_back(time(i), duration(i)); }
    return details::to_shared_vec(std::move(ans));
}

template<TType T, typename unit = typename T::unit>
auto controlchange_from_numpy(NDARR(unit, 1) time, NDARR(u8, 1) number, NDARR(u8, 1) value) {
    auto size = time.size();
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
    auto size = time.size();
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
    auto size = time.size();
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
    auto size = time.size();
    if (size != mspq.size()) { throw std::invalid_argument("time, mspq must have the same size"); }
    vec<Tempo<T>> ans;
    ans.reserve(size);
    for (size_t i = 0; i < size; ++i) { ans.emplace_back(time(i), mspq(i)); }
    return details::to_shared_vec(std::move(ans));
}

template<TType T, typename unit = typename T::unit>
auto textmeta_from_numpy(NDARR(unit, 1) time, NDARR(std::string, 1) text) {
    throw std::runtime_error("TextMeta does not support numpy");
}

template<typename T>
auto bind_time_stamp(nb::module_& m, const std::string& name) {
    typedef typename T::unit unit;
    typedef shared<T>        self_t;
    typedef shared<pyvec<T>> vec_t;

    auto copy_func = [](const shared<T>& self) { return std::make_shared<T>(*self); };

    // clang-format off
    auto event = nb::class_<shared<T>>(m, name.c_str())
        .def_prop_rw(RW_COPY(unit, "time", time))
        .def_prop_ro("ttype", [](const self_t& self) { return typename T::ttype(); })
        .def(
            "shift_time",
            [](const self_t& self, const unit offset, const bool inplace) {
                if (inplace) {
                    self->shift_time_inplace(offset);
                    return self;
                }   return std::make_shared<T>(std::move(self->shift_time(offset)));
            },
            nb::rv_policy::copy,
            nb::arg("offset"),
            nb::arg("inplace") = false,
            "Shift the event time by offset"
        )
        .def("__init__", [](self_t *self, const self_t& other) {
            new (self) shared<T>(std::move(std::make_shared<T>(*other)));
        }, nb::arg("other"))
        .def("copy", copy_func)
        .def("deepcopy", copy_func)
        .def("__copy__", copy_func)
        .def("__deepcopy__", copy_func)
        .def("__repr__", [](const shared<T>& self) { return self->to_string(); })
        .def(nb::self == nb::self)  // NOLINT
        .def(nb::self != nb::self)  // NOLINT
        .def("__getstate__", &to_bytes<T>)
        .def("__setstate__", &from_bytes<T>)
        .def("__use_count", [](const shared<T>& self) { return self.use_count(); })
    ;

    auto vec_class = bind_shared_pyvec<T>(m, (name+"List").c_str())
        .def_prop_ro("ttype", [](const vec_t& self) { return typename T::ttype(); })
        .def("__getstate__", &vec_to_bytes<T>)
        .def("__setstate__", &vec_from_bytes<T>)
        .def("sort", [](vec_t& v, const nb::object & key, const bool reverse, const bool inplace) -> vec_t {
            vec_t ans = inplace? v : std::make_shared<pyvec<T>>(std::move(v->copy()));
            if(key.is_none()) {
                ans->sort(reverse, [](const auto& e) { return e.default_key(); });
            } else {
                ans->sort(reverse, key);
            }
            return ans;
        },  nb::rv_policy::copy,
        nb::arg("key") = nb::none(), nb::arg("reverse") = false, nb::arg("inplace") = true)
        .def("filter", [](vec_t& v, const nb::object & func, const bool inplace) -> vec_t {
            vec_t ans = inplace? v : std::make_shared<pyvec<T>>(std::move(v->copy()));
            if(func.is_none()) {
                throw std::invalid_argument("symusic::filter: You need to provide a function, not None!");
            }
            ans->filter(func);
            return ans;
        }, nb::rv_policy::copy,
        nb::arg("func") = nb::none(), nb::arg("inplace") = true)
    ;
    // clang-format on
    return std::make_tuple(event, vec_class);
}

#define HELPER(x) x

#define ADD_TO_DICT(__COUNT, NAME)                                                  \
    ans[#NAME] = nb::ndarray<nb::numpy, typename decltype(temp->NAME)::value_type>( \
        temp->NAME.data(), {size}, deleter                                          \
    );

#define TO_NUMPY_INNER(ARR_TYPE, ...)                                                             \
    [](const shared<pyvec<self_inner>>& self) {                                                   \
        auto*       temp = new ARR_TYPE<T>(*self);                                                \
        nb::capsule deleter(temp, [](void* p) noexcept { delete static_cast<ARR_TYPE<T>*>(p); }); \
        size_t      size = self->size();                                                          \
        nb::dict    ans{};                                                                        \
        REPEAT_ON(ADD_TO_DICT, __VA_ARGS__)                                                       \
        return ans;                                                                               \
    }

#define TO_NUMPY(...) HELPER(TO_NUMPY_INNER(__VA_ARGS__))


template<TType T>
auto bind_note(nb::module_& m, const std::string& name_) {
    typedef typename T::unit unit;
    typedef shared<Note<T>>  self_t;
    using self_inner = Note<T>;

    const std::string name = "Note" + name_;
    auto [note, note_vec]  = bind_time_stamp<Note<T>>(m, name);

    // auto to_numpy = TO_NUMPY(NoteArr, time, duration, pitch, velocity);
    auto to_numpy = [](const shared<pyvec<self_inner>>& self) {
        auto*       temp = new NoteArr<T>(*self);
        nb::capsule deleter(temp, [](void* p) noexcept { delete static_cast<NoteArr<T>*>(p); });
        size_t      size = self->size();
        nb::dict    ans{};
        ans["time"] = nb::ndarray<nb::numpy, typename decltype(temp->time)::value_type>(
            temp->time.data(), {size}, deleter
        );
        ans["duration"] = nb::ndarray<nb::numpy, typename decltype(temp->duration)::value_type>(
            temp->duration.data(), {size}, deleter
        );
        ans["pitch"] = nb::ndarray<nb::numpy, typename decltype(temp->pitch)::value_type>(
            temp->pitch.data(), {size}, deleter
        );
        ans["velocity"] = nb::ndarray<nb::numpy, typename decltype(temp->velocity)::value_type>(
            temp->velocity.data(), {size}, deleter
        );
        return ans;
    };
    // clang-format off
    note.def_prop_rw(RW_COPY(unit,  "start",    time))
        .def_prop_rw(RW_COPY(unit,  "duration", duration))
        .def_prop_rw(RW_COPY(i8,    "pitch",    pitch))
        .def_prop_rw(RW_COPY(i8,    "velocity", velocity))
        .def("__init__", &pyinit<Note<T>, unit, unit, i8, i8>,
            nb::arg("time"), nb::arg("duration"), nb::arg("pitch"), nb::arg("velocity")=0)
    ;

    note_vec
        .def("to_numpy", to_numpy)
        .def_static("from_numpy", &note_from_numpy<T>)
    ;
    // clang-format on
    return std::make_tuple(note, note_vec);
}

}   // namespace pyutils
#endif   // PY_UTILS_H

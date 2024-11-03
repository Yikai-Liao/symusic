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

using namespace symusic;
namespace nb = nanobind;

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
auto pitchbend_from_numpy(NDARR(unit, 1) time, NDARR(i32, 1) value) {
    auto size = time.size();
    if (size != value.size()) {
        throw std::invalid_argument("time, value must have the same size");
    }
    vec<PitchBend<T>> ans;
    ans.reserve(size);
    for (size_t i = 0; i < size; ++i) { ans.emplace_back(time(i), value(i)); }
    return details::to_shared_vec(std::move(ans));
}

// template<TType T, typename unit = typename T::unit>
// auto textmeta_from_numpy(NDARR(unit, 1) time, NDARR(std::string, 1) text) {
//     throw std::runtime_error("TextMeta does not support numpy");
// }

template<typename T>
auto bind_time_stamp(nb::module_& m, const std::string& name) {
    typedef typename T::unit unit;
    typedef shared<T>        self_t;
    typedef shared<pyvec<T>> vec_t;

    auto copy_func = [](const shared<T>& self) { return std::make_shared<T>(*self); };
    auto deepcopy_func = [](const shared<T>& self, const nb::handle& memo, const nb::handle& nil) {
        return std::make_shared<T>(*self);
    };

    // clang-format off
    auto event = nb::class_<shared<T>>(m, name.c_str())
        .def_prop_rw(RW_COPY(unit, "time", time))
        .def_prop_ro("ttype", [](const self_t& self) { return typename T::ttype(); })
        .def(
            "shift_time",
            [](const self_t& self, const unit offset, const bool inplace) {
                self_t ans = inplace? self : std::make_shared<T>(*self);
                ans->shift_time(offset);
                return ans;
            },
            nb::rv_policy::copy,
            nb::arg("offset"),
            nb::arg("inplace") = false,
            "Shift the event time by offset"
        )
        .def("__init__", [](self_t *self, const self_t& other) {
            new (self) shared<T>(std::move(std::make_shared<T>(*other)));
        }, nb::arg("other"))
        .def("copy", [](const shared<T>& self, const bool deep) {
            return std::make_shared<T>(*self);
        }, nb::arg("deep") = true)
        .def("__copy__", copy_func)
        .def("__deepcopy__", deepcopy_func, nb::arg("memo")=nb::none(), nb::arg("_nil")=nb::none())
        .def("__repr__", [](const shared<T>& self) { return self->to_string(); })
        .def("__eq__", [](const shared<T>& self, const shared<T>& other) -> bool { return *self == *other; })
        .def("__eq__", [](const shared<T>&, const nb::object&) -> bool { return false; })
        .def("__ne__", [](const shared<T>& self, const shared<T>& other) -> bool { return *self != *other; })
        .def("__ne__", [](const shared<T>&, const nb::object&) -> bool { return true; })
        .def(nb::self != nb::self)  // NOLINT
        .def("__getstate__", &to_bytes<T>)
        .def("__setstate__", &from_bytes<T>)
        .def("__use_count", [](const shared<T>& self) { return self.use_count(); })
    ;

    auto vec_class = bind_shared_pyvec<T>(m, (name+"List").c_str())
        .def_prop_ro("ttype", [](const vec_t& self) { return typename T::ttype(); })
        .def("__getstate__", &vec_to_bytes<T>)
        .def("__setstate__", &vec_from_bytes<T>)
        .def("__repr__", [](const vec_t& self) { return fmt::format("{::s}", *self); })
        .def("sort", [](vec_t& v, const nb::object & key, const bool reverse, const bool inplace) -> vec_t {
                vec_t ans = inplace? v : std::make_shared<pyvec<T>>(std::move(v->copy()));
                if(key.is_none()) {
                    ans->sort([](const auto& e) { return e.default_key(); }, reverse);
                } else {
                    ans->sort_shared(nb::cast<nb::callable>(key), reverse);
                }
                return ans;
            },  nb::rv_policy::copy,
            nb::arg("key") = nb::none(), nb::arg("reverse") = false, nb::arg("inplace") = true
        )
        .def("filter", [](vec_t &v, nb::object &func, const bool inplace) -> vec_t {
                vec_t ans = inplace? v : std::make_shared<pyvec<T>>(std::move(v->copy()));
                if(func.is_none()) {
                    throw std::invalid_argument("symusic::filter: You need to provide a function, not None!");
                }
                ans->filter_shared([func](const shared<T>& e) -> bool {
                    return nb::cast<bool>(func(e));
                });
                return ans;
            }, nb::rv_policy::copy,
            nb::arg("function") = nb::none(), nb::arg("inplace") = true
        )
        .def("is_sorted", [](const vec_t& v, nb::object & key, bool reverse) -> bool {
                if(key.is_none()) {
                    return v->is_sorted([](const auto& e) { return e.default_key(); }, reverse);
                } else {
                    return v->is_sorted_shared( nb::cast<nb::callable>(key), reverse);
                }
            }, nb::arg("key") = nb::none(), nb::arg("reverse") = false
        )
        .def("adjust_time", [](vec_t& v, const vec<unit>& original_times, const vec<unit>& new_times, const bool inplace) -> vec_t {
                vec_t ans = inplace? v : std::make_shared<pyvec<T>>(std::move(v->deepcopy()));
                ops::adjust_time_inplace(*ans, original_times, new_times);
                return ans;
            }, nb::rv_policy::copy,
            nb::arg("original_times"), nb::arg("new_times"), nb::arg("inplace") = false
        )
    ;
    // clang-format on
    nb::implicitly_convertible<nb::list, shared<pyvec<T>>>();
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

    auto to_numpy = TO_NUMPY(NoteArr, time, duration, pitch, velocity);

    // clang-format off
    note.def_prop_rw(RW_COPY(unit,  "start",    time))
        .def_prop_rw(RW_COPY(unit,  "duration", duration))
        .def_prop_rw(RW_COPY(i8,    "pitch",    pitch))
        .def_prop_rw(RW_COPY(i8,    "velocity", velocity))
        .def("__init__", &pyinit<Note<T>, unit, unit, i8, i8>,
            nb::arg("time"), nb::arg("duration"), nb::arg("pitch"), nb::arg("velocity")=0)
        .def_prop_rw(
            "end", [](const self_t& self) { return self->end(); },
            [](self_t& self, unit value) { self->duration = value - self->time;})
        .def("empty", [](const self_t& self) { return self->empty(); })
        .def("ent_time", [](const self_t& self) { return self->end(); })
        .def(
            "shift_pitch",
            [](self_t& self, const int8_t offset, const bool inplace) {
                self_t ans = inplace? self : std::make_shared<Note<T>>(*self);
                ans->shift_pitch_inplace(offset);
                return ans;
            }, nb::rv_policy::copy,
            nb::arg("offset"),
            nb::arg("inplace") = false,
            "Shift the pitch by offset"
        )
        .def(
            "shift_velocity",
            [](Note<T>& self, const int8_t offset, const bool inplace) {
                Note<T> ans = inplace? self : Note<T>(self);
                ans.shift_velocity_inplace(offset);
                return ans;
            }, nb::rv_policy::copy,
            nb::arg("offset"),
            nb::arg("inplace") = false,
            "Shift the velocity by offset"
        )
    ;

    note_vec
        .def("numpy", to_numpy)
        .def_static("from_numpy", &note_from_numpy<T>)
    ;
    // clang-format on
    return std::make_tuple(note, note_vec);
}

template<TType T>
auto bind_keysig(nb::module_& m, const std::string& name_) {
    typedef typename T::unit        unit;
    typedef shared<KeySignature<T>> self_t;
    using self_inner = KeySignature<T>;

    const std::string name    = "KeySignature" + name_;
    auto [keysig, keysig_vec] = bind_time_stamp<KeySignature<T>>(m, name);

    auto to_numpy = TO_NUMPY(KeySignatureArr, time, key, tonality);
    // clang-format off
    keysig
        .def_prop_rw(RW_COPY(i8, "key", key))
        .def_prop_rw(RW_COPY(i8, "tonality", tonality))
        .def("__init__", &pyinit<KeySignature<T>, unit, i8, i8>,
            nb::arg("time"), nb::arg("key"), nb::arg("tonality")=0)
    ;

    keysig_vec
        .def("numpy", to_numpy)
        .def_static("from_numpy", &keysig_from_numpy<T>)
    ;
    // clang-format on
    return std::make_tuple(keysig, keysig_vec);
}

template<TType T>
auto bind_timesig(nb::module_& m, const std::string& name_) {
    typedef typename T::unit         unit;
    typedef shared<TimeSignature<T>> self_t;
    using self_inner = TimeSignature<T>;

    const std::string name      = "TimeSignature" + name_;
    auto [timesig, timesig_vec] = bind_time_stamp<TimeSignature<T>>(m, name);

    auto to_numpy = TO_NUMPY(TimeSignatureArr, time, numerator, denominator);

    // clang-format off
    timesig
        .def_prop_rw(RW_COPY(u8, "numerator", numerator))
        .def_prop_rw(RW_COPY(u8, "denominator", denominator))
        .def("__init__", &pyinit<TimeSignature<T>, unit, u8, u8>,
            nb::arg("time"), nb::arg("numerator"), nb::arg("denominator"))
    ;

    timesig_vec
        .def("numpy", to_numpy)
        .def_static("from_numpy", &timesig_from_numpy<T>)
    ;
    // clang-format on
    return std::make_tuple(timesig, timesig_vec);
}

template<TType T>
auto bind_controlchange(nb::module_& m, const std::string& name_) {
    typedef typename T::unit         unit;
    typedef shared<ControlChange<T>> self_t;
    using self_inner = ControlChange<T>;

    const std::string name                  = "ControlChange" + name_;
    auto [controlchange, controlchange_vec] = bind_time_stamp<ControlChange<T>>(m, name);

    auto to_numpy = TO_NUMPY(ControlChangeArr, time, number, value);

    // clang-format off
    controlchange
        .def_prop_rw(RW_COPY(u8, "number", number))
        .def_prop_rw(RW_COPY(u8, "value", value))
        .def("__init__", &pyinit<ControlChange<T>, unit, u8, u8>,
            nb::arg("time"), nb::arg("number"), nb::arg("value"))
    ;

    controlchange_vec
        .def("numpy", to_numpy)
        .def_static("from_numpy", &controlchange_from_numpy<T>)
    ;
    // clang-format on
    return std::make_tuple(controlchange, controlchange_vec);
}

template<TType T>
auto bind_pedal(nb::module_& m, const std::string& name_) {
    typedef typename T::unit unit;
    typedef shared<Pedal<T>> self_t;
    using self_inner = Pedal<T>;

    const std::string name  = "Pedal" + name_;
    auto [pedal, pedal_vec] = bind_time_stamp<Pedal<T>>(m, name);

    auto to_numpy = TO_NUMPY(PedalArr, time, duration);

    // clang-format off
    pedal
        .def_prop_rw(RW_COPY(unit, "start", time))
        .def_prop_rw(RW_COPY(unit, "duration", duration))
        .def_prop_rw("end", [](const self_t& self) { return self->end(); },
            [](self_t& self, unit value) { self->duration = value - self->time;})
        .def("__init__", &pyinit<Pedal<T>, unit, unit>,
            nb::arg("time"), nb::arg("duration"))
    ;

    pedal_vec
        .def("numpy", to_numpy)
        .def_static("from_numpy", &pedal_from_numpy<T>)
    ;
    // clang-format on
    return std::make_tuple(pedal, pedal_vec);
}

template<TType T>
auto bind_tempo(nb::module_& m, const std::string& name_) {
    typedef typename T::unit unit;
    typedef shared<Tempo<T>> self_t;
    using self_inner = Tempo<T>;

    const std::string name  = "Tempo" + name_;
    auto [tempo, tempo_vec] = bind_time_stamp<Tempo<T>>(m, name);

    auto to_numpy = TO_NUMPY(TempoArr, time, mspq);

    // clang-format off
    tempo
        .def_prop_rw(RW_COPY(i32, "mspq", mspq))
        .def("__init__", [](self_t *self, const unit time, std::optional<f64> qpm, std::optional<i32> mspq) {
            new (self) shared<Tempo<T>>(std::move(std::make_shared<Tempo<T>>()));
            self_t & t = *self;
            t->time = time;
            if(qpm.has_value()) {
                t->set_qpm(*qpm);
            } else if(mspq.has_value()) {
                t->mspq = *mspq;
            } else {
                throw std::invalid_argument("qpm or mspq must be specified");
            }
        }, nb::arg("time"), nb::arg("qpm")=nb::none(), nb::arg("mspq")=nb::none())
        .def_prop_rw(
            "tempo", [](const self_t& self) { return self->qpm(); },
            [](self_t& self, f64 value) { self->set_qpm(value); })
        .def_prop_rw(
            "qpm", [](const self_t& self) { return self->qpm(); },
            [](self_t& self, f64 value) { self->set_qpm(value); })
    ;

    tempo_vec
        .def("numpy", to_numpy)
        .def_static("from_numpy", &tempo_from_numpy<T>)
    ;
    // clang-format on
    return std::make_tuple(tempo, tempo_vec);
}

template<TType T>
auto bind_pitchbend(nb::module_& m, const std::string& name_) {
    typedef typename T::unit     unit;
    typedef shared<PitchBend<T>> self_t;
    using self_inner = PitchBend<T>;

    const std::string name          = "PitchBend" + name_;
    auto [pitchbend, pitchbend_vec] = bind_time_stamp<PitchBend<T>>(m, name);

    auto to_numpy = TO_NUMPY(PitchBendArr, time, value);

    // clang-format off
    pitchbend
        .def_prop_rw(RW_COPY(i32, "value", value))
        .def("__init__", &pyinit<PitchBend<T>, unit, i32>,
            nb::arg("time"), nb::arg("value"))
    ;

    pitchbend_vec
        .def("numpy", to_numpy)
        .def_static("from_numpy", &pitchbend_from_numpy<T>)
    ;
    // clang-format on
    return std::make_tuple(pitchbend, pitchbend_vec);
}

template<TType T>
auto bind_textmeta(nb::module_& m, const std::string& name_) {
    typedef typename T::unit    unit;
    typedef shared<TextMeta<T>> self_t;
    using self_inner = TextMeta<T>;

    const std::string name        = "TextMeta" + name_;
    auto [textmeta, textmeta_vec] = bind_time_stamp<TextMeta<T>>(m, name);

    // clang-format off
    textmeta
        .def_prop_rw(RW_COPY(std::string, "text", text))
        .def("__init__", &pyinit<TextMeta<T>, unit, std::string>,
            nb::arg("time"), nb::arg("text"))
    ;

    textmeta_vec
        .def("numpy", [](const self_t & self) {
            throw std::runtime_error("TextMeta does not support numpy");
        })
        .def_static("from_numpy", [](){ throw std::runtime_error("TextMeta does not support from_numpy"); })
    ;
    // clang-format on
    return std::make_tuple(textmeta, textmeta_vec);
}


}   // namespace pyutils
#endif   // PY_UTILS_H

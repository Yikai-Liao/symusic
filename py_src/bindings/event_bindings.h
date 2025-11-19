//
// Bindings for the time based events (note, pedal, tempo, ...).
//

#pragma once

#include <optional>
#include <tuple>

#include "MetaMacro.h"
#include "../utils/python_helpers.h"
#include "../utils/vector_bindings.h"
#include <fmt/format.h>

namespace pyutils {

namespace nb = nanobind;
using namespace symusic;

template<typename T>
auto bind_time_stamp(nb::module_& m, const std::string& name) {
    using unit   = typename T::unit;
    using self_t = shared<T>;
    using vec_t  = shared<pyvec<T>>;

    auto copy_func     = [](const shared<T>& self) { return std::make_shared<T>(*self); };
    auto deepcopy_func = [](const shared<T>& self, const nb::handle&, const nb::handle&) {
        return std::make_shared<T>(*self);
    };

    // clang-format off
    auto event = nb::class_<shared<T>>(m, name.c_str())
        .def_prop_rw(RW_COPY(unit, "time", time))
        .def_prop_ro("ttype", [](const self_t&) { return typename T::ttype(); })
        .def(
            "shift_time",
            [](const self_t& self, const unit offset, const bool inplace) {
                self_t ans = inplace ? self : std::make_shared<T>(*self);
                ans->shift_time(offset);
                return ans;
            },
            nb::rv_policy::copy,
            nb::arg("offset"),
            nb::arg("inplace") = false,
            "Shift the event time by offset"
        )
        .def("__init__", [](self_t *self, const self_t& other) {
            new (self) shared<T>(std::make_shared<T>(*other));
        }, nb::arg("other"))
        .def("copy", [](const shared<T>& self, const bool /*deep*/) {
            return std::make_shared<T>(*self);
        }, nb::arg("deep") = true)
        .def("__copy__", copy_func)
        .def("__deepcopy__", deepcopy_func, nb::arg("memo") = nb::none(), nb::arg("_nil") = nb::none())
        .def("__repr__", [](const shared<T>& self) { return self->to_string(); })
        .def("__eq__", [](const shared<T>& self, const shared<T>& other) { return *self == *other; })
        .def("__eq__", [](const shared<T>&, const nb::object&) { return false; })
        .def("__ne__", [](const shared<T>& self, const shared<T>& other) { return *self != *other; })
        .def("__ne__", [](const shared<T>&, const nb::object&) { return true; })
        .def(nb::self != nb::self) // NOLINT
        .def("__getstate__", &to_bytes<T>)
        .def("__setstate__", &from_bytes<T>)
        .def("__use_count", [](const shared<T>& self) { return self.use_count(); })
    ;

    auto vec_class = bind_shared_pyvec<T>(m, (name + "List").c_str())
        .def_prop_ro("ttype", [](const vec_t&) { return typename T::ttype(); })
        .def("__getstate__", &vec_to_bytes<T>)
        .def("__setstate__", &vec_from_bytes<T>)
        .def("__repr__", [](const vec_t& self) { return fmt::format("{::s}", *self); })
        .def("sort", [](vec_t& v, const nb::object & key, const bool reverse, const bool inplace) -> vec_t {
                vec_t ans = inplace ? v : std::make_shared<pyvec<T>>(v->copy());
                if (key.is_none()) {
                    ans->sort([](const auto& e) { return e.default_key(); }, reverse);
                } else {
                    ans->sort_shared(nb::cast<nb::callable>(key), reverse);
                }
                return ans;
            },
            nb::rv_policy::copy,
            nb::arg("key") = nb::none(),
            nb::arg("reverse") = false,
            nb::arg("inplace") = true
        )
        .def("filter", [](vec_t &v, nb::object &func, const bool inplace) -> vec_t {
                if (func.is_none()) {
                    throw std::invalid_argument("symusic::filter expects a callable");
                }
                vec_t ans = inplace ? v : std::make_shared<pyvec<T>>(v->copy());
                ans->filter_shared([func](const shared<T>& e) { return nb::cast<bool>(func(e)); });
                return ans;
            },
            nb::rv_policy::copy,
            nb::arg("function"),
            nb::arg("inplace") = true
        )
        .def("is_sorted", [](const vec_t& v, nb::object & key, bool reverse) -> bool {
                if (key.is_none()) {
                    return v->is_sorted([](const auto& e) { return e.default_key(); }, reverse);
                }
                return v->is_sorted_shared(nb::cast<nb::callable>(key), reverse);
            },
            nb::arg("key") = nb::none(),
            nb::arg("reverse") = false
        )
        .def("adjust_time", [](vec_t& v, const vec<unit>& original_times, const vec<unit>& new_times, const bool inplace) -> vec_t {
                vec_t ans = inplace ? v : std::make_shared<pyvec<T>>(v->deepcopy());
                ops::adjust_time_inplace(*ans, original_times, new_times);
                return ans;
            },
            nb::rv_policy::copy,
            nb::arg("original_times"),
            nb::arg("new_times"),
            nb::arg("inplace") = false
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
    using unit       = typename T::unit;
    using self_t     = shared<Note<T>>;
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
            nb::arg("time"), nb::arg("duration"), nb::arg("pitch"), nb::arg("velocity") = 0)
        .def_prop_rw(
            "end",
            [](const self_t& self) { return self->end(); },
            [](self_t& self, unit value) { self->duration = value - self->time;})
        .def("empty", [](const self_t& self) { return self->empty(); })
        .def("ent_time", [](const self_t& self) { return self->end(); })
        .def(
            "shift_pitch",
            [](self_t& self, const int8_t offset, const bool inplace) {
                self_t ans = inplace ? self : std::make_shared<Note<T>>(*self);
                ans->shift_pitch_inplace(offset);
                return ans;
            }, nb::rv_policy::copy,
            nb::arg("offset"),
            nb::arg("inplace") = false,
            "Shift the pitch by offset"
        )
        .def(
            "shift_velocity",
            [](self_t& self, const int8_t offset, const bool inplace) {
                self_t ans = inplace ? self : std::make_shared<Note<T>>(*self);
                ans->shift_velocity_inplace(offset);
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
    using unit       = typename T::unit;
    using self_t     = shared<KeySignature<T>>;
    using self_inner = KeySignature<T>;

    const std::string name    = "KeySignature" + name_;
    auto [keysig, keysig_vec] = bind_time_stamp<KeySignature<T>>(m, name);

    auto to_numpy = TO_NUMPY(KeySignatureArr, time, key, tonality);

    // clang-format off
    keysig.def_prop_rw(RW_COPY(i8,    "key", key))
        .def_prop_rw(RW_COPY(u8,      "tonality", tonality))
        .def("__init__", &pyinit<KeySignature<T>, unit, i8, u8>,
            nb::arg("time"), nb::arg("key"), nb::arg("tonality"))
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
    using unit       = typename T::unit;
    using self_t     = shared<TimeSignature<T>>;
    using self_inner = TimeSignature<T>;

    const std::string name      = "TimeSignature" + name_;
    auto [timesig, timesig_vec] = bind_time_stamp<TimeSignature<T>>(m, name);

    auto to_numpy = TO_NUMPY(TimeSignatureArr, time, numerator, denominator);

    // clang-format off
    timesig.def_prop_rw(RW_COPY(u8, "numerator", numerator))
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
    using unit       = typename T::unit;
    using self_t     = shared<ControlChange<T>>;
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
    using unit       = typename T::unit;
    using self_t     = shared<Pedal<T>>;
    using self_inner = Pedal<T>;

    const std::string name  = "Pedal" + name_;
    auto [pedal, pedal_vec] = bind_time_stamp<Pedal<T>>(m, name);

    auto to_numpy = TO_NUMPY(PedalArr, time, duration);

    // clang-format off
    pedal
        .def_prop_rw(RW_COPY(unit, "start", time))
        .def_prop_rw(RW_COPY(unit, "duration", duration))
        .def_prop_rw(
            "end",
            [](const self_t& self) { return self->end(); },
            [](self_t& self, unit value) { self->duration = value - self->time; })
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
    using unit       = typename T::unit;
    using self_t     = shared<Tempo<T>>;
    using self_inner = Tempo<T>;

    const std::string name  = "Tempo" + name_;
    auto [tempo, tempo_vec] = bind_time_stamp<Tempo<T>>(m, name);

    auto to_numpy = TO_NUMPY(TempoArr, time, mspq);

    // clang-format off
    tempo
        .def_prop_rw(RW_COPY(i32, "mspq", mspq))
        .def("__init__", [](self_t* self, const unit time, std::optional<f64> qpm, std::optional<i32> mspq) {
            new (self) self_t(std::make_shared<Tempo<T>>());
            self_t& t = *self;
            t->time   = time;
            if (qpm.has_value()) {
                t->set_qpm(*qpm);
            } else if (mspq.has_value()) {
                t->mspq = *mspq;
            } else {
                throw std::invalid_argument("qpm or mspq must be specified");
            }
        }, nb::arg("time"), nb::arg("qpm") = nb::none(), nb::arg("mspq") = nb::none())
        .def_prop_rw(
            "tempo",
            [](const self_t& self) { return self->qpm(); },
            [](self_t& self, f64 value) { self->set_qpm(value); })
        .def_prop_rw(
            "qpm",
            [](const self_t& self) { return self->qpm(); },
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
    using unit       = typename T::unit;
    using self_t     = shared<PitchBend<T>>;
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
    using unit   = typename T::unit;
    using self_t = shared<TextMeta<T>>;

    const std::string name        = "TextMeta" + name_;
    auto [textmeta, textmeta_vec] = bind_time_stamp<TextMeta<T>>(m, name);

    // clang-format off
    textmeta
        .def_prop_rw(RW_COPY(std::string, "text", text))
        .def("__init__", &pyinit<TextMeta<T>, unit, std::string>,
            nb::arg("time"), nb::arg("text"))
    ;

    textmeta_vec
        .def("numpy", [](const self_t&) {
            throw std::runtime_error("TextMeta does not support numpy");
        })
        .def_static("from_numpy", []() {
            throw std::runtime_error("TextMeta does not support from_numpy");
        })
    ;
    // clang-format on
    return std::make_tuple(textmeta, textmeta_vec);
}

#undef TO_NUMPY
#undef TO_NUMPY_INNER
#undef ADD_TO_DICT
#undef HELPER

}   // namespace pyutils

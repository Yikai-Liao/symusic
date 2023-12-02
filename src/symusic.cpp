//
// Created by lyk on 23-9-20.
//
#include <string>
#include <pybind11/pybind11.h>
#include <pybind11/stl_bind.h>
//#include <pybind11/functional.h>
//#include <pybind11/eigen.h>
#include "Score.hpp"

namespace py = pybind11;
using namespace score;

#define OPAQUE_VEC(i, TYPE) \
    PYBIND11_MAKE_OPAQUE(vec<TYPE<Tick>>) \
    PYBIND11_MAKE_OPAQUE(vec<TYPE<Quarter>>) \
    PYBIND11_MAKE_OPAQUE(vec<TYPE<Second>>)

REPEAT_ON(OPAQUE_VEC, Note, ControlChange, score::TimeSignature, score::KeySignature, Tempo, PitchBend, TextMeta, Track)


// a template functions for binding all specializations of Note, and return it
template<typename T>
py::class_<Note<T>> bind_note_class(py::module &m, const std::string & name_) {
    typedef typename T::unit unit;
    auto name = "Note" + name_;
    py::bind_vector<vec<Note<T>>>(m, name + "List")
        .def("sort_inplace", [](vec<Note<T>> &self) {
            utils::sort_by_time(self);
            return self;
        })
        .def("sort", [](const vec<Note<T>> &self) {
            auto copy = self;
            utils::sort_by_time(copy);
            return copy;
        })
        .def("__repr__", [](const vec<Note<T>> &self) {
            return to_string(self);
        });
    
    py::implicitly_convertible<py::list, vec<Note<T>>>();
    
    return py::class_<Note<T>>(m, name.c_str())
        .def(py::init<unit, unit, int8_t, int8_t>())
        .def(py::init<const Note<T> &>(), "Copy constructor")
        .def_readwrite("start", &Note<T>::time)
        .def_readwrite("time", &Note<T>::time)
        .def_readwrite("duration", &Note<T>::duration)
        .def_readwrite("pitch", &Note<T>::pitch)
        .def_readwrite("velocity", &Note<T>::velocity)
        .def("empty", &Note<T>::empty, "duration <= 0 or velocity <= 0")
        .def("copy", &Note<T>::copy, "Deep copy the note", py::return_value_policy::move)
        .def("__copy__", &Note<T>::copy, "Deep copy the note")
        .def("__deepcopy__", &Note<T>::copy, "Deep copy the note")
        .def("end", &Note<T>::end)
        .def("end_time", &Note<T>::end)
        .def("shift_pitch", &Note<T>::shift_pitch, py::return_value_policy::move)
        .def("shift_velocity", &Note<T>::shift_velocity, py::return_value_policy::move)
        .def("shift_time", &Note<T>::shift_time)
        // def all inplace operation
        .def("shift_pitch_inplace", &Note<T>::shift_pitch_inplace, py::return_value_policy::reference)
        .def("shift_velocity_inplace", &Note<T>::shift_velocity_inplace, py::return_value_policy::reference)
        .def("shift_time_inplace", &Note<T>::shift_time_inplace, py::return_value_policy::reference)
        .def("__repr__", &Note<T>::to_string);
}

// bind score::KeySignature<T>
template<typename T>
py::class_<score::KeySignature<T>> bind_key_signature_class(py::module &m, const std::string & name_) {
    typedef typename T::unit unit;
    auto name = "KeySignature" + name_;
    py::bind_vector<vec<score::KeySignature<T>>>(m, name + "List")
        .def("sort_inplace", [](vec<score::KeySignature<T>> &self) {
            utils::sort_by_time(self);
            return self;
        })
        .def("sort", [](const vec<score::KeySignature<T>> &self) {
            auto copy = self;
            utils::sort_by_time(copy);
            return copy;
        })
        .def("__repr__", [](const vec<score::KeySignature<T>> &self) {
            return to_string(self);
        });

    py::implicitly_convertible<py::list, vec<score::KeySignature<T>>>();
    
    return py::class_<score::KeySignature<T>>(m, name.c_str())
        .def(py::init<unit, i8, u8>())
        .def(py::init<const score::KeySignature<T> &>(), "Copy constructor")
        .def_readwrite("time", &score::KeySignature<T>::time)
        .def_readwrite("key", &score::KeySignature<T>::key)
        .def_readwrite("tonality", &score::KeySignature<T>::tonality)
        .def("shift_time", &score::KeySignature<T>::shift_time)
        .def("shift_time_inplace", &score::KeySignature<T>::shift_time_inplace)
        .def("copy", &score::KeySignature<T>::copy, "Deep copy", py::return_value_policy::move)
        .def("__copy__", &score::KeySignature<T>::copy, "Deep copy")
        .def("__deepcopy__", &score::KeySignature<T>::copy, "Deep copy")
        .def("__repr__", &score::KeySignature<T>::to_string);
}

// bind score::TimeSignature<T>
template<typename T>
py::class_<score::TimeSignature<T>> bind_time_signature_class(py::module &m, const std::string & name_) {
    typedef typename T::unit unit;
    auto name = "TimeSignature" + name_;
    py::bind_vector<vec<score::TimeSignature<T>>>(m, name + "List")
        .def("sort_inplace", [](vec<score::TimeSignature<T>> &self) {
            utils::sort_by_time(self);
            return self;
        })
        .def("sort", [](const vec<score::TimeSignature<T>> &self) {
            auto copy = self;
            utils::sort_by_time(copy);
            return copy;
        })
        .def("__repr__", [](const vec<score::TimeSignature<T>> &self) {
            return to_string(self);
        });

    py::implicitly_convertible<py::list, vec<score::TimeSignature<T>>>();
    
    return py::class_<score::TimeSignature<T>>(m, name.c_str())
        .def(py::init<unit, u8, u8>())
        .def(py::init<const score::TimeSignature<T> &>(), "Copy constructor")
        .def_readwrite("time", &score::TimeSignature<T>::time)
        .def_readwrite("numerator", &score::TimeSignature<T>::numerator)
        .def_readwrite("denominator", &score::TimeSignature<T>::denominator)
        .def("shift_time", &score::TimeSignature<T>::shift_time)
        .def("shift_time_inplace", &score::TimeSignature<T>::shift_time_inplace)
        .def("copy", &score::TimeSignature<T>::copy, "Deep copy", py::return_value_policy::move)
        .def("__copy__", &score::TimeSignature<T>::copy, "Deep copy")
        .def("__deepcopy__", &score::TimeSignature<T>::copy, "Deep copy")
        .def("__repr__", &score::TimeSignature<T>::to_string);
}

// bind score::ControlChange<T>
template<typename T>
py::class_<score::ControlChange<T>> bind_control_change_class(py::module &m, const std::string & name_) {
    typedef typename T::unit unit;
    auto name = "ControlChange" + name_;
    py::bind_vector<vec<score::ControlChange<T>>>(m, name + "List")
        .def("sort_inplace", [](vec<score::ControlChange<T>> &self) {
            utils::sort_by_time(self);
            return self;
        })
        .def("sort", [](const vec<score::ControlChange<T>> &self) {
            auto copy = self;
            utils::sort_by_time(copy);
            return copy;
        })
        .def("__repr__", [](const vec<score::ControlChange<T>> &self) {
            return to_string(self);
        });

    py::implicitly_convertible<py::list, vec<score::ControlChange<T>>>();

    return py::class_<score::ControlChange<T>>(m, name.c_str())
        .def(py::init<unit, u8, u8>())
        .def(py::init<const score::ControlChange<T> &>(), "Copy constructor")
        .def_readwrite("time", &score::ControlChange<T>::time)
        .def_readwrite("value", &score::ControlChange<T>::value)
        .def("shift_time", &score::ControlChange<T>::shift_time)
        .def("shift_time_inplace", &score::ControlChange<T>::shift_time_inplace)
        .def("copy", &score::ControlChange<T>::copy, "Deep copy", py::return_value_policy::move)
        .def("__copy__", &score::ControlChange<T>::copy, "Deep copy")
        .def("__deepcopy__", &score::ControlChange<T>::copy, "Deep copy")
        .def("__repr__", &score::ControlChange<T>::to_string);
}

// bind score::Tempo<T>
template<typename T>
py::class_<score::Tempo<T>> bind_tempo_class(py::module &m, const std::string & name_) {
    typedef typename T::unit unit;
    auto name = "Tempo" + name_;
    py::bind_vector<vec<score::Tempo<T>>>(m, name + "List")
        .def("sort_inplace", [](vec<score::Tempo<T>> &self) {
            utils::sort_by_time(self);
            return self;
        })
        .def("sort", [](const vec<score::Tempo<T>> &self) {
            auto copy = self;
            utils::sort_by_time(copy);
            return copy;
        })
        .def("__repr__", [](const vec<score::Tempo<T>> &self) {
            return to_string(self);
        });

    py::implicitly_convertible<py::list, vec<score::Tempo<T>>>();
    
    return py::class_<score::Tempo<T>>(m, name.c_str())
        .def(py::init<unit, float>())
        .def(py::init<const score::Tempo<T> &>(), "Copy constructor")
        .def_readwrite("time", &score::Tempo<T>::time)
        .def_readwrite("tempo", &score::Tempo<T>::qpm)
        .def("shift_time", &score::Tempo<T>::shift_time)
        .def("shift_time_inplace", &score::Tempo<T>::shift_time_inplace)
        .def("copy", &score::Tempo<T>::copy, "Deep copy", py::return_value_policy::move)
        .def("__copy__", &score::Tempo<T>::copy, "Deep copy")
        .def("__deepcopy__", &score::Tempo<T>::copy, "Deep copy")
        .def("__repr__", &score::Tempo<T>::to_string);
}

// bind score::PitchBend<T>
template<typename T>
py::class_<score::PitchBend<T>> bind_pitch_bend_class(py::module &m, const std::string & name_) {
    typedef typename T::unit unit;
    auto name = "PitchBend" + name_;
    
    py::bind_vector<vec<score::PitchBend<T>>>(m, name + "List")
        .def("sort_inplace", [](vec<score::PitchBend<T>> &self) {
            utils::sort_by_time(self);
            return self;
        })
        .def("sort", [](const vec<score::PitchBend<T>> &self) {
            auto copy = self;
            utils::sort_by_time(copy);
            return copy;
        })
        .def("__repr__", [](const vec<score::PitchBend<T>> &self) {
            return to_string(self);
        });

    py::implicitly_convertible<py::list, vec<score::PitchBend<T>>>();
    
    return py::class_<score::PitchBend<T>>(m, name.c_str())
        .def(py::init<unit, i32>())
        .def(py::init<const score::PitchBend<T> &>(), "Copy constructor")
        .def_readwrite("time", &score::PitchBend<T>::time)
        .def_readwrite("value", &score::PitchBend<T>::value)
        .def("shift_time", &score::PitchBend<T>::shift_time)
        .def("shift_time_inplace", &score::PitchBend<T>::shift_time_inplace)
        .def("copy", &score::PitchBend<T>::copy, "Deep copy", py::return_value_policy::move)
        .def("__copy__", &score::PitchBend<T>::copy, "Deep copy")
        .def("__deepcopy__", &score::PitchBend<T>::copy, "Deep copy")
        .def("__repr__", &score::PitchBend<T>::to_string);
}

// bind score::TextMeta<T>
template<typename T>
py::class_<score::TextMeta<T>> bind_text_meta_class(py::module &m, const std::string & name_) {
    typedef typename T::unit unit;
    auto name = "TextMeta" + name_;
    
    py::bind_vector<vec<score::TextMeta<T>>>(m, name + "List")
        .def("sort_inplace", [](vec<score::TextMeta<T>> &self) {
            utils::sort_by_time(self);
            return self;
        })
        .def("sort", [](const vec<score::TextMeta<T>> &self) {
            auto copy = self;
            utils::sort_by_time(copy);
            return copy;
        })
        .def("__repr__", [](const vec<score::TextMeta<T>> &self) {
            return to_string(self);
        });

    py::implicitly_convertible<py::list, vec<score::TextMeta<T>>>();

    return py::class_<score::TextMeta<T>>(m, name.c_str())
        .def(py::init<unit, std::string &>())
        .def(py::init<const score::TextMeta<T> &>(), "Copy constructor")
        .def_readwrite("time", &score::TextMeta<T>::time)
        .def_readwrite("text", &score::TextMeta<T>::text)
        .def("shift_time", &score::TextMeta<T>::shift_time)
        .def("shift_time_inplace", &score::TextMeta<T>::shift_time_inplace)
        .def("copy", &score::TextMeta<T>::copy, "Deep copy", py::return_value_policy::move)
        .def("__copy__", &score::TextMeta<T>::copy, "Deep copy")
        .def("__deepcopy__", &score::TextMeta<T>::copy, "Deep copy")
        .def("__repr__", &score::TextMeta<T>::to_string);
}

// bind score::Track<T>
template<typename T>
py::class_<score::Track<T>> bind_track_class(py::module &m, const std::string & name_) {
    typedef typename T::unit unit;
    auto name = "Track" + name_;

    py::bind_vector<vec<score::Track<T>>>(m, name + "List")
        .def("__repr__", [](const vec<score::Track<T>> &self) {
            return to_string(self);
        });

    py::implicitly_convertible<py::list, vec<score::Track<T>>>();

    return py::class_<score::Track<T>>(m, name.c_str())
        .def(py::init<>())
        .def(py::init<const score::Track<T> &>(), "Copy constructor")
        .def("copy", &score::Track<T>::copy, "Deep copy", py::return_value_policy::move)
        .def("__copy__", &score::Track<T>::copy, "Deep copy")
        .def("__deepcopy__", &score::Track<T>::copy, "Deep copy")
        .def("__repr__", &score::Track<T>::to_string)
        .def_readwrite("notes", &score::Track<T>::notes)
        .def_readwrite("controls", &score::Track<T>::controls)
        .def_readwrite("pitch_bends", &score::Track<T>::pitch_bends)
        .def("sort", &score::Track<T>::sort, py::return_value_policy::move)
        .def("shift_pitch", &score::Track<T>::shift_pitch)
        .def("shift_time", &score::Track<T>::shift_time)
        .def("shift_velocity", &score::Track<T>::shift_velocity)
        .def("clip", &Track<T>::clip, "Clip notes and controls to a given time range")
        .def("note_num", &score::Track<T>::note_num)
        .def("start", &score::Track<T>::start)
        .def("end", &Track<T>::end);
}

// bind score::Score<T>
template<typename T>
py::class_<Score<T>> bind_score_class(py::module &m, const std::string & name_) {
    typedef typename T::unit unit;
    auto name = "Score" + name_;
    return py::class_<Score<T>>(m, name.c_str())
        .def(py::init<>())
        .def(py::init<const Score<T> &>(), "Copy constructor")
        .def(py::init(&Score<T>::from_file))
        .def("copy", &Score<T>::copy, "Deep copy", py::return_value_policy::move)
        .def("__copy__", &Score<T>::copy, "Deep copy")
        .def("__deepcopy__", &Score<T>::copy, "Deep copy")
        .def("__repr__", &Score<T>::to_string)
        .def_property_readonly_static("from_file", [](const py::object &) {
            return py::cpp_function([](std::string &x) { return Score<T>::from_file(x); });
        })  // binding class method in an erratic way: https://github.com/pybind/pybind11/issues/1693
        .def_readwrite("ticks_per_quarter", &Score<T>::ticks_per_quarter)
        .def_readwrite("tracks", &Score<T>::tracks)
        .def_readwrite("tempos", &Score<T>::tempos)
        .def_readwrite("time_signatures", &Score<T>::time_signatures)
        .def_readwrite("key_signatures", &Score<T>::key_signatures)
        .def_readwrite("lyrics", &Score<T>::lyrics)
        .def_readwrite("markers", &Score<T>::markers)
        .def("empty", &Score<T>::empty)
        .def("sort", &Score<T>::sort, py::return_value_policy::move)
        .def("sort_inplace", &Score<T>::sort_inplace)
        .def("shift_pitch", &Score<T>::shift_pitch)
        .def("shift_time", &Score<T>::shift_time)
        .def("shift_velocity", &Score<T>::shift_velocity);
}

py::module & core_module(py::module & m){
    const std::string tick = "Tick", quarter = "Quarter", second = "Second";

    bind_note_class<Tick>(m, tick);
    bind_note_class<Quarter>(m, quarter);
    bind_note_class<Second>(m, second);


    bind_control_change_class<Tick>(m, tick);
    bind_control_change_class<Quarter>(m, quarter);
    bind_control_change_class<Second>(m, second);

    bind_tempo_class<Tick>(m, tick);
    bind_tempo_class<Quarter>(m, quarter);
    bind_tempo_class<Second>(m, second);

    bind_time_signature_class<Tick>(m, tick);
    bind_time_signature_class<Quarter>(m, quarter);
    bind_time_signature_class<Second>(m, second);

    bind_key_signature_class<Tick>(m, tick);
    bind_key_signature_class<Quarter>(m, quarter);
    bind_key_signature_class<Second>(m, second);

    bind_pitch_bend_class<Tick>(m, tick);
    bind_pitch_bend_class<Quarter>(m, quarter);
    bind_pitch_bend_class<Second>(m, second);

    bind_text_meta_class<Tick>(m, tick);
    bind_text_meta_class<Quarter>(m, quarter);
    bind_text_meta_class<Second>(m, second);

    bind_track_class<Tick>(m, tick);
    bind_track_class<Quarter>(m, quarter);
    bind_track_class<Second>(m, second);

    bind_score_class<Tick>(m, tick);
    bind_score_class<Quarter>(m, quarter);
    //    bind_score_class<Second>(m, second);

    return m;
}

const Tick tick_inst;
const Quarter quarter_inst;
const Second second_inst;

PYBIND11_MODULE(symusic, m) {

    py::class_<Tick>(m, "Tick")
        .def(py::init<>())
        .def("__repr__", [](const Tick &) { return "TimeUnit::Tick"; });

    py::class_<Quarter>(m, "Quarter")
        .def(py::init<>())
        .def("__repr__", [](const Quarter &) { return "TimeUnit::Quarter"; });

    py::class_<Second>(m, "Second")
        .def(py::init<>())
        .def("__repr__", [](const Second &) { return "TimeUnit::Second"; });

    auto typing = m.def_submodule("typing");
    typing.attr("Tick") = &tick_inst;
    typing.attr("Quarter") = &quarter_inst;
    typing.attr("Second") = &second_inst;

    auto core = m.def_submodule("core");
    core_module(core);

    m.def("Note", [](i32 t, const i32 &d, i8 p, i8 v, Tick _) {
        return Note<Tick>(t, d, p, v);
    }, py::arg("time"), py::arg("duration"), py::arg("pitch"), py::arg("velocity"), py::arg("time_unit"));

    m.def("Note", [](f32 t, const f32 &d, i8 p, i8 v, Quarter _) {
        return Note<Quarter>(t, d, p, v);
    }, py::arg("time"), py::arg("duration"), py::arg("pitch"), py::arg("velocity"), py::arg("time_unit"));

    m.def("Note", [](f32 t, const f32 &d, i8 p, i8 v, Second _) {
        return Note<Second>(t, d, p, v);
    },py::arg("time"), py::arg("duration"), py::arg("pitch"), py::arg("velocity"), py::arg("time_unit"));

    m.def("Note", [](py::object &t, py::object &d, i8 p, i8 v, const py::str& time_unit){
            // time_unit is a string, which can be "Tick", "Quarter", "Second"
            // first call lower() to convert it to lower case
            auto unit = time_unit.attr("lower")().cast<std::string>();
            // switch the unit, and convert the corresponding Note to python object using cast
            if (unit == "tick") {
                return py::cast(Note<Tick>(t.cast<i32>(), d.cast<i32>(), p, v));
            } else if (unit == "quarter") {
                return py::cast(Note<Quarter>(t.cast<f32>(), d.cast<f32>(), p, v));
            } else if (unit == "second") {
                return py::cast(Note<Second>(t.cast<f32>(), d.cast<f32>(), p, v));
            } else {
                throw std::invalid_argument("time_unit must be one of 'tick', 'quarter', 'second', not '" + unit + "'.");
            }
        }, "Create a Note object from time, duration, pitch, velocity and time_unit",
        py::arg("time"), py::arg("duration"), py::arg("pitch"),
        py::arg("velocity"), py::arg("time_unit")
    );

    m.def("ControlChange", [](i32 t, u8 n, u8 v, Tick _) {
        return ControlChange<Tick>(t, n, v);
    }, py::arg("time"), py::arg("number"), py::arg("value"), py::arg("time_unit"));

    m.def("ControlChange", [](f32 t, u8 n, u8 v, Quarter _) {
        return ControlChange<Quarter>(t, n, v);
    }, py::arg("time"), py::arg("number"), py::arg("value"), py::arg("time_unit"));

    m.def("ControlChange", [](f32 t, u8 n, u8 v, Second _) {
        return ControlChange<Second>(t, n, v);
    }, py::arg("time"), py::arg("number"), py::arg("value"), py::arg("time_unit"));

    m.def("ControlChange", [](py::object &t, u8 n, u8 v, const py::str& time_unit){
            // time_unit is a string, which can be "Tick", "Quarter", "Second"
            // first call lower() to convert it to lower case
            auto unit = time_unit.attr("lower")().cast<std::string>();
            // switch the unit, and convert the corresponding Note to python object using cast
            if (unit == "tick") {
                return py::cast(ControlChange<Tick>(t.cast<i32>(), n, v));
            } else if (unit == "quarter") {
                return py::cast(ControlChange<Quarter>(t.cast<f32>(), n, v));
            } else if (unit == "second") {
                return py::cast(ControlChange<Second>(t.cast<f32>(), n, v));
            } else {
                throw std::invalid_argument("time_unit must be one of 'tick', 'quarter', 'second', not '" + unit + "'.");
            }
        }, "Create a ControlChange object from time, number, value and time_unit",
        py::arg("time"), py::arg("number"), py::arg("value"), py::arg("time_unit")
    );

    m.def("KeySignature", [](i32 time, i8 key, u8 tonality, Tick _) {
        return score::KeySignature<Tick>(time, key, tonality);
    }, py::arg("time"), py::arg("key"), py::arg("tonality"), py::arg("time_unit"));

    m.def("KeySignature", [](f32 time, i8 key, u8 tonality, Quarter _) {
        return score::KeySignature<Quarter>(time, key, tonality);
    }, py::arg("time"), py::arg("key"), py::arg("tonality"), py::arg("time_unit"));

    m.def("KeySignature", [](f32 time, i8 key, u8 tonality, Second _) {
        return score::KeySignature<Second>(time, key, tonality);
    }, py::arg("time"), py::arg("key"), py::arg("tonality"), py::arg("time_unit"));

    m.def("KeySignature", [](py::object &time, i8 key, u8 tonality, const py::str& time_unit){
            // time_unit is a string, which can be "Tick", "Quarter", "Second"
            // first call lower() to convert it to lower case
            auto unit = time_unit.attr("lower")().cast<std::string>();
            // switch the unit, and convert the corresponding Note to python object using cast
            if (unit == "tick") {
                return py::cast(score::KeySignature<Tick>(time.cast<i32>(), key, tonality));
            } else if (unit == "quarter") {
                return py::cast(score::KeySignature<Quarter>(time.cast<f32>(), key, tonality));
            } else if (unit == "second") {
                return py::cast(score::KeySignature<Second>(time.cast<f32>(), key, tonality));
            } else {
                throw std::invalid_argument("time_unit must be one of 'tick', 'quarter', 'second', not '" + unit + "'.");
            }
        }, "Create a KeySignature object from time, key, tonality and time_unit",
        py::arg("time"), py::arg("key"), py::arg("tonality"), py::arg("time_unit")
    );

    m.def("TimeSignature", [](i32 time, u8 numerator, u8 denominator, Tick _) {
        return score::TimeSignature<Tick>(time, numerator, denominator);
    }, py::arg("time"), py::arg("numerator"), py::arg("denominator"), py::arg("time_unit"));

    m.def("TimeSignature", [](f32 time, u8 numerator, u8 denominator, Quarter _) {
        return score::TimeSignature<Quarter>(time, numerator, denominator);
    }, py::arg("time"), py::arg("numerator"), py::arg("denominator"), py::arg("time_unit"));

    m.def("TimeSignature", [](f32 time, u8 numerator, u8 denominator, Second _) {
        return score::TimeSignature<Second>(time, numerator, denominator);
    }, py::arg("time"), py::arg("numerator"), py::arg("denominator"), py::arg("time_unit"));

    m.def("TimeSignature", [](py::object &time, u8 numerator, u8 denominator, const py::str& time_unit){
            // time_unit is a string, which can be "Tick", "Quarter", "Second"
            // first call lower() to convert it to lower case
            auto unit = time_unit.attr("lower")().cast<std::string>();
            // switch the unit, and convert the corresponding Note to python object using cast
            if (unit == "tick") {
                return py::cast(score::TimeSignature<Tick>(time.cast<i32>(), numerator, denominator));
            } else if (unit == "quarter") {
                return py::cast(score::TimeSignature<Quarter>(time.cast<f32>(), numerator, denominator));
            } else if (unit == "second") {
                return py::cast(score::TimeSignature<Second>(time.cast<f32>(), numerator, denominator));
            } else {
                throw std::invalid_argument("time_unit must be one of 'tick', 'quarter', 'second', not '" + unit + "'.");
            }
        }, "Create a TimeSignature object from time, numerator, denominator and time_unit",
        py::arg("time"), py::arg("numerator"), py::arg("denominator"), py::arg("time_unit")
    );

    m.def("Tempo", [](i32 time, float qpm, Tick _) {
        return score::Tempo<Tick>(time, qpm);
    }, py::arg("time"), py::arg("qpm"), py::arg("time_unit"));

    m.def("Tempo", [](f32 time, float qpm, Quarter _) {
        return score::Tempo<Quarter>(time, qpm);
    }, py::arg("time"), py::arg("qpm"), py::arg("time_unit"));

    m.def("Tempo", [](f32 time, float qpm, Second _) {
        return score::Tempo<Second>(time, qpm);
    }, py::arg("time"), py::arg("qpm"), py::arg("time_unit"));

    m.def("Tempo", [](py::object &time, float qpm, const py::str& time_unit){
            // time_unit is a string, which can be "Tick", "Quarter", "Second"
            // first call lower() to convert it to lower case
            auto unit = time_unit.attr("lower")().cast<std::string>();
            // switch the unit, and convert the corresponding Note to python object using cast
            if (unit == "tick") {
                return py::cast(score::Tempo<Tick>(time.cast<i32>(), qpm));
            } else if (unit == "quarter") {
                return py::cast(score::Tempo<Quarter>(time.cast<f32>(), qpm));
            } else if (unit == "second") {
                return py::cast(score::Tempo<Second>(time.cast<f32>(), qpm));
            } else {
                throw std::invalid_argument("time_unit must be one of 'tick', 'quarter', 'second', not '" + unit + "'.");
            }
        }, "Create a Tempo object from time, qpm and time_unit",
        py::arg("time"), py::arg("qpm"), py::arg("time_unit")
    );

    m.def("PitchBend", [](i32 time, i32 value, Tick _) {
        return score::PitchBend<Tick>(time, value);
    }, py::arg("time"), py::arg("value"), py::arg("time_unit"));

    m.def("PitchBend", [](f32 time, i32 value, Quarter _) {
        return score::PitchBend<Quarter>(time, value);
    }, py::arg("time"), py::arg("value"), py::arg("time_unit"));

    m.def("PitchBend", [](f32 time, i32 value, Second _) {
        return score::PitchBend<Second>(time, value);
    }, py::arg("time"), py::arg("value"), py::arg("time_unit"));

    m.def("PitchBend", [](py::object &time, i32 value, const py::str& time_unit){
            // time_unit is a string, which can be "Tick", "Quarter", "Second"
            // first call lower() to convert it to lower case
            auto unit = time_unit.attr("lower")().cast<std::string>();
            // switch the unit, and convert the corresponding Note to python object using cast
            if (unit == "tick") {
                return py::cast(score::PitchBend<Tick>(time.cast<i32>(), value));
            } else if (unit == "quarter") {
                return py::cast(score::PitchBend<Quarter>(time.cast<f32>(), value));
            } else if (unit == "second") {
                return py::cast(score::PitchBend<Second>(time.cast<f32>(), value));
            } else {
                throw std::invalid_argument("time_unit must be one of 'tick', 'quarter', 'second', not '" + unit + "'.");
            }
        }, "Create a PitchBend object from time, value and time_unit",
        py::arg("time"), py::arg("value"), py::arg("time_unit")
    );

    m.def("TextMeta", [](i32 time, std::string &text, Tick _) {
        return score::TextMeta<Tick>(time, text);
    }, py::arg("time"), py::arg("text"), py::arg("time_unit"));

    m.def("TextMeta", [](f32 time, std::string &text, Quarter _) {
        return score::TextMeta<Quarter>(time, text);
    }, py::arg("time"), py::arg("text"), py::arg("time_unit"));

    m.def("TextMeta", [](f32 time, std::string &text, Second _) {
        return score::TextMeta<Second>(time, text);
    }, py::arg("time"), py::arg("text"), py::arg("time_unit"));

    m.def("TextMeta", [](py::object &time, std::string &text, const py::str& time_unit){
            // time_unit is a string, which can be "Tick", "Quarter", "Second"
            // first call lower() to convert it to lower case
            auto unit = time_unit.attr("lower")().cast<std::string>();
            // switch the unit, and convert the corresponding Note to python object using cast
            if (unit == "tick") {
                return py::cast(score::TextMeta<Tick>(time.cast<i32>(), text));
            } else if (unit == "quarter") {
                return py::cast(score::TextMeta<Quarter>(time.cast<f32>(), text));
            } else if (unit == "second") {
                return py::cast(score::TextMeta<Second>(time.cast<f32>(), text));
            } else {
                throw std::invalid_argument("time_unit must be one of 'tick', 'quarter', 'second', not '" + unit + "'.");
            }
        }, "Create a TextMeta object from time, text and time_unit",
        py::arg("time"), py::arg("text"), py::arg("time_unit")
    );

    m.def("Track", [](const std::string& name, u8 program, bool is_drum, Tick _) {
        return score::Track<Tick>(name, program, is_drum);
    }, py::arg("name"), py::arg("program"), py::arg("is_drum"), py::arg("time_unit"));

    m.def("Track", [](const std::string& name, u8 program, bool is_drum, Quarter _) {
        return score::Track<Quarter>(name, program, is_drum);
    }, py::arg("name"), py::arg("program"), py::arg("is_drum"), py::arg("time_unit"));

    m.def("Track", [](const std::string& name, u8 program, bool is_drum, Second _) {
        return score::Track<Second>(name, program, is_drum);
    }, py::arg("name"), py::arg("program"), py::arg("is_drum"), py::arg("time_unit"));

    m.def("Track", [](const std::string& name, u8 program, bool is_drum, const py::str& time_unit){
            // time_unit is a string, which can be "Tick", "Quarter", "Second"
            // first call lower() to convert it to lower case
            auto unit = time_unit.attr("lower")().cast<std::string>();
            // switch the unit, and convert the corresponding Note to python object using cast
            if (unit == "tick") {
                return py::cast(score::Track<Tick>(name, program, is_drum));
            } else if (unit == "quarter") {
                return py::cast(score::Track<Quarter>(name, program, is_drum));
            } else if (unit == "second") {
                return py::cast(score::Track<Second>(name, program, is_drum));
            } else {
                throw std::invalid_argument("time_unit must be one of 'tick', 'quarter', 'second', not '" + unit + "'.");
            }
        }, "Create a Track object from name, program, is_drum and time_unit",
        py::arg("name"), py::arg("program"), py::arg("is_drum"), py::arg("time_unit")
    );

    m.def("Score", [](const std::string & filename) {
        return Score<Tick>::from_file(filename);
    }, py::arg("filename"));

    m.def("Score", [](const std::string & filename, Tick _) {
        return Score<Tick>::from_file(filename);
    }, py::arg("filename"), py::arg("time_unit"));

    m.def("Score", [](const std::string & filename, Quarter _) {
        return Score<Quarter>::from_file(filename);
    }, py::arg("filename"), py::arg("time_unit"));

//    m.def("Score", [](const std::string & filename, Second _) {
//        return Score<Second>::from_file(filename);
//    }, py::arg("filename"), py::arg("time_unit"));

    m.def("Score", [](const std::string & filename, const py::str& time_unit){
            // time_unit is a string, which can be "Tick", "Quarter", "Second"
            // first call lower() to convert it to lower case
            auto unit = time_unit.attr("lower")().cast<std::string>();
            // switch the unit, and convert the corresponding Note to python object using cast
            if (unit == "tick") {
                return py::cast(Score<Tick>::from_file(filename));
            } else if (unit == "quarter") {
                return py::cast(Score<Quarter>::from_file(filename));
            } else if (unit == "second") {
                throw std::invalid_argument("Second is not supported yet.");
            } else {
                throw std::invalid_argument("time_unit must be one of 'tick', 'quarter', 'second', not '" + unit + "'.");
            }
        }, "Create a Score object from filename and time_unit",
        py::arg("filename"), py::arg("time_unit")
    );

    m.def("Score", []() {return Score<Tick>();});
    m.def("Score", [](Tick _) {return Score<Tick>();}, py::arg("time_unit"));
    m.def("Score", [](Quarter _) {return Score<Quarter>();}, py::arg("time_unit"));
    m.def("Score", [](Second _) {return Score<Second>();}, py::arg("time_unit"));



#ifdef VERSION_INFO
    m.attr("__version__") = MACRO_STRINGIFY(VERSION_INFO);
#else
    m.attr("__version__") = "0.1.0";
#endif
}
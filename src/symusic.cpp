//
// Created by lyk on 23-9-20.
//
#include <string>
#include <pybind11/pybind11.h>
#include <pybind11/stl_bind.h>
#include <pybind11/functional.h>
//#include <pybind11/eigen.h>
#include "Score.hpp"

namespace py = pybind11;
using namespace score;

#define OPAQUE_VEC(i, TYPE) \
    PYBIND11_MAKE_OPAQUE(std::vector<TYPE<Tick>>) \
    PYBIND11_MAKE_OPAQUE(std::vector<TYPE<Quarter>>) \
    PYBIND11_MAKE_OPAQUE(std::vector<TYPE<Second>>)

REPEAT_ON(OPAQUE_VEC, Note, ControlChange, score::TimeSignature, score::KeySignature, Tempo, PitchBend, TextMeta, Track)


// a template functions for binding all specializations of Note, and return it
template<typename T>
py::class_<Note<T>> bind_note_class(py::module &m, const std::string & name_) {
    typedef typename T::unit unit;
    auto name = "Note" + name_;
    py::bind_vector<std::vector<Note<T>>>(m, name + "List")
        .def("sort_inplace", [](std::vector<Note<T>> &self) {
            utils::sort_by_time(self);
            return self;
        })
        .def("sort", [](const std::vector<Note<T>> &self) {
            auto copy = self;
            utils::sort_by_time(copy);
            return copy;
        })
        .def("__repr__", [](const std::vector<Note<T>> &self) {
            return to_string(self);
        });
    
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
    py::bind_vector<std::vector<score::KeySignature<T>>>(m, name + "List")
        .def("sort_inplace", [](std::vector<score::KeySignature<T>> &self) {
            utils::sort_by_time(self);
            return self;
        })
        .def("sort", [](const std::vector<score::KeySignature<T>> &self) {
            auto copy = self;
            utils::sort_by_time(copy);
            return copy;
        })
        .def("__repr__", [](const std::vector<score::KeySignature<T>> &self) {
            return to_string(self);
        });
    
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
    py::bind_vector<std::vector<score::TimeSignature<T>>>(m, name + "List")
        .def("sort_inplace", [](std::vector<score::TimeSignature<T>> &self) {
            utils::sort_by_time(self);
            return self;
        })
        .def("sort", [](const std::vector<score::TimeSignature<T>> &self) {
            auto copy = self;
            utils::sort_by_time(copy);
            return copy;
        })
        .def("__repr__", [](const std::vector<score::TimeSignature<T>> &self) {
            return to_string(self);
        });
    
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
    py::bind_vector<std::vector<score::ControlChange<T>>>(m, name + "List")
        .def("sort_inplace", [](std::vector<score::ControlChange<T>> &self) {
            utils::sort_by_time(self);
            return self;
        })
        .def("sort", [](const std::vector<score::ControlChange<T>> &self) {
            auto copy = self;
            utils::sort_by_time(copy);
            return copy;
        })
        .def("__repr__", [](const std::vector<score::ControlChange<T>> &self) {
            return to_string(self);
        });

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
    py::bind_vector<std::vector<score::Tempo<T>>>(m, name + "List")
        .def("sort_inplace", [](std::vector<score::Tempo<T>> &self) {
            utils::sort_by_time(self);
            return self;
        })
        .def("sort", [](const std::vector<score::Tempo<T>> &self) {
            auto copy = self;
            utils::sort_by_time(copy);
            return copy;
        })
        .def("__repr__", [](const std::vector<score::Tempo<T>> &self) {
            return to_string(self);
        });
    
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
    
    py::bind_vector<std::vector<score::PitchBend<T>>>(m, name + "List")
        .def("sort_inplace", [](std::vector<score::PitchBend<T>> &self) {
            utils::sort_by_time(self);
            return self;
        })
        .def("sort", [](const std::vector<score::PitchBend<T>> &self) {
            auto copy = self;
            utils::sort_by_time(copy);
            return copy;
        })
        .def("__repr__", [](const std::vector<score::PitchBend<T>> &self) {
            return to_string(self);
        });
    
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
    
    py::bind_vector<std::vector<score::TextMeta<T>>>(m, name + "List")
        .def("sort_inplace", [](std::vector<score::TextMeta<T>> &self) {
            utils::sort_by_time(self);
            return self;
        })
        .def("sort", [](const std::vector<score::TextMeta<T>> &self) {
            auto copy = self;
            utils::sort_by_time(copy);
            return copy;
        })
        .def("__repr__", [](const std::vector<score::TextMeta<T>> &self) {
            return to_string(self);
        });
    
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

    py::bind_vector<std::vector<score::Track<T>>>(m, name + "List")
        .def("__repr__", [](const std::vector<score::Track<T>> &self) {
            return to_string(self);
        });

    return py::class_<score::Track<T>>(m, name.c_str())
        .def(py::init<>())
        .def(py::init<const score::Track<T> &>(), "Copy constructor")
        .def("copy", &score::Track<T>::copy, "Deep copy", py::return_value_policy::move)
        .def("__copy__", &score::Track<T>::copy, "Deep copy")
        .def("__deepcopy__", &score::Track<T>::copy, "Deep copy")
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
        .def_property_readonly_static("from_file", [](const py::object &) {
            return py::cpp_function([](std::string &x) { return Score<T>::from_file(x); });
        })  // binding class method in an erratic way: https://github.com/pybind/pybind11/issues/1693
        .def("sort", &Score<T>::sort, py::return_value_policy::move)
        .def("shift_pitch", &Score<T>::shift_pitch)
        .def("shift_time", &Score<T>::shift_time)
        .def("shift_velocity", &Score<T>::shift_velocity);
}

PYBIND11_MODULE(symusic, m) {
    const std::string tick = "Tick", quarter = "Quarter", second = "Second";

    bind_note_class<Tick>(m, tick);
    bind_note_class<Quarter>(m, quarter);
//    bind_note_class<Second>(m, second);

    bind_control_change_class<Tick>(m, tick);
    bind_control_change_class<Quarter>(m, quarter);
//    bind_control_change_class<Second>(m, second);
    
    bind_tempo_class<Tick>(m, tick);
    bind_tempo_class<Quarter>(m, quarter);
//    bind_tempo_class<Second>(m, second);
    
    bind_time_signature_class<Tick>(m, tick);
    bind_time_signature_class<Quarter>(m, quarter);
//    bind_time_signature_class<Second>(m, second);
    
    bind_key_signature_class<Tick>(m, tick);
    bind_key_signature_class<Quarter>(m, quarter);
//    bind_key_signature_class<Second>(m, second);
    
    bind_pitch_bend_class<Tick>(m, tick);
    bind_pitch_bend_class<Quarter>(m, quarter);
//    bind_pitch_bend_class<Second>(m, second);
    
    bind_text_meta_class<Tick>(m, tick);
    bind_text_meta_class<Quarter>(m, quarter);
//    bind_text_meta_class<Second>(m, second);

    bind_track_class<Tick>(m, tick);
    bind_track_class<Quarter>(m, quarter);
//    bind_track_class<Second>(m, second);

    bind_score_class<Tick>(m, tick);
    bind_score_class<Quarter>(m, quarter);
//    bind_score_class<Second>(m, second);

#ifdef VERSION_INFO
    m.attr("__version__") = MACRO_STRINGIFY(VERSION_INFO);
#else
    m.attr("__version__") = "0.0.6";
#endif
}
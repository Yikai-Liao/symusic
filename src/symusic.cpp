//
// Created by lyk on 23-9-20.
//
#include <string>
#include <pybind11/cast.h>
#include <pybind11/pytypes.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl_bind.h>
#include <pybind11/numpy.h>
#include "Score.hpp"

namespace py = pybind11;
using namespace score;

#define OPAQUE_VEC(i, TYPE) \
    PYBIND11_MAKE_OPAQUE(vec<TYPE<Tick>>) \
    PYBIND11_MAKE_OPAQUE(vec<TYPE<Quarter>>) \
    PYBIND11_MAKE_OPAQUE(vec<TYPE<Second>>)

REPEAT_ON(OPAQUE_VEC, Note, ControlChange, Pedal, TimeSignature, KeySignature, Tempo, PitchBend, TextMeta, Track)

template<typename T>
void sort_by_py_key(vec<T> &self, const py::function & key) {
    pdqsort(self.begin(), self.end(), [&key](const T &a, const T &b) {
        return key(a) < key(b);
    });
}

template<typename T>
vec<T> & py_sort_inplace(vec<T> &self, const py::object & key, const bool reverse) {
    if (key.is_none()) score::utils::sort_by_time(self);
    else sort_by_py_key(self, key.cast<py::function>());
    if (reverse) std::reverse(self.begin(), self.end());
    return self;
}

template<typename T>
vec<T> py_sort(const vec<T> &self, const py::object & key, bool reverse) {
    auto copy = self;
    py_sort_inplace<T>(copy, key, reverse);
    return copy;
}

// a template functions for binding all specializations of Note, and return it
template<typename T>
py::class_<Note<T>> bind_note_class(py::module &m, const std::string & name_) {
    typedef typename T::unit unit;
    const auto name = "Note" + name_;

    auto event = py::class_<Note<T>>(m, name.c_str())
        .def(py::init<unit, unit, int8_t, int8_t>())
        .def(py::init<const Note<T> &>(), "Copy constructor")
        .def_readwrite("start", &Note<T>::time)
        .def_readwrite("time", &Note<T>::time)
        .def_readwrite("duration", &Note<T>::duration)
        .def_readwrite("pitch", &Note<T>::pitch)
        .def_readwrite("velocity", &Note<T>::velocity)
        .def_property_readonly("end", &Note<T>::end)
        .def("empty", &Note<T>::empty, "duration <= 0 or velocity <= 0")
        .def("copy", &Note<T>::copy, "Deep copy the note", py::return_value_policy::move)
        .def("__copy__", &Note<T>::copy, "Deep copy the note")
        .def("__deepcopy__", &Note<T>::copy, "Deep copy the note")
        .def("end_time", &Note<T>::end)
        .def("shift_pitch", &Note<T>::shift_pitch, py::return_value_policy::move)
        .def("shift_velocity", &Note<T>::shift_velocity, py::return_value_policy::move)
        .def("shift_time", &Note<T>::shift_time)
        // def all inplace operation
        .def("shift_pitch_inplace", &Note<T>::shift_pitch_inplace, py::return_value_policy::reference)
        .def("shift_velocity_inplace", &Note<T>::shift_velocity_inplace, py::return_value_policy::reference)
        .def("shift_time_inplace", &Note<T>::shift_time_inplace, py::return_value_policy::reference)
        .def("__repr__", &Note<T>::to_string);

    py::bind_vector<vec<Note<T>>>(m, name + "List")
        .def("sort_inplace", &py_sort_inplace<Note<T>>, py::arg("key")=py::none(), py::arg("reverse")=false)
        .def("sort", &py_sort<Note<T>>, py::arg("key")=py::none(), py::arg("reverse")=false)
        .def("__repr__", [](const vec<Note<T>> &self) {
            return to_string(self);
        });
    
    py::implicitly_convertible<py::list, vec<Note<T>>>();
    
    return event;
}

// bind score::KeySignature<T>
template<typename T>
py::class_<score::KeySignature<T>> bind_key_signature_class(py::module &m, const std::string & name_) {
    typedef typename T::unit unit;
    const auto name = "KeySignature" + name_;

    auto event = py::class_<score::KeySignature<T>>(m, name.c_str())
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

    py::bind_vector<vec<score::KeySignature<T>>>(m, name + "List")
        .def("sort_inplace", &py_sort_inplace<score::KeySignature<T>>, py::arg("key")=py::none(), py::arg("reverse")=false)
        .def("sort", &py_sort<score::KeySignature<T>>, py::arg("key")=py::none(), py::arg("reverse")=false)
        .def("__repr__", [](const vec<score::KeySignature<T>> &self) {
            return to_string(self);
        });

    py::implicitly_convertible<py::list, vec<score::KeySignature<T>>>();
    
    return event;
}

// bind score::TimeSignature<T>
template<typename T>
py::class_<score::TimeSignature<T>> bind_time_signature_class(py::module &m, const std::string & name_) {
    typedef typename T::unit unit;
    const auto name = "TimeSignature" + name_;

    auto event = py::class_<score::TimeSignature<T>>(m, name.c_str())
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

    py::bind_vector<vec<score::TimeSignature<T>>>(m, name + "List")
        .def("sort_inplace", &py_sort_inplace<score::TimeSignature<T>>, py::arg("key")=py::none(), py::arg("reverse")=false)
        .def("sort", &py_sort<score::TimeSignature<T>>, py::arg("key")=py::none(), py::arg("reverse")=false)
        .def("__repr__", [](const vec<score::TimeSignature<T>> &self) {
            return to_string(self);
        });

    py::implicitly_convertible<py::list, vec<score::TimeSignature<T>>>();
    
    return event;
}

// bind score::ControlChange<T>
template<typename T>
py::class_<score::ControlChange<T>> bind_control_change_class(py::module &m, const std::string & name_) {
    typedef typename T::unit unit;
    const auto name = "ControlChange" + name_;

    auto event = py::class_<score::ControlChange<T>>(m, name.c_str())
        .def(py::init<unit, u8, u8>())
        .def(py::init<const score::ControlChange<T> &>(), "Copy constructor")
        .def_readwrite("time", &score::ControlChange<T>::time)
        .def_readwrite("value", &score::ControlChange<T>::value)
        .def_readwrite("number", &score::ControlChange<T>::number)
        .def("shift_time", &score::ControlChange<T>::shift_time)
        .def("shift_time_inplace", &score::ControlChange<T>::shift_time_inplace)
        .def("copy", &score::ControlChange<T>::copy, "Deep copy", py::return_value_policy::move)
        .def("__copy__", &score::ControlChange<T>::copy, "Deep copy")
        .def("__deepcopy__", &score::ControlChange<T>::copy, "Deep copy")
        .def("__repr__", &score::ControlChange<T>::to_string);

    py::bind_vector<vec<score::ControlChange<T>>>(m, name + "List")
        .def("sort_inplace", &py_sort_inplace<score::ControlChange<T>>, py::arg("key")=py::none(), py::arg("reverse")=false)
        .def("sort", &py_sort<score::ControlChange<T>>, py::arg("key")=py::none(), py::arg("reverse")=false)
        .def("__repr__", [](const vec<score::ControlChange<T>> &self) {
            return to_string(self);
        });

    py::implicitly_convertible<py::list, vec<score::ControlChange<T>>>();

    return event;
}

// bind Pedal<T>
template<typename T>
py::class_<score::Pedal<T>> bind_pedal_class(py::module &m, const std::string & name_) {
    typedef typename T::unit unit;
    const auto name = "Pedal" + name_;

    auto event = py::class_<score::Pedal<T>>(m, name.c_str())
        .def(py::init<unit, bool>())
        .def(py::init<const score::Pedal<T> &>(), "Copy constructor")
        .def_readwrite("time", &score::Pedal<T>::time)
        .def_readwrite("duration", &score::Pedal<T>::duration)
        .def("shift_time", &score::Pedal<T>::shift_time)
        .def("shift_time_inplace", &score::Pedal<T>::shift_time_inplace)
        .def("copy", &score::Pedal<T>::copy, "Deep copy", py::return_value_policy::move)
        .def("__copy__", &score::Pedal<T>::copy, "Deep copy")
        .def("__deepcopy__", &score::Pedal<T>::copy, "Deep copy")
        .def("__repr__", &score::Pedal<T>::to_string);

    py::bind_vector<vec<score::Pedal<T>>>(m, name + "List")
        .def("sort_inplace", &py_sort_inplace<score::Pedal<T>>, py::arg("key")=py::none(), py::arg("reverse")=false)
        .def("sort", &py_sort<score::Pedal<T>>, py::arg("key")=py::none(), py::arg("reverse")=false)
        .def("__repr__", [](const vec<score::Pedal<T>> &self) {
            return to_string(self);
        });

    py::implicitly_convertible<py::list, vec<score::Pedal<T>>>();

    return event;
}

// bind score::Tempo<T>
template<typename T>
py::class_<score::Tempo<T>> bind_tempo_class(py::module &m, const std::string & name_) {
    typedef typename T::unit unit;
    const auto name = "Tempo" + name_;
    auto event = py::class_<score::Tempo<T>>(m, name.c_str())
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

    py::bind_vector<vec<score::Tempo<T>>>(m, name + "List")
        .def("sort_inplace", &py_sort_inplace<score::Tempo<T>>, py::arg("key")=py::none(), py::arg("reverse")=false)
        .def("sort", &py_sort<score::Tempo<T>>, py::arg("key")=py::none(), py::arg("reverse")=false)
        .def("__repr__", [](const vec<score::Tempo<T>> &self) {
            return to_string(self);
        });

    py::implicitly_convertible<py::list, vec<score::Tempo<T>>>();

    return event;
}

// bind score::PitchBend<T>
template<typename T>
py::class_<score::PitchBend<T>> bind_pitch_bend_class(py::module &m, const std::string & name_) {
    typedef typename T::unit unit;
    const auto name = "PitchBend" + name_;

    auto event = py::class_<score::PitchBend<T>>(m, name.c_str())
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
    
    py::bind_vector<vec<score::PitchBend<T>>>(m, name + "List")
        .def("sort_inplace", &py_sort_inplace<PitchBend<T>>, py::arg("key")=py::none(), py::arg("reverse")=false)
        .def("sort", &py_sort<PitchBend<T>>, py::arg("key")=py::none(), py::arg("reverse")=false)
        .def("__repr__", [](const vec<score::PitchBend<T>> &self) {
            return to_string(self);
        });

    py::implicitly_convertible<py::list, vec<score::PitchBend<T>>>();
    
    return event;
}

// bind score::TextMeta<T>
template<typename T>
py::class_<score::TextMeta<T>> bind_text_meta_class(py::module &m, const std::string & name_) {
    typedef typename T::unit unit;
    const auto name = "TextMeta" + name_;

    auto event = py::class_<score::TextMeta<T>>(m, name.c_str())
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
    
    py::bind_vector<vec<score::TextMeta<T>>>(m, name + "List")
        .def("sort_inplace", &py_sort_inplace<TextMeta<T>>, py::arg("key")=py::none(), py::arg("reverse")=false)
        .def("sort", &py_sort<TextMeta<T>>, py::arg("key")=py::none(), py::arg("reverse")=false)
        .def("__repr__", [](const vec<score::TextMeta<T>> &self) {
            return to_string(self);
        });

    py::implicitly_convertible<py::list, vec<score::TextMeta<T>>>();

    return event;
}
template<typename T>
Track<T>&py_sort_track_inplace(Track<T> &self, py::object key, bool reverse) {
    py_sort_inplace(self.notes, key, reverse);
    py_sort_inplace(self.controls, key, reverse);
    py_sort_inplace(self.pitch_bends, key, reverse);
    py_sort_inplace(self.pedals, key, reverse);
    return self;
};

template<typename T>
Track<T> py_sort_track(const Track<T> &self, py::object key, bool reverse) {
    auto copy = self;
    return py_sort_track_inplace(copy, key, reverse);
};

// bind score::Track<T>
template<typename T>
py::class_<score::Track<T>> bind_track_class(py::module &m, const std::string & name_) {
    const auto name = "Track" + name_;
    auto event = py::class_<score::Track<T>>(m, name.c_str())
        .def(py::init<>())
        .def(py::init<const score::Track<T> &>(), "Copy constructor")
        .def("copy", &score::Track<T>::copy, "Deep copy", py::return_value_policy::move)
        .def("__copy__", &score::Track<T>::copy, "Deep copy")
        .def("__deepcopy__", &score::Track<T>::copy, "Deep copy")
        .def("__repr__", &score::Track<T>::to_string)
        .def_readwrite("notes", &score::Track<T>::notes)
        .def_readwrite("controls", &score::Track<T>::controls)
        .def_readwrite("pitch_bends", &score::Track<T>::pitch_bends)
        .def_readwrite("pedals", &score::Track<T>::pedals)
        .def_readwrite("is_drum", &score::Track<T>::is_drum)
        .def_readwrite("program", &score::Track<T>::program)
        .def_readwrite("name", &score::Track<T>::name)
        .def("sort_inplace", &py_sort_track_inplace<T>, py::arg("key")=py::none(), py::arg("reverse")=false)
        .def("sort", &py_sort_track<T>, py::arg("key")=py::none(), py::arg("reverse")=false)
        .def("end", &Track<T>::end)
        .def("start", &score::Track<T>::start)
        .def("note_num", &score::Track<T>::note_num)
        .def("empty", &score::Track<T>::empty)
        .def("clip", &Track<T>::clip, "Clip notes and controls to a given time range")
        .def("shift_time_inplace", &score::Track<T>::shift_time_inplace)
        .def("shift_time", &score::Track<T>::shift_time, py::return_value_policy::move)
        .def("shift_pitch_inplace", &score::Track<T>::shift_pitch_inplace)
        .def("shift_pitch", &score::Track<T>::shift_pitch)
        .def("shift_velocity_inplace", &score::Track<T>::shift_velocity_inplace)
        .def("shift_velocity", &score::Track<T>::shift_velocity)
        .def("onset_pianoroll", [](score::Track<T> &self, float quantization) {
            score::pianoroll::TrackPianoRoll pianoroll = self.onset_pianoroll(quantization);

            return py::array_t<float>(py::buffer_info{
                pianoroll.data,
                sizeof(float),
                py::format_descriptor<float>::format(),
                2,
                { pianoroll.pitch_dim, pianoroll.time_dim },
                { sizeof(float) * pianoroll.time_dim,
                sizeof(float) }
            });
        })
        .def("frame_pianoroll", [](score::Track<T> &self, float quantization) {
            score::pianoroll::TrackPianoRoll pianoroll = self.frame_pianoroll(quantization);

            return py::array_t<float>(py::buffer_info{
                pianoroll.data,
                sizeof(float),
                py::format_descriptor<float>::format(),
                2,
                { pianoroll.pitch_dim, pianoroll.time_dim },
                { sizeof(float) * pianoroll.time_dim,
                sizeof(float) }
            });
        });
        
    py::bind_vector<vec<score::Track<T>>>(m, name + "List")
        .def("sort_inplace", [](vec<score::Track<T>> &self, const py::object & key, const bool reverse) {
            if (key.is_none()) throw std::invalid_argument("key must be specified");
            else sort_by_py_key(self, key.cast<py::function>());
            if (reverse) std::reverse(self.begin(), self.end());
            return self;
        }, py::arg("key"), py::arg("reverse")=false)
        .def("sort", [](const vec<score::Track<T>> &self, const py::object & key, const bool reverse) {
            auto copy = self;
            if (key.is_none()) throw std::invalid_argument("key must be specified");
            else sort_by_py_key(copy, key.cast<py::function>());
            if (reverse) std::reverse(copy.begin(), copy.end());
            return copy;
        }, py::arg("key"), py::arg("reverse")=false)
        .def("__repr__", [](const vec<score::Track<T>> &self) {
            return to_string(self);
        });

    py::implicitly_convertible<py::list, vec<score::Track<T>>>();

    return event;
}

// py sort score
template<typename T>
Score<T>& py_sort_score_inplace(Score<T> &self, py::object key, bool reverse) {
    py_sort_inplace(self.time_signatures, key, reverse);
    py_sort_inplace(self.key_signatures, key, reverse);
    py_sort_inplace(self.tempos, key, reverse);
    py_sort_inplace(self.lyrics, key, reverse);
    py_sort_inplace(self.markers, key, reverse);
    for(auto &track: self.tracks) 
        py_sort_track_inplace(track, key, reverse);
    return self;
};

template<typename T>
Score<T> py_sort_score(const Score<T> &self, py::object key, bool reverse) {
    auto copy = self;
    return py_sort_score_inplace(copy, key, reverse);
};

// bind score::Score<T>
template<typename T>
py::class_<Score<T>> bind_score_class(py::module &m, const std::string & name_) {
    const auto name = "Score" + name_;

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
        .def_readwrite("time_signatures", &Score<T>::time_signatures)
        .def_readwrite("key_signatures", &Score<T>::key_signatures)
        .def_readwrite("tempos", &Score<T>::tempos)
        .def_readwrite("lyrics", &Score<T>::lyrics)
        .def_readwrite("markers", &Score<T>::markers)
        .def("sort_inplace", &py_sort_score_inplace<T>, py::arg("key")=py::none(), py::arg("reverse")=false)
        .def("sort", &py_sort_score<T>, py::arg("key")=py::none(), py::arg("reverse")=false)
        .def("clip", &Score<T>::clip, "Clip events a given time range", py::arg("start"), py::arg("end"), py::arg("clip_end")=false)
        .def("shift_time_inplace", &Score<T>::shift_time_inplace)
        .def("shift_time", &Score<T>::shift_time)
        .def("shift_pitch_inplace", &Score<T>::shift_pitch_inplace)
        .def("shift_pitch", &Score<T>::shift_pitch)
        .def("shift_velocity_inplace", &Score<T>::shift_velocity_inplace)
        .def("shift_velocity", &Score<T>::shift_velocity)
        .def("start", &Score<T>::start)
        .def("end", &Score<T>::end)
        .def("note_num", &Score<T>::note_num)
        .def("empty", &Score<T>::empty);
}

py::module & core_module(py::module & m){
    const std::string tick = "Tick", quarter = "Quarter", second = "Second";

    bind_note_class<Tick>(m, tick);
    bind_note_class<Quarter>(m, quarter);
    bind_note_class<Second>(m, second);


    bind_control_change_class<Tick>(m, tick);
    bind_control_change_class<Quarter>(m, quarter);
    bind_control_change_class<Second>(m, second);

    bind_pedal_class<Tick>(m, tick);
    bind_pedal_class<Quarter>(m, quarter);
    bind_pedal_class<Second>(m, second);

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

    auto score_tick = bind_score_class<Tick>(m, tick);
    auto score_quarter = bind_score_class<Quarter>(m, quarter);
    //    bind_score_class<Second>(m, second);
    return m;
}

PYBIND11_MODULE(core, m) {

    auto tick = py::class_<Tick>(m, "Tick")
        .def(py::init<>())
        .def("__repr__", [](const Tick &) { return "TimeUnit::Tick"; })
        .def("is_time_unit", [](const Tick &) { return true; });

    auto quarter = py::class_<Quarter>(m, "Quarter")
        .def(py::init<>())
        .def("__repr__", [](const Quarter &) { return "TimeUnit::Quarter"; })
        .def("is_time_unit", [](const Quarter &) { return true; });

    auto second = py::class_<Second>(m, "Second")
        .def(py::init<>())
        .def("__repr__", [](const Second &) { return "TimeUnit::Second"; })
        .def("is_time_unit", [](const Second &) { return true; });

    // def __eq__ for all time units
    tick.def("__eq__", [](const Tick &, const py::object &other) {
        if (py::isinstance<Tick>(other)) return true;
        return false;
    });

    quarter.def("__eq__", [](const Quarter &, const py::object &other) {
        if (py::isinstance<Quarter>(other)) return true;
        return false;
    });

    second.def("__eq__", [](const Second &, const py::object &other) {
        if (py::isinstance<Second>(other)) return true;
        return false;
    });

    core_module(m);

}
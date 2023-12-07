//
// Created by lyk on 23-9-20.
//
#include <pybind11/detail/common.h>
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
py::object py_sort(vec<T> &self, const py::object & key, const bool reverse, const bool inplace = false) {
    if (inplace) {
        py_sort_inplace(self, key, reverse);
        return py::cast(self, py::return_value_policy::reference);
    } else {
        auto copy = vec(self);
        return py::cast(py_sort_inplace(copy, key, reverse), py::return_value_policy::move);
    }
}

template<typename T>
py::class_<T> time_stamp_base(py::module &m, const std::string &name) {
    typedef typename T::unit unit;
    auto event = py::class_<T>(m, name.c_str())
        .def_readwrite("time", &T::time)
        .def("shift_time", [](T &self, const unit offset, const bool inplace) {
                if (inplace) return self.shift_time_inplace(offset);
                else return self.shift_time(offset);
            }, py::arg("offset"), 
            py::arg("inplace")=false, "Shift the event time by offset")
        .def(py::init<const T &>(), "Copy constructor", py::arg("other"))
        .def("copy", &T::copy, "Deep copy", py::return_value_policy::move)
        .def("__copy__", &T::copy, "Deep copy")
        .def("__deepcopy__", &T::copy, "Deep copy")
        .def("__repr__", &T::to_string);

    py::bind_vector<vec<T>>(m, name + "List")
    .def("sort", &py_sort<T>, py::arg("key")=py::none(), py::arg("reverse")=false, py::arg("inplace")=true)
    .def("__repr__", [](const vec<T> &self) { return to_string(self);});
    
    py::implicitly_convertible<py::list, vec<T>>();
    return event;
}

// a template functions for binding all specializations of Note, and return it
template<typename T>
py::class_<Note<T>> bind_note_class(py::module &m, const std::string & name_) {
    typedef typename T::unit unit;
    const std::string name = "Note" + name_;

    return time_stamp_base<Note<T>>(m, name)
        .def(py::init<unit, unit, int8_t, int8_t>(), py::arg("start"), py::arg("duration"), py::arg("pitch"), py::arg("velocity"))
        .def_readwrite("start", &Note<T>::time)
        .def_readwrite("duration", &Note<T>::duration)
        .def_readwrite("pitch", &Note<T>::pitch)
        .def_readwrite("velocity", &Note<T>::velocity)
        .def_property_readonly("end", &Note<T>::end)
        .def("empty", &Note<T>::empty, "duration <= 0 or velocity <= 0")
        .def("end_time", &Note<T>::end)
        .def("shift_pitch", [](Note<T> &self, const int8_t offset, const bool inplace) {
                if (inplace) return py::cast(self.shift_pitch_inplace(offset), py::return_value_policy::reference);
                else return py::cast(self.shift_pitch(offset), py::return_value_policy::move);
            }, py::arg("offset"), py::arg("inplace")=false, "Shift the pitch by offset")
        .def("shift_velocity", [](Note<T> &self, const int8_t offset, const bool inplace) {
                if (inplace) return py::cast(self.shift_velocity_inplace(offset), py::return_value_policy::reference);
                else return py::cast(self.shift_velocity(offset), py::return_value_policy::move);
            }, py::arg("offset"), py::arg("inplace")=false, "Shift the velocity by offset");
}

// bind score::KeySignature<T>
template<typename T>
py::class_<score::KeySignature<T>> bind_key_signature_class(py::module &m, const std::string & name_) {
    typedef typename T::unit unit;
    const auto name = "KeySignature" + name_;

    return time_stamp_base<score::KeySignature<T>>(m, name)
        .def(py::init<unit, i8, u8>(), py::arg("time"), py::arg("key"), py::arg("tonality"))
        .def_readwrite("key", &score::KeySignature<T>::key)
        .def_readwrite("tonality", &score::KeySignature<T>::tonality);
}

// bind score::TimeSignature<T>
template<typename T>
py::class_<score::TimeSignature<T>> bind_time_signature_class(py::module &m, const std::string & name_) {
    typedef typename T::unit unit;
    const auto name = "TimeSignature" + name_;

    return time_stamp_base<score::TimeSignature<T>>(m, name)
        .def(py::init<unit, u8, u8>())
        .def_readwrite("numerator", &score::TimeSignature<T>::numerator)
        .def_readwrite("denominator", &score::TimeSignature<T>::denominator);
}

// bind score::ControlChange<T>
template<typename T>
py::class_<score::ControlChange<T>> bind_control_change_class(py::module &m, const std::string & name_) {
    typedef typename T::unit unit;
    const auto name = "ControlChange" + name_;

    return time_stamp_base<score::ControlChange<T>>(m, name)
        .def(py::init<unit, u8, u8>(), py::arg("time"), py::arg("number"), py::arg("value"))
        .def_readwrite("value", &score::ControlChange<T>::value)
        .def_readwrite("number", &score::ControlChange<T>::number);
}

// bind Pedal<T>
template<typename T>
py::class_<score::Pedal<T>> bind_pedal_class(py::module &m, const std::string & name_) {
    typedef typename T::unit unit;
    const auto name = "Pedal" + name_;

    return time_stamp_base<score::Pedal<T>>(m, name)
        .def(py::init<unit, unit>(), py::arg("time"), py::arg("duration"))
        .def_readwrite("duration", &score::Pedal<T>::duration);
}

// bind score::Tempo<T>
template<typename T>
py::class_<score::Tempo<T>> bind_tempo_class(py::module &m, const std::string & name_) {
    typedef typename T::unit unit;
    const auto name = "Tempo" + name_;
    return time_stamp_base<score::Tempo<T>>(m, name)
        .def(py::init<unit, float>(), py::arg("time"), py::arg("qpm"))
        .def_readwrite("tempo", &score::Tempo<T>::qpm);
}

// bind score::PitchBend<T>
template<typename T>
py::class_<score::PitchBend<T>> bind_pitch_bend_class(py::module &m, const std::string & name_) {
    typedef typename T::unit unit;
    const auto name = "PitchBend" + name_;

    return time_stamp_base<score::PitchBend<T>>(m, name)
        .def(py::init<unit, i32>(), py::arg("time"), py::arg("value"))
        .def_readwrite("value", &score::PitchBend<T>::value);
}

// bind score::TextMeta<T>
template<typename T>
py::class_<score::TextMeta<T>> bind_text_meta_class(py::module &m, const std::string & name_) {
    typedef typename T::unit unit;
    const auto name = "TextMeta" + name_;

    return time_stamp_base<score::TextMeta<T>>(m, name)
        .def(py::init<unit, std::string &>(), py::arg("time"), py::arg("text"))
        .def_readwrite("text", &score::TextMeta<T>::text);
}

template<typename T>
Track<T>&py_sort_track_inplace(Track<T> &self, py::object key, const bool reverse) {
    py_sort_inplace(self.notes, key, reverse);
    py_sort_inplace(self.controls, key, reverse);
    py_sort_inplace(self.pitch_bends, key, reverse);
    py_sort_inplace(self.pedals, key, reverse);
    return self;
};

template<typename T>
py::object py_sort_track(Track<T> &self, py::object key, const bool reverse, const bool inplace = false) {
    if (inplace) {
        py_sort_track_inplace(self, key, reverse);
        return py::cast(self, py::return_value_policy::reference);
    } else {
        auto copy = self;
        return py::cast(py_sort_track_inplace(copy, key, reverse), py::return_value_policy::move);
    }
};


// shift time track
template<typename T>
py::object py_shift_time_track(Track<T> &self, const typename T::unit offset, const bool inplace = true) {
    if (inplace) {
        return py::cast(self.shift_time_inplace(offset), py::return_value_policy::reference);
    } else {
        return py::cast(self.shift_time(offset), py::return_value_policy::move);
    }
};

// shift pitch track
template<typename T>
py::object py_shift_pitch_track(Track<T> &self, const int8_t offset, const bool inplace = true) {
    if (inplace) {
        return py::cast(self.shift_pitch_inplace(offset), py::return_value_policy::reference);
    } else {
        return py::cast(self.shift_pitch(offset), py::return_value_policy::move);
    }
};

// shift velocity track
template<typename T>
py::object py_shift_velocity_track(Track<T> &self, const int8_t offset, const bool inplace = true) {
    if (inplace) {
        return py::cast(self.shift_velocity_inplace(offset), py::return_value_policy::reference);
    } else {
        return py::cast(self.shift_velocity(offset), py::return_value_policy::move);
    }
};

// bind score::Track<T>
template<typename T>
py::class_<score::Track<T>> bind_track_class(py::module &m, const std::string & name_) {
    const auto name = "Track" + name_;
    auto event = py::class_<score::Track<T>>(m, name.c_str())
        .def(py::init<>())
        .def(py::init<const score::Track<T> &>(), "Copy constructor", py::arg("other"))
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
        .def("sort", &py_sort_track<T>, py::arg("key")=py::none(), py::arg("reverse")=false, py::arg("inplace")=true)
        .def("end", &Track<T>::end)
        .def("start", &score::Track<T>::start)
        .def("note_num", &score::Track<T>::note_num)
        .def("empty", &score::Track<T>::empty)
        .def("clip", &Track<T>::clip, "Clip notes and controls to a given time range", py::arg("start"), py::arg("end"), py::arg("clip_end")=false)
        .def("shift_time", &py_shift_time_track<T>, py::arg("offset"), py::arg("inplace")=false)
        .def("shift_pitch", &py_shift_pitch_track<T>, py::arg("offset"), py::arg("inplace")=false)
        .def("shift_velocity", &py_shift_velocity_track<T>, py::arg("offset"), py::arg("inplace")=false)
        .def("onset_pianoroll", [](score::Track<T> &self, float quantization) {
            score::pianoroll::TrackPianoRoll pianoroll = self.onset_pianoroll(quantization);

            return py::array_t<pianoroll::pianoroll_t>(py::buffer_info{
                pianoroll.data,
                sizeof(pianoroll::pianoroll_t),
                py::format_descriptor<pianoroll::pianoroll_t>::format(),
                2,
                { pianoroll.pitch_dim, pianoroll.time_dim },
                { sizeof(pianoroll::pianoroll_t) * pianoroll.time_dim,
                sizeof(pianoroll::pianoroll_t) }
            });
        })
        .def("frame_pianoroll", [](score::Track<T> &self, float quantization) {
            score::pianoroll::TrackPianoRoll pianoroll = self.frame_pianoroll(quantization);

            return py::array_t<pianoroll::pianoroll_t>(py::buffer_info{
                pianoroll.data,
                sizeof(pianoroll::pianoroll_t),
                py::format_descriptor<pianoroll::pianoroll_t>::format(),
                2,
                { pianoroll.pitch_dim, pianoroll.time_dim },
                { sizeof(pianoroll::pianoroll_t) * pianoroll.time_dim,
                sizeof(pianoroll::pianoroll_t) }
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
py::object py_sort_score(Score<T> &self, py::object key, bool reverse, bool inplace = false) {
    if (inplace) {
        py_sort_score_inplace(self, key, reverse);
        return py::cast(self, py::return_value_policy::reference);
    } else {
        auto copy = self;
        py_sort_score_inplace(copy, key, reverse);
        return py::cast(copy, py::return_value_policy::move);
    }
};

// py shift time score
template<typename T>
py::object py_shift_time_score(Score<T> &self, const typename T::unit offset, const bool inplace = true) {
    if (inplace) {
        return py::cast(self.shift_time_inplace(offset), py::return_value_policy::reference);
    } else {
        return py::cast(self.shift_time(offset), py::return_value_policy::move);
    }
};

// py shift pitch score
template<typename T>
py::object py_shift_pitch_score(Score<T> &self, const int8_t offset, const bool inplace = true) {
    if (inplace) {
        return py::cast(self.shift_pitch_inplace(offset), py::return_value_policy::reference);
    } else {
        return py::cast(self.shift_pitch(offset), py::return_value_policy::move);
    }
};

// py shift velocity score
template<typename T>
py::object py_shift_velocity_score(Score<T> &self, const int8_t offset, const bool inplace = true) {
    if (inplace) {
        return py::cast(self.shift_velocity_inplace(offset), py::return_value_policy::reference);
    } else {
        return py::cast(self.shift_velocity(offset), py::return_value_policy::move);
    }
};


// bind score::Score<T>
template<typename T>
py::class_<Score<T>> bind_score_class(py::module &m, const std::string & name_) {
    const auto name = "Score" + name_;

    return py::class_<Score<T>>(m, name.c_str())
        .def(py::init<>())
        .def(py::init<const Score<T> &>(), "Copy constructor", py::arg("other"))
        .def(py::init(&Score<T>::from_file), "Load from midi file", py::arg("path"))
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
        .def("sort", &py_sort_score<T>, py::arg("key")=py::none(), py::arg("reverse")=false, py::arg("inplace")=true)
        .def("clip", &Score<T>::clip, "Clip events a given time range", py::arg("start"), py::arg("end"), py::arg("clip_end")=false)
        .def("shift_time", &py_shift_time_score<T>, py::arg("offset"), py::arg("inplace")=false)
        .def("shift_pitch", &py_shift_pitch_score<T>, py::arg("offset"), py::arg("inplace")=false)
        .def("shift_velocity", &py_shift_velocity_score<T>, py::arg("offset"), py::arg("inplace")=false)
        .def("start", &Score<T>::start)
        .def("end", &Score<T>::end)
        .def("note_num", &Score<T>::note_num)
        .def("empty", &Score<T>::empty)
        .def("onset_pianoroll", [](score::Score<T> &self, float quantization) {
            score::pianoroll::ScorePianoRoll pianoroll = self.onset_pianoroll(quantization);

            return py::array_t<pianoroll::pianoroll_t>(py::buffer_info{
                pianoroll.data,
                sizeof(pianoroll::pianoroll_t),
                py::format_descriptor<pianoroll::pianoroll_t>::format(),
                3,
                { pianoroll.track_dim, pianoroll.pitch_dim, pianoroll.time_dim },
                { sizeof(pianoroll::pianoroll_t) * pianoroll.time_dim * pianoroll.pitch_dim,
                sizeof(pianoroll::pianoroll_t) * pianoroll.time_dim,
                sizeof(pianoroll::pianoroll_t) }
            });
        })
        .def("frame_pianoroll", [](score::Score<T> &self, float quantization) {
            score::pianoroll::ScorePianoRoll pianoroll = self.frame_pianoroll(quantization);

            return py::array_t<pianoroll::pianoroll_t>(py::buffer_info{
                pianoroll.data,
                sizeof(pianoroll::pianoroll_t),
                py::format_descriptor<pianoroll::pianoroll_t>::format(),
                3,
                { pianoroll.track_dim, pianoroll.pitch_dim, pianoroll.time_dim },
                { sizeof(pianoroll::pianoroll_t) * pianoroll.time_dim * pianoroll.pitch_dim,
                sizeof(pianoroll::pianoroll_t) * pianoroll.time_dim,
                sizeof(pianoroll::pianoroll_t) }
            });
        });
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
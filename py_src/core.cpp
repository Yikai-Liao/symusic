//
// Created by lyk on 23-9-20.
//
#include <string>
#include <functional>
#include <nanobind/nanobind.h>
#include <nanobind/stl/optional.h>
#include <nanobind/stl/string.h>
#include <nanobind/stl/vector.h>
#include <nanobind/ndarray.h>
#include <nanobind/stl/filesystem.h>
// #include <nanobind/operators.h>
#include <nanobind/stl/bind_vector.h>

#include "symusic.h"

namespace py = nanobind;
namespace symusic {

#define DECLARE_OBJ(__COUNT, NAME)          \
    extern template struct NAME<Tick>;      \
    extern template struct NAME<Quarter>;   \
    extern template struct NAME<Second>;

REPEAT_ON(DECLARE_OBJ, Note, ControlChange, Pedal, TimeSignature, KeySignature, Tempo, PitchBend, TextMeta, Track, Score)

}

#undef DECLARE_OBJ

using namespace symusic;


#define OPAQUE_VEC(i, TYPE)                     \
    NB_MAKE_OPAQUE(vec<TYPE<Tick>>)       \
    NB_MAKE_OPAQUE(vec<TYPE<Quarter>>)    \
    NB_MAKE_OPAQUE(vec<TYPE<Second>>)

REPEAT_ON(OPAQUE_VEC, Note, ControlChange, Pedal, TimeSignature, KeySignature, Tempo, PitchBend, TextMeta, Track)

NB_MAKE_OPAQUE(vec<Tick::unit>)
NB_MAKE_OPAQUE(vec<Quarter::unit>)
// PYBIND11_MAKE_OPAQUE(vec<Second::unit>)

template<typename T>
void sort_by_py_key(vec<T> &self, const py::object & key) {
    pdqsort(self.begin(), self.end(), [&key](const T &a, const T &b) {
        return key(a) < key(b);
    });
}

template<typename T>
vec<T> & py_sort_inplace(vec<T> &self, const py::callable & key, const bool reverse) {
    if (key.is_none()) ops::sort_by_time(self);
    else sort_by_py_key(self, key);
    if (reverse) std::reverse(self.begin(), self.end());
    return self;
}

template<typename T>
py::object py_sort(vec<T> &self, const py::callable & key, const bool reverse, const bool inplace = false) {
    if (inplace) {
        py_sort_inplace(self, key, reverse);
        return py::cast(self, py::rv_policy::reference);
    } else {
        auto copy = vec<T>(self);
        return py::cast(py_sort_inplace(copy, key, reverse), py::rv_policy::move);
    }
}

template<typename T>
py::bytes py_to_bytes(const T &self) {
    auto data = dumps<DataFormat::ZPP>(self);
    return py::bytes(reinterpret_cast<const char *>(data.data()), data.size());
}

template <typename T>
T py_from_bytes(const py::bytes &bytes) {
    // cast bytes to string_view
    const auto data = std::string_view(bytes.c_str(), bytes.size());
    const std::span span(reinterpret_cast<const u8 *>(data.data()), data.size());
    return parse<DataFormat::ZPP, T>(span);
}

template<typename T>
py::object py_filter (vec<T> & self, py::callable & func, const bool inplace) {
    vec<T> ans; ans.reserve(self.size());
    for(auto & item: self) {
        if (py::cast<bool>(func(item))) ans.push_back(item);
    }   if(inplace) {
        self = std::move(ans);
        return py::cast(self, py::rv_policy::reference);
    }   return py::cast(ans, py::rv_policy::move);
}


#define NDARR(DTYPE, DIM) py::ndarray<DTYPE, py::ndim<DIM>, py::c_contig, py::device::cpu>

template<typename T>
std::tuple<py::class_<T>, py::class_<vec<T>>> time_stamp_base(py::module_ &m, const std::string &name) {
    typedef typename T::unit unit;
    auto event = py::class_<T>(m, name.c_str())
        .def_rw("time", &T::time)
        .def_prop_ro("ttype", [](const T &) { return typename T::ttype(); })
        .def("shift_time", [](T &self, const unit offset, const bool inplace) {
                if (inplace) return self.shift_time_inplace(offset);
                else return self.shift_time(offset);
            }, py::arg("offset"),
            py::arg("inplace")=false, "Shift the event time by offset")
        .def(py::init<const T &>(), "Copy constructor", py::arg("other"))
        .def("copy", &T::copy, "Deep copy", py::rv_policy::move)
        .def("__copy__", &T::copy, "Deep copy")
        .def("__deepcopy__", &T::copy, "Deep copy")
        .def("__repr__", &T::to_string)
        .def(py::self == py::self)  // NOLINT
        .def(py::self != py::self)  // NOLINT
        // .def(py::pickle( &py_to_bytes<T>, &py_from_bytes<T>));
        .def("__getstate__", &py_to_bytes<T>)
        .def("__setstate__", &py_from_bytes<T>);

    auto vec_T = py::bind_vector<vec<T>>(m, std::string(name + "List").c_str())
        .def_prop_ro("ttype", [](const T &) { return typename T::ttype(); })
        .def("sort", &py_sort<T>, py::arg("key")=py::none(), py::arg("reverse")=false, py::arg("inplace")=true)
        .def("__repr__", [](const vec<T> &self) {
            return fmt::format("{}", fmt::join(self, ",\n"));
        })
        // .def(py::pickle( &py_to_bytes<vec<T>>, &py_from_bytes<vec<T>>));
        .def("__getstate__", &py_to_bytes<vec<T>>)
        .def("__setstate__", &py_from_bytes<vec<T>>)
        .def("filter", &py_filter<T>, py::arg("func"), py::arg("inplace")=false);
    return std::make_tuple(event, vec_T);
}

void show(NDARR(float, 1) arg) {

}

// a template functions for binding all specializations of Note, and return it
template<typename T>
py::class_<Note<T>> bind_note_class(py::module_ &m, const std::string & name_) {
    typedef typename T::unit unit;
    const std::string name = "Note" + name_;

    auto [note, note_vec] = time_stamp_base<Note<T>>(m, name);
    note_vec.def(
        "numpy", [](const vec<Note<T>> &self) {
            auto * temp = new NoteArr<T>{self};
            py::capsule deleter(temp, [](void *p) noexcept {
                delete static_cast<NoteArr<T> *>(p);
            });
            size_t size = self.size();
            py::dict ans{};
            ans["time"] = py::ndarray<py::numpy, unit>{temp->time.data(), {size}, deleter};
            ans["duration"] = py::ndarray<py::numpy, unit>{temp->duration.data(), {size}, deleter};
            ans["pitch"] = py::ndarray<py::numpy, i8>{temp->pitch.data(), {size}, deleter};
            ans["velocity"] = py::ndarray<py::numpy, i8>{temp->velocity.data(), {size}, deleter};
            return ans;
        }
    );
    return note
        .def(py::init<unit, unit, int8_t, int8_t>(), py::arg("start"), py::arg("duration"), py::arg("pitch"), py::arg("velocity"))
        .def_rw("start", &Note<T>::time)
        .def_rw("duration", &Note<T>::duration)
        .def_rw("pitch", &Note<T>::pitch)
        .def_rw("velocity", &Note<T>::velocity)
        .def_prop_ro("end", &Note<T>::end)
        .def("empty", &Note<T>::empty, "duration <= 0 or velocity <= 0")
        .def("end_time", &Note<T>::end)
        .def("shift_pitch", [](Note<T> &self, const int8_t offset, const bool inplace) {
                if (inplace) return py::cast(self.shift_pitch_inplace(offset), py::rv_policy::reference);
                else return py::cast(self.shift_pitch(offset), py::rv_policy::move);
            }, py::arg("offset"), py::arg("inplace")=false, "Shift the pitch by offset")
        .def("shift_velocity", [](Note<T> &self, const int8_t offset, const bool inplace) {
                if (inplace) return py::cast(self.shift_velocity_inplace(offset), py::rv_policy::reference);
                else return py::cast(self.shift_velocity(offset), py::rv_policy::move);
            }, py::arg("offset"), py::arg("inplace")=false, "Shift the velocity by offset")
        .def("from_numpy", [](NDARR(unit, 1) time, NDARR(unit, 1) duration, NDARR(i8, 1) pitch, NDARR(i8, 1) velocity) {
            if(time.size() != duration.size() || time.size() != pitch.size() || time.size() != velocity.size()) {
                throw std::invalid_argument("time, duration, pitch, velocity must have the same size");
            }
            auto size = time.size();
            vec<Note<T>> ans; ans.reserve(size);
            for(size_t i = 0; i < size; ++i) {
                ans.emplace_back(time(i), duration(i), pitch(i), velocity(i));
            }   return ans;
        });
}

// bind symusic::KeySignature<T>
template<typename T>
py::class_<symusic::KeySignature<T>> bind_key_signature_class(py::module_ &m, const std::string & name_) {
    typedef typename T::unit unit;
    const auto name = "KeySignature" + name_;

    auto [k, k_vec] = time_stamp_base<symusic::KeySignature<T>>(m, name);
    k_vec.def(
        "numpy", [](const vec<symusic::KeySignature<T>> &self) {
            auto * temp = new KeySignatureArr<T>{self};
            py::capsule deleter(temp, [](void *p) noexcept {
                delete static_cast<KeySignatureArr<T> *>(p);
            });
            size_t size = self.size();
            py::dict ans{};
            ans["time"] = py::ndarray<py::numpy, unit>{temp->time.data(), {size}, deleter};
            ans["key"] = py::ndarray<py::numpy, i8>{temp->key.data(), {size}, deleter};
            ans["tonality"] = py::ndarray<py::numpy, i8>{temp->tonality.data(), {size}, deleter};
            return ans;
        }
    );
    return k
        .def(py::init<unit, i8, i8>(), py::arg("time"), py::arg("key"), py::arg("tonality"))
        .def_rw("key", &symusic::KeySignature<T>::key)
        .def_rw("tonality", &symusic::KeySignature<T>::tonality)
        .def_prop_ro("degree", &symusic::KeySignature<T>::degree)
        .def("from_numpy", [](NDARR(unit, 1) time, NDARR(i8, 1) key, NDARR(i8, 1) tonality) {
            if(time.size() != key.size() || time.size() != tonality.size()) {
                throw std::invalid_argument("time, key, tonality must have the same size");
            }
            auto size = time.size();
            vec<symusic::KeySignature<T>> ans; ans.reserve(size);
            for(size_t i = 0; i < size; ++i) {
                ans.emplace_back(time(i), key(i), tonality(i));
            }   return ans;
        });
}

// bind symusic::TimeSignature<T>
template<typename T>
py::class_<symusic::TimeSignature<T>> bind_time_signature_class(py::module_ &m, const std::string & name_) {
    typedef typename T::unit unit;
    const auto name = "TimeSignature" + name_;
    auto [time_sig, time_sig_vec] = time_stamp_base<symusic::TimeSignature<T>>(m, name);
    time_sig_vec.def(
        "numpy", [](const vec<symusic::TimeSignature<T>> &self) {
            auto * temp = new TimeSignatureArr<T>{self};
            py::capsule deleter(temp, [](void *p) noexcept {
                delete static_cast<TimeSignatureArr<T> *>(p);
            });
            size_t size = self.size();
            py::dict ans{};
            ans["time"] = py::ndarray<py::numpy, unit>{temp->time.data(), {size}, deleter};
            ans["numerator"] = py::ndarray<py::numpy, u8>{temp->numerator.data(), {size}, deleter};
            ans["denominator"] = py::ndarray<py::numpy, u8>{temp->denominator.data(), {size}, deleter};
            return ans;
        }
    );
    return time_sig
        .def(py::init<unit, u8, u8>())
        .def_rw("numerator", &symusic::TimeSignature<T>::numerator)
        .def_rw("denominator", &symusic::TimeSignature<T>::denominator)
        .def("from_numpy", [](NDARR(unit, 1) time, NDARR(u8, 1) numerator, NDARR(u8, 1) denominator) {
            if(time.size() != numerator.size() || time.size() != denominator.size()) {
                throw std::invalid_argument("time, numerator, denominator must have the same size");
            }
            auto size = time.size();
            vec<symusic::TimeSignature<T>> ans; ans.reserve(size);
            for(size_t i = 0; i < size; ++i) {
                ans.emplace_back(time(i), numerator(i), denominator(i));
            }   return ans;
        });
}

// bind symusic::ControlChange<T>
template<typename T>
py::class_<symusic::ControlChange<T>> bind_control_change_class(py::module_ &m, const std::string & name_) {
    typedef typename T::unit unit;
    const auto name = "ControlChange" + name_;
    auto [control_change, control_change_vec] = time_stamp_base<symusic::ControlChange<T>>(m, name);
    control_change_vec.def(
        "numpy", [](const vec<symusic::ControlChange<T>> &self) {
            auto * temp = new ControlChangeArr<T>{self};
            py::capsule deleter(temp, [](void *p) noexcept {
                delete static_cast<ControlChangeArr<T> *>(p);
            });
            size_t size = self.size();
            py::dict ans{};
            ans["time"] = py::ndarray<py::numpy, unit>{temp->time.data(), {size}, deleter};
            ans["number"] = py::ndarray<py::numpy, u8>{temp->number.data(), {size}, deleter};
            ans["value"] = py::ndarray<py::numpy, u8>{temp->value.data(), {size}, deleter};
            return ans;
        }
    );

    return control_change
        .def(py::init<unit, u8, u8>(), py::arg("time"), py::arg("number"), py::arg("value"))
        .def_rw("value", &symusic::ControlChange<T>::value)
        .def_rw("number", &symusic::ControlChange<T>::number)
        .def("from_numpy", [](NDARR(unit, 1) time, NDARR(u8, 1) number, NDARR(u8, 1) value) {
            if(time.size() != number.size() || time.size() != value.size()) {
                throw std::invalid_argument("time, number, value must have the same size");
            }
            auto size = time.size();
            vec<symusic::ControlChange<T>> ans; ans.reserve(size);
            for(size_t i = 0; i < size; ++i) {
                ans.emplace_back(time(i), number(i), value(i));
            }   return ans;
        });
}

// bind Pedal<T>
template<typename T>
py::class_<symusic::Pedal<T>> bind_pedal_class(py::module_ &m, const std::string & name_) {
    typedef typename T::unit unit;
    const auto name = "Pedal" + name_;

    auto [pedal, pedal_vec] = time_stamp_base<symusic::Pedal<T>>(m, name);

    pedal_vec.def(
        "numpy", [](const vec<symusic::Pedal<T>> &self) {
            auto * temp = new PedalArr<T>{self};
            py::capsule deleter(temp, [](void *p) noexcept {
                delete static_cast<PedalArr<T> *>(p);
            });
            size_t size = self.size();
            py::dict ans{};
            ans["time"] = py::ndarray<py::numpy, unit>{temp->time.data(), {size}, deleter};
            ans["duration"] = py::ndarray<py::numpy, unit>{temp->duration.data(), {size}, deleter};
            return ans;
        }
    );


    return pedal
        .def(py::init<unit, unit>(), py::arg("time"), py::arg("duration"))
        .def_rw("duration", &symusic::Pedal<T>::duration)
        .def_prop_ro("end", &symusic::Pedal<T>::end)
        .def("from_numpy", [](NDARR(unit, 1) time, NDARR(unit, 1) duration) {
            if(time.size() != duration.size()) {
                throw std::invalid_argument("time, duration must have the same size");
            }
            auto size = time.size();
            vec<symusic::Pedal<T>> ans; ans.reserve(size);
            for(size_t i = 0; i < size; ++i) {
                ans.emplace_back(time(i), duration(i));
            }   return ans;
        });
}

// bind symusic::Tempo<T>
template<typename T>
py::class_<symusic::Tempo<T>> bind_tempo_class(py::module_ &m, const std::string & name_) {
    typedef typename T::unit unit;
    const auto name = "Tempo" + name_;
    auto [tempo, tempo_vec] = time_stamp_base<symusic::Tempo<T>>(m, name);
    tempo_vec.def(
        "numpy", [](const vec<symusic::Tempo<T>> &self) {
            auto * temp = new TempoArr<T>{self};
            py::capsule deleter(temp, [](void *p) noexcept {
                delete static_cast<TempoArr<T> *>(p);
            });
            size_t size = self.size();
            py::dict ans{};
            ans["time"] = py::ndarray<py::numpy, unit>{temp->time.data(), {size}, deleter};
            ans["mspq"] = py::ndarray<py::numpy, i32>{temp->mspq.data(), {size}, deleter};
            return ans;
        }
    );
    return tempo
        .def("__init__", [](Tempo<T>* self, unit time, std::optional<double> qpm, std::optional<i32> mspq) {
            new (self) Tempo<T>(); self->time = time;
            if (qpm.has_value()) self->set_qpm(*qpm);
            else if (mspq.has_value()) self->mspq = *mspq;
            else throw std::invalid_argument("qpm or mspq must be specified");
        }, py::arg("time"), py::arg("qpm")=py::none(), py::arg("mspq")=py::none())
        .def_rw("mspq", &symusic::Tempo<T>::mspq, "Microseconds per quarter note")
        .def_prop_rw("tempo", &Tempo<T>::qpm, &Tempo<T>::set_qpm, "The same as qpm")
        .def_prop_rw("qpm", &Tempo<T>::qpm, &Tempo<T>::set_qpm, "Quarter per minute, the same as tempo")
        .def("from_numpy", [](NDARR(unit, 1) time, NDARR(i32, 1) mspq) {
            if(time.size() != mspq.size()) {
                throw std::invalid_argument("time, mspq must have the same size");
            }
            auto size = time.size();
            vec<symusic::Tempo<T>> ans; ans.reserve(size);
            for(size_t i = 0; i < size; ++i) {
                ans.emplace_back(time(i), mspq(i));
            }   return ans;
        });
}

// bind symusic::PitchBend<T>
template<typename T>
py::class_<symusic::PitchBend<T>> bind_pitch_bend_class(py::module_ &m, const std::string & name_) {
    typedef typename T::unit unit;
    const auto name = "PitchBend" + name_;
    auto [pitch_bend, pitch_bend_vec] = time_stamp_base<symusic::PitchBend<T>>(m, name);
    pitch_bend_vec.def(
        "numpy", [](const vec<symusic::PitchBend<T>> &self) {
            auto * temp = new PitchBendArr<T>{self};
            py::capsule deleter(temp, [](void *p) noexcept {
                delete static_cast<PitchBendArr<T> *>(p);
            });
            size_t size = self.size();
            py::dict ans{};
            ans["time"] = py::ndarray<py::numpy, unit>{temp->time.data(), {size}, deleter};
            ans["value"] = py::ndarray<py::numpy, i32>{temp->value.data(), {size}, deleter};
            return ans;
        }
    );

    return pitch_bend
        .def(py::init<unit, i32>(), py::arg("time"), py::arg("value"))
        .def_rw("value", &symusic::PitchBend<T>::value)
        .def("from_numpy", [](NDARR(unit, 1) time, NDARR(i32, 1) value) {
            if(time.size() != value.size()) {
                throw std::invalid_argument("time, value must have the same size");
            }
            auto size = time.size();
            vec<symusic::PitchBend<T>> ans; ans.reserve(size);
            for(size_t i = 0; i < size; ++i) {
                ans.emplace_back(time(i), value(i));
            }   return ans;
        });
}

// bind symusic::TextMeta<T>
template<typename T>
py::class_<symusic::TextMeta<T>> bind_text_meta_class(py::module_ &m, const std::string & name_) {
    typedef typename T::unit unit;
    const auto name = "TextMeta" + name_;

    auto [text_meta, text_meta_vec] = time_stamp_base<symusic::TextMeta<T>>(m, name);
    text_meta_vec.def(
        "numpy", [](const vec<symusic::TextMeta<T>> &self) {
            throw std::runtime_error("TextMeta does not support numpy");
        }
    );

    return text_meta
        .def(py::init<unit, std::string &>(), py::arg("time"), py::arg("text"))
        .def_rw("text", &symusic::TextMeta<T>::text)
        .def("from_numpy", [](NDARR(unit, 1) time, NDARR(std::string, 1) text) {
            throw std::runtime_error("TextMeta does not support numpy");
        });
}

template<typename T>
Track<T>&py_sort_track_inplace(Track<T> &self, py::callable &key, const bool reverse) {
    py_sort_inplace(self.notes, key, reverse);
    py_sort_inplace(self.controls, key, reverse);
    py_sort_inplace(self.pitch_bends, key, reverse);
    py_sort_inplace(self.pedals, key, reverse);
    return self;
};

template<typename T>
py::object py_sort_track(Track<T> &self, py::callable &key, const bool reverse, const bool inplace = false) {
    if (inplace) {
        py_sort_track_inplace(self, key, reverse);
        return py::cast(self, py::rv_policy::reference);
    } else {
        auto copy = self;
        return py::cast(py_sort_track_inplace(copy, key, reverse), py::rv_policy::move);
    }
};


// shift time track
template<typename T>
py::object py_shift_time_track(Track<T> &self, const typename T::unit offset, const bool inplace = true) {
    if (inplace) {
        return py::cast(self.shift_time_inplace(offset), py::rv_policy::reference);
    } else {
        return py::cast(self.shift_time(offset), py::rv_policy::move);
    }
};

// shift pitch track
template<typename T>
py::object py_shift_pitch_track(Track<T> &self, const int8_t offset, const bool inplace = true) {
    if (inplace) {
        return py::cast(self.shift_pitch_inplace(offset), py::rv_policy::reference);
    } else {
        return py::cast(self.shift_pitch(offset), py::rv_policy::move);
    }
};

// shift velocity track
template<typename T>
py::object py_shift_velocity_track(Track<T> &self, const int8_t offset, const bool inplace = true) {
    if (inplace) {
        return py::cast(self.shift_velocity_inplace(offset), py::rv_policy::reference);
    } else {
        return py::cast(self.shift_velocity(offset), py::rv_policy::move);
    }
};

// bind Track<T>
template<typename T>
py::class_<Track<T>> bind_track_class(py::module_ &m, const std::string & name_) {
    const auto name = "Track" + name_;
    auto event = py::class_<Track<T>>(m, name.c_str())
        .def(py::init<>())
        .def(py::init<const Track<T> &>(), "Copy constructor", py::arg("other"))
        .def("copy", &Track<T>::copy, "Deep copy", py::rv_policy::move)
        .def("__copy__", &Track<T>::copy, "Deep copy")
        .def("__deepcopy__", &Track<T>::copy, "Deep copy")
        .def("__repr__", &Track<T>::to_string)
        .def_rw("notes", &Track<T>::notes)
        .def_rw("controls", &Track<T>::controls)
        .def_rw("pitch_bends", &Track<T>::pitch_bends)
        .def_rw("pedals", &Track<T>::pedals)
        .def_rw("is_drum", &Track<T>::is_drum)
        .def_rw("program", &Track<T>::program)
        .def_rw("name", &Track<T>::name)
        .def_prop_ro("ttype", [](const Track<T> &) { return T(); })
        // .def(py::pickle( &py_to_bytes<Track<T>>, &py_from_bytes<Track<T>>))
        .def("__getstate__", &py_to_bytes<Track<T>>)
        .def("__setstate__", &py_from_bytes<Track<T>>)
        .def(py::self == py::self)  // NOLINT
        .def(py::self != py::self)  // NOLINT
        .def("sort", &py_sort_track<T>, py::arg("key")=py::none(), py::arg("reverse")=false, py::arg("inplace")=false)
        .def("end", &Track<T>::end)
        .def("start", &Track<T>::start)
        .def("note_num", &Track<T>::note_num)
        .def("empty", &Track<T>::empty)
        .def("clip", &Track<T>::clip, "Clip notes and controls to a given time range", py::arg("start"), py::arg("end"), py::arg("clip_end")=false)
        .def("shift_time", &py_shift_time_track<T>, py::arg("offset"), py::arg("inplace")=false)
        .def("shift_pitch", &py_shift_pitch_track<T>, py::arg("offset"), py::arg("inplace")=false)
        .def("shift_velocity", &py_shift_velocity_track<T>, py::arg("offset"), py::arg("inplace")=false)
        .def("pianoroll", [](Track<Tick> &self,
            const std::vector<std::string>& modes,
            const std::pair<uint8_t, uint8_t> pitchRange,
            bool encodeVelocity) {
            std::vector<PianorollMode> modesEnum(modes.size());
            for (int i = 0; i < modes.size(); ++i) {
                modesEnum[i] = symusic::str_to_pianoroll_mode(modes[i]);
            }
            TrackPianoroll pianoroll = TrackPianoroll::from_track(self, modesEnum, pitchRange, encodeVelocity);

            return py::ndarray<py::numpy, pianoroll_t>{const_cast<uint8_t*>(pianoroll.release()),
                {std::get<0>(pianoroll.dims()), std::get<1>(pianoroll.dims()), std::get<2>(pianoroll.dims()), }};
        }, py::arg("modes"), py::arg("pitchRange")=std::pair<uint8_t, uint8_t>(0, 127), py::arg("encodeVelocity")=false);

    py::bind_vector<vec<Track<T>>>(m, std::string(name + "List").c_str())
        .def("sort", [](vec<Track<T>> &self, const py::object & key, const bool reverse, const bool inplace) {
            if (key.is_none()) throw std::invalid_argument("key must be specified");
            if (inplace) {
                sort_by_py_key(self, key);
                if (reverse) std::reverse(self.begin(), self.end());
                return py::cast(self, py::rv_policy::reference);
            } else { // copy
                auto copy = vec<Track<T>>(self);
                sort_by_py_key(copy, key);
                if (reverse) std::reverse(copy.begin(), copy.end());
                return py::cast(copy, py::rv_policy::move);
            }
        }, py::arg("key"), py::arg("reverse")=false, py::arg("inplace")=true)
        .def("__repr__", [](const vec<Track<T>> &self) {
            vec<std::string> strs; strs.reserve(self.size());
            for(const auto & track: self) {
                strs.emplace_back(track.to_string());
            }
            return fmt::format("[{}]", fmt::join(strs, ", "));
        })
        // .def(py::pickle( &py_to_bytes<vec<Track<T>>>, &py_from_bytes<vec<Track<T>>>))
        .def("__getstate__", &py_to_bytes<vec<Track<T>>>)
        .def("__setstate__", &py_from_bytes<vec<Track<T>>>)
        .def_prop_ro("ttype", [](const vec<Track<T>> &){ return T(); })
        .def("filter", &py_filter<Track<T>>, py::arg("func"), py::arg("inplace")=false);

    py::implicitly_convertible<py::list, vec<Track<T>>>();

    return event;
}

// py sort score
template<typename T>
Score<T>& py_sort_score_inplace(Score<T> &self, py::callable& key, bool reverse) {
    py_sort_inplace(self.time_signatures, key, reverse);
    py_sort_inplace(self.key_signatures, key, reverse);
    py_sort_inplace(self.tempos, key, reverse);
    py_sort_inplace(self.lyrics, key, reverse);
    py_sort_inplace(self.markers, key, reverse);
    for(auto &track: self.tracks)
        py_sort_track_inplace(track, key, reverse);
    return self;
}

template<typename T>
py::object py_sort_score(Score<T> &self, py::callable& key, bool reverse, bool inplace = false) {
    if (inplace) {
        py_sort_score_inplace(self, key, reverse);
        return py::cast(self, py::rv_policy::reference);
    } else {
        auto copy = self;
        py_sort_score_inplace(copy, key, reverse);
        return py::cast(copy, py::rv_policy::move);
    }
}

// py shift time score
template<typename T>
py::object py_shift_time_score(Score<T> &self, const typename T::unit offset, const bool inplace = true) {
    if (inplace) {
        return py::cast(self.shift_time_inplace(offset), py::rv_policy::reference);
    } else {
        return py::cast(self.shift_time(offset), py::rv_policy::move);
    }
}

// py shift pitch score
template<typename T>
py::object py_shift_pitch_score(Score<T> &self, const int8_t offset, const bool inplace = true) {
    if (inplace) {
        return py::cast(self.shift_pitch_inplace(offset), py::rv_policy::reference);
    } else {
        return py::cast(self.shift_pitch(offset), py::rv_policy::move);
    }
}

// py shift velocity score
template<typename T>
py::object py_shift_velocity_score(Score<T> &self, const int8_t offset, const bool inplace = true) {
    if (inplace) {
        return py::cast(self.shift_velocity_inplace(offset), py::rv_policy::reference);
    } else {
        return py::cast(self.shift_velocity(offset), py::rv_policy::move);
    }
}

template<TType T, typename PATH>
Score<T> midi2score(PATH path) {
    auto data = read_file(path);
    Score<T> s = Score<T>::template parse<DataFormat::MIDI>(data);
    return s;
}

template<TType T, typename PATH>
void dump_midi(const Score<T> &self, PATH path) {
    const auto data = self.template dumps<DataFormat::MIDI>();
    write_file(path, data);
}


// bind symusic::Score<T>
template<typename T>
py::class_<Score<T>> bind_score_class(py::module_ &m, const std::string & name_) {
    const auto name = "Score" + name_;

    return py::class_<Score<T>>(m, name.c_str())
        .def(py::init<const i32>(), py::arg("tpq"))
        // .def(py::init([](const Score<T> &other) { return other.copy(); }), "Copy constructor", py::arg("other"))
        .def("__init__", [](Score<T> *self, const Score<T> &other) { new (self) Score<T>(other); }, "Copy constructor", py::arg("other"))
        // .def(py::init(&midi2score<T,std::string>), "Load from midi file", py::arg("path"))
        .def("__init__", [](Score<T> *self, const std::string &path) {
            new (self) Score<T>(std::move(midi2score<T, std::string>(path)));
        }, "Load from midi file", py::arg("path"))
        // .def(py::init(&midi2score<T, std::filesystem::path>), "Load from midi file", py::arg("path"))
        .def("__init__", [](Score<T> *self, const std::filesystem::path &path) {
            new (self) Score<T>(std::move(midi2score<T, std::filesystem::path>(path)));
        }, "Load from midi file", py::arg("path"))
        .def("copy", &Score<T>::copy, "Deep copy", py::rv_policy::move)
        .def("__copy__", &Score<T>::copy, "Deep copy")
        .def("__deepcopy__", &Score<T>::copy, "Deep copy")
        .def("__repr__", &Score<T>::to_string)
        .def_static("from_file", [](const std::string &path) {
            return midi2score<T, std::string>(path);
        }, "Load from midi file", py::arg("path"))
        // pybind11 will binding class method in an erratic way: https://github.com/pybind/pybind11/issues/1693
        .def("dump_midi", &dump_midi<T, std::string>, "Dump to midi file", py::arg("path"))
        .def("dump_midi", &dump_midi<T, std::filesystem::path>, "Dump to midi file", py::arg("path"))
        .def_rw("ticks_per_quarter", &Score<T>::ticks_per_quarter)
        .def_rw("tracks", &Score<T>::tracks)
        .def_rw("time_signatures", &Score<T>::time_signatures)
        .def_rw("key_signatures", &Score<T>::key_signatures)
        .def_rw("tempos", &Score<T>::tempos)
        .def_rw("lyrics", &Score<T>::lyrics)
        .def_rw("markers", &Score<T>::markers)
        .def_prop_ro("ttype", [](const Score<T> &) { return T(); })
        // .def(py::pickle( &py_to_bytes<Score<T>>, &py_from_bytes<Score<T>>))
        .def("__getstate__", &py_to_bytes<Score<T>>)
        .def("__setstate__", &py_from_bytes<Score<T>>)
        .def(py::self == py::self)  // NOLINT
        .def(py::self != py::self)  // NOLINT
        .def("sort", &py_sort_score<T>, py::arg("key")=py::none(), py::arg("reverse")=false, py::arg("inplace")=false)
        .def("clip", &Score<T>::clip, "Clip events a given time range", py::arg("start"), py::arg("end"), py::arg("clip_end")=false)
        .def("shift_time", &py_shift_time_score<T>, py::arg("offset"), py::arg("inplace")=false)
        .def("shift_pitch", &py_shift_pitch_score<T>, py::arg("offset"), py::arg("inplace")=false)
        .def("shift_velocity", &py_shift_velocity_score<T>, py::arg("offset"), py::arg("inplace")=false)
        .def("start", &Score<T>::start)
        .def("end", &Score<T>::end)
        .def("note_num", &Score<T>::note_num)
        .def("empty", &Score<T>::empty)
        .def("pianoroll", [](Score<Tick> &self,
            const std::vector<std::string>& modes,
            const std::pair<uint8_t, uint8_t> pitchRange,
            bool encodeVelocity) {
            std::vector<PianorollMode> modesEnum(modes.size());
            for (int i = 0; i < modes.size(); ++i) {
                modesEnum[i] = symusic::str_to_pianoroll_mode(modes[i]);
            }
            ScorePianoroll pianoroll = ScorePianoroll::from_score(self, modesEnum, pitchRange, encodeVelocity);

            return py::ndarray<py::numpy, pianoroll_t>{const_cast<uint8_t*>(pianoroll.release()),
                { std::get<0>(pianoroll.dims()),
                    std::get<1>(pianoroll.dims()),
                    std::get<2>(pianoroll.dims()),
                    std::get<3>(pianoroll.dims()) }
            };
        }, py::arg("modes"), py::arg("pitchRange")=std::pair<uint8_t, uint8_t>(0, 127), py::arg("encodeVelocity")=false);
}

template<TType T>
typename T::unit cast_time(const py::object &t) {
    typedef typename T::unit unit;
    if(t.is_none()) return static_cast<unit>(0);
    return py::cast<unit>(t);
}

template<typename T>
py::object convert_score(const Score<T> &self, const py::object &ttype, const py::object & min_dur){
    if (ttype.is_none()) throw std::invalid_argument("ttype must be specified");
    if (py::isinstance<Tick>(ttype))    return py::cast(convert<Tick>(self, cast_time<Tick>(min_dur)), py::rv_policy::move);
    if (py::isinstance<Quarter>(ttype)) return py::cast(convert<Quarter>(self, cast_time<Quarter>(min_dur)), py::rv_policy::move);
    if (py::isinstance<Second>(ttype))  return py::cast(convert<Second>(self, cast_time<Second>(min_dur)), py::rv_policy::move);
    if (py::isinstance<py::str>(ttype)) {
        // convert ttype to lower case
        const auto ttype_str = py::cast<std::string>(ttype.attr("lower")());
        if (ttype_str == "tick")    return py::cast(convert<Tick>(self, cast_time<Tick>(min_dur)), py::rv_policy::move);
        if (ttype_str == "quarter") return py::cast(convert<Quarter>(self, cast_time<Quarter>(min_dur)), py::rv_policy::move);
        if (ttype_str == "second")  return py::cast(convert<Second>(self, cast_time<Second>(min_dur)), py::rv_policy::move);
    }   throw std::invalid_argument("ttype must be Tick, Quarter, Second or string");
}
//
// template<TType T>
// py::class_<NoteArr<T>> bind_note_arr(py::module_ &m, const std::string & name_) {
//     const auto name = "NoteArr" + name_;
//
//     return py::class_<NoteArr<T>>(m, name.c_str())
//         .def(py::init<const NoteArr<T> &>(), "Copy constructor", py::arg("other"))
//         .def(py::init<std::string, u8, bool>(), "create with name, program and is_drum", py::arg("name")="", py::arg("program")=0, py::arg("is_drum")=false)
//         // .def(py::init([](
//         //     std::string name, u8 program, bool is_drum,
//         //     const py::array_t<typename T::unit> &time,
//         //     const py::array_t<typename T::unit> &duration,
//         //     const py::array_t<int8_t> &pitch,
//         //     const py::array_t<int8_t> &velocity) {
//         //     if (time.size() != duration.size() || time.size() != pitch.size() || time.size() != velocity.size())
//         //         throw std::invalid_argument("time, duration, pitch and velocity must have the same size");
//         //     const auto size = time.size();
//         //     NoteArr<T> arr(name, program, is_drum);
//         //     arr.reserve(size);
//         //     for (size_t i = 0; i < size; ++i) {
//         //         arr.emplace_back(time.at(i), duration.at(i), pitch.at(i), velocity.at(i));
//         //     }
//         //     return arr;
//         // }), "Creat from numpy", py::arg("name")="", py::arg("program")=0, py::arg("is_drum")=false,
//         //     py::arg("time")=py::array_t<typename T::unit>(),
//         //     py::arg("duration")=py::array_t<typename T::unit>(),
//         //     py::arg("pitch")=py::array_t<int8_t>(),
//         //     py::arg("velocity")=py::array_t<int8_t>())
//         .def("copy", &NoteArr<T>::copy, "Deep copy", py::rv_policy::move)
//         .def("__copy__", &NoteArr<T>::copy, "Deep copy")
//         .def("__deepcopy__", &NoteArr<T>::copy, "Deep copy")
//         .def("__repr__", &NoteArr<T>::to_string)
//         .def(py::self == py::self)  // NOLINT
//         .def(py::self != py::self)  // NOLINT
//         // getter convert vector to numpy array (reference)
//         // setter accept numpy array using copy
//         // .def(py::pickle( &py_to_bytes<NoteArr<T>>, &py_from_bytes<NoteArr<T>>))
//         .def("__getstate__", &py_to_bytes<NoteArr<T>>)
//         .def("__setstate__", &py_from_bytes<NoteArr<T>>)
//         .def("start", &NoteArr<T>::start)
//         .def("end", &NoteArr<T>::end)
//         .def("note_num", &NoteArr<T>::note_num)
//         .def("empty", &NoteArr<T>::empty)
//         .def("valid", &NoteArr<T>::valid)
//         .def("summary" , &NoteArr<T>::summary)
//         .def("sort", &NoteArr<T>::sort, py::arg("reverse")=false)
//         .def("clip", &NoteArr<T>::clip, "Clip notes and controls to a given time range", py::arg("start"), py::arg("end"), py::arg("clip_end")=false)
//         .def("reserve", &NoteArr<T>::reserve, "Reserve memory for notes", py::arg("size"))
//         .def("push_back", &NoteArr<T>::emplace_back, py::arg("time"), py::arg("duration"), py::arg("pitch"), py::arg("velocity"))
//         .def("push_back", &NoteArr<T>::push_back, py::arg("note"))
//         .def_rw("pitch", &NoteArr<T>::pitch)
//         .def_rw("velocity", &NoteArr<T>::velocity)
//         .def_rw("time", &NoteArr<T>::time)
//         .def_rw("duration", &NoteArr<T>::duration);
//         // .def("numpy", [](NoteArr<T> &self) {
//         //     using namespace pybind11::literals; // to bring in the `_a` literal
//         //     // convert vector to numpy array
//         //     return py::dict("time"_a=py::array_t<typename T::unit>(self.time.size(), self.time.data()),
//         //                     "duration"_a=py::array_t<typename T::unit>(self.duration.size(), self.duration.data()),
//         //                     "pitch"_a=py::array_t<int8_t>(self.pitch.size(), self.pitch.data()),
//         //                     "velocity"_a=py::array_t<int8_t>(self.velocity.size(), self.velocity.data()));
//         // });
// }

py::module_ & core_module(py::module_ & m){
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

    auto tt = bind_track_class<Tick>(m, tick);
    auto tq = bind_track_class<Quarter>(m, quarter);
    auto ts = bind_track_class<Second>(m, second);

    // auto narr_t = bind_note_arr<Tick>(m, tick);
    // auto narr_q = bind_note_arr<Quarter>(m, quarter);
    // auto narr_s = bind_note_arr<Second>(m, second);
    //
    // tt.def("note_arr", &to_note_arr<Tick>, "Convert to NoteArr");
    // tq.def("note_arr", &to_note_arr<Quarter>, "Convert to NoteArr");
    // ts.def("note_arr", &to_note_arr<Second>, "Convert to NoteArr");
    //
    // narr_t.def("to_track", &to_track<Tick>, "Convert to Track");
    // narr_q.def("to_track", &to_track<Quarter>, "Convert to Track");
    // narr_s.def("to_track", &to_track<Second>, "Convert to Track");

    auto score_tick = bind_score_class<Tick>(m, tick);
    auto score_quarter = bind_score_class<Quarter>(m, quarter);
    auto score_second = bind_score_class<Second>(m, second);

    score_tick
        .def("to", &convert_score<Tick>, py::arg("ttype"), py::arg("min_dur") = py::none(), "Convert to another time unit")
        .def("resample", [](const Score<Tick> &self, const i32 tpq, const std::optional<i32> min_dur) {
            const auto min_dur_ = min_dur.has_value()? *min_dur: 0;
            return resample(self, tpq, min_dur_);
        }, py::arg("tpq"), py::arg("min_dur")=py::none(), "Resample to another ticks per quarter");
    score_quarter
        .def("to", &convert_score<Quarter>, py::arg("ttype"), py::arg("min_dur")=py::none(), "Convert to another time unit")
        .def("resample", [](const Score<Quarter> &self, const i32 tpq, const std::optional<i32> min_dur) {
            const auto min_dur_ = min_dur.has_value()? *min_dur: 0;
            return resample(self, tpq, min_dur_);
        }, py::arg("tpq"), py::arg("min_dur")=py::none(), "Resample to another ticks per quarter");

    score_second
        .def("to", &convert_score<Second>, py::arg("ttype"), py::arg("min_dur")=py::none(), "Convert to another time unit")
        .def("resample", [](const Score<Second> &self, const i32 tpq, const std::optional<i32> min_dur) {
            const auto min_dur_ = min_dur.has_value()? *min_dur: 0;
            return resample(self, tpq, min_dur_);
        }, py::arg("tpq"), py::arg("min_dur")=py::none(), "Resample to another ticks per quarter");
    return m;
}

NB_MODULE(core, m) {

    auto tick = py::class_<Tick>(m, "Tick")
        .def(py::init<>())
        .def("__repr__", [](const Tick &) { return "symsuic.core.Tick"; })
        .def("is_time_unit", [](const Tick &) { return true; });

    auto quarter = py::class_<Quarter>(m, "Quarter")
        .def(py::init<>())
        .def("__repr__", [](const Quarter &) { return "symsuic.core.Quarter"; })
        .def("is_time_unit", [](const Quarter &) { return true; });

    auto second = py::class_<Second>(m, "Second")
        .def(py::init<>())
        .def("__repr__", [](const Second &) { return "symsuic.core.Second"; })
        .def("is_time_unit", [](const Second &) { return true; });

    // bind vec for Tick::unit and Quarter::unit
    py::bind_vector<vec<i32>>(m, "i32List");
    py::bind_vector<vec<f32>>(m, "f32List");

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

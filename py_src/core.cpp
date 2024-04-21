//
// Created by lyk on 23-9-20.
//
#include <string>
#include <random>
#include <nanobind/nanobind.h>
// #include <nanobind/stl/shared_ptr.h>
// #include <nanobind/stl/optional.h>
#include <nanobind/stl/string.h>
#include <nanobind/stl/vector.h>
// #include <nanobind/ndarray.h>
// #include <nanobind/eigen/dense.h>
// #include <nanobind/stl/filesystem.h>
// #include <nanobind/stl/bind_vector.h>
#include "symusic.h"
#include "py_utils.h"
#include "MetaMacro.h"

#pragma warning(disable : 4996)

namespace nb = nanobind;
namespace symusic {
nb::module_& bind_synthesizer(nb::module_& m) {
    nb::class_<Synthesizer>(m, "Synthesizer")
        .def(
            nb::init<const std::string&, u32, u8>(),
            nb::arg("sf_path"),
            nb::arg("sample_rate"),
            nb::arg("quality")
        )
        .def(
            nb::init<const std::filesystem::path&, u32, u8>(),
            nb::arg("sf_path"),
            nb::arg("sample_rate"),
            nb::arg("quality")
        )
        .def(
            "render",
            [](Synthesizer& self, const shared<Score<Tick>>& score, const bool stereo) {
                return self.render(*score, stereo);
            }
        )
        .def(
            "render",
            [](Synthesizer& self, const shared<Score<Quarter>>& score, const bool stereo) {
                return self.render(*score, stereo);
            }
        )
        .def(
            "render",
            [](Synthesizer& self, const shared<Score<Second>>& score, const bool stereo) {
                return self.render(*score, stereo);
            }
        );


    m.def(
        "dump_wav",
        &psynth::write_audio,
        nb::arg("path"),
        nb::arg("data"),
        nb::arg("sample_rate"),
        nb::arg("use_int16") = true
    );
    return m;
}

template<typename T>
shared<vec<shared<T>>> deepcopy(const shared<vec<shared<T>>>& self) {
    auto ans = std::make_shared<vec<shared<T>>>();
    ans->reserve(self->size());
    for (const auto& item : *self) {
        ans->push_back(std::make_shared<T>(std::move(item->deepcopy())));
    }
    return ans;
}


using namespace pyutils;
template<TType T>
auto bind_track(nb::module_& m, const std::string& name_) {
    const auto name = "Track" + name_;
    using unit      = typename T::unit;
    using self_t    = shared<Track<T>>;
    using track_t   = Track<T>;
    using vec_t     = shared<vec<self_t>>;

    auto copy_func = [](const self_t& self) { return std::make_shared<track_t>(*self); };
    auto deepcopy_func
        = [](const self_t& self) { return std::make_shared<track_t>(std::move(self->deepcopy())); };

    // clang-format off
    auto track = nb::class_<shared<Track<T>>>(m, name.c_str())
        .def("__init__", &pyinit<Track<T>>)
        .def("__init__", &pyinit<Track<T>, std::string, u8, const bool>,
            nb::arg("name"), nb::arg("program")=0, nb::arg("is_drum")=false)
        .def("__init__", [](self_t *self, const self_t& other) {
            new (self) std::shared_ptr<track_t>(std::move(std::make_shared<track_t>(std::move(other->deepcopy()))));
        }, "Copy constructor", nb::arg("other"))
        .def("copy", copy_func, nb::rv_policy::copy)
        .def("__copy__", copy_func, nb::rv_policy::copy)
        .def("deepcopy", deepcopy_func, nb::rv_policy::copy)
        .def("__deepcopy__", deepcopy_func, nb::rv_policy::copy)
        .def("__repr__", [](const self_t& self) { return self->to_string(); })
        .def("__getstate__", [](const self_t& self) {
            const auto native = to_native(*self);
            const vec<unsigned char> data = dumps<DataFormat::ZPP>(native);
            return nb::bytes(reinterpret_cast<const char*>(data.data()), data.size());
        })
        .def("__setstate__", [](self_t& self, const nb::bytes& bytes) {
            const auto      data = std::string_view(bytes.c_str(), bytes.size());
            const std::span span(reinterpret_cast<const unsigned char*>(data.data()), data.size());
            auto ans = symusic::parse<symusic::DataFormat::ZPP, TrackNative<T>>(span);
            new (&self) self_t(std::move(std::make_shared<Track<T>>(std::move(to_shared(std::move(ans))))));
        })
        .def_prop_ro("ttype", [](const self_t&) { return T(); })
        .def_prop_rw(RW_COPY(shared<pyvec<Note<T>>>, "notes", notes))
        .def_prop_rw(RW_COPY(shared<pyvec<ControlChange<T>>>, "controls", controls))
        .def_prop_rw(RW_COPY(shared<pyvec<Pedal<T>>>, "pedals", pedals))
        .def_prop_rw(RW_COPY(shared<pyvec<PitchBend<T>>>, "pitch_bends", pitch_bends))
        .def_prop_rw(RW_COPY(bool, "is_drum", is_drum))
        .def_prop_rw(RW_COPY(u8, "program", program))
        .def_prop_rw(RW_COPY(std::string, "name", name))
        .def(nb::self == nb::self)   // NOLINT
        .def(nb::self != nb::self)   // NOLINT
        .def("end", [](const self_t& self) { return self->end(); })
        .def("start", [](const self_t& self) { return self->start(); })
        .def("note_num", [](const self_t& self) { return self->note_num(); })
        .def("empty", [](const self_t& self) { return self->empty(); })
        // TODO: we still need to determine the default value of "inplace"
        // TODO: we still need to determine the copy depth
        .def("clip", [](self_t& self, const unit start, const unit end, const bool clip_end, const bool inplace) {
            self_t ans = inplace ? self : std::make_shared<track_t>(std::move(self->deepcopy()));
            ans->clip_inplace(start, end, clip_end);
            return ans;
        }, nb::arg("start"), nb::arg("end"), nb::arg("clip_end") = false, nb::arg("inplace") = false)
        .def("sort", [](self_t& self, const bool reverse, const bool inplace) {
            self_t ans = inplace ? self : std::make_shared<track_t>(std::move(self->deepcopy()));
            ans->sort_inplace(reverse);
            return ans;
        }, nb::arg("reverse") = false, nb::arg("inplace") = false)
        .def("adjust_time", [](self_t& self, const vec<unit>& original_times, const vec<unit>& new_times, const bool inplace) {
            self_t ans = inplace ? self : std::make_shared<track_t>(std::move(self->deepcopy()));
            ops::adjust_time_inplace(*ans, original_times, new_times);
            return ans;
        }, nb::arg("original_times"), nb::arg("new_times"), nb::arg("inplace") = false)
        .def("shift_time", [](self_t& self, const unit offset, const bool inplace) {
            self_t ans = inplace ? self : std::make_shared<track_t>(std::move(self->deepcopy()));
            ans->shift_time_inplace(offset);
            return ans;
        }, nb::arg("offset"), nb::arg("inplace") = false)
        .def("shift_pitch", [](self_t& self, const i8 offset, const bool inplace) {
            self_t ans = inplace ? self : std::make_shared<track_t>(std::move(self->deepcopy()));
            ans->shift_pitch_inplace(offset);
            return ans;
        }, nb::arg("offset"), nb::arg("inplace") = false)
        .def("shift_velocity", [](self_t& self, const i8 offset, const bool inplace) {
            self_t ans = inplace ? self : std::make_shared<track_t>(std::move(self->deepcopy()));
            ans->shift_velocity_inplace(offset);
            return ans;
        }, nb::arg("offset"), nb::arg("inplace") = false)
    ;

    if constexpr (std::is_same_v<T, Tick>) {
        track.def("pianoroll", [](
            const self_t&           self,
            const vec<std::string>& modes,
            const std::pair<i8, i8> pitch_range,
            const bool              encode_velocity) {
            vec<PianorollMode> mode_enums(modes.size());
            for (int i = 0; i < modes.size(); ++i) {
                mode_enums[i] = str_to_pianoroll_mode(modes[i]);
            }
            auto pianoroll = TrackPianoroll::from_track(*self, mode_enums, pitch_range, encode_velocity);
            return nb::ndarray<nb::numpy, pianoroll_t>{
                const_cast<u8*>(pianoroll.release()), {
                    std::get<0>(pianoroll.dims()),
                    std::get<1>(pianoroll.dims()),
                    std::get<2>(pianoroll.dims()),
                }
            };
        });
    }

    auto track_vec = bind_shared_vector_copy<vec<self_t>>(m, (name + "List").c_str())
        .def_prop_ro("ttype", [](const vec_t&) { return T(); })
        .def("filter", [](const vec_t& self, nb::object & func, const bool inplace) {
            auto ans = inplace ? self : std::make_shared<vec<self_t>>(self->begin(), self->end());
            auto it = std::remove_if(ans->begin(), ans->end(), [&](const self_t& t) {
                return !nb::cast<bool>(func(t));
            });
            ans->erase(it, ans->end());
            return ans;
        })
        .def("sort", [](vec_t& self, const bool reverse, nb::object& key, const bool inplace) {
            auto ans = inplace ? self : std::make_shared<vec<self_t>>(self->begin(), self->end());
            if(key.is_none()) {
                auto cmp = [](const self_t& a, const self_t& b) { return a->default_key() < b->default_key(); };
                if (reverse) std::sort(ans->rbegin(), ans->rend(), cmp);
                else std::sort(ans->begin(), ans->end(), cmp);
            } else {
                auto cmp = [&](const self_t& a, const self_t& b) { return key(a) < key(b); };
                if (reverse) std::sort(ans->rbegin(), ans->rend(), cmp);
                else std::sort(ans->begin(), ans->end(), cmp);
            }   return ans;
        })
        .def("adjust_time", [](vec_t& self, const vec<unit>& original_times, const vec<unit>& new_times, const bool inplace) {
            auto ans = inplace ? self : deepcopy(self);
            for (auto& t : *ans) ops::adjust_time_inplace(*t, original_times, new_times);
            return ans;
        }, nb::arg("original_times"), nb::arg("new_times"), nb::arg("inplace") = false)
        .def("copy",          [](const vec_t& self) { return std::make_shared<vec<self_t>>(self->begin(), self->end()); })
        .def("__copy__",      [](const vec_t& self) { return std::make_shared<vec<self_t>>(self->begin(), self->end()); })
        .def("deepcopy",      &deepcopy<Track<T>>)
        .def("__deepcopy__",  &deepcopy<Track<T>>)
        .def("__getstate__",  [](const vec_t& self) {
            vec<TrackNative<T>> native;
            native.reserve(self->size());
            for (const auto& item : *self) { native.emplace_back(std::move(to_native(*item))); }
            const vec<unsigned char> data = dumps<DataFormat::ZPP>(native);
            return nb::bytes(reinterpret_cast<const char*>(data.data()), data.size());
        })
        .def("__setstate__",  [](vec_t& self, const nb::bytes& bytes) {
            const auto      data = std::string_view(bytes.c_str(), bytes.size());
            const std::span span(reinterpret_cast<const unsigned char*>(data.data()), data.size());
            auto            native = symusic::parse<symusic::DataFormat::ZPP, vec<TrackNative<T>>>(span);
            new (&self) shared<vec<self_t>>(std::make_shared<vec<self_t>>());
            for (auto& item : native) {
                self->push_back(std::make_shared<track_t>(std::move(to_shared(std::move(item)))));
            }
        })
    ;
    // clang-format on
    return std::make_tuple(track, track_vec);
}

template<TType T>
typename T::unit cast_time(const nb::object& t) {
    typedef typename T::unit unit;
    if (t.is_none()) return static_cast<unit>(0);
    return nb::cast<unit>(t);
}

template<TType To, TType From>
nb::object convert_score_inner(const shared<Score<From>>& self, const nb::object& min_dur) {
    return nb::cast(
        std::make_shared<Score<To>>(std::move(convert<To>(*self, cast_time<To>(min_dur)))),
        nb::rv_policy::copy
    );
}

template<typename T>
nb::object convert_score(
    const shared<Score<T>>& self, const nb::object& ttype, const nb::object& min_dur
) {
    if (ttype.is_none()) throw std::invalid_argument("ttype must be specified");
    if (nb::isinstance<Tick>(ttype)) return convert_score_inner<Tick>(self, min_dur);
    if (nb::isinstance<Quarter>(ttype)) return convert_score_inner<Quarter>(self, min_dur);
    if (nb::isinstance<Second>(ttype)) return convert_score_inner<Second>(self, min_dur);
    if (nb::isinstance<nb::str>(ttype)) {
        // convert ttype to lower case
        const auto ttype_str = nb::cast<std::string>(ttype.attr("lower")());
        if (ttype_str == "tick") return convert_score_inner<Tick>(self, min_dur);
        if (ttype_str == "quarter") return convert_score_inner<Quarter>(self, min_dur);
        if (ttype_str == "second") return convert_score_inner<Second>(self, min_dur);
    }
    throw std::invalid_argument("ttype must be Tick, Quarter, Second or string");
}

template<TType T, typename PATH>
shared<Score<T>> midi2score(PATH path) {
    auto     data = read_file(path);
    Score<T> s    = Score<T>:: parse<DataFormat::MIDI>(data);
    return std::make_shared<Score<T>>(std::move(s));
}

template<TType T, typename PATH>
void dump_midi(const shared<Score<T>>& self, PATH path) {
    const auto data = self->template dumps<DataFormat::MIDI>();
    write_file(path, data);
}

template<TType T>
void dump_abc_str(const shared<Score<T>>& self, const std::string& path, const bool warn) {
    // path to executable
    // const auto midi2abc = py::cast<std::string>(m.attr("_MIDI2ABC"));
    const auto midi2abc = std::string(getenv("SYMUSIC_MIDI2ABC"));
    if (midi2abc.empty()) { throw std::runtime_error("midi2abc not found"); }
    // dump a tmp midi file, using random int as name
    // const auto midi_path = dir / ("tmp_write_" + std::to_string(random_int) + ".mid");
    const std::string midi_path = std::tmpnam(nullptr);
    dump_midi(self, midi_path);
    // call midi2abc
    auto       cmd = fmt::format(R"({} "{}" -o "{}")", midi2abc, midi_path, path);
    const auto ret = std::system(cmd.c_str());
    if (std::filesystem::exists(path)) {
        std::filesystem::remove(midi_path);
    } else {
        throw std::runtime_error(fmt::format("midi2abc failed({}): {}", ret, cmd));
    }
}

template<TType T>
void dump_abc_path(
    const shared<Score<T>>& self, const std::filesystem::path& path, const bool warn
) {
    dump_abc_str<T>(self, path.string(), warn);
}

template<TType T>
std::string dumps_abc(const shared<Score<T>>& self, const bool warn) {
    const std::string abc_path = std::tmpnam(nullptr);
    dump_abc_str(self, abc_path, warn);
    auto data = read_file(abc_path);
    std::filesystem::remove(abc_path);
    // move the data to a string, not copy
    return {data.begin(), data.end()};
}

inline std::string get_format(const std::string& path) {
    const auto ext = std::filesystem::path(path).extension().string();
    if (ext == ".mid" || ext == ".midi" || ext == ".MID" || ext == ".MIDI") {
        return "midi";
    } else if (ext == ".abc") {
        return "abc";
    } else {
        throw std::invalid_argument("Unknown file format");
    }
}
template<TType T>
shared<Score<T>> from_abc_file(const std::string& path) {
    const auto abc2midi = std::string(getenv("SYMUSIC_ABC2MIDI"));
    if (abc2midi.empty()) { throw std::runtime_error("abc2midi not found"); }
    // convert the abc file to midi file
    const std::string midi_path = std::tmpnam(nullptr);
    const auto        cmd = fmt::format(R"({} "{}" -o "{}" -silent)", abc2midi, path, midi_path);
    const auto        ret = std::system(cmd.c_str());
    if (ret != 0) { throw std::runtime_error(fmt::format("abc2midi failed({}): {}", ret, cmd)); }
    // read the midi file
    const auto data = read_file(midi_path);
    // remove the tmp midi file
    std::filesystem::remove(midi_path);
    // parse the midi file
    auto s = Score<T>::template parse<DataFormat::MIDI>(data);
    return std::make_shared<Score<T>>(std::move(s));
}

template<TType T>
shared<Score<T>> from_abc(const std::string& abc) {
    // dump the abc string to a tmp file
    const std::string abc_path = std::tmpnam(nullptr);
    write_file(abc_path, std::span(reinterpret_cast<const u8*>(abc.data()), abc.size()));
    // convert the abc file to midi file
    return from_abc_file<T>(abc_path);
}

template<TType T>
shared<Score<T>> from_file(const std::string& path, const std::optional<std::string>& format) {
    std::string format_ = format.has_value() ? *format : get_format(path);
    // convert format_ to lower case
    std::transform(format_.begin(), format_.end(), format_.begin(), ::tolower);
    if (format_ == "midi" || format_ == "mid") {
        return midi2score<T, std::string>(path);
    } else if (format_ == "abc") {
        return from_abc_file<T>(path);
    } else {
        throw std::invalid_argument("Unknown file format");
    }
}

template<TType T>
auto bind_score(nb::module_& m, const std::string& name_) {
    const auto name = "Score" + name_;
    using unit      = typename T::unit;
    using self_t    = shared<Score<T>>;

    auto copy_func     = [](const self_t& self) { return std::make_shared<Score<T>>(*self); };
    auto deepcopy_func = [](const self_t& self) {
        return std::make_shared<Score<T>>(std::move(self->deepcopy()));
    };

    // clang-format off
    auto score = nb::class_<self_t>(m, name.c_str())
        .def("__init__", &pyinit<Score<T>, i32>, nb::arg("tpq"))
        .def("__init__", [](self_t* self, const self_t& other) {
            new (self) self_t(std::move(std::make_shared<Score<T>>(std::move(other->deepcopy()))));
        }, "Copy constructor", nb::arg("other"))
        .def("copy", copy_func, "Shallow copy", nb::rv_policy::copy)
        .def("__copy__", copy_func, "Shallow copy", nb::rv_policy::copy)
        .def("deepcopy", deepcopy_func, "Deep copy", nb::rv_policy::copy)
        .def("__deepcopy__", deepcopy_func, "Deep copy", nb::rv_policy::copy)
        .def("__repr__", [](const self_t& self) { return self->to_string(); })
        .def(nb::self == nb::self)   // NOLINT
        .def(nb::self != nb::self)   // NOLINT
        // parse and dump
        .def("__getstate__", [](const self_t& self) {
            const auto native = to_native(*self);
            const vec<unsigned char> data = dumps<DataFormat::ZPP>(native);
            return nb::bytes(reinterpret_cast<const char*>(data.data()), data.size());
        })
        .def("__setstate__", [](self_t& self, const nb::bytes& bytes) {
            const auto      data = std::string_view(bytes.c_str(), bytes.size());
            const std::span span(reinterpret_cast<const unsigned char*>(data.data()), data.size());
            auto ans = symusic::parse<symusic::DataFormat::ZPP, ScoreNative<T>>(span);
            new (&self) self_t(std::move(std::make_shared<Score<T>>(std::move(to_shared(std::move(ans))))));
        })
        .def("__init__", [](self_t* self, const std::string& path) {
            new (self) self_t(std::move(midi2score<T>(path)));
        }, "Load from midi file", nb::arg("path"))
        .def("__init__", [](self_t* self, const std::filesystem::path& path) {
            new (self) self_t(std::move(midi2score<T>(path)));
        }, "Load from midi file", nb::arg("path"))
        .def_static("from_file", &from_file<T>, nb::arg("path"), nb::arg("format") = nb::none())
        .def_static("from_file", [](const std::filesystem::path& path, const std::optional<std::string>& format) {
            return from_file<T>(path.string(), format);
        }, nb::arg("path"), nb::arg("format") = nb::none())
        .def_static("from_midi", [](const nb::bytes& data) {
            const auto str  = std::string_view(data.c_str(), data.size());
            const auto span = std::span(reinterpret_cast<const u8*>(str.data()), str.size());
            return std::make_shared<Score<T>>(std::move(parse<DataFormat::MIDI, Score<T>>(span)));
        }, nb::arg("data"), "Load from midi in memory(bytes)")
        .def_static("from_abc", &from_abc<T>, nb::arg("abc"), "Load from abc string")
        .def("dump_midi", &dump_midi<T, std::string>, nb::arg("path"), "Dump to midi file")
        .def("dump_midi", &dump_midi<T, std::filesystem::path>, nb::arg("path"), "Dump to midi file")
        .def("dumps_midi", [](const self_t& self) {
            auto data = self->template dumps<DataFormat::MIDI>();
            return nb::bytes(reinterpret_cast<const char*>(data.data()), data.size());
        }, "Dump to midi in memory(bytes)")
        .def("dump_abc", &dump_abc_str<T>, nb::arg("path"), nb::arg("warn") = false, "Dump to abc file")
        .def("dump_abc", &dump_abc_path<T>, nb::arg("path"), nb::arg("warn") = false, "Dump to abc file")
        .def("dumps_abc", &dumps_abc<T>, nb::arg("warn") = false, "Dump to abc string")
        // attributes
        .def_prop_rw(RW_COPY(i32, "ticks_per_quarter", ticks_per_quarter))
        .def_prop_rw(RW_COPY(i32, "tpq", ticks_per_quarter))
        .def_prop_rw(RW_COPY(shared<vec<shared<Track<T>>>>, "tracks", tracks))
        .def_prop_rw(RW_COPY(shared<pyvec<TimeSignature<T>>>, "time_signatures", time_signatures))
        .def_prop_rw(RW_COPY(shared<pyvec<KeySignature<T>>>, "key_signatures", key_signatures))
        .def_prop_rw(RW_COPY(shared<pyvec<Tempo<T>>>, "tempos", tempos))
        .def_prop_rw(RW_COPY(shared<pyvec<TextMeta<T>>>, "lyrics", lyrics))
        .def_prop_rw(RW_COPY(shared<pyvec<TextMeta<T>>>, "markers", markers))
        .def_prop_ro("ttype", [](const self_t&) { return T(); })
        // member functions
        .def("to", &convert_score<T>, nb::arg("ttype"), nb::arg("min_dur") = nb::none(), "Convert to another time unit")
        .def("resample", [](const self_t& self, const i32 tpq, const std::optional<unit> min_dur) {
            const unit min_dur_ = min_dur.has_value() ? *min_dur : 0;
            return std::make_shared<Score<Tick>>(std::move(resample(*self, tpq, min_dur_)));
        }, nb::arg("tpq"), nb::arg("min_dur") = nb::none(), nb::rv_policy::copy, "Resample to another ticks per quarter")
        .def("sort", [](self_t& self, const bool reverse, const bool inplace) {
            if (inplace) {
                self->sort_inplace(reverse);
                return self;
            }   return std::make_shared<Score<T>>(std::move(self->sort(reverse)));
        }, nb::arg("reverse") = false, nb::arg("inplace") = false)
        .def("shift_time", [](self_t& self, const unit offset, const bool inplace) {
            if (inplace) {
                self->shift_time_inplace(offset);
                return self;
            }   return std::make_shared<Score<T>>(std::move(self->shift_time(offset)));
        }, nb::arg("offset"), nb::arg("inplace") = false)
        .def("shift_pitch", [](self_t& self, const i8 offset, const bool inplace) {
            if (inplace) {
                self->shift_pitch_inplace(offset);
                return self;
            }   return std::make_shared<Score<T>>(std::move(self->shift_pitch(offset)));
        }, nb::arg("offset"), nb::arg("inplace") = false)
        .def("shift_velocity", [](self_t& self, const i8 offset, const bool inplace) {
            if (inplace) {
                self->shift_velocity_inplace(offset);
                return self;
            }   return std::make_shared<Score<T>>(std::move(self->shift_velocity(offset)));
        }, nb::arg("offset"), nb::arg("inplace") = false)
        .def("start", [](const self_t& self) { return self->start(); })
        .def("end", [](const self_t& self) { return self->end(); })
        .def("note_num", [](const self_t& self) { return self->note_num(); })
        .def("empty", [](const self_t& self) { return self->empty(); })
        .def("adjust_time", [](self_t& self, const vec<unit>& original_times, const vec<unit>& new_times, const bool inplace) {
            if (inplace) {
                ops::adjust_time_inplace(*self, original_times, new_times);
                return self;
            }   return std::make_shared<Score<T>>(std::move(ops::adjust_time(*self, original_times, new_times)));
        }, nb::arg("original_times"), nb::arg("new_times"), nb::arg("inplace") = false)
    ;
    // clang-format on
    if constexpr (std::is_same_v<T, Tick>) {
        score.def(
            "pianoroll",
            [](const shared<Score<Tick>>& self,
               const vec<std::string>&    modes,
               const std::pair<i8, i8>    pitch_range,
               const bool                 encode_velocity) {
                vec<PianorollMode> mode_enums(modes.size());
                for (int i = 0; i < modes.size(); ++i) {
                    mode_enums[i] = str_to_pianoroll_mode(modes[i]);
                }
                auto pianoroll
                    = ScorePianoroll::from_score(*self, mode_enums, pitch_range, encode_velocity);
                return nb::ndarray<nb::numpy, pianoroll_t>{
                    const_cast<u8*>(pianoroll.release()),
                    {std::get<0>(pianoroll.dims()),
                     std::get<1>(pianoroll.dims()),
                     std::get<2>(pianoroll.dims()),
                     std::get<3>(pianoroll.dims())}
                };
            },
            nb::arg("modes"),
            nb::arg("pitch_range")     = std::pair<i8, i8>(0, 127),
            nb::arg("encode_velocity") = false
        );
    }
    return score;
}

#define BIND_EVENT(__COUNT, BIND_FUNC) \
    BIND_FUNC<Tick>(m, "Tick");        \
    BIND_FUNC<Quarter>(m, "Quarter");  \
    BIND_FUNC<Second>(m, "Second");

using namespace pyutils;
NB_MODULE(core, m) {
    m.attr("_MIDI2ABC") = std::string("");

    // clang-format off
    auto tick = nb::class_<Tick>(m, "Tick")
        .def(nb::init<>())
        .def("__repr__", [](const Tick&) { return "symsuic.core.Tick"; })
        .def("is_time_unit", [](const Tick&) { return true; });

    auto quarter = nb::class_<Quarter>(m, "Quarter")
       .def(nb::init<>())
       .def("__repr__", [](const Quarter&) { return "symsuic.core.Quarter"; })
       .def("is_time_unit", [](const Quarter&) { return true; });

    auto second = nb::class_<Second>(m, "Second")
      .def(nb::init<>())
      .def("__repr__", [](const Second&) { return "symsuic.core.Second"; })
      .def("is_time_unit", [](const Second&) { return true; });
    // clang-format on

    // // def __eq__ for all time units
    tick.def("__eq__", [](const Tick&, const nb::object& other) {
        if (nb::isinstance<Tick>(other)) return true;
        return false;
    });

    quarter.def("__eq__", [](const Quarter&, const nb::object& other) {
        if (nb::isinstance<Quarter>(other)) return true;
        return false;
    });

    second.def("__eq__", [](const Second&, const nb::object& other) {
        if (nb::isinstance<Second>(other)) return true;
        return false;
    });

    // clang-format off
    REPEAT_ON(
        BIND_EVENT,
        bind_note, bind_keysig, bind_timesig, bind_tempo,
        bind_controlchange, bind_pedal, bind_pitchbend, bind_textmeta,
        bind_track, bind_score
    )
    #undef BIND_EVENT
    // clang-format on
    bind_synthesizer(m);
}
}   // namespace symusic
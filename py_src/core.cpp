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
// template<typename T>
// Track<T>& py_sort_track_inplace(Track<T>& self, py::object& key, const bool reverse) {
//     py_sort_inplace(*self.notes, key, reverse);
//     py_sort_inplace(*self.controls, key, reverse);
//     py_sort_inplace(*self.pitch_bends, key, reverse);
//     py_sort_inplace(*self.pedals, key, reverse);
//     return self;
// };
//
// template<typename T>
// py::object py_sort_track(
//     shared<Track<T>>& self, py::object& key, const bool reverse, const bool inplace = false
// ) {
//     if (inplace) {
//         py_sort_track_inplace(*self, key, reverse);
//         return py::cast(self, py::rv_policy::reference);
//     } else {
//         auto copy = self;
//         return py::cast(py_sort_track_inplace(copy, key, reverse), py::rv_policy::move);
//     }
// };
//
//
// // shift time track
// template<typename T>
// py::object py_shift_time_track(
//     Track<T>& self, const typename T::unit offset, const bool inplace = true
// ) {
//     if (inplace) {
//         return py::cast(self.shift_time_inplace(offset), py::rv_policy::reference);
//     } else {
//         return py::cast(self.shift_time(offset), py::rv_policy::move);
//     }
// };
//
// // shift pitch track
// template<typename T>
// py::object py_shift_pitch_track(Track<T>& self, const int8_t offset, const bool inplace = true) {
//     if (inplace) {
//         return py::cast(self.shift_pitch_inplace(offset), py::rv_policy::reference);
//     } else {
//         return py::cast(self.shift_pitch(offset), py::rv_policy::move);
//     }
// };
//
// // shift velocity track
// template<typename T>
// py::object py_shift_velocity_track(Track<T>& self, const int8_t offset, const bool inplace =
// true) {
//     if (inplace) {
//         return py::cast(self.shift_velocity_inplace(offset), py::rv_policy::reference);
//     } else {
//         return py::cast(self.shift_velocity(offset), py::rv_policy::move);
//     }
// };
//
// // bind Track<T>
// template<typename T>
// py::class_<Track<T>> bind_track_class(py::module_& m, const std::string& name_) {
//     const auto               name = "Track" + name_;
//     typedef typename T::unit unit;
//     auto                     event =
//         py::class_<Track<T>>(m, name.c_str())
//             .def(py::init<>())
//             .def(py::init<const Track<T>&>(), "Copy constructor", py::arg("other"))
//             .def("copy", &Track<T>::copy, "Deep copy", py::rv_policy::move)
//             .def("__copy__", &Track<T>::copy, "Deep copy")
//             .def("__deepcopy__", &Track<T>::copy, "Deep copy")
//             .def("__repr__", &Track<T>::to_string)
//             .def_rw("notes", &Track<T>::notes)
//             .def_rw("controls", &Track<T>::controls)
//             .def_rw("pitch_bends", &Track<T>::pitch_bends)
//             .def_rw("pedals", &Track<T>::pedals)
//             .def_rw("is_drum", &Track<T>::is_drum)
//             .def_rw("program", &Track<T>::program)
//             .def_rw("name", &Track<T>::name)
//             .def_prop_ro("ttype", [](const Track<T>&) { return T(); })
//             .def("__getstate__", &py_to_bytes<Track<T>>)
//             .def("__setstate__", &py_from_bytes<Track<T>>)
//             .def(py::self == py::self)   // NOLINT
//             .def(py::self != py::self);   // NOLINT
//             // .def(
//             //     "sort",
//             //     &py_sort_track<T>,
//             //     py::arg("key")     = py::none(),
//             //     py::arg("reverse") = false,
//             //     py::arg("inplace") = true
//             // )
//             // .def("end", &Track<T>::end)
//             // .def("start", &Track<T>::start)
//             // .def("note_num", &Track<T>::note_num)
//             // .def("empty", &Track<T>::empty)
//             // .def(
//             //     "clip",
//             //     &Track<T>::clip,
//             //     "Clip notes and controls to a given time range",
//             //     py::arg("start"),
//             //     py::arg("end"),
//             //     py::arg("clip_end") = false
//             // )
//             // .def(
//             //     "shift_time", &py_shift_time_track<T>, py::arg("offset"), py::arg("inplace") =
//             false
//             // )
//             // .def(
//             //     "shift_pitch",
//             //     &py_shift_pitch_track<T>,
//             //     py::arg("offset"),
//             //     py::arg("inplace") = false
//             // )
//             // .def(
//             //     "shift_velocity",
//             //     &py_shift_velocity_track<T>,
//             //     py::arg("offset"),
//             //     py::arg("inplace") = false
//             // )
//             // .def(
//             //     "adjust_time",
//             //     py::overload_cast<
//             //         const Track<T>&,
//             //         const vec<unit>&,
//             //         const vec<unit>&,
//             //         bool>(&ops::adjust_time<true, T>),
//             //     py::arg("original_times"),
//             //     py::arg("new_times"),
//             //     py::arg("is_sorted") = false
//             // )
//             // .def(
//             //     "pianoroll",
//             //     [](const Track<Tick>&                self,
//             //        const std::vector<std::string>&   modes,
//             //        const std::pair<uint8_t, uint8_t> pitchRange,
//             //        bool                              encodeVelocity) {
//             //         std::vector<PianorollMode> modesEnum(modes.size());
//             //         for (int i = 0; i < modes.size(); ++i) {
//             //             modesEnum[i] = symusic::str_to_pianoroll_mode(modes[i]);
//             //         }
//             //         TrackPianoroll pianoroll =
//             //             TrackPianoroll::from_track(self, modesEnum, pitchRange,
//             encodeVelocity);
//             //
//             //         return py::ndarray<py::numpy, pianoroll_t>{
//             //             const_cast<uint8_t*>(pianoroll.release()),
//             //             {
//             //                 std::get<0>(pianoroll.dims()),
//             //                 std::get<1>(pianoroll.dims()),
//             //                 std::get<2>(pianoroll.dims()),
//             //             }
//             //         };
//             //     },
//             //     py::arg("modes"),
//             //     py::arg("pitch_range")     = std::pair<uint8_t, uint8_t>(0, 128),
//             //     py::arg("encode_velocity") = false
//             // );
//
//     py::bind_vector<vec<shared<Track<T>>>>(m, std::string(name + "List").c_str())
//         // .def(
//         //     "sort",
//         //     &py_sort<Track<T>>,
//         //     py::arg("key")     = py::none(),
//         //     py::arg("reverse") = false,
//         //     py::arg("inplace") = true
//         // )
//         // .def(
//         //     "__repr__",
//         //     [](const vec<Track<T>>& self) {
//         //         vec<std::string> strs;
//         //         strs.reserve(self.size());
//         //         for (const auto& track : self) { strs.emplace_back(track.to_string()); }
//         //         return fmt::format("[{}]", fmt::join(strs, ", "));
//         //     }
//         // )
//         // .def("__getstate__", &py_to_bytes<vec<Track<T>>>)
//         // .def("__setstate__", &py_from_bytes<vec<Track<T>>>)
//         // .def("copy", &copy_vec<Track<T>>)
//         // .def("__copy__", &copy_vec<Track<T>>)
//         // .def("__deepcopy__", &copy_vec<Track<T>>)
//         .def_prop_ro("ttype", [](const vec<shared<Track<T>>>&) { return T(); });
//         // .def("filter", &py_filter<Track<T>>, py::arg("func"), py::arg("inplace") = false);
//
//     py::implicitly_convertible<py::list, vec<shared<Track<T>>>>();
//
//     return event;
// }
//
// // py sort score
// template<typename T>
// Score<T>& py_sort_score_inplace(Score<T>& self, py::object& key, bool reverse) {
//     py_sort_inplace(self.time_signatures, key, reverse);
//     py_sort_inplace(self.key_signatures, key, reverse);
//     py_sort_inplace(self.tempos, key, reverse);
//     py_sort_inplace(self.lyrics, key, reverse);
//     py_sort_inplace(self.markers, key, reverse);
//     for (auto& track : self.tracks) py_sort_track_inplace(track, key, reverse);
//     return self;
// }
//
// template<typename T>
// py::object py_sort_score(Score<T>& self, py::object& key, bool reverse, bool inplace = false) {
//     if (inplace) {
//         py_sort_score_inplace(self, key, reverse);
//         return py::cast(self, py::rv_policy::reference);
//     } else {
//         auto copy = self;
//         py_sort_score_inplace(copy, key, reverse);
//         return py::cast(copy, py::rv_policy::move);
//     }
// }
//
// // py shift time score
// template<typename T>
// py::object py_shift_time_score(
//     Score<T>& self, const typename T::unit offset, const bool inplace = true
// ) {
//     if (inplace) {
//         return py::cast(self.shift_time_inplace(offset), py::rv_policy::reference);
//     } else {
//         return py::cast(self.shift_time(offset), py::rv_policy::move);
//     }
// }
//
// // py shift pitch score
// template<typename T>
// py::object py_shift_pitch_score(Score<T>& self, const int8_t offset, const bool inplace = true) {
//     if (inplace) {
//         return py::cast(self.shift_pitch_inplace(offset), py::rv_policy::reference);
//     } else {
//         return py::cast(self.shift_pitch(offset), py::rv_policy::move);
//     }
// }
//
// // py shift velocity score
// template<typename T>
// py::object py_shift_velocity_score(Score<T>& self, const int8_t offset, const bool inplace =
// true) {
//     if (inplace) {
//         return py::cast(self.shift_velocity_inplace(offset), py::rv_policy::reference);
//     } else {
//         return py::cast(self.shift_velocity(offset), py::rv_policy::move);
//     }
// }
//
// template<TType T, typename PATH>
// Score<T> midi2score(PATH path) {
//     auto     data = read_file(path);
//     Score<T> s    = Score<T>::template parse<DataFormat::MIDI>(data);
//     return s;
// }
//
// template<TType T, typename PATH>
// void dump_midi(const Score<T>& self, PATH path) {
//     const auto data = self.template dumps<DataFormat::MIDI>();
//     write_file(path, data);
// }
//
// template<TType T>
// void dump_abc_str(const Score<T>& self, const std::string& path, const bool warn) {
//     // path to executable
//     // const auto midi2abc = py::cast<std::string>(m.attr("_MIDI2ABC"));
//     const auto midi2abc = std::string(getenv("SYMUSIC_MIDI2ABC"));
//     if (midi2abc.empty()) { throw std::runtime_error("midi2abc not found"); }
//     // dump a tmp midi file, using random int as name
//     // const auto midi_path = dir / ("tmp_write_" + std::to_string(random_int) + ".mid");
//     const std::string midi_path = std::tmpnam(nullptr);
//     dump_midi(self, midi_path);
//     // call midi2abc
//     auto       cmd = fmt::format(R"({} "{}" -o "{}")", midi2abc, midi_path, path);
//     const auto ret = std::system(cmd.c_str());
//     if (std::filesystem::exists(path)) {
//         std::filesystem::remove(midi_path);
//     } else {
//         throw std::runtime_error(fmt::format("midi2abc failed({}): {}", ret, cmd));
//     }
// }
//
// template<TType T>
// void dump_abc_path(const Score<T>& self, const std::filesystem::path& path, const bool warn) {
//     dump_abc_str<T>(self, path.string(), warn);
// }
//
// template<TType T>
// std::string dumps_abc(const Score<T>& self, const bool warn) {
//     const std::string abc_path = std::tmpnam(nullptr);
//     dump_abc_str(self, abc_path, warn);
//     auto data = read_file(abc_path);
//     std::filesystem::remove(abc_path);
//     // move the data to a string, not copy
//     return {data.begin(), data.end()};
// }
//
// inline std::string get_format(const std::string& path) {
//     const auto ext = std::filesystem::path(path).extension().string();
//     if (ext == ".mid" || ext == ".midi" || ext == ".MID" || ext == ".MIDI") {
//         return "midi";
//     } else if (ext == ".abc") {
//         return "abc";
//     } else {
//         throw std::invalid_argument("Unknown file format");
//     }
// }
//
// template<TType T>
// Score<T> from_abc_file(const std::string& path) {
//     const auto abc2midi = std::string(getenv("SYMUSIC_ABC2MIDI"));
//     if (abc2midi.empty()) { throw std::runtime_error("abc2midi not found"); }
//     // convert the abc file to midi file
//     const std::string midi_path = std::tmpnam(nullptr);
//     const auto        cmd = fmt::format(R"({} "{}" -o "{}" -silent)", abc2midi, path, midi_path);
//     const auto        ret = std::system(cmd.c_str());
//     if (ret != 0) { throw std::runtime_error(fmt::format("abc2midi failed({}): {}", ret, cmd)); }
//     // read the midi file
//     const auto data = read_file(midi_path);
//     // remove the tmp midi file
//     std::filesystem::remove(midi_path);
//     // parse the midi file
//     return Score<T>::template parse<DataFormat::MIDI>(data);
// }
//
// template<TType T>
// Score<T> from_abc(const std::string& abc) {
//     // dump the abc string to a tmp file
//     const std::string abc_path = std::tmpnam(nullptr);
//     write_file(abc_path, std::span(reinterpret_cast<const u8*>(abc.data()), abc.size()));
//     // convert the abc file to midi file
//     return from_abc_file<T>(abc_path);
// }
//
// template<TType T>
// Score<T> from_file(const std::string& path, const std::optional<std::string>& format) {
//     std::string format_ = format.has_value() ? *format : get_format(path);
//     // convert format_ to lower case
//     std::transform(format_.begin(), format_.end(), format_.begin(), ::tolower);
//     if (format_ == "midi" || format_ == "mid") {
//         return midi2score<T, std::string>(path);
//     } else if (format_ == "abc") {
//         return from_abc_file<T>(path);
//     } else {
//         throw std::invalid_argument("Unknown file format");
//     }
// }
//
// // bind symusic::Score<T>
// template<typename T>
// py::class_<Score<T>> bind_score_class(py::module_& m, const std::string& name_) {
//     const auto               name = "Score" + name_;
//     typedef typename T::unit unit;
//     return py::class_<Score<T>>(m, name.c_str())
//         .def(py::init<const i32>(), py::arg("tpq"))
//         .def(
//             "__init__",
//             [](Score<T>* self, const Score<T>& other) { new (self) Score<T>(other); },
//             "Copy constructor",
//             py::arg("other")
//         )
//         .def(
//             "__init__",
//             [](Score<T>* self, const std::string& path) {
//                 new (self) Score<T>(std::move(midi2score<T, std::string>(path)));
//             },
//             "Load from midi file",
//             py::arg("path")
//         )
//         .def(
//             "__init__",
//             [](Score<T>* self, const std::filesystem::path& path) {
//                 new (self) Score<T>(std::move(midi2score<T, std::filesystem::path>(path)));
//             },
//             "Load from midi file",
//             py::arg("path")
//         )
//         .def("copy", &Score<T>::copy, "Deep copy", py::rv_policy::move)
//         .def("__copy__", &Score<T>::copy, "Deep copy")
//         .def("__deepcopy__", &Score<T>::copy, "Deep copy")
//         .def("__repr__", &Score<T>::to_string)
//         .def_static(
//             "from_file",
//             &from_file<T>,
//             "Load from midi file",
//             py::arg("path"),
//             py::arg("fmt") = py::none()
//         )
//         .def_static(
//             "from_file",
//             [](const std::filesystem::path& path, const std::optional<std::string>& format) {
//                 return from_file<T>(path.string(), format);
//             },
//             "Load from midi file",
//             py::arg("path"),
//             py::arg("fmt") = py::none()
//         )
//         .def_static(
//             "from_midi",
//             [](const py::bytes& data) {
//                 const auto str = std::string_view(data.c_str(), data.size());
//                 return Score<T>::template parse<DataFormat::MIDI>(
//                     std::span(reinterpret_cast<const u8*>(str.data()), str.size())
//                 );
//             },
//             "Load from midi in memory(bytes)"
//         )
//         .def_static("from_abc", &from_abc<T>, "Load from abc string", py::arg("abc"))
//         // pybind11 will binding class method in an erratic way:
//         // https://github.com/pybind/pybind11/issues/1693
//         .def("dump_midi", &dump_midi<T, std::string>, "Dump to midi file", py::arg("path"))
//         .def(
//             "dump_midi", &dump_midi<T, std::filesystem::path>, "Dump to midi file",
//             py::arg("path")
//         )
//         .def(
//             "dumps_midi",
//             [](const Score<T>& self) {
//                 auto data = self.template dumps<DataFormat::MIDI>();
//                 return py::bytes(reinterpret_cast<const char*>(data.data()), data.size());
//             },
//             "Dump to midi in memory(bytes)"
//         )
//         .def(
//             "dump_abc",
//             &dump_abc_str<T>,
//             "Dump to abc file",
//             py::arg("path"),
//             py::arg("warn") = true
//         )
//         .def(
//             "dump_abc",
//             &dump_abc_path<T>,
//             "Dump to abc file",
//             py::arg("path"),
//             py::arg("warn") = true
//         )
//         .def("dumps_abc", &dumps_abc<T>, "Dump to abc string", py::arg("warn") = true)
//         .def_rw("ticks_per_quarter", &Score<T>::ticks_per_quarter)
//         .def_rw(
//             "tpq",
//             &Score<T>::ticks_per_quarter,
//             "Ticks per quarter note, the same as ticks_per_quarter"
//         )
//         .def_rw("tracks", &Score<T>::tracks)
//         .def_rw("time_signatures", &Score<T>::time_signatures)
//         .def_rw("key_signatures", &Score<T>::key_signatures)
//         .def_rw("tempos", &Score<T>::tempos)
//         .def_rw("lyrics", &Score<T>::lyrics)
//         .def_rw("markers", &Score<T>::markers)
//         .def_prop_ro("ttype", [](const Score<T>&) { return T(); })
//         // .def(py::pickle( &py_to_bytes<Score<T>>, &py_from_bytes<Score<T>>))
//         .def("__getstate__", &py_to_bytes<Score<T>>)
//         .def("__setstate__", &py_from_bytes<Score<T>>)
//         .def(py::self == py::self)   // NOLINT
//         .def(py::self != py::self);   // NOLINT
//         // .def(
//         //     "sort",
//         //     &py_sort_score<T>,
//         //     py::arg("key")     = py::none(),
//         //     py::arg("reverse") = false,
//         //     py::arg("inplace") = true
//         // )
//         // .def(
//         //     "clip",
//         //     &Score<T>::clip,
//         //     "Clip events a given time range",
//         //     py::arg("start"),
//         //     py::arg("end"),
//         //     py::arg("clip_end") = false
//         // )
//         // .def("shift_time", &py_shift_time_score<T>, py::arg("offset"), py::arg("inplace") =
//         false)
//         // .def("shift_pitch", &py_shift_pitch_score<T>, py::arg("offset"), py::arg("inplace") =
//         false)
//         // .def(
//         //     "shift_velocity",
//         //     &py_shift_velocity_score<T>,
//         //     py::arg("offset"),
//         //     py::arg("inplace") = false
//         // )
//         // .def("start", &Score<T>::start)
//         // .def("end", &Score<T>::end)
//         // .def("note_num", &Score<T>::note_num)
//         // .def("empty", &Score<T>::empty)
//         // .def(
//         //     "adjust_time",
//         //     py::overload_cast<
//         //         const Score<T>&,
//         //         const vec<unit>&,
//         //         const vec<unit>&,
//         //         bool>(&ops::adjust_time<true, T>),
//         //     py::arg("original_times"),
//         //     py::arg("new_times"),
//         //     py::arg("is_sorted") = false
//         // )
//         // .def(
//         //     "pianoroll",
//         //     [](const Score<Tick>&                self,
//         //        const std::vector<std::string>&   modes,
//         //        const std::pair<uint8_t, uint8_t> pitchRange,
//         //        bool                              encodeVelocity) {
//         //         std::vector<PianorollMode> modesEnum(modes.size());
//         //         for (int i = 0; i < modes.size(); ++i) {
//         //             modesEnum[i] = str_to_pianoroll_mode(modes[i]);
//         //         }
//         //         ScorePianoroll pianoroll =
//         //             ScorePianoroll::from_score(self, modesEnum, pitchRange, encodeVelocity);
//         //
//         //         return py::ndarray<py::numpy, pianoroll_t>{
//         //             const_cast<uint8_t*>(pianoroll.release()),
//         //             {std::get<0>(pianoroll.dims()),
//         //              std::get<1>(pianoroll.dims()),
//         //              std::get<2>(pianoroll.dims()),
//         //              std::get<3>(pianoroll.dims())}
//         //         };
//         //     },
//         //     py::arg("modes"),
//         //     py::arg("pitch_range")     = std::pair<uint8_t, uint8_t>(0, 127),
//         //     py::arg("encode_velocity") = false
//         // );
// }
//
// template<TType T>
// typename T::unit cast_time(const py::object& t) {
//     typedef typename T::unit unit;
//     if (t.is_none()) return static_cast<unit>(0);
//     return py::cast<unit>(t);
// }
//
// template<typename T>
// py::object convert_score(const Score<T>& self, const py::object& ttype, const py::object&
// min_dur) {
//     if (ttype.is_none()) throw std::invalid_argument("ttype must be specified");
//     if (py::isinstance<Tick>(ttype))
//         return py::cast(convert<Tick>(self, cast_time<Tick>(min_dur)), py::rv_policy::move);
//     if (py::isinstance<Quarter>(ttype))
//         return py::cast(convert<Quarter>(self, cast_time<Quarter>(min_dur)),
//         py::rv_policy::move);
//     if (py::isinstance<Second>(ttype))
//         return py::cast(convert<Second>(self, cast_time<Second>(min_dur)), py::rv_policy::move);
//     if (py::isinstance<py::str>(ttype)) {
//         // convert ttype to lower case
//         const auto ttype_str = py::cast<std::string>(ttype.attr("lower")());
//         if (ttype_str == "tick")
//             return py::cast(convert<Tick>(self, cast_time<Tick>(min_dur)), py::rv_policy::move);
//         if (ttype_str == "quarter")
//             return py::cast(
//                 convert<Quarter>(self, cast_time<Quarter>(min_dur)), py::rv_policy::move
//             );
//         if (ttype_str == "second")
//             return py::cast(convert<Second>(self, cast_time<Second>(min_dur)),
//             py::rv_policy::move);
//     }
//     throw std::invalid_argument("ttype must be Tick, Quarter, Second or string");
// }
//
// py::module_& core_module(py::module_& m) {
//     const std::string tick = "Tick", quarter = "Quarter", second = "Second";
//
//     bind_note_class<Tick>(m, tick);
//     bind_note_class<Quarter>(m, quarter);
//     bind_note_class<Second>(m, second);
//
//
//     bind_control_change_class<Tick>(m, tick);
//     bind_control_change_class<Quarter>(m, quarter);
//     bind_control_change_class<Second>(m, second);
//
//     bind_pedal_class<Tick>(m, tick);
//     bind_pedal_class<Quarter>(m, quarter);
//     bind_pedal_class<Second>(m, second);
//
//     bind_tempo_class<Tick>(m, tick);
//     bind_tempo_class<Quarter>(m, quarter);
//     bind_tempo_class<Second>(m, second);
//
//     bind_time_signature_class<Tick>(m, tick);
//     bind_time_signature_class<Quarter>(m, quarter);
//     bind_time_signature_class<Second>(m, second);
//
//     bind_key_signature_class<Tick>(m, tick);
//     bind_key_signature_class<Quarter>(m, quarter);
//     bind_key_signature_class<Second>(m, second);
//
//     bind_pitch_bend_class<Tick>(m, tick);
//     bind_pitch_bend_class<Quarter>(m, quarter);
//     bind_pitch_bend_class<Second>(m, second);
//
//     bind_text_meta_class<Tick>(m, tick);
//     bind_text_meta_class<Quarter>(m, quarter);
//     bind_text_meta_class<Second>(m, second);
//
//     auto tt = bind_track_class<Tick>(m, tick);
//     auto tq = bind_track_class<Quarter>(m, quarter);
//     auto ts = bind_track_class<Second>(m, second);
//
//     auto score_tick    = bind_score_class<Tick>(m, tick);
//     auto score_quarter = bind_score_class<Quarter>(m, quarter);
//     auto score_second  = bind_score_class<Second>(m, second);
//
//     score_tick
//         .def(
//             "to",
//             &convert_score<Tick>,
//             py::arg("ttype"),
//             py::arg("min_dur") = py::none(),
//             "Convert to another time unit"
//         )
//         .def(
//             "resample",
//             [](const Score<Tick>& self, const i32 tpq, const std::optional<i32> min_dur) {
//                 const auto min_dur_ = min_dur.has_value() ? *min_dur : 0;
//                 return resample(self, tpq, min_dur_);
//             },
//             py::arg("tpq"),
//             py::arg("min_dur") = py::none(),
//             "Resample to another ticks per quarter"
//         );
//     score_quarter
//         .def(
//             "to",
//             &convert_score<Quarter>,
//             py::arg("ttype"),
//             py::arg("min_dur") = py::none(),
//             "Convert to another time unit"
//         )
//         .def(
//             "resample",
//             [](const Score<Quarter>& self, const i32 tpq, const std::optional<i32> min_dur) {
//                 const auto min_dur_ = min_dur.has_value() ? *min_dur : 0;
//                 return resample(self, tpq, min_dur_);
//             },
//             py::arg("tpq"),
//             py::arg("min_dur") = py::none(),
//             "Resample to another ticks per quarter"
//         );
//
//     score_second
//         .def(
//             "to",
//             &convert_score<Second>,
//             py::arg("ttype"),
//             py::arg("min_dur") = py::none(),
//             "Convert to another time unit"
//         )
//         .def(
//             "resample",
//             [](const Score<Second>& self, const i32 tpq, const std::optional<i32> min_dur) {
//                 const auto min_dur_ = min_dur.has_value() ? *min_dur : 0;
//                 return resample(self, tpq, min_dur_);
//             },
//             py::arg("tpq"),
//             py::arg("min_dur") = py::none(),
//             "Resample to another ticks per quarter"
//         );
//     return m;
// }
//
// py::module_& bind_synthesizer(py::module_& m) {
//     py::class_<Synthesizer>(m, "Synthesizer")
//         .def(
//             py::init<std::string&, u32, u8>(),
//             py::arg("sf_path"),
//             py::arg("sample_rate"),
//             py::arg("quality")
//         );
//         // .def("render", &Synthesizer::render<Tick>, py::arg("score"), py::arg("stereo") = true)
//         // .def("render", &Synthesizer::render<Quarter>, py::arg("score"), py::arg("stereo") =
//         true)
//         // .def("render", &Synthesizer::render<Second>, py::arg("score"), py::arg("stereo") =
//         true);
//
//     // m.def(
//     //     "dump_wav",
//     //     &psynth::write_audio,
//     //     py::arg("path"),
//     //     py::arg("data"),
//     //     py::arg("sample_rate"),
//     //     py::arg("use_int16") = true
//     // );
//     return m;
// }

template<typename T>
shared<vec<shared<T>>> deepcopy(const shared<vec<shared<T>>>& self) {
    auto ans = std::make_shared<vec<shared<T>>>();
    ans->reserve(self->size());
    for (const auto& item : *self) { ans->push_back(std::make_shared<T>(std::move(item->deepcopy()))); }
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
        .def("__init__", [](self_t *self, const self_t& other) {
            new (self) std::shared_ptr<track_t>(std::move(std::make_shared<track_t>(*other)));
        }, "Copy constructor", nb::arg("other"))
        .def("copy", copy_func, nb::rv_policy::copy)
        .def("__copy__", copy_func, nb::rv_policy::copy)
        .def("deepcopy", deepcopy_func, nb::rv_policy::copy)
        .def("__deepcopy__", deepcopy_func, nb::rv_policy::copy)
        .def("__repr__", [](const self_t& self) { return self->to_string(); })
        .def("__getstate__", &to_bytes<track_t>)
        .def("__setstate__", &from_bytes<track_t>)
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
            auto pianoroll = TrackPianoroll::from_track(self, mode_enums, pitch_range, encode_velocity);
            return nb::ndarray<nb::numpy, pianoroll_t>{
                const_cast<u8*>(pianoroll.release()), {
                    std::get<0>(pianoroll.dims()),
                    std::get<1>(pianoroll.dims()),
                    std::get<2>(pianoroll.dims()),
                }
            };
        });
    }

    auto track_vec = bind_shared_vector_copy<vec<self_t>>(m, name + "List")
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
    ;
    // clang-format on

    return track;
}



#define BIND_EVENT(__COUNT, BIND_FUNC) \
    BIND_FUNC<Tick>(m, "Tick");        \
    BIND_FUNC<Quarter>(m, "Quarter");  \
    BIND_FUNC<Second>(m, "Second");

using namespace pyutils;
NB_MODULE(core, m) {
    m.attr("_MIDI2ABC") = std::string("");
    // clang-format off
    REPEAT_ON(
        BIND_EVENT,
        bind_note, bind_keysig, bind_timesig, bind_tempo,
        bind_controlchange, bind_pedal, bind_pitchbend, bind_textmeta
    )
    #undef BIND_EVENT
    // clang-format on

    bind_track<Tick>(m, "Tick");
    bind_track<Quarter>(m, "Quarter");
    bind_track<Second>(m, "Second");




    // py::bind_vector<vec<f32>>(m, "f32List");
    // py::bind_vector<vec<i32>>(m, "i32List");
    // py::implicitly_convertible<py::list, vec<f32>>();
    // py::implicitly_convertible<py::list, vec<i32>>();

    // auto tick = py::class_<Tick>(m, "Tick")
    //                 .def(py::init<>())
    //                 .def("__repr__", [](const Tick&) { return "symsuic.core.Tick"; })
    //                 .def("is_time_unit", [](const Tick&) { return true; });
    //
    // auto quarter = py::class_<Quarter>(m, "Quarter")
    //                    .def(py::init<>())
    //                    .def("__repr__", [](const Quarter&) { return "symsuic.core.Quarter"; })
    //                    .def("is_time_unit", [](const Quarter&) { return true; });
    //
    // auto second = py::class_<Second>(m, "Second")
    //                   .def(py::init<>())
    //                   .def("__repr__", [](const Second&) { return "symsuic.core.Second"; })
    //                   .def("is_time_unit", [](const Second&) { return true; });
    //
    // // def __eq__ for all time units
    // tick.def("__eq__", [](const Tick&, const py::object& other) {
    //     if (py::isinstance<Tick>(other)) return true;
    //     return false;
    // });
    //
    // quarter.def("__eq__", [](const Quarter&, const py::object& other) {
    //     if (py::isinstance<Quarter>(other)) return true;
    //     return false;
    // });
    //
    // second.def("__eq__", [](const Second&, const py::object& other) {
    //     if (py::isinstance<Second>(other)) return true;
    //     return false;
    // });
    //
    // core_module(m);
    // bind_synthesizer(m);
}
}   // namespace symusic
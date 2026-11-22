//
// Score bindings and file conversion utilities.
//

#pragma once

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <optional>
#include <span>
#include <string>
#include <string_view>

#include <fmt/format.h>
#include <nanobind/ndarray.h>
#include <nanobind/nanobind.h>
#include <nanobind/stl/filesystem.h>
#include <nanobind/stl/string.h>

#include "../../utils/python_helpers.h"
#include "../../utils/process_runner.h"
#include "../core/binding_common.h"
#include "../docstring_helpers.h"

namespace symusic {

namespace nb = nanobind;

namespace score_docstrings {

template<TType T>
const char* doc() {
    constexpr auto           flavor   = docstring_helpers::time_flavor<T>();
    static const std::string rendered = fmt::format(
        "A score: global tempos, time signatures, key signatures and markers, and a list of "
        "tracks.\n\nTimes are in {} ({}). ``ticks_per_quarter`` is kept in every unit.",
        flavor.units,
        flavor.scalar_label
    );
    return rendered.c_str();
}

constexpr const char* kTpqCtorDoc
    = R"pbdoc(Create an empty score with *tpq* ticks per quarter note.)pbdoc";
constexpr const char* kPathCtorDoc
    = R"pbdoc(Read a MIDI file. ``symusic.Score(path)`` also reads ABC files.)pbdoc";
constexpr const char* kFromFileDoc = R"pbdoc(
Read a MIDI or ABC file. The extension selects the format (``.mid``, ``.midi``, ``.MID``, ``.MIDI``
or ``.abc``) unless *format* names it (``"midi"`` or ``"abc"``). *sanitize_data* applies to MIDI
only; see ``from_midi``.
)pbdoc";
constexpr const char* kFromMidiDoc = R"pbdoc(
Read MIDI bytes. A channel message data byte above 127 raises ``RuntimeError``; with
*sanitize_data* it is clamped to 127 instead.
)pbdoc";
constexpr const char* kFromAbcDoc  = R"pbdoc(
Read ABC text by converting it with the bundled ``abc2midi``. Only the first tune is read.
)pbdoc";
constexpr const char* kDumpMidiDoc = R"pbdoc(
Write the score to a MIDI file. ``pedals`` are not written; the sustain pedal reaches the file
through control change 64 in ``controls``.
)pbdoc";
constexpr const char* kDumpsMidiDoc
    = R"pbdoc(Return the score as MIDI bytes, as ``dump_midi`` writes them.)pbdoc";
constexpr const char* kDumpAbcDoc = R"pbdoc(
Write the score as ABC by converting its MIDI form with the bundled ``midi2abc``. With *warn*, the
converter's error output is shown.
)pbdoc";
constexpr const char* kDumpsAbcDoc
    = R"pbdoc(Return the score as ABC text, as ``dump_abc`` writes it.)pbdoc";
constexpr const char* kGetBeatsDoc     = R"pbdoc(
Return the beat times from *start_time* to the end of the score as a NumPy array. As in
pretty_midi, compound meters (numerator 6, 9, 12, ...) count one beat per three notes of the
denominator. Without a time signature a beat is a quarter note.
)pbdoc";
constexpr const char* kGetDownbeatsDoc = R"pbdoc(
Return the time of the first beat of every bar from *start_time* to the end of the score as a
NumPy array. Without a time signature at *start_time*, 4/4 is assumed.
)pbdoc";
constexpr const char* kToDoc           = R"pbdoc(
Return a new score converted to *ttype*: ``"tick"``, ``"quarter"``, ``"second"`` or a time unit
object. Notes and pedals shorter than *min_dur*, in the target unit, are extended to it; pass
``min_dur=1`` when converting to ticks so that no note rounds to duration 0.
)pbdoc";
constexpr const char* kResampleDoc     = R"pbdoc(
Return a tick score with *tpq* ticks per quarter note, with every time rounded to the new grid.
Notes and pedals shorter than *min_dur* ticks are extended to it.
)pbdoc";
constexpr const char* kPianorollDoc    = R"pbdoc(
Return a ``uint8`` array of shape ``(modes, tracks, pitches, time)`` with one column per tick.
*modes* lists ``"frame"``, ``"onset"`` or ``"offset"``; *pitch_range* is the pitch interval ``[low,
high)``; *encode_velocity* writes velocities instead of 1. Raises ``ValueError`` for a note with
negative time or duration, or when the score ends before tick 0.
)pbdoc";

constexpr const char* kCopyCtorDoc = R"pbdoc(Create a deep copy of *other*.)pbdoc";
constexpr const char* kCopyDoc     = R"pbdoc(
Return a deep copy, or with ``deep=False`` a new score that shares the ``tracks`` list and every
event.
)pbdoc";
constexpr const char* kSortDoc     = R"pbdoc(
Sort the event lists of every track and the global lists by their default sort key; ``tracks``
keeps its order. Works in place unless *inplace* is false.
)pbdoc";
constexpr const char* kClipDoc     = R"pbdoc(
Keep the events that start in ``[start, end)``; with *clip_end*, notes and pedals must also end by
*end*. Times are not shifted. The tempo, time signature and key signature in effect at *start* move
to *start*. Returns a new score unless *inplace* is true.
)pbdoc";
constexpr const char* kTrimDoc     = R"pbdoc(
Keep notes and pedals that overlap ``[start, end)`` by at least *min_overlap*. At each boundary,
``"remove"`` drops crossing events, ``"truncate"`` cuts them at the boundary, and ``"retain"``
keeps them whole. Invalid modes and negative *min_overlap* raise ``ValueError``. Other events are clipped by time, and the tempo and signatures in effect at
*start* are kept there. Returns a new score with independent tracks and events.
)pbdoc";
constexpr const char* kShiftTimeDoc
    = R"pbdoc(Add *offset* to every time. Returns a new score unless *inplace* is true.)pbdoc";
constexpr const char* kShiftPitchDoc    = R"pbdoc(
Add *offset* to every note's pitch. Returns a new score unless *inplace* is true. Raises
``ValueError`` when a result leaves -128..127.
)pbdoc";
constexpr const char* kShiftVelocityDoc = R"pbdoc(
Add *offset* to every note's velocity. Returns a new score unless *inplace* is true. Raises
``ValueError`` when a result leaves -128..127.
)pbdoc";
constexpr const char* kStartDoc
    = R"pbdoc(Return the earliest event time, or 0 when the score has no events.)pbdoc";
constexpr const char* kEndDoc
    = R"pbdoc(Return the latest event end, or 0 when the score has no events.)pbdoc";
constexpr const char* kNoteNumDoc = R"pbdoc(Return the number of notes in all tracks.)pbdoc";
constexpr const char* kEmptyDoc
    = R"pbdoc(Return whether the score has no tracks and no global events.)pbdoc";
constexpr const char* kAdjustTimeDoc = R"pbdoc(
Map times piecewise linearly through the anchor pairs ``(original_times[i], new_times[i])``. The
lists must be the same length and have at least two anchors. Original times must be strictly
increasing, and new times must be nondecreasing. Events outside the anchor range are dropped.
Returns a new score unless *inplace* is true.
)pbdoc";
constexpr const char* kUseCountDoc
    = R"pbdoc(Return the number of references to the score, for debugging.)pbdoc";
}   // namespace score_docstrings

inline std::string path_to_utf8_string(const std::filesystem::path& path) {
#ifdef _WIN32
    const auto utf8 = path.u8string();
    return std::string(reinterpret_cast<const char*>(utf8.c_str()), utf8.size());
#else
    return path.string();
#endif
}

template<TType T>
typename T::unit cast_time(const nb::object& t) {
    using unit = typename T::unit;
    if (t.is_none()) { return static_cast<unit>(0); }
    return nb::cast<unit>(t);
}

template<typename To, typename From>
nb::object convert_score_inner(const shared<Score<From>>& self, const nb::object& min_dur) {
    return nb::cast(
        std::make_shared<Score<To>>(convert<To>(*self, cast_time<To>(min_dur))), nb::rv_policy::copy
    );
}

template<typename From>
nb::object convert_score(
    const shared<Score<From>>& self, const nb::object& ttype, const nb::object& min_dur
) {
    if (ttype.is_none()) throw std::invalid_argument("ttype must be specified");
    if (nb::isinstance<Tick>(ttype)) return convert_score_inner<Tick>(self, min_dur);
    if (nb::isinstance<Quarter>(ttype)) return convert_score_inner<Quarter>(self, min_dur);
    if (nb::isinstance<Second>(ttype)) return convert_score_inner<Second>(self, min_dur);
    if (nb::isinstance<nb::str>(ttype)) {
        const auto ttype_str = nb::cast<std::string>(ttype.attr("lower")());
        if (ttype_str == "tick") return convert_score_inner<Tick>(self, min_dur);
        if (ttype_str == "quarter") return convert_score_inner<Quarter>(self, min_dur);
        if (ttype_str == "second") return convert_score_inner<Second>(self, min_dur);
    }
    throw std::invalid_argument("ttype must be Tick, Quarter, Second or string");
}

template<TType T>
shared<Score<T>> midi2score(const std::filesystem::path& path, const bool sanitize_data = false) {
    auto     data = read_file(path);
    Score<T> s    = parse<DataFormat::MIDI, Score<T>>(data, sanitize_data);
    return std::make_shared<Score<T>>(std::move(s));
}

template<TType T>
void dump_midi(const shared<Score<T>>& self, const std::filesystem::path& path) {
    const auto data = self->template dumps<DataFormat::MIDI>();
    write_file(path, data);
}

template<TType T>
void dump_abc_fs(const shared<Score<T>>& self, const std::filesystem::path& path, const bool warn) {
    const auto midi2abc_env = std::getenv("SYMUSIC_MIDI2ABC");
    if (!midi2abc_env) {
        throw std::runtime_error("SYMUSIC_MIDI2ABC environment variable not set");
    }
    const std::filesystem::path midi2abc = midi2abc_env;
    if (midi2abc.empty()) { throw std::runtime_error("midi2abc path is empty"); }

    process_runner::ScopedTempDir temp_dir;
    const auto                    temp_midi_path = temp_dir.path() / "input.mid";
#ifdef _WIN32
    const auto  temp_abc_path         = temp_dir.path() / "output.abc";
    const auto& converter_output_path = temp_abc_path;
#else
    const auto& converter_output_path = path;
#endif

    dump_midi(self, temp_midi_path);
    process_runner::run_process_checked(
        midi2abc, {temp_midi_path, std::filesystem::path("-o"), converter_output_path}, !warn
    );

    if (!std::filesystem::exists(converter_output_path)) {
        throw std::runtime_error(
            fmt::format("midi2abc did not create {}", path_to_utf8_string(path))
        );
    }

#ifdef _WIN32
    std::error_code error;
    std::filesystem::copy_file(
        converter_output_path, path, std::filesystem::copy_options::overwrite_existing, error
    );
    if (error) {
        throw std::runtime_error(
            fmt::format(
                "Failed to copy ABC output to {}: {}", path_to_utf8_string(path), error.message()
            )
        );
    }
#endif
}

template<TType T>
void dump_abc_path(
    const shared<Score<T>>& self, const std::filesystem::path& path, const bool warn
) {
    dump_abc_fs<T>(self, path, warn);
}

template<TType T>
std::string dumps_abc(const shared<Score<T>>& self, const bool warn) {
    process_runner::ScopedTempDir temp_dir;
    const auto                    temp_abc_path = temp_dir.path() / "output.abc";
    dump_abc_fs(self, temp_abc_path, warn);
    const auto data = read_file(temp_abc_path);
    return std::string(reinterpret_cast<const char*>(data.data()), data.size());
}

inline std::string get_format(const std::filesystem::path& path) {
    const auto ext = path.extension().string();
    if (ext == ".mid" || ext == ".midi" || ext == ".MID" || ext == ".MIDI") { return "midi"; }
    if (ext == ".abc") { return "abc"; }
    throw std::invalid_argument("Unknown file format");
}

template<TType T>
shared<Score<T>> from_abc_file(const std::filesystem::path& path) {
    const auto abc2midi_env = std::getenv("SYMUSIC_ABC2MIDI");
    if (!abc2midi_env) { throw std::runtime_error("abc2midi not found"); }
    const std::filesystem::path abc2midi = abc2midi_env;
    if (abc2midi.empty()) { throw std::runtime_error("abc2midi not found"); }

    process_runner::ScopedTempDir temp_dir;
#ifdef _WIN32
    const auto      temp_abc_path = temp_dir.path() / "input.abc";
    std::error_code copy_error;
    std::filesystem::copy_file(
        path, temp_abc_path, std::filesystem::copy_options::overwrite_existing, copy_error
    );
    if (copy_error) {
        throw std::runtime_error(
            fmt::format(
                "Failed to stage ABC input {}: {}", path_to_utf8_string(path), copy_error.message()
            )
        );
    }
    const auto& converter_input_path = temp_abc_path;
#else
    const auto& converter_input_path = path;
#endif
    const auto midi_path = temp_dir.path() / "output.mid";
    process_runner::run_process_checked(
        abc2midi,
        {
            converter_input_path,
            std::filesystem::path("-o"),
            midi_path,
            std::filesystem::path("-silent"),
        },
        false
    );
    if (!std::filesystem::exists(midi_path)) {
        throw std::runtime_error(fmt::format("abc2midi did not create {}", midi_path.string()));
    }

    const auto data = read_file(midi_path);
    auto       s    = Score<T>::template parse<DataFormat::MIDI>(data);
    return std::make_shared<Score<T>>(std::move(s));
}

template<TType T>
shared<Score<T>> from_abc(const std::string& abc) {
    process_runner::ScopedTempDir temp_dir;
    const auto                    temp_abc_path = temp_dir.path() / "input.abc";
    write_file(temp_abc_path, std::span(reinterpret_cast<const u8*>(abc.data()), abc.size()));
    return from_abc_file<T>(temp_abc_path);
}

template<TType T>
shared<Score<T>> from_file(
    const std::filesystem::path&      path,
    const std::optional<std::string>& format,
    const bool                        sanitize_data
) {
    std::string format_ = format.has_value() ? *format : "";
    if (format_.empty()) {
        format_ = get_format(path);
    } else {
        std::transform(format_.begin(), format_.end(), format_.begin(), ::tolower);
    }

    if (format_ == "midi" || format_ == "mid") { return midi2score<T>(path, sanitize_data); }
    if (format_ == "abc") {
        if (sanitize_data) {
            throw std::invalid_argument("sanitize_data is only supported for MIDI input");
        }
        return from_abc_file<T>(path);
    }
    throw std::invalid_argument("Unknown file format");
}

template<typename Scalar>
auto to_numpy_array(const vec<Scalar>& values) {
    auto* data = new Scalar[values.size()];
    std::copy(values.begin(), values.end(), data);
    nb::capsule owner(data, [](void* p) noexcept { delete[] static_cast<Scalar*>(p); });
    return nb::ndarray<nb::numpy, Scalar>(data, {values.size()}, owner);
}

template<TType T>
auto get_beats_array(const shared<Score<T>>& self, const typename T::unit start_time) {
    return to_numpy_array(self->get_beats(start_time));
}

template<TType T>
auto get_downbeats_array(const shared<Score<T>>& self, const typename T::unit start_time) {
    return to_numpy_array(self->get_downbeats(start_time));
}

template<TType T>
auto bind_score(nb::module_& m, const std::string& name_) {
    using unit   = typename T::unit;
    using self_t = shared<Score<T>>;

    const auto     name      = "Score" + name_;
    const auto     score_sig = fmt::format("class {}(Score[{}])", name, name_);
    constexpr auto flavor    = docstring_helpers::time_flavor<T>();

    auto copy_func     = [](const self_t& self) { return std::make_shared<Score<T>>(*self); };
    auto deepcopy_func = [](const self_t& self) {
        return std::make_shared<Score<T>>(std::move(self->deepcopy()));
    };

    // clang-format off
    auto score = nb::class_<self_t>(m, name.c_str(), nb::sig(score_sig.c_str()), score_docstrings::doc<T>())
        .def("__init__", &pyutils::pyinit<Score<T>, i32>, nb::arg("tpq"), score_docstrings::kTpqCtorDoc,
             nb::sig("def __init__(self, tpq: int, /) -> None"))
        .def("__init__", [](self_t* self, const self_t& other) {
            new (self) self_t(std::make_shared<Score<T>>(other->deepcopy()));
        }, nb::arg("other"), score_docstrings::kCopyCtorDoc,
           nb::sig(fmt::format("def __init__(self, other: {}, /) -> None", name).c_str()))
        .def("copy", [&](const self_t &self, const bool deep) {
            if (deep) return deepcopy_func(self);
            return copy_func(self);
        }, nb::arg("deep") = true, nb::rv_policy::copy, score_docstrings::kCopyDoc,
           nb::sig(fmt::format("def copy(self, deep: bool = True, /) -> {}", name).c_str()))
        .def("__copy__", copy_func, nb::rv_policy::copy,
           nb::sig(fmt::format("def __copy__(self, /) -> {}", name).c_str()))
        .def("__deepcopy__", [&](const self_t& self, const nb::handle, const nb::handle) {
            return deepcopy_func(self);
        }, nb::arg("memo")=nb::none(), nb::arg("_nil")=nb::none(), nb::rv_policy::move,
           nb::sig(fmt::format("def __deepcopy__(self, memo: object, _nil: object, /) -> {}", name).c_str()))
        .def("__repr__", [](const self_t& self) { return self->to_string(); },
           nb::sig("def __repr__(self, /) -> str"))
        .def("__eq__", [](const self_t& self, const self_t& other) { return self == other || *self == *other; },
           nb::sig(fmt::format("def __eq__(self, other: {}, /) -> bool", name).c_str()))
        .def("__eq__", [](const self_t&, nb::handle) { return false; },
           nb::sig("def __eq__(self, other: object, /) -> bool"))
        .def("__ne__", [](const self_t& self, const self_t& other) { return self != other && *self != *other; },
           nb::sig(fmt::format("def __ne__(self, other: {}, /) -> bool", name).c_str()))
        .def("__ne__", [](const self_t&, nb::handle) { return true; },
           nb::sig("def __ne__(self, other: object, /) -> bool"))
        .def("__getstate__", [](const self_t& self) {
            const auto native = to_native(*self);
            const vec<unsigned char> data = dumps<DataFormat::ZPP>(native);
            return nb::bytes(reinterpret_cast<const char*>(data.data()), data.size());
        }, nb::sig("def __getstate__(self, /) -> bytes"))
        .def("__setstate__", [](self_t& self, const nb::bytes& bytes) {
            const auto      data = std::string_view(bytes.c_str(), bytes.size());
            const std::span span(reinterpret_cast<const unsigned char*>(data.data()), data.size());
            auto ans = symusic::parse<symusic::DataFormat::ZPP, ScoreNative<T>>(span);
            new (&self) self_t(std::make_shared<Score<T>>(std::move(to_shared(std::move(ans)))));
        }, nb::sig(fmt::format("def __setstate__(self, state: bytes, /) -> {}", name).c_str()))
        .def("__init__", [](self_t* self, const std::filesystem::path& path) {
            new (self) self_t(midi2score<T>(path));
        }, nb::arg("path"), score_docstrings::kPathCtorDoc,
           nb::sig("def __init__(self, path: pathlib.Path, /) -> None"))
        .def_static("from_file", &from_file<T>, nb::arg("path"), nb::arg("format") = nb::none(), nb::arg("sanitize_data") = false, score_docstrings::kFromFileDoc)
        .def_static("from_midi", [](const nb::bytes& data, bool sanitize_data) {
            const auto str  = std::string_view(data.c_str(), data.size());
            const auto span = std::span(reinterpret_cast<const u8*>(str.data()), str.size());
            return std::make_shared<Score<T>>(parse<DataFormat::MIDI, Score<T>>(span, sanitize_data));
        },
            nb::arg("data"),
            nb::arg("sanitize_data") = false,
            score_docstrings::kFromMidiDoc
        )
        .def_static("from_abc", &from_abc<T>, nb::arg("abc"), score_docstrings::kFromAbcDoc)
        .def("dump_midi", &dump_midi<T>, nb::arg("path"), score_docstrings::kDumpMidiDoc)
        .def("dumps_midi", [](const self_t& self) {
            auto data = self->template dumps<DataFormat::MIDI>();
            return nb::bytes(reinterpret_cast<const char*>(data.data()), data.size());
        }, score_docstrings::kDumpsMidiDoc)
        .def("dump_abc", &dump_abc_path<T>, nb::arg("path"), nb::arg("warn") = false, score_docstrings::kDumpAbcDoc)
        .def("dumps_abc", &dumps_abc<T>, nb::arg("warn") = false, score_docstrings::kDumpsAbcDoc)
        .def("get_beats", &get_beats_array<T>, nb::arg("start_time") = static_cast<unit>(0), score_docstrings::kGetBeatsDoc)
        .def("get_downbeats", &get_downbeats_array<T>, nb::arg("start_time") = static_cast<unit>(0), score_docstrings::kGetDownbeatsDoc)
        .def_prop_rw(RW_COPY(i32, "ticks_per_quarter", ticks_per_quarter), "Ticks per quarter note, the MIDI resolution.")
        .def_prop_rw(RW_COPY(i32, "tpq", ticks_per_quarter), "Alias of ``ticks_per_quarter``.")
        .def_prop_rw(RW_COPY(shared<vec<shared<Track<T>>>>, "tracks", tracks), "The ``Track`` objects.")
        .def_prop_rw(RW_COPY(shared<pyvec<TimeSignature<T>>>, "time_signatures", time_signatures), "The ``TimeSignature`` events.")
        .def_prop_rw(RW_COPY(shared<pyvec<KeySignature<T>>>, "key_signatures", key_signatures), "The ``KeySignature`` events.")
        .def_prop_rw(RW_COPY(shared<pyvec<Tempo<T>>>, "tempos", tempos), "The ``Tempo`` events.")
        .def_prop_rw(RW_COPY(shared<pyvec<TextMeta<T>>>, "markers", markers), "The markers, as ``TextMeta`` events.")
        .def_prop_ro("ttype", [](const self_t&) { return T(); }, "The time unit.")
        .def("__use_count", [](const self_t& self) { return self.use_count(); }, score_docstrings::kUseCountDoc)
        .def("to", &convert_score<T>, nb::arg("ttype"), nb::arg("min_dur") = nb::none(), score_docstrings::kToDoc)
        .def("resample", [](const self_t& self, const i32 tpq, const std::optional<unit> min_dur) {
            const unit min_dur_ = min_dur.has_value() ? *min_dur : 0;
            return std::make_shared<Score<Tick>>(resample(*self, tpq, min_dur_));
        }, nb::arg("tpq"), nb::arg("min_dur") = nb::none(), nb::rv_policy::copy, score_docstrings::kResampleDoc)
        .def("sort", [](self_t& self, const bool reverse, const bool inplace) {
            if (inplace) {
                self->sort_inplace(reverse);
                return self;
            }   return std::make_shared<Score<T>>(self->sort(reverse));
        }, nb::arg("reverse") = false, nb::arg("inplace") = true, score_docstrings::kSortDoc)
        .def("clip", [](self_t& self, const unit start, const unit end, const bool clip_end, const bool inplace) {
            if (inplace) {
                self->clip_inplace(start, end, clip_end);
                return self;
            }   return std::make_shared<Score<T>>(self->clip(start, end, clip_end));
        }, nb::arg("start"), nb::arg("end"), nb::arg("clip_end") = false, nb::arg("inplace") = false, score_docstrings::kClipDoc)
        .def("trim", [](const self_t& self, const unit start, const unit end, const unit min_overlap, const std::string& start_mode, const std::string& end_mode) {
            return std::make_shared<Score<T>>(self->trim(start, end, min_overlap, start_mode, end_mode));
        }, nb::arg("start"), nb::arg("end"), nb::arg("min_overlap") = unit{0}, nb::arg("start_mode") = "remove", nb::arg("end_mode") = "remove", score_docstrings::kTrimDoc,
            nb::sig(fmt::format(
                "def trim(self, start: {0}, end: {0}, min_overlap: {0} = {1}, "
                "start_mode: typing.Literal['remove', 'truncate', 'retain'] = 'remove', "
                "end_mode: typing.Literal['remove', 'truncate', 'retain'] = 'remove') -> {2}",
                flavor.scalar_label, std::is_same_v<T, Tick> ? "0" : "0.0", name
            ).c_str()))
        .def("shift_time", [](self_t& self, const unit offset, const bool inplace) {
            if (inplace) {
                self->shift_time_inplace(offset);
                return self;
            }   return std::make_shared<Score<T>>(self->shift_time(offset));
        }, nb::arg("offset"), nb::arg("inplace") = false, score_docstrings::kShiftTimeDoc)
        .def("shift_pitch", [](self_t& self, const i8 offset, const bool inplace) {
            if (inplace) {
                self->shift_pitch_inplace(offset);
                return self;
            }   return std::make_shared<Score<T>>(self->shift_pitch(offset));
        }, nb::arg("offset"), nb::arg("inplace") = false, score_docstrings::kShiftPitchDoc)
        .def("shift_velocity", [](self_t& self, const i8 offset, const bool inplace) {
            if (inplace) {
                self->shift_velocity_inplace(offset);
                return self;
            }   return std::make_shared<Score<T>>(self->shift_velocity(offset));
        }, nb::arg("offset"), nb::arg("inplace") = false, score_docstrings::kShiftVelocityDoc)
        .def("start", [](const self_t& self) { return self->start(); }, score_docstrings::kStartDoc)
        .def("end", [](const self_t& self) { return self->end(); }, score_docstrings::kEndDoc)
        .def("note_num", [](const self_t& self) { return self->note_num(); }, score_docstrings::kNoteNumDoc)
        .def("empty", [](const self_t& self) { return self->empty(); }, score_docstrings::kEmptyDoc)
        .def("adjust_time", [](self_t& self, const vec<unit>& original_times, const vec<unit>& new_times, const bool inplace) {
            if (inplace) {
                ops::adjust_time_inplace(*self, original_times, new_times);
                return self;
            }   return std::make_shared<Score<T>>(ops::adjust_time(*self, original_times, new_times));
        }, nb::arg("original_times"), nb::arg("new_times"), nb::arg("inplace") = false, score_docstrings::kAdjustTimeDoc)
    ;
    // clang-format on

    if constexpr (std::is_same_v<T, Tick>) {
        score.def(
            "pianoroll",
            [](const shared<Score<Tick>>& self,
               const vec<std::string>&    modes,
               const std::pair<i64, i64>  pitch_range,
               const bool                 encode_velocity) {
                vec<PianorollMode> mode_enums(modes.size());
                for (size_t i = 0; i < modes.size(); ++i) {
                    mode_enums[i] = str_to_pianoroll_mode(modes[i]);
                }
                auto pianoroll = ScorePianoroll::from_score(
                    *self, mode_enums, validate_pitch_range(pitch_range), encode_velocity
                );
                auto*       data = const_cast<u8*>(pianoroll.release());
                nb::capsule owner(data, [](void* d) noexcept { delete[] static_cast<u8*>(d); });
                return nb::ndarray<nb::numpy, pianoroll_t>{
                    data,
                    {std::get<0>(pianoroll.dims()),
                     std::get<1>(pianoroll.dims()),
                     std::get<2>(pianoroll.dims()),
                     std::get<3>(pianoroll.dims())},
                    owner
                };
            },
            nb::arg("modes")           = vec<std::string>{"frame", "onset"},
            nb::arg("pitch_range")     = std::pair<i64, i64>(0, 128),
            nb::arg("encode_velocity") = false,
            score_docstrings::kPianorollDoc
        );
    }
    return score;
}

}   // namespace symusic

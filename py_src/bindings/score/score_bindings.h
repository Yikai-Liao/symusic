//
// Score bindings and file conversion utilities.
//

#pragma once

#include "utils/storage.h"
#include <algorithm>
#include <filesystem>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>

#include <fmt/format.h>
#include <fmt/std.h>
#include <nanobind/ndarray.h>
#include <nanobind/nanobind.h>
#include <nanobind/stl/filesystem.h>
#include <nanobind/stl/string.h>

#include "../../utils/python_helpers.h"
#include "../core/binding_common.h"
#include "../docstring_helpers.h"

namespace symusic::py {

namespace nb = nanobind;

template<TType T>
const char* doc() {
    constexpr auto           flavor   = time_flavor<T>();
    static const std::string rendered = fmt::format(
        "A score: global tempos, time signatures, key signatures and markers, and a list of "
        "tracks.\n\nTimes are in {} ({}). ``ticks_per_quarter`` is kept in every unit.",
        flavor.units,
        flavor.scalar_label
    );
    return rendered.c_str();
}

constexpr const char* kScoreTpqCtorDoc
    = R"pbdoc(Create an empty score with *tpq* ticks per quarter note.)pbdoc";
constexpr const char* kScorePathCtorDoc = R"pbdoc(Read a MIDI file.)pbdoc";
constexpr const char* kScoreFromMidiDoc = R"pbdoc(
Read MIDI bytes. A channel message data byte above 127 raises ``RuntimeError``; with
*sanitize_data* it is clamped to 127 instead.
)pbdoc";
constexpr const char* kScoreDumpMidiDoc = R"pbdoc(
Write the score to a MIDI file. ``pedals`` are not written; the sustain pedal reaches the file
through control change 64 in ``controls``.
)pbdoc";
constexpr const char* kScoreDumpsMidiDoc
    = R"pbdoc(Return the score as MIDI bytes, as ``dump_midi`` writes them.)pbdoc";
constexpr const char* kScoreGetBeatsDoc     = R"pbdoc(
Return the beat times from *start_time* to the end of the score as a NumPy array. As in
pretty_midi, compound meters (numerator 6, 9, 12, ...) count one beat per three notes of the
denominator. Without a time signature a beat is a quarter note.
)pbdoc";
constexpr const char* kScoreGetDownbeatsDoc = R"pbdoc(
Return the time of the first beat of every bar from *start_time* to the end of the score as a
NumPy array. Without a time signature at *start_time*, 4/4 is assumed.
)pbdoc";
constexpr const char* kScoreToDoc           = R"pbdoc(
Return a new score converted to *ttype*: ``"tick"``, ``"quarter"``, ``"second"`` or a time unit
object. Notes and pedals shorter than *min_dur*, in the target unit, are extended to it; pass
``min_dur=1`` when converting to ticks so that no note rounds to duration 0.
)pbdoc";
constexpr const char* kScoreResampleDoc     = R"pbdoc(
Return a tick score with *tpq* ticks per quarter note, with every time rounded to the new grid.
Notes and pedals shorter than *min_dur* ticks are extended to it.
)pbdoc";
constexpr const char* kScorePianorollDoc    = R"pbdoc(
Return a ``uint8`` array of shape ``(modes, tracks, pitches, time)`` with one column per tick.
*modes* lists ``"frame"``, ``"onset"`` or ``"offset"``; *pitch_range* is the pitch interval ``[low,
high)``; *encode_velocity* writes velocities instead of 1. Raises ``ValueError`` for a note with
negative time or duration, or when the score ends before tick 0.
)pbdoc";

constexpr const char* kScoreCopyCtorDoc = R"pbdoc(Create a deep copy of *other*.)pbdoc";
constexpr const char* kScoreCopyDoc     = R"pbdoc(
Return a deep copy, or with ``deep=False`` a new score that shares the ``tracks`` list and every
event.
)pbdoc";
constexpr const char* kScoreSortDoc     = R"pbdoc(
Sort the event lists of every track and the global lists by their default sort key; ``tracks``
keeps its order. Works in place unless *inplace* is false.
)pbdoc";
constexpr const char* kScoreClipDoc     = R"pbdoc(
Keep the events that start in ``[start, end)``; with *clip_end*, notes and pedals must also end by
*end*. Times are not shifted. The tempo, time signature and key signature in effect at *start* move
to *start*. Returns a new score unless *inplace* is true.
)pbdoc";
constexpr const char* kScoreTrimDoc     = R"pbdoc(
Keep notes and pedals that overlap ``[start, end)`` by at least *min_overlap*. At each boundary,
``"remove"`` drops crossing events, ``"truncate"`` cuts them at the boundary, and ``"retain"``
keeps them whole. Invalid modes and negative *min_overlap* raise ``ValueError``. Other events are clipped by time, and the tempo and signatures in effect at
*start* are kept there. Returns a new score with independent tracks and events unless *inplace* is
true, in which case it modifies and returns this score.
)pbdoc";
constexpr const char* kScoreShiftTimeDoc
    = R"pbdoc(Add *offset* to every time. Returns a new score unless *inplace* is true.)pbdoc";
constexpr const char* kScoreShiftPitchDoc    = R"pbdoc(
Add *offset* to every note's pitch. Returns a new score unless *inplace* is true. Raises
``ValueError`` when a result leaves -128..127.
)pbdoc";
constexpr const char* kScoreShiftVelocityDoc = R"pbdoc(
Add *offset* to every note's velocity. Returns a new score unless *inplace* is true. Raises
``ValueError`` when a result leaves -128..127.
)pbdoc";
constexpr const char* kScoreStartDoc
    = R"pbdoc(Return the earliest event time, or 0 when the score has no events.)pbdoc";
constexpr const char* kScoreEndDoc
    = R"pbdoc(Return the latest event end, or 0 when the score has no events.)pbdoc";
constexpr const char* kScoreNoteNumDoc = R"pbdoc(Return the number of notes in all tracks.)pbdoc";
constexpr const char* kScoreEmptyDoc
    = R"pbdoc(Return whether the score has no tracks and no global events.)pbdoc";
constexpr const char* kScoreAdjustTimeDoc = R"pbdoc(
Map times piecewise linearly through the anchor pairs ``(original_times[i], new_times[i])``. The
lists must be the same length and have at least two anchors. Original times must be strictly
increasing, and new times must be nondecreasing. Events outside the anchor range are dropped.
Returns a new score unless *inplace* is true.
)pbdoc";
constexpr const char* kScoreUseCountDoc
    = R"pbdoc(Return the number of references to the score, for debugging.)pbdoc";

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
shared<Score<T>> midi2score(const std::filesystem::path& path) {
    if (!std::filesystem::is_regular_file(path)) {
        throw std::runtime_error(fmt::format("path must refer to a regular file, got {}", path));
    }
    auto     data = read_file(path);
    Score<T> s    = parse<DataFormat::MIDI, Score<T>>(data, false);
    return std::make_shared<Score<T>>(std::move(s));
}

template<TType T>
void dump_midi(const shared<Score<T>>& self, const std::filesystem::path& path) {
    const auto data = self->template dumps<DataFormat::MIDI>();
    write_file(path, data);
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
    constexpr auto flavor    = time_flavor<T>();

    auto copy_func     = [](const self_t& self) { return std::make_shared<Score<T>>(*self); };
    auto deepcopy_func = [](const self_t& self) {
        return std::make_shared<Score<T>>(std::move(self->deepcopy()));
    };

    // clang-format off
    auto score = nb::class_<self_t>(m, name.c_str(), nb::sig(score_sig.c_str()), doc<T>())
        .def("__init__", &pyinit<Score<T>, i32>, nb::arg("tpq"), kScoreTpqCtorDoc,
             nb::sig("def __init__(self, tpq: int, /) -> None"))
        .def("__init__", [](self_t* self, const self_t& other) {
            new (self) self_t(std::make_shared<Score<T>>(other->deepcopy()));
        }, nb::arg("other"), kScoreCopyCtorDoc,
           nb::sig(fmt::format("def __init__(self, other: {}, /) -> None", name).c_str()))
        .def("copy", [&](const self_t &self, const bool deep) {
            if (deep) return deepcopy_func(self);
            return copy_func(self);
        }, nb::arg("deep") = true, nb::rv_policy::copy, kScoreCopyDoc,
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
            const vec<u8> data = dumps<DataFormat::ZPP>(*self);
            return nb::bytes(reinterpret_cast<const char*>(data.data()), data.size());
        }, nb::sig("def __getstate__(self, /) -> bytes"))
        .def("__setstate__", [](self_t& self, const nb::bytes& bytes) {
            const auto      data = std::string_view(bytes.c_str(), bytes.size());
            const std::span span(reinterpret_cast<const unsigned char*>(data.data()), data.size());
            auto ans = symusic::parse<symusic::DataFormat::ZPP, Score<T>>(span);
            new (&self) self_t(std::make_shared<Score<T>>(std::move(ans)));
        }, nb::sig(fmt::format("def __setstate__(self, state: bytes, /) -> {}", name).c_str()))
        .def("__init__", [](self_t* self, const std::filesystem::path& path) {
            new (self) self_t(midi2score<T>(path));
        }, nb::arg("path"), kScorePathCtorDoc,
           nb::sig("def __init__(self, path: pathlib.Path, /) -> None"))
        .def_static("from_midi", [](const nb::bytes& data, bool sanitize_data) {
            const auto str  = std::string_view(data.c_str(), data.size());
            const auto span = std::span(reinterpret_cast<const u8*>(str.data()), str.size());
            return std::make_shared<Score<T>>(parse<DataFormat::MIDI, Score<T>>(span, sanitize_data));
        },
            nb::arg("data"),
            nb::arg("sanitize_data") = false,
            kScoreFromMidiDoc
        )
        .def("dump_midi", &dump_midi<T>, nb::arg("path"), kScoreDumpMidiDoc)
        .def("dumps_midi", [](const self_t& self) {
            auto data = self->template dumps<DataFormat::MIDI>();
            return nb::bytes(reinterpret_cast<const char*>(data.data()), data.size());
        }, kScoreDumpsMidiDoc)
        .def("get_beats", &get_beats_array<T>, nb::arg("start_time") = static_cast<unit>(0), kScoreGetBeatsDoc)
        .def("get_downbeats", &get_downbeats_array<T>, nb::arg("start_time") = static_cast<unit>(0), kScoreGetDownbeatsDoc)
        .def_prop_rw(RW_COPY(i32, "ticks_per_quarter", ticks_per_quarter), "Ticks per quarter note, the MIDI resolution.")
        .def_prop_rw(RW_COPY(i32, "tpq", ticks_per_quarter), "Alias of ``ticks_per_quarter``.")
        .def_prop_rw(RW_COPY(shared<vec<shared<Track<T>>>>, "tracks", tracks), "The ``Track`` objects.")
        .def_prop_rw(RW_COPY(shared<pyvec<TimeSignature<T>>>, "time_signatures", time_signatures), "The ``TimeSignature`` events.")
        .def_prop_rw(RW_COPY(shared<pyvec<KeySignature<T>>>, "key_signatures", key_signatures), "The ``KeySignature`` events.")
        .def_prop_rw(RW_COPY(shared<pyvec<Tempo<T>>>, "tempos", tempos), "The ``Tempo`` events.")
        .def_prop_rw(RW_COPY(shared<pyvec<TextMeta<T>>>, "markers", markers), "The markers, as ``TextMeta`` events.")
        .def_prop_ro("ttype", [](const self_t&) { return T(); }, "The time unit.")
        .def("__use_count", [](const self_t& self) { return self.use_count(); }, kScoreUseCountDoc)
        .def("to", &convert_score<T>, nb::arg("ttype"), nb::arg("min_dur") = nb::none(), kScoreToDoc)
        .def("resample", [](const self_t& self, const i32 tpq, const std::optional<unit> min_dur) {
            const unit min_dur_ = min_dur.has_value() ? *min_dur : 0;
            return std::make_shared<Score<Tick>>(resample(*self, tpq, min_dur_));
        }, nb::arg("tpq"), nb::arg("min_dur") = nb::none(), nb::rv_policy::copy, kScoreResampleDoc)
        .def("sort", [](self_t& self, const bool reverse, const bool inplace) {
            if (inplace) {
                self->sort_inplace(reverse);
                return self;
            }   return std::make_shared<Score<T>>(self->sort(reverse));
        }, nb::arg("reverse") = false, nb::arg("inplace") = true, kScoreSortDoc)
        .def("clip", [](self_t& self, const unit start, const unit end, const bool clip_end, const bool inplace) {
            if (inplace) {
                self->clip_inplace(start, end, clip_end);
                return self;
            }   return std::make_shared<Score<T>>(self->clip(start, end, clip_end));
        }, nb::arg("start"), nb::arg("end"), nb::arg("clip_end") = false, nb::arg("inplace") = false, kScoreClipDoc)
        .def("trim", [](nb::object object, const unit start, const unit end, const unit min_overlap, const std::string& start_mode, const std::string& end_mode, const bool inplace) {
            auto& self = nb::cast<self_t&>(object);
            if (inplace) {
                self->trim_inplace(start, end, min_overlap, start_mode, end_mode);
                return object;
            }
            return nb::cast(std::make_shared<Score<T>>(self->trim(start, end, min_overlap, start_mode, end_mode)));
        }, nb::arg("start"), nb::arg("end"), nb::arg("min_overlap") = unit{0}, nb::arg("start_mode") = "remove", nb::arg("end_mode") = "remove", nb::arg("inplace") = false, kScoreTrimDoc,
            nb::sig(fmt::format(
                "def trim(self, start: {0}, end: {0}, min_overlap: {0} = {1}, "
                "start_mode: typing.Literal['remove', 'truncate', 'retain'] = 'remove', "
                "end_mode: typing.Literal['remove', 'truncate', 'retain'] = 'remove', inplace: bool = False) -> {2}",
                flavor.scalar_label, std::is_same_v<T, Tick> ? "0" : "0.0", name
            ).c_str()))
        .def("shift_time", [](self_t& self, const unit offset, const bool inplace) {
            if (inplace) {
                self->shift_time_inplace(offset);
                return self;
            }   return std::make_shared<Score<T>>(self->shift_time(offset));
        }, nb::arg("offset"), nb::arg("inplace") = false, kScoreShiftTimeDoc)
        .def("shift_pitch", [](self_t& self, const i8 offset, const bool inplace) {
            if (inplace) {
                self->shift_pitch_inplace(offset);
                return self;
            }   return std::make_shared<Score<T>>(self->shift_pitch(offset));
        }, nb::arg("offset"), nb::arg("inplace") = false, kScoreShiftPitchDoc)
        .def("shift_velocity", [](self_t& self, const i8 offset, const bool inplace) {
            if (inplace) {
                self->shift_velocity_inplace(offset);
                return self;
            }   return std::make_shared<Score<T>>(self->shift_velocity(offset));
        }, nb::arg("offset"), nb::arg("inplace") = false, kScoreShiftVelocityDoc)
        .def("start", [](const self_t& self) { return self->start(); }, kScoreStartDoc)
        .def("end", [](const self_t& self) { return self->end(); }, kScoreEndDoc)
        .def("note_num", [](const self_t& self) { return self->note_num(); }, kScoreNoteNumDoc)
        .def("empty", [](const self_t& self) { return self->empty(); }, kScoreEmptyDoc)
        .def("adjust_time", [](self_t& self, const vec<unit>& original_times, const vec<unit>& new_times, const bool inplace) {
            if (inplace) {
                ops::adjust_time_inplace(*self, original_times, new_times);
                return self;
            }   return std::make_shared<Score<T>>(ops::adjust_time(*self, original_times, new_times));
        }, nb::arg("original_times"), nb::arg("new_times"), nb::arg("inplace") = false, kScoreAdjustTimeDoc)
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
            kScorePianorollDoc
        );
    }
    return score;
}

}   // namespace symusic::py

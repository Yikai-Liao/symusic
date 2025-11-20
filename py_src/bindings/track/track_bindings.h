//
// Track bindings and related container helpers.
//

#pragma once

#include <algorithm>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include <nanobind/nanobind.h>
#include <nanobind/ndarray.h>
#include <nanobind/stl/vector.h>
#include <fmt/format.h>

#include "../../utils/python_helpers.h"
#include "../../utils/vector_bindings.h"
#include "../core/binding_common.h"
#include "../docstring_helpers.h"

namespace symusic {

namespace nb = nanobind;

namespace track_docstrings {
constexpr std::string_view kTrackDocTemplate = R"pbdoc(
{signature}
{underline}

Container for per-channel events (notes, controllers, pedals, pitch bends, metadata) tied to a MIDI
program. Tracks emulate Python lists: copy, compare, repr, and pickling propagate through the shared
pointer. This variant {timeline_sentence}

Parameters
----------
name:
    Human-friendly identifier used when printing or exporting.
program:
    MIDI program number (0-127). Defaults to acoustic grand piano.
is_drum:
    Flag drum/percussion channels. Useful when writing to SMF channel 10.

Note
----
- {measurement}
- Optimized for {best_for}
- Durations and controller timestamps share the same {timeline_noun} units

Examples
--------
.. code-block:: python

{example}
)pbdoc";

template<TType T>
const char* doc() {
    static const std::string rendered = [] {
        constexpr auto flavor = docstring_helpers::time_flavor<T>();
        const auto signature = fmt::format(
            "Track{}(name: str = \"\", program: int = 0, is_drum: bool = False)", flavor.suffix
        );
        const std::string underline(signature.size(), '=');
        const auto example = fmt::format(
            "    from symusic import Track{0}, Note{0}\n"
            "    track = Track{0}(name=\\\"Piano\\\")\n"
            "    track.notes.append(Note{0}(0, 120, 60, 90))",
            flavor.suffix
        );
        return fmt::format(
            fmt::runtime(kTrackDocTemplate),
            fmt::arg("signature", signature),
            fmt::arg("underline", underline),
            fmt::arg("timeline_sentence", flavor.timeline_sentence),
            fmt::arg("measurement", flavor.measurement_sentence),
            fmt::arg("best_for", flavor.best_for),
            fmt::arg("timeline_noun", flavor.timeline_noun),
            fmt::arg("example", example)
        );
    }();
    return rendered.c_str();
}

constexpr const char* kDefaultCtorDoc
    = R"pbdoc(Create an empty track bound to the current time unit.)pbdoc";
constexpr const char* kNamedCtorDoc = R"pbdoc(
Create a track with a friendly name, MIDI program, and optional drum flag.

Parameters mirror the attributes displayed on :class:`symusic.Track`.
)pbdoc";
constexpr const char* kCopyCtorDoc = R"pbdoc(Clone another track, deep-copying its events.)pbdoc";
constexpr const char* kCopyDoc = R"pbdoc(Return a shallow or deep copy depending on *deep*.)pbdoc";
constexpr const char* kCopyMethodDoc = R"pbdoc(Return a shallow copy of the track.)pbdoc";
constexpr const char* kDeepcopyDoc
    = R"pbdoc(Create a deep copy suitable for Python's copy module.)pbdoc";
constexpr const char* kReprDoc
    = R"pbdoc(Generate a textual summary of the track for debugging.)pbdoc";
constexpr const char* kGetStateDoc
    = R"pbdoc(Serialize the track into ZPP bytes for pickling.)pbdoc";
constexpr const char* kSetStateDoc = R"pbdoc(Rehydrate a track from serialized ZPP bytes.)pbdoc";
constexpr const char* kEqDoc
    = R"pbdoc(Tracks compare equal when their shared pointers or values match.)pbdoc";
constexpr const char* kClipDoc
    = R"pbdoc(Return a track clipped to [start, end). When ``inplace`` is False operate on a deep copy.)pbdoc";
constexpr const char* kTrimDoc
    = R"pbdoc(Trim notes to fall within [start, end) using overlap/window strategies. Optionally mutate in place.)pbdoc";
constexpr const char* kSortDoc
    = R"pbdoc(Sort notes chronologically. When *inplace* is False the result is a cloned track.)pbdoc";
constexpr const char* kAdjustTimeDoc
    = R"pbdoc(Remap timestamps using two aligned lists of original/new time values.)pbdoc";
constexpr const char* kShiftTimeDoc
    = R"pbdoc(Shift every timestamp by *offset*. Operates in place unless *inplace* is False.)pbdoc";
constexpr const char* kShiftPitchDoc
    = R"pbdoc(Shift every note's pitch by *offset*. Operates in place unless *inplace* is False.)pbdoc";
constexpr const char* kShiftVelocityDoc
    = R"pbdoc(Shift every note's velocity by *offset*. Operates in place unless *inplace* is False.)pbdoc";
constexpr const char* kStartDoc = R"pbdoc(Return the earliest event time (or max if empty).)pbdoc";
constexpr const char* kEndDoc   = R"pbdoc(Return the exclusive end of the track.)pbdoc";
constexpr const char* kNoteNumDoc = R"pbdoc(Return the number of notes on the track.)pbdoc";
constexpr const char* kEmptyDoc   = R"pbdoc(Return True when the track has no notes.)pbdoc";
constexpr const char* kPianorollDoc
    = R"pbdoc(Export a tick-based track as a pianoroll NumPy tensor. Modes control channels, pitch_range bounds the vertical axis, and encode_velocity switches from binary to velocity encoding.)pbdoc";

constexpr const char* kVecFilterDoc
    = R"pbdoc(Filter tracks with a Python callable. When *inplace* is False operate on a cloned list.)pbdoc";
constexpr const char* kVecSortDoc
    = R"pbdoc(Sort tracks by their default key or a supplied callable. Optionally return a cloned list.)pbdoc";
constexpr const char* kVecIsSortedDoc
    = R"pbdoc(Check whether the track list is already sorted by the default key or supplied callable.)pbdoc";
constexpr const char* kVecAdjustDoc
    = R"pbdoc(Adjust the timeline of every track in the list using aligned original/new timestamps.)pbdoc";
constexpr const char* kVecCopyDoc
    = R"pbdoc(Return a shallow copy or deep clone depending on *deep*.)pbdoc";
constexpr const char* kVecCopyMethodDoc = R"pbdoc(Return a shallow copy of the track list.)pbdoc";
constexpr const char* kVecDeepCopyDoc
    = R"pbdoc(Create a deep copy of the track list for Python's copy module.)pbdoc";
constexpr const char* kVecGetStateDoc = R"pbdoc(Serialize the track list into ZPP bytes.)pbdoc";
constexpr const char* kVecSetStateDoc
    = R"pbdoc(Rebuild a track list from serialized ZPP bytes.)pbdoc";
constexpr const char* kUseCountDoc
    = R"pbdoc(Return the reference count of the shared track pointer.)pbdoc";
}   // namespace track_docstrings

/// Deep copy a vector of shared pointers to keep Python level semantics clear.
template<typename T>
shared<vec<shared<T>>> clone_track_vector(const shared<vec<shared<T>>>& self) {
    auto ans = std::make_shared<vec<shared<T>>>();
    ans->reserve(self->size());
    for (const auto& item : *self) {
        ans->push_back(std::make_shared<T>(std::move(item->deepcopy())));
    }
    return ans;
}

template<TType T>
auto bind_track(nb::module_& m, const std::string& name_) {
    using unit    = typename T::unit;
    using self_t  = shared<Track<T>>;
    using track_t = Track<T>;
    using vec_t   = shared<vec<self_t>>;

    const auto name      = "Track" + name_;
    const auto track_sig = fmt::format("class {}(Track[{}])", name, name_);
    constexpr auto flavor = docstring_helpers::time_flavor<T>();

    auto copy_func = [](const self_t& self) { return std::make_shared<track_t>(*self); };
    auto deepcopy_func
        = [](const self_t& self) { return std::make_shared<track_t>(std::move(self->deepcopy())); };

    // clang-format off
    auto track = nb::class_<shared<Track<T>>>(m, name.c_str(), nb::sig(track_sig.c_str()), track_docstrings::doc<T>())
        .def("__init__", &pyutils::pyinit<Track<T>>, track_docstrings::kDefaultCtorDoc,
             nb::sig("def __init__(self, /) -> None"))
        .def("__init__", &pyutils::pyinit<Track<T>, std::string, u8, const bool>,
             nb::arg("name"), nb::arg("program") = 0, nb::arg("is_drum") = false,
             track_docstrings::kNamedCtorDoc,
             nb::sig("def __init__(self, name: str = '', program: int = 0, is_drum: bool = False, /) -> None"))
        .def("__init__", [](self_t *self, const self_t& other) {
            new (self) std::shared_ptr<track_t>(std::make_shared<track_t>(other->deepcopy()));
        }, "Copy constructor", nb::arg("other"), track_docstrings::kCopyCtorDoc,
           nb::sig(fmt::format("def __init__(self, other: {}, /) -> None", name).c_str()))
        .def("copy", [&](const self_t &self, const bool deep) {
            if (deep) return deepcopy_func(self);
            return copy_func(self);
        }, nb::arg("deep") = true, nb::rv_policy::copy, track_docstrings::kCopyDoc,
           nb::sig(fmt::format("def copy(self, deep: bool = True, /) -> {}", name).c_str()))
        .def("__copy__", copy_func, nb::rv_policy::copy, track_docstrings::kCopyMethodDoc,
           nb::sig(fmt::format("def __copy__(self, /) -> {}", name).c_str()))
        .def("__deepcopy__", [&](const self_t& self, const nb::handle, const nb::handle) {
            return deepcopy_func(self);
        }, nb::arg("memo")=nb::none(), nb::arg("_nil")=nb::none(), nb::rv_policy::copy, track_docstrings::kDeepcopyDoc,
           nb::sig(fmt::format("def __deepcopy__(self, memo: object, _nil: object, /) -> {}", name).c_str()))
        .def("__repr__", [](const self_t& self) { return self->to_string(); }, track_docstrings::kReprDoc,
           nb::sig("def __repr__(self, /) -> str"))
        .def("__getstate__", [](const self_t& self) {
            const auto native = to_native(*self);
            const vec<unsigned char> data = dumps<DataFormat::ZPP>(native);
            return nb::bytes(reinterpret_cast<const char*>(data.data()), data.size());
        }, track_docstrings::kGetStateDoc, nb::sig("def __getstate__(self, /) -> bytes"))
        .def("__setstate__", [](self_t& self, const nb::bytes& bytes) {
            const auto      data = std::string_view(bytes.c_str(), bytes.size());
            const std::span span(reinterpret_cast<const unsigned char*>(data.data()), data.size());
            auto ans = symusic::parse<symusic::DataFormat::ZPP, TrackNative<T>>(span);
            new (&self) self_t(std::make_shared<Track<T>>(std::move(to_shared(std::move(ans)))));
        }, track_docstrings::kSetStateDoc, nb::sig(fmt::format("def __setstate__(self, state: bytes, /) -> {}", name).c_str()))
        .def_prop_ro("ttype", [](const self_t&) { return T(); }, "Time unit type — Tick/Quarter/Second",
            nb::for_getter(nb::sig(fmt::format("def ttype(self, /) -> symusic.core.{}", docstring_helpers::time_flavor<T>().suffix).c_str())))
        .def("__use_count", [](const self_t& self) { return self.use_count(); }, track_docstrings::kUseCountDoc,
            nb::sig("def __use_count(self, /) -> int"))
        .def_prop_rw(RW_COPY(shared<pyvec<Note<T>>>, "notes", notes), "List of Note events on this track",
            nb::for_getter(nb::sig(fmt::format("def notes(self, /) -> symusic.core.Note{}List", flavor.suffix).c_str())),
            nb::for_setter(nb::sig(fmt::format("def notes(self, value: symusic.core.Note{}List, /) -> None", flavor.suffix).c_str())))
        .def_prop_rw(RW_COPY(shared<pyvec<ControlChange<T>>>, "controls", controls), "List of ControlChange events",
            nb::for_getter(nb::sig(fmt::format("def controls(self, /) -> symusic.core.ControlChange{}List", flavor.suffix).c_str())),
            nb::for_setter(nb::sig(fmt::format("def controls(self, value: symusic.core.ControlChange{}List, /) -> None", flavor.suffix).c_str())))
        .def_prop_rw(RW_COPY(shared<pyvec<Pedal<T>>>, "pedals", pedals), "List of Pedal events",
            nb::for_getter(nb::sig(fmt::format("def pedals(self, /) -> symusic.core.Pedal{}List", flavor.suffix).c_str())),
            nb::for_setter(nb::sig(fmt::format("def pedals(self, value: symusic.core.Pedal{}List, /) -> None", flavor.suffix).c_str())))
        .def_prop_rw(RW_COPY(shared<pyvec<PitchBend<T>>>, "pitch_bends", pitch_bends), "List of PitchBend events",
            nb::for_getter(nb::sig(fmt::format("def pitch_bends(self, /) -> symusic.core.PitchBend{}List", flavor.suffix).c_str())),
            nb::for_setter(nb::sig(fmt::format("def pitch_bends(self, value: symusic.core.PitchBend{}List, /) -> None", flavor.suffix).c_str())))
        .def_prop_rw(RW_COPY(shared<pyvec<TextMeta<T>>>, "lyrics", lyrics), "List of TextMeta events (lyrics/markers)",
            nb::for_getter(nb::sig(fmt::format("def lyrics(self, /) -> symusic.core.TextMeta{}List", flavor.suffix).c_str())),
            nb::for_setter(nb::sig(fmt::format("def lyrics(self, value: symusic.core.TextMeta{}List, /) -> None", flavor.suffix).c_str())))
        .def_prop_rw(RW_COPY(bool, "is_drum", is_drum), "Flag drum/percussion channel (GM channel 10)",
            nb::for_getter(nb::sig("def is_drum(self, /) -> bool")),
            nb::for_setter(nb::sig("def is_drum(self, value: bool, /) -> None")))
        .def_prop_rw(RW_COPY(u8, "program", program), "MIDI program number (0–127)",
            nb::for_getter(nb::sig("def program(self, /) -> int")),
            nb::for_setter(nb::sig("def program(self, value: int, /) -> None")))
        .def_prop_rw(RW_COPY(std::string, "name", name), "Human-friendly track name",
            nb::for_getter(nb::sig("def name(self, /) -> str")),
            nb::for_setter(nb::sig("def name(self, value: str, /) -> None")))
        .def("__eq__", [](const self_t& self, const self_t& other) { return self == other || *self == *other; }, track_docstrings::kEqDoc,
            nb::sig(fmt::format("def __eq__(self, other: {}, /) -> bool", name).c_str()))
        .def("__eq__", [](const self_t&, nb::handle) { return false; }, track_docstrings::kEqDoc,
            nb::sig("def __eq__(self, other: object, /) -> bool"))
        .def("__ne__", [](const self_t& self, const self_t& other) { return self != other && *self != *other; }, track_docstrings::kEqDoc,
            nb::sig(fmt::format("def __ne__(self, other: {}, /) -> bool", name).c_str()))
        .def("__ne__", [](const self_t&, nb::object) { return true; }, track_docstrings::kEqDoc,
            nb::sig("def __ne__(self, other: object, /) -> bool"))
        .def("end", [](const self_t& self) { return self->end(); }, track_docstrings::kEndDoc,
            nb::sig(fmt::format("def end(self, /) -> {}", flavor.scalar_label).c_str()))
        .def("start", [](const self_t& self) { return self->start(); }, track_docstrings::kStartDoc,
            nb::sig(fmt::format("def start(self, /) -> {}", flavor.scalar_label).c_str()))
        .def("note_num", [](const self_t& self) { return self->note_num(); }, track_docstrings::kNoteNumDoc,
            nb::sig("def note_num(self, /) -> int"))
        .def("empty", [](const self_t& self) { return self->empty(); }, track_docstrings::kEmptyDoc,
            nb::sig("def empty(self, /) -> bool"))
        .def("clip", [](self_t& self, const unit start, const unit end, const bool clip_end, const bool inplace) {
            self_t ans = inplace ? self : std::make_shared<track_t>(std::move(self->deepcopy()));
            ans->clip_inplace(start, end, clip_end);
            return ans;
        }, nb::arg("start"), nb::arg("end"), nb::arg("clip_end") = false, nb::arg("inplace") = false,
            track_docstrings::kClipDoc,
            nb::sig(fmt::format("def clip(self, start: {}, end: {}, clip_end: bool = False, inplace: bool = False) -> {}", flavor.scalar_label, flavor.scalar_label, name).c_str()))
        .def("trim", [](self_t& self, const unit start, const unit end, const unit min_overlap, const std::string &start_mode, const std::string &end_mode, const bool inplace) {
            self_t ans = inplace ? self : std::make_shared<track_t>(std::move(self->deepcopy()));
            ans->trim_inplace(start, end, min_overlap, start_mode, end_mode);
            return ans;
        }, nb::arg("start"), nb::arg("end"), nb::arg("min_overlap") = 0, nb::arg("start_mode") = "remove", nb::arg("end_mode") = "remove", nb::arg("inplace") = false,
            track_docstrings::kTrimDoc,
            nb::sig(fmt::format("def trim(self, start: {}, end: {}, min_overlap: {} = 0, start_mode: str = 'remove', end_mode: str = 'remove', inplace: bool = False) -> {}", flavor.scalar_label, flavor.scalar_label, flavor.scalar_label, name).c_str()))
        .def("sort", [](self_t& self, const bool reverse, const bool inplace) {
            self_t ans = inplace ? self : std::make_shared<track_t>(std::move(self->deepcopy()));
            ans->sort_inplace(reverse);
            return ans;
        }, nb::arg("reverse") = false, nb::arg("inplace") = true, track_docstrings::kSortDoc,
            nb::sig(fmt::format("def sort(self, reverse: bool = False, inplace: bool = True) -> {}", name).c_str()))
        .def("adjust_time", [](self_t& self, const vec<unit>& original_times, const vec<unit>& new_times, const bool inplace) {
            self_t ans = inplace ? self : std::make_shared<track_t>(std::move(self->deepcopy()));
            ops::adjust_time_inplace(*ans, original_times, new_times);
            return ans;
        }, nb::arg("original_times"), nb::arg("new_times"), nb::arg("inplace") = false, track_docstrings::kAdjustTimeDoc,
            nb::sig(fmt::format("def adjust_time(self, original_times: typing.Iterable[{}], new_times: typing.Iterable[{}], inplace: bool = False) -> {}", flavor.scalar_label, flavor.scalar_label, name).c_str()))
        .def("shift_time", [](self_t& self, const unit offset, const bool inplace) {
            self_t ans = inplace ? self : std::make_shared<track_t>(std::move(self->deepcopy()));
            ans->shift_time_inplace(offset);
            return ans;
        }, nb::arg("offset"), nb::arg("inplace") = false, track_docstrings::kShiftTimeDoc,
            nb::sig(fmt::format("def shift_time(self, offset: {}, inplace: bool = False) -> {}", flavor.scalar_label, name).c_str()))
        .def("shift_pitch", [](self_t& self, const i8 offset, const bool inplace) {
            self_t ans = inplace ? self : std::make_shared<track_t>(std::move(self->deepcopy()));
            ans->shift_pitch_inplace(offset);
            return ans;
        }, nb::arg("offset"), nb::arg("inplace") = false, track_docstrings::kShiftPitchDoc,
            nb::sig(fmt::format("def shift_pitch(self, offset: int, inplace: bool = False) -> {}", name).c_str()))
        .def("shift_velocity", [](self_t& self, const i8 offset, const bool inplace) {
            self_t ans = inplace ? self : std::make_shared<track_t>(std::move(self->deepcopy()));
            ans->shift_velocity_inplace(offset);
            return ans;
        }, nb::arg("offset"), nb::arg("inplace") = false, track_docstrings::kShiftVelocityDoc,
            nb::sig(fmt::format("def shift_velocity(self, offset: int, inplace: bool = False) -> {}", name).c_str()))
    ;

    if constexpr (std::is_same_v<T, Tick>) {
        track.def("pianoroll", [](
            const self_t&           self,
            const vec<std::string>& modes,
            const std::pair<i64, i64> pitch_range,
            const bool              encode_velocity) {
                vec<PianorollMode> mode_enums(modes.size());
                for (size_t i = 0; i < modes.size(); ++i) {
                    mode_enums[i] = str_to_pianoroll_mode(modes[i]);
                }
                auto pianoroll = TrackPianoroll::from_track(*self, mode_enums, validate_pitch_range(pitch_range), encode_velocity);
                auto* data = const_cast<u8*>(pianoroll.release());
                nb::capsule owner(data, [](void* d) noexcept { delete[] static_cast<u8*>(d); });
                return nb::ndarray<nb::numpy, pianoroll_t>{
                    data, {
                        std::get<0>(pianoroll.dims()),
                        std::get<1>(pianoroll.dims()),
                        std::get<2>(pianoroll.dims()),
                    }, owner
                };
            },
            nb::arg("modes")           = vec<std::string>{"frame", "onset"},
            nb::arg("pitch_range")     = std::pair<i64, i64>(0, 128),
            nb::arg("encode_velocity") = false,
            track_docstrings::kPianorollDoc,
            nb::sig("def pianoroll(self, modes: typing.Iterable[str] = ('frame','onset'), pitch_range: typing.Tuple[int,int] = (0, 128), encode_velocity: bool = False) -> numpy.ndarray"));
    }

    const auto list_name = name + "List";
    const auto vec_sig = fmt::format("class {}(typing.List[symusic.core.{}])", list_name, name);
    auto track_vec = pyutils::bind_shared_vector_copy<vec<self_t>>(m, (name + "List").c_str(), nb::sig(vec_sig.c_str()))
        .def_prop_ro("ttype", [](const vec_t&) { return T(); },
            nb::for_getter(nb::sig(fmt::format("def ttype(self, /) -> symusic.core.{}", docstring_helpers::time_flavor<T>().suffix).c_str())))
        .def("filter", [](const vec_t& self, const nb::object & func, const bool inplace) {
            auto ans = inplace ? self : std::make_shared<vec<self_t>>(self->begin(), self->end());
            auto it = std::remove_if(ans->begin(), ans->end(), [&](const self_t& t) {
                return !nb::cast<bool>(nb::cast<nb::callable>(func)(t));
            });
            ans->erase(it, ans->end());
            return ans;
        }, nb::arg("function"), nb::arg("inplace") = true, track_docstrings::kVecFilterDoc,
           nb::sig(fmt::format("def filter(self, function: typing.Callable, inplace: bool = True) -> {}", list_name).c_str()))
        .def("sort", [](vec_t& self, const nb::object& key, const bool reverse,  const bool inplace) {
            auto ans = inplace ? self : std::make_shared<vec<self_t>>(self->begin(), self->end());
            if(key.is_none()) {
                auto cmp = [](const self_t& a, const self_t& b) { return a->default_key() < b->default_key(); };
                if (reverse) gfx::timsort(ans->rbegin(), ans->rend(), cmp);
                else gfx::timsort(ans->begin(), ans->end(), cmp);
            } else {
                auto key_ = nb::cast<nb::callable>(key);
                auto cmp = [&](const self_t& a, const self_t& b) { return key_(a) < key_(b); };
                if (reverse) gfx::timsort(ans->rbegin(), ans->rend(), cmp);
                else gfx::timsort(ans->begin(), ans->end(), cmp);
            }   return ans;
        }, nb::arg("key") = nb::none(), nb::arg("reverse") = false, nb::arg("inplace") = true,
           nb::sig(fmt::format("def sort(self, key: typing.Optional[typing.Callable] = None, reverse: bool = False, inplace: bool = True) -> {}", list_name).c_str()))
        .def("is_sorted", [](const vec_t& self, const nb::object& key, const bool reverse) {
            if(key.is_none()) {
                auto cmp = [](const self_t& a, const self_t& b) { return a->default_key() < b->default_key(); };
                if (reverse) return std::is_sorted(self->rbegin(), self->rend(), cmp);
                else return std::is_sorted(self->begin(), self->end(), cmp);
            } else {
                auto key_ = nb::cast<nb::callable>(key);
                auto cmp = [&](const self_t& a, const self_t& b) { return key_(a) < key_(b); };
                if (reverse) return std::is_sorted(self->rbegin(), self->rend(), cmp);
                else return std::is_sorted(self->begin(), self->end(), cmp);
            }
        }, nb::arg("key") = nb::none(), nb::arg("reverse") = false, track_docstrings::kVecIsSortedDoc,
           nb::sig("def is_sorted(self, key: typing.Optional[typing.Callable] = None, reverse: bool = False, /) -> bool"))
        .def("adjust_time", [](vec_t& self, const vec<unit>& original_times, const vec<unit>& new_times, const bool inplace) {
            auto ans = inplace ? self : clone_track_vector(self);
            for (auto& t : *ans) ops::adjust_time_inplace(*t, original_times, new_times);
            return ans;
        }, nb::arg("original_times"), nb::arg("new_times"), nb::arg("inplace") = false, track_docstrings::kVecAdjustDoc,
           nb::sig(fmt::format("def adjust_time(self, original_times: typing.Iterable[{}], new_times: typing.Iterable[{}], inplace: bool = False) -> {}", flavor.scalar_label, flavor.scalar_label, list_name).c_str()))
        .def("copy",          [&](const vec_t& self, const bool deep) {
            return deep?clone_track_vector(self):std::make_shared<vec<self_t>>(self->begin(), self->end());
        }, nb::arg("deep") = true, nb::rv_policy::copy, track_docstrings::kVecCopyDoc,
           nb::sig(fmt::format("def copy(self, deep: bool = True, /) -> {}", list_name).c_str()))
        .def("__copy__",      [](const vec_t& self) { return std::make_shared<vec<self_t>>(self->begin(), self->end()); },
            track_docstrings::kVecCopyMethodDoc,
            nb::sig(fmt::format("def __copy__(self, /) -> {}", list_name).c_str()))
        .def("__deepcopy__",  [](const vec_t& self, const nb::handle, const nb::handle) { return clone_track_vector(self); },
            nb::arg("memo")=nb::none(), nb::arg("_nil")=nb::none(), nb::rv_policy::move, track_docstrings::kVecDeepCopyDoc,
            nb::sig(fmt::format("def __deepcopy__(self, memo: object, _nil: object, /) -> {}", list_name).c_str()))
        .def("__getstate__",  [](const vec_t& self) {
            vec<TrackNative<T>> native;
            native.reserve(self->size());
            for (const auto& item : *self) { native.emplace_back(std::move(to_native(*item))); }
            const vec<unsigned char> data = dumps<DataFormat::ZPP>(native);
            return nb::bytes(reinterpret_cast<const char*>(data.data()), data.size());
        }, track_docstrings::kVecGetStateDoc, nb::sig("def __getstate__(self, /) -> bytes"))
        .def("__setstate__",  [](vec_t& self, const nb::bytes& bytes) {
            const auto      data = std::string_view(bytes.c_str(), bytes.size());
            const std::span span(reinterpret_cast<const unsigned char*>(data.data()), data.size());
            auto ans = symusic::parse<symusic::DataFormat::ZPP, vec<TrackNative<T>>>(span);
            auto tracks = std::make_shared<vec<self_t>>();
            tracks->reserve(ans.size());
            for (auto& item : ans) { tracks->emplace_back(std::make_shared<Track<T>>(std::move(to_shared(std::move(item))))); }
            new (&self) vec_t(std::move(tracks));
        }, track_docstrings::kVecSetStateDoc, nb::sig(fmt::format("def __setstate__(self, state: bytes, /) -> {}", list_name).c_str()))
    ;
    // clang-format on

    return std::make_tuple(track, track_vec);
}

}   // namespace symusic

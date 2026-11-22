//
// Track bindings and related container helpers.
//

#pragma once

#include "utils/storage.h"
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

namespace symusic::py {

namespace nb = nanobind;

template<TType T>
const char* doc() {
    static const std::string rendered = [] {
        constexpr auto flavor = time_flavor<T>();
        return fmt::format(
            "A part: notes, control changes, pitch bends, pedals and lyrics, with a name, a "
            "General "
            "MIDI program and a drum flag.\n\nTimes are in {} ({}).",
            flavor.units,
            flavor.scalar_label
        );
    }();
    return rendered.c_str();
}

constexpr const char* kTrackCopyCtorDoc   = R"pbdoc(Create a deep copy of *other*.)pbdoc";
constexpr const char* kTrackCopyDoc       = R"pbdoc(
Return a deep copy, or with ``deep=False`` a new track that shares the event lists.
)pbdoc";
constexpr const char* kTrackClipDoc       = R"pbdoc(
Keep the events that start in ``[start, end)``; with *clip_end*, notes and pedals must also end by
*end*. Times are not shifted. Returns a deep copy unless *inplace* is true.
)pbdoc";
constexpr const char* kTrackTrimDoc       = R"pbdoc(
Keep notes and pedals that overlap ``[start, end)`` by at least *min_overlap*. At each boundary,
``"remove"`` drops crossing events, ``"truncate"`` cuts them at the boundary, and ``"retain"``
keeps them whole. Other events are clipped by time. Invalid modes and negative *min_overlap* raise ``ValueError``. Returns a
deep copy unless *inplace* is true.
)pbdoc";
constexpr const char* kTrackSortDoc       = R"pbdoc(
Sort every event list by its default sort key. Works in place unless *inplace* is false, which
returns a deep copy.
)pbdoc";
constexpr const char* kTrackAdjustTimeDoc = R"pbdoc(
Map times piecewise linearly through the anchor pairs ``(original_times[i], new_times[i])``. The
lists must be the same length and have at least two anchors. Original times must be strictly
increasing, and new times must be nondecreasing. Events outside the anchor range are dropped.
Returns a deep copy unless *inplace* is true.
)pbdoc";
constexpr const char* kTrackShiftTimeDoc
    = R"pbdoc(Add *offset* to every time. Returns a deep copy unless *inplace* is true.)pbdoc";
constexpr const char* kTrackShiftPitchDoc    = R"pbdoc(
Add *offset* to every note's pitch. Returns a deep copy unless *inplace* is true. Raises
``ValueError`` when a result leaves -128..127.
)pbdoc";
constexpr const char* kTrackShiftVelocityDoc = R"pbdoc(
Add *offset* to every note's velocity. Returns a deep copy unless *inplace* is true. Raises
``ValueError`` when a result leaves -128..127.
)pbdoc";
constexpr const char* kTrackStartDoc
    = R"pbdoc(Return the earliest event time, or 0 when the track has no events.)pbdoc";
constexpr const char* kTrackEndDoc
    = R"pbdoc(Return the latest event end, or 0 when the track has no events.)pbdoc";
constexpr const char* kTrackNoteNumDoc   = R"pbdoc(Return the number of notes.)pbdoc";
constexpr const char* kTrackEmptyDoc     = R"pbdoc(Return whether the track has no events.)pbdoc";
constexpr const char* kTrackPianorollDoc = R"pbdoc(
Return a ``uint8`` array of shape ``(modes, pitches, time)`` with one column per tick. *modes*
lists ``"frame"``, ``"onset"`` or ``"offset"``; *pitch_range* is the pitch interval ``[low,
high)``; *encode_velocity* writes velocities instead of 1. Raises ``ValueError`` for a note with
negative time or duration, or when the track ends before tick 0.
)pbdoc";
constexpr const char* kTrackUseCountDoc
    = R"pbdoc(Return the number of references to the track, for debugging.)pbdoc";

constexpr const char* kTrackVecFilterDoc = R"pbdoc(
Keep the tracks for which *function* returns true. Works in place unless *inplace* is false; the
returned list shares the tracks.
)pbdoc";
constexpr const char* kTrackVecSortDoc   = R"pbdoc(
Sort by *key*, or by ``(is_drum, program, name, number of notes)``. Works in place unless *inplace*
is false; the returned list shares the tracks.
)pbdoc";
constexpr const char* kTrackVecIsSortedDoc
    = R"pbdoc(Return whether the list is sorted by *key*, or by the default sort key.)pbdoc";
constexpr const char* kTrackVecAdjustDoc = R"pbdoc(
Apply ``adjust_time`` to every track. Returns deep copies of the tracks unless *inplace* is true.
)pbdoc";
constexpr const char* kTrackVecCopyDoc   = R"pbdoc(
Return a list of deep copies of the tracks, or with ``deep=False`` a list that shares them.
)pbdoc";

/// Return a list of deep copies of the tracks.
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

    const auto     name      = "Track" + name_;
    const auto     track_sig = fmt::format("class {}(Track[{}])", name, name_);
    constexpr auto flavor    = time_flavor<T>();

    auto copy_func = [](const self_t& self) { return std::make_shared<track_t>(*self); };
    auto deepcopy_func
        = [](const self_t& self) { return std::make_shared<track_t>(std::move(self->deepcopy())); };

    // clang-format off
    auto track = nb::class_<shared<Track<T>>>(m, name.c_str(), nb::sig(track_sig.c_str()), doc<T>())
        .def("__init__", &pyinit<Track<T>>,
             nb::sig("def __init__(self, /) -> None"))
        .def("__init__", &pyinit<Track<T>, std::string, u8, const bool>,
             nb::arg("name"), nb::arg("program") = 0, nb::arg("is_drum") = false,
             nb::sig("def __init__(self, name: str = '', program: int = 0, is_drum: bool = False, /) -> None"))
        .def("__init__", [](self_t *self, const self_t& other) {
            new (self) std::shared_ptr<track_t>(std::make_shared<track_t>(other->deepcopy()));
        }, nb::arg("other"), kTrackCopyCtorDoc,
           nb::sig(fmt::format("def __init__(self, other: {}, /) -> None", name).c_str()))
        .def("copy", [&](const self_t &self, const bool deep) {
            if (deep) return deepcopy_func(self);
            return copy_func(self);
        }, nb::arg("deep") = true, nb::rv_policy::copy, kTrackCopyDoc,
           nb::sig(fmt::format("def copy(self, deep: bool = True, /) -> {}", name).c_str()))
        .def("__copy__", copy_func, nb::rv_policy::copy,
           nb::sig(fmt::format("def __copy__(self, /) -> {}", name).c_str()))
        .def("__deepcopy__", [&](const self_t& self, const nb::handle, const nb::handle) {
            return deepcopy_func(self);
        }, nb::arg("memo")=nb::none(), nb::arg("_nil")=nb::none(), nb::rv_policy::copy,
           nb::sig(fmt::format("def __deepcopy__(self, memo: object, _nil: object, /) -> {}", name).c_str()))
        .def("__repr__", [](const self_t& self) { return self->to_string(); },
           nb::sig("def __repr__(self, /) -> str"))
        .def("__getstate__", [](const self_t& self) {
            const vec<u8> data = dumps<DataFormat::ZPP>(*self);
            return nb::bytes(reinterpret_cast<const char*>(data.data()), data.size());
        }, nb::sig("def __getstate__(self, /) -> bytes"))
        .def("__setstate__", [](self_t& self, const nb::bytes& bytes) {
            const auto      data = std::string_view(bytes.c_str(), bytes.size());
            const std::span span(reinterpret_cast<const unsigned char*>(data.data()), data.size());
            auto ans = symusic::parse<symusic::DataFormat::ZPP, Track<T>>(span);
            new (&self) self_t(std::make_shared<Track<T>>(std::move(ans)));
        }, nb::sig(fmt::format("def __setstate__(self, state: bytes, /) -> {}", name).c_str()))
        .def_prop_ro("ttype", [](const self_t&) { return T(); }, "The time unit.",
            nb::for_getter(nb::sig(fmt::format("def ttype(self, /) -> symusic.core.{}", time_flavor<T>().suffix).c_str())))
        .def("__use_count", [](const self_t& self) { return self.use_count(); }, kTrackUseCountDoc,
            nb::sig("def __use_count(self, /) -> int"))
        .def_prop_rw(RW_COPY(shared<pyvec<Note<T>>>, "notes", notes), "The ``Note`` events.",
            nb::for_getter(nb::sig(fmt::format("def notes(self, /) -> symusic.core.Note{}List", flavor.suffix).c_str())),
            nb::for_setter(nb::sig(fmt::format("def notes(self, value: symusic.core.Note{}List, /) -> None", flavor.suffix).c_str())))
        .def_prop_rw(RW_COPY(shared<pyvec<ControlChange<T>>>, "controls", controls), "The ``ControlChange`` events.",
            nb::for_getter(nb::sig(fmt::format("def controls(self, /) -> symusic.core.ControlChange{}List", flavor.suffix).c_str())),
            nb::for_setter(nb::sig(fmt::format("def controls(self, value: symusic.core.ControlChange{}List, /) -> None", flavor.suffix).c_str())))
        .def_prop_rw(RW_COPY(shared<pyvec<Pedal<T>>>, "pedals", pedals), "The ``Pedal`` events, derived from control change 64 when reading MIDI.",
            nb::for_getter(nb::sig(fmt::format("def pedals(self, /) -> symusic.core.Pedal{}List", flavor.suffix).c_str())),
            nb::for_setter(nb::sig(fmt::format("def pedals(self, value: symusic.core.Pedal{}List, /) -> None", flavor.suffix).c_str())))
        .def_prop_rw(RW_COPY(shared<pyvec<PitchBend<T>>>, "pitch_bends", pitch_bends), "The ``PitchBend`` events.",
            nb::for_getter(nb::sig(fmt::format("def pitch_bends(self, /) -> symusic.core.PitchBend{}List", flavor.suffix).c_str())),
            nb::for_setter(nb::sig(fmt::format("def pitch_bends(self, value: symusic.core.PitchBend{}List, /) -> None", flavor.suffix).c_str())))
        .def_prop_rw(RW_COPY(shared<pyvec<TextMeta<T>>>, "lyrics", lyrics), "The lyrics, as ``TextMeta`` events.",
            nb::for_getter(nb::sig(fmt::format("def lyrics(self, /) -> symusic.core.TextMeta{}List", flavor.suffix).c_str())),
            nb::for_setter(nb::sig(fmt::format("def lyrics(self, value: symusic.core.TextMeta{}List, /) -> None", flavor.suffix).c_str())))
        .def_prop_rw(RW_COPY(bool, "is_drum", is_drum), "Whether the track plays on the drum channel, MIDI channel 10.",
            nb::for_getter(nb::sig("def is_drum(self, /) -> bool")),
            nb::for_setter(nb::sig("def is_drum(self, value: bool, /) -> None")))
        .def_prop_rw(RW_COPY(u8, "program", program), "General MIDI program, 0-127.",
            nb::for_getter(nb::sig("def program(self, /) -> int")),
            nb::for_setter(nb::sig("def program(self, value: int, /) -> None")))
        .def_prop_rw(RW_COPY(std::string, "name", name), "Track name.",
            nb::for_getter(nb::sig("def name(self, /) -> str")),
            nb::for_setter(nb::sig("def name(self, value: str, /) -> None")))
        .def("__eq__", [](const self_t& self, const self_t& other) { return self == other || *self == *other; },
            nb::sig(fmt::format("def __eq__(self, other: {}, /) -> bool", name).c_str()))
        .def("__eq__", [](const self_t&, nb::handle) { return false; },
            nb::sig("def __eq__(self, other: object, /) -> bool"))
        .def("__ne__", [](const self_t& self, const self_t& other) { return self != other && *self != *other; },
            nb::sig(fmt::format("def __ne__(self, other: {}, /) -> bool", name).c_str()))
        .def("__ne__", [](const self_t&, nb::object) { return true; },
            nb::sig("def __ne__(self, other: object, /) -> bool"))
        .def("end", [](const self_t& self) { return self->end(); }, kTrackEndDoc,
            nb::sig(fmt::format("def end(self, /) -> {}", flavor.scalar_label).c_str()))
        .def("start", [](const self_t& self) { return self->start(); }, kTrackStartDoc,
            nb::sig(fmt::format("def start(self, /) -> {}", flavor.scalar_label).c_str()))
        .def("note_num", [](const self_t& self) { return self->note_num(); }, kTrackNoteNumDoc,
            nb::sig("def note_num(self, /) -> int"))
        .def("empty", [](const self_t& self) { return self->empty(); }, kTrackEmptyDoc,
            nb::sig("def empty(self, /) -> bool"))
        .def("clip", [](self_t& self, const unit start, const unit end, const bool clip_end, const bool inplace) {
            self_t ans = inplace ? self : std::make_shared<track_t>(std::move(self->deepcopy()));
            ans->clip_inplace(start, end, clip_end);
            return ans;
        }, nb::arg("start"), nb::arg("end"), nb::arg("clip_end") = false, nb::arg("inplace") = false,
            kTrackClipDoc,
            nb::sig(fmt::format("def clip(self, start: {}, end: {}, clip_end: bool = False, inplace: bool = False) -> {}", flavor.scalar_label, flavor.scalar_label, name).c_str()))
        .def("trim", [](self_t& self, const unit start, const unit end, const unit min_overlap, const std::string &start_mode, const std::string &end_mode, const bool inplace) {
            self_t ans = inplace ? self : std::make_shared<track_t>(std::move(self->deepcopy()));
            ans->trim_inplace(start, end, min_overlap, start_mode, end_mode);
            return ans;
        }, nb::arg("start"), nb::arg("end"), nb::arg("min_overlap") = 0, nb::arg("start_mode") = "remove", nb::arg("end_mode") = "remove", nb::arg("inplace") = false,
            kTrackTrimDoc,
            nb::sig(fmt::format("def trim(self, start: {}, end: {}, min_overlap: {} = 0, start_mode: typing.Literal['remove', 'truncate', 'retain'] = 'remove', end_mode: typing.Literal['remove', 'truncate', 'retain'] = 'remove', inplace: bool = False) -> {}", flavor.scalar_label, flavor.scalar_label, flavor.scalar_label, name).c_str()))
        .def("sort", [](self_t& self, const bool reverse, const bool inplace) {
            self_t ans = inplace ? self : std::make_shared<track_t>(std::move(self->deepcopy()));
            ans->sort_inplace(reverse);
            return ans;
        }, nb::arg("reverse") = false, nb::arg("inplace") = true, kTrackSortDoc,
            nb::sig(fmt::format("def sort(self, reverse: bool = False, inplace: bool = True) -> {}", name).c_str()))
        .def("adjust_time", [](self_t& self, const vec<unit>& original_times, const vec<unit>& new_times, const bool inplace) {
            self_t ans = inplace ? self : std::make_shared<track_t>(std::move(self->deepcopy()));
            ops::adjust_time_inplace(*ans, original_times, new_times);
            return ans;
        }, nb::arg("original_times"), nb::arg("new_times"), nb::arg("inplace") = false, kTrackAdjustTimeDoc,
            nb::sig(fmt::format("def adjust_time(self, original_times: typing.Iterable[{}], new_times: typing.Iterable[{}], inplace: bool = False) -> {}", flavor.scalar_label, flavor.scalar_label, name).c_str()))
        .def("shift_time", [](self_t& self, const unit offset, const bool inplace) {
            self_t ans = inplace ? self : std::make_shared<track_t>(std::move(self->deepcopy()));
            ans->shift_time_inplace(offset);
            return ans;
        }, nb::arg("offset"), nb::arg("inplace") = false, kTrackShiftTimeDoc,
            nb::sig(fmt::format("def shift_time(self, offset: {}, inplace: bool = False) -> {}", flavor.scalar_label, name).c_str()))
        .def("shift_pitch", [](self_t& self, const i8 offset, const bool inplace) {
            self_t ans = inplace ? self : std::make_shared<track_t>(std::move(self->deepcopy()));
            ans->shift_pitch_inplace(offset);
            return ans;
        }, nb::arg("offset"), nb::arg("inplace") = false, kTrackShiftPitchDoc,
            nb::sig(fmt::format("def shift_pitch(self, offset: int, inplace: bool = False) -> {}", name).c_str()))
        .def("shift_velocity", [](self_t& self, const i8 offset, const bool inplace) {
            self_t ans = inplace ? self : std::make_shared<track_t>(std::move(self->deepcopy()));
            ans->shift_velocity_inplace(offset);
            return ans;
        }, nb::arg("offset"), nb::arg("inplace") = false, kTrackShiftVelocityDoc,
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
            kTrackPianorollDoc,
            nb::sig("def pianoroll(self, modes: typing.Iterable[str] = ('frame','onset'), pitch_range: typing.Tuple[int,int] = (0, 128), encode_velocity: bool = False) -> numpy.ndarray"));
    }

    const auto list_name = name + "List";
    const auto vec_sig = fmt::format("class {}(typing.List[symusic.core.{}])", list_name, name);
    auto track_vec = bind_shared_vector_copy<vec<self_t>>(m, (name + "List").c_str(), nb::sig(vec_sig.c_str()))
        .def_prop_ro("ttype", [](const vec_t&) { return T(); }, "The time unit.",
            nb::for_getter(nb::sig(fmt::format("def ttype(self, /) -> symusic.core.{}", time_flavor<T>().suffix).c_str())))
        .def("filter", [](const vec_t& self, const nb::object & func, const bool inplace) {
            auto ans = inplace ? self : std::make_shared<vec<self_t>>(self->begin(), self->end());
            auto it = std::remove_if(ans->begin(), ans->end(), [&](const self_t& t) {
                return !nb::cast<bool>(nb::cast<nb::callable>(func)(t));
            });
            ans->erase(it, ans->end());
            return ans;
        }, nb::arg("function"), nb::arg("inplace") = true, kTrackVecFilterDoc,
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
        }, nb::arg("key") = nb::none(), nb::arg("reverse") = false, nb::arg("inplace") = true, kTrackVecSortDoc,
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
        }, nb::arg("key") = nb::none(), nb::arg("reverse") = false, kTrackVecIsSortedDoc,
           nb::sig("def is_sorted(self, key: typing.Optional[typing.Callable] = None, reverse: bool = False, /) -> bool"))
        .def("adjust_time", [](vec_t& self, const vec<unit>& original_times, const vec<unit>& new_times, const bool inplace) {
            auto ans = inplace ? self : clone_track_vector(self);
            ops::details::adjust_track_times(*ans, original_times, new_times);
            return ans;
        }, nb::arg("original_times"), nb::arg("new_times"), nb::arg("inplace") = false, kTrackVecAdjustDoc,
           nb::sig(fmt::format("def adjust_time(self, original_times: typing.Iterable[{}], new_times: typing.Iterable[{}], inplace: bool = False) -> {}", flavor.scalar_label, flavor.scalar_label, list_name).c_str()))
        .def("copy",          [&](const vec_t& self, const bool deep) {
            return deep?clone_track_vector(self):std::make_shared<vec<self_t>>(self->begin(), self->end());
        }, nb::arg("deep") = true, nb::rv_policy::copy, kTrackVecCopyDoc,
           nb::sig(fmt::format("def copy(self, deep: bool = True, /) -> {}", list_name).c_str()))
        .def("__copy__",      [](const vec_t& self) { return std::make_shared<vec<self_t>>(self->begin(), self->end()); },
            nb::sig(fmt::format("def __copy__(self, /) -> {}", list_name).c_str()))
        .def("__deepcopy__",  [](const vec_t& self, const nb::handle, const nb::handle) { return clone_track_vector(self); },
            nb::arg("memo")=nb::none(), nb::arg("_nil")=nb::none(), nb::rv_policy::move,
            nb::sig(fmt::format("def __deepcopy__(self, memo: object, _nil: object, /) -> {}", list_name).c_str()))
        .def("__getstate__",  [](const vec_t& self) {
            const auto data = detail::dump_track_list<T, SharedStorage>(self);
            return nb::bytes(reinterpret_cast<const char*>(data.data()), data.size());
        }, nb::sig("def __getstate__(self, /) -> bytes"))
        .def("__setstate__",  [](vec_t& self, const nb::bytes& bytes) {
            const auto      data = std::string_view(bytes.c_str(), bytes.size());
            const std::span span(reinterpret_cast<const unsigned char*>(data.data()), data.size());
            auto ans = symusic::parse<symusic::DataFormat::ZPP, vec<symusic::Track<T>>>(span);
            auto tracks = std::make_shared<vec<self_t>>();
            tracks->reserve(ans.size());
            for (auto& item : ans) { tracks->emplace_back(std::make_shared<Track<T>>(detail::adopt_storage<SharedStorage>(std::move(item)))); }
            new (&self) vec_t(std::move(tracks));
        }, nb::sig(fmt::format("def __setstate__(self, state: bytes, /) -> {}", list_name).c_str()))
    ;
    // clang-format on

    return std::make_tuple(track, track_vec);
}

}   // namespace symusic::py

//
// Bindings for the time based events (note, pedal, tempo, ...).
//

#pragma once

#include "utils/storage.h"
#include <optional>
#include <string>
#include <tuple>

#include "../../utils/python_helpers.h"
#include "../../utils/vector_bindings.h"
#include <fmt/format.h>
#include "../docstring_helpers.h"

namespace symusic::py {

namespace nb = nanobind;

// Class docstring: the description followed by the unit of the time fields. Rendered once per
// event type and unit and kept alive for the lifetime of the module.
template<TType T>
std::string with_units(std::string_view description, std::string_view time_fields) {
    constexpr auto flavor = time_flavor<T>();
    return fmt::format(
        "{}\n\n{} in {} ({}).", description, time_fields, flavor.units, flavor.scalar_label
    );
}

template<TType T>
const char* note_doc() {
    static const std::string doc = with_units<T>(
        "A note. ``pitch`` and ``velocity`` are stored in 8 bits (-128..127); symusic does not "
        "enforce the MIDI range 0-127.",
        "``time`` and ``duration`` are"
    );
    return doc.c_str();
}

template<TType T>
const char* key_signature_doc() {
    static const std::string doc = with_units<T>(
        "A key signature change. ``key`` counts sharps (positive) or flats (negative); "
        "``tonality`` is 0 for major and 1 for minor.",
        "``time`` is"
    );
    return doc.c_str();
}

template<TType T>
const char* time_signature_doc() {
    static const std::string doc = with_units<T>(
        "A time signature change, such as ``numerator=6, denominator=8``.", "``time`` is"
    );
    return doc.c_str();
}

template<TType T>
const char* control_change_doc() {
    static const std::string doc = with_units<T>(
        "A MIDI control change: controller ``number`` and ``value``, both 0-127. Control change "
        "64 is the sustain pedal.",
        "``time`` is"
    );
    return doc.c_str();
}

template<TType T>
const char* pedal_doc() {
    static const std::string doc = with_units<T>(
        "A sustain pedal region. Reading MIDI derives pedals from control change 64; writing MIDI "
        "ignores them and writes ``controls`` instead.",
        "``time`` and ``duration`` are"
    );
    return doc.c_str();
}

template<TType T>
const char* tempo_doc() {
    static const std::string doc = with_units<T>(
        "A tempo change. It stores microseconds per quarter note in ``mspq``; ``qpm`` and its "
        "alias ``tempo`` give quarter notes per minute.",
        "``time`` is"
    );
    return doc.c_str();
}

template<TType T>
const char* pitch_bend_doc() {
    static const std::string doc = with_units<T>(
        "A pitch bend. MIDI files hold ``value`` in -8192..8191; 0 is no bend.", "``time`` is"
    );
    return doc.c_str();
}

template<TType T>
const char* text_meta_doc() {
    static const std::string doc = with_units<T>(
        "A text event: a lyric in ``Track.lyrics`` or a marker in ``Score.markers``.", "``time`` is"
    );
    return doc.c_str();
}

constexpr const char* kEventShiftTimeDoc
    = R"pbdoc(Add *offset* to ``time``. Returns a new event unless *inplace* is true.)pbdoc";
constexpr const char* kEventCopyCtorDoc = R"pbdoc(Create a copy of *other*.)pbdoc";
constexpr const char* kEventCopyDoc
    = R"pbdoc(Return a copy of the event. *deep* has no effect, because events share no data.)pbdoc";
constexpr const char* kEventUseCountDoc
    = R"pbdoc(Return the number of references to the event, for debugging.)pbdoc";

constexpr const char* kEventVecSortDoc   = R"pbdoc(
Sort by *key*, or by the default sort key of the event type. Works in place unless *inplace* is
false; the returned copy shares the events.
)pbdoc";
constexpr const char* kEventVecFilterDoc = R"pbdoc(
Keep the events for which *function* returns true. Works in place unless *inplace* is false; the
returned copy shares the events.
)pbdoc";
constexpr const char* kEventVecIsSortedDoc
    = R"pbdoc(Return whether the list is sorted by *key*, or by the default sort key.)pbdoc";
constexpr const char* kEventVecAdjustTimeDoc = R"pbdoc(
Map times piecewise linearly through the anchor pairs ``(original_times[i], new_times[i])``. The
lists must be the same length and have at least two anchors. Original times must be strictly
increasing, and new times must be nondecreasing. Events outside the anchor range are dropped.
Returns a list of new events unless *inplace* is true.
)pbdoc";
constexpr const char* kEventVecNumpyDoc
    = R"pbdoc(Return a dict with one NumPy array per field. The arrays are copies.)pbdoc";
constexpr const char* kEventVecFromNumpyDoc
    = R"pbdoc(Build a list from one NumPy array per field; the arrays must have equal length.)pbdoc";
constexpr const char* kEventTextMetaNumpyDoc
    = R"pbdoc(Not supported for text events; raises ``RuntimeError``.)pbdoc";

constexpr const char* kEventNoteEmptyDoc
    = R"pbdoc(Return whether the duration or the velocity is 0 or less.)pbdoc";
constexpr const char* kEventEndDoc
    = R"pbdoc(``time + duration``. Setting it changes ``duration``.)pbdoc";
constexpr const char* kEventNoteShiftPitchDoc    = R"pbdoc(
Add *offset* to ``pitch``. Returns a new note unless *inplace* is true. Raises ``ValueError`` when
the result leaves -128..127.
)pbdoc";
constexpr const char* kEventNoteShiftVelocityDoc = R"pbdoc(
Add *offset* to ``velocity``. Returns a new note unless *inplace* is true. Raises ``ValueError``
when the result leaves -128..127.
)pbdoc";

constexpr const char* kEventTempoInitDoc = R"pbdoc(
Create a tempo change from *qpm* or *mspq*; *qpm* wins when both are given. Raises ``ValueError``
when neither is given.
)pbdoc";
constexpr const char* kEventTempoQpmDoc  = R"pbdoc(
Quarter notes per minute, computed from ``mspq``. Setting it rounds ``mspq`` to the nearest
microsecond.
)pbdoc";

template<typename T>
auto bind_time_stamp(
    nb::module_&       m,
    const std::string& name,
    const std::string& generic_base,
    const std::string& unit_suffix,
    const char*        class_doc = nullptr
) {
    using unit   = typename T::unit;
    using self_t = shared<T>;
    using vec_t  = shared<pyvec<T>>;

    auto copy_func     = [](const shared<T>& self) { return std::make_shared<T>(*self); };
    auto deepcopy_func = [](const shared<T>& self, const nb::handle&, const nb::handle&) {
        return std::make_shared<T>(*self);
    };

    const auto event_sig = fmt::format("class {}({}[{}])", name, generic_base, unit_suffix);

    auto event = class_doc ?
                     nb::class_<shared<T>>(m, name.c_str(), nb::sig(event_sig.c_str()), class_doc) :
                     nb::class_<shared<T>>(m, name.c_str(), nb::sig(event_sig.c_str()));

    // clang-format off
    constexpr auto flavor = time_flavor<typename T::ttype>();
    event
        .def_prop_rw(
            RW_COPY(unit, "time", time),
            "Time of the event.",
            nb::for_getter(nb::sig(fmt::format("def time(self, /) -> {}", flavor.scalar_label).c_str())),
            nb::for_setter(nb::sig(fmt::format("def time(self, value: {}, /) -> None", flavor.scalar_label).c_str()))
        )
        .def_prop_ro(
            "ttype",
            [](const self_t&) { return typename T::ttype(); },
            "The time unit.",
            nb::for_getter(nb::sig(fmt::format("def ttype(self, /) -> symusic.core.{}", time_flavor<typename T::ttype>().suffix).c_str()))
        )
        .def(
            "shift_time",
            [](const self_t& self, const unit offset, const bool inplace) {
                self_t ans = inplace ? self : std::make_shared<T>(*self);
                symusic::shift_time(*ans, offset);
                return ans;
            },
            nb::rv_policy::copy,
            nb::arg("offset"),
            nb::arg("inplace") = false,
            kEventShiftTimeDoc,
            nb::sig(fmt::format("def shift_time(self, offset: {}, inplace: bool = False) -> {}",
                                flavor.scalar_label,
                                name).c_str())
        )
        .def(
            "__init__",
            [](self_t* self, const self_t& other) { new (self) shared<T>(std::make_shared<T>(*other)); },
            nb::arg("other"),
            kEventCopyCtorDoc,
            nb::sig(fmt::format("def __init__(self, other: {}, /) -> None", name).c_str())
        )
        .def(
            "copy",
            [](const shared<T>& self, const bool deep) { return std::make_shared<T>(*self); },
            nb::arg("deep") = true,
            kEventCopyDoc,
            nb::sig(fmt::format("def copy(self, deep: bool = True, /) -> {}", name).c_str())
        )
        .def("__copy__", copy_func, nb::sig(fmt::format("def __copy__(self, /) -> {}", name).c_str()))
        .def(
            "__deepcopy__",
            deepcopy_func,
            nb::arg("memo") = nb::none(),
            nb::arg("_nil") = nb::none(),
            nb::sig(fmt::format("def __deepcopy__(self, memo: object, _nil: object, /) -> {}", name).c_str())
        )
        .def("__repr__", [](const shared<T>& self) { return self->to_string(); }, nb::sig("def __repr__(self, /) -> str"))
        .def("__eq__", [](const shared<T>& self, const shared<T>& other) { return *self == *other; }, nb::sig(fmt::format("def __eq__(self, other: {}, /) -> bool", name).c_str()))
        .def("__eq__", [](const shared<T>&, const nb::object&) { return false; }, nb::sig("def __eq__(self, other: object, /) -> bool"))
        .def("__ne__", [](const shared<T>& self, const shared<T>& other) { return *self != *other; }, nb::sig(fmt::format("def __ne__(self, other: {}, /) -> bool", name).c_str()))
        .def("__ne__", [](const shared<T>&, const nb::object&) { return true; }, nb::sig("def __ne__(self, other: object, /) -> bool"))
        .def(nb::self != nb::self) // NOLINT
        .def("__getstate__", &to_bytes<T>, nb::sig("def __getstate__(self, /) -> bytes"))
        .def("__setstate__", &from_bytes<T>, nb::sig(fmt::format("def __setstate__(self, state: bytes, /) -> {}", name).c_str()))
        .def("__use_count", [](const shared<T>& self) { return self.use_count(); }, kEventUseCountDoc, nb::sig("def __use_count(self, /) -> int"))
    ;

    const auto list_name = name + "List";
    const auto vec_sig = fmt::format("class {}(PyVec[{}[{}]])", list_name, generic_base, unit_suffix);
    auto vec_class = bind_shared_pyvec<T>(m, list_name.c_str(), nb::sig(vec_sig.c_str()))
        .def_prop_ro("ttype", [](const vec_t&) { return typename T::ttype(); }, "The time unit.")
        .def(
            "__getstate__",
            [](const vec_t& self) { return vec_to_bytes<T>(*self); },
            nb::sig("def __getstate__(self, /) -> bytes")
        )
        .def(
            "__setstate__",
            [](vec_t& self, const nb::bytes& bytes) { vec_from_bytes<T>(self, bytes); },
            nb::sig(fmt::format("def __setstate__(self, state: bytes, /) -> {}", list_name).c_str())
        )
        .def("__repr__", [](const vec_t& self) { return fmt::format("{::s}", *self); })
        .def(
            "sort",
            [](vec_t& v, const nb::object& key, const bool reverse, const bool inplace) -> vec_t {
                vec_t ans = inplace ? v : std::make_shared<pyvec<T>>(v->copy());
                if (key.is_none()) {
                    ans->sort([](const auto& e) { return e.default_key(); }, reverse);
                } else {
                    ans->sort_shared(nb::cast<nb::callable>(key), reverse);
                }
                return ans;
            },
            nb::rv_policy::copy,
            nb::arg("key")      = nb::none(),
            nb::arg("reverse")  = false,
            nb::arg("inplace")  = true,
            kEventVecSortDoc,
            nb::sig(fmt::format("def sort(self, key: typing.Optional[typing.Callable] = None, reverse: bool = False, inplace: bool = True) -> {}", list_name).c_str())
        )
        .def(
            "filter",
            [](vec_t& v, nb::object& func, const bool inplace) -> vec_t {
                if (func.is_none()) {
                    throw std::invalid_argument("symusic::filter expects a callable");
                }
                vec_t ans = inplace ? v : std::make_shared<pyvec<T>>(v->copy());
                ans->filter_shared([func](const shared<T>& e) { return nb::cast<bool>(func(e)); });
                return ans;
            },
            nb::rv_policy::copy,
            nb::arg("function"),
            nb::arg("inplace") = true,
            kEventVecFilterDoc,
            nb::sig(fmt::format("def filter(self, function: typing.Callable, inplace: bool = True) -> {}", list_name).c_str())
        )
        .def(
            "is_sorted",
            [](const vec_t& v, nb::object& key, bool reverse) -> bool {
                if (key.is_none()) {
                    return v->is_sorted([](const auto& e) { return e.default_key(); }, reverse);
                }
                return v->is_sorted_shared(nb::cast<nb::callable>(key), reverse);
            },
            nb::arg("key")     = nb::none(),
            nb::arg("reverse") = false,
            kEventVecIsSortedDoc,
            nb::sig("def is_sorted(self, key: typing.Optional[typing.Callable] = None, reverse: bool = False) -> bool")
        )
        .def(
            "adjust_time",
            [](vec_t& v, const vec<unit>& original_times, const vec<unit>& new_times, const bool inplace) -> vec_t {
                vec_t ans = inplace ? v : std::make_shared<pyvec<T>>(v->deepcopy());
                ops::adjust_time_inplace<true, SharedStorage>(*ans, original_times, new_times);
                return ans;
            },
            nb::rv_policy::copy,
            nb::arg("original_times"),
            nb::arg("new_times"),
            nb::arg("inplace") = false,
            kEventVecAdjustTimeDoc,
            nb::sig(fmt::format("def adjust_time(self, original_times: typing.Iterable[{}], new_times: typing.Iterable[{}], inplace: bool = False) -> {}", flavor.scalar_label, flavor.scalar_label, list_name).c_str())
        )
    ;
    // clang-format on

    nb::implicitly_convertible<nb::list, shared<pyvec<T>>>();
    return std::make_tuple(event, vec_class);
}

template<TimeEvent E>
nb::dict events_to_numpy(const shared<pyvec<E>>& events) {
    auto        columns = std::make_unique<EventColumns<E>>(*events);
    auto*       data    = columns.get();
    nb::capsule owner(data, [](void* p) noexcept { delete static_cast<EventColumns<E>*>(p); });
    columns.release();

    nb::dict result;
    data->for_each([&](const char* name, auto& column) {
        using Value  = typename std::remove_reference_t<decltype(column)>::value_type;
        result[name] = nb::ndarray<nb::numpy, Value>(column.data(), {column.size()}, owner);
    });
    return result;
}

template<TType T>
auto bind_note(nb::module_& m, const std::string& name_) {
    using unit       = typename T::unit;
    using self_t     = shared<Note<T>>;
    using self_inner = Note<T>;

    const std::string name = "Note" + name_;
    auto [note, note_vec]  = bind_time_stamp<Note<T>>(m, name, "Note", name_, note_doc<T>());

    auto to_numpy = &events_to_numpy<self_inner>;

    // clang-format off
    note.def_prop_rw(RW_COPY(unit,  "start",    time), "Alias of ``time``.")
        .def_prop_rw(RW_COPY(unit,  "duration", duration))
        .def_prop_rw(RW_COPY(i8,    "pitch",    pitch))
        .def_prop_rw(RW_COPY(i8,    "velocity", velocity))
        .def("__init__", &pyinit<Note<T>, unit, unit, i8, i8>,
            nb::arg("time"), nb::arg("duration"), nb::arg("pitch"), nb::arg("velocity") = 0)
        .def_prop_rw(
            "end",
            [](const self_t& self) { return self->end(); },
            [](self_t& self, unit value) { self->duration = symusic::detail::sub_time(value, self->time);},
            kEventEndDoc)
        .def("empty", [](const self_t& self) { return self->empty(); }, kEventNoteEmptyDoc)
        .def(
            "shift_pitch",
            [](self_t& self, const int8_t offset, const bool inplace) {
                self_t ans = inplace ? self : std::make_shared<Note<T>>(*self);
                ans->shift_pitch_inplace(offset);
                return ans;
            }, nb::rv_policy::copy,
            nb::arg("offset"),
            nb::arg("inplace") = false,
            kEventNoteShiftPitchDoc
        )
        .def(
            "shift_velocity",
            [](self_t& self, const int8_t offset, const bool inplace) {
                self_t ans = inplace ? self : std::make_shared<Note<T>>(*self);
                ans->shift_velocity_inplace(offset);
                return ans;
            }, nb::rv_policy::copy,
            nb::arg("offset"),
            nb::arg("inplace") = false,
            kEventNoteShiftVelocityDoc
        )
    ;

    note_vec
        .def("numpy", to_numpy, kEventVecNumpyDoc)
        .def_static("from_numpy", &note_from_numpy<T>, kEventVecFromNumpyDoc)
    ;
    // clang-format on
    return std::make_tuple(note, note_vec);
}

template<TType T>
auto bind_keysig(nb::module_& m, const std::string& name_) {
    using unit       = typename T::unit;
    using self_t     = shared<KeySignature<T>>;
    using self_inner = KeySignature<T>;

    const std::string name = "KeySignature" + name_;
    auto [keysig, keysig_vec]
        = bind_time_stamp<KeySignature<T>>(m, name, "KeySignature", name_, key_signature_doc<T>());

    auto to_numpy = &events_to_numpy<self_inner>;

    // clang-format off
    keysig.def_prop_rw(RW_COPY(i8,    "key", key))
        .def_prop_rw(RW_COPY(u8,      "tonality", tonality))
        .def("__init__", &pyinit<KeySignature<T>, unit, i8, u8>,
            nb::arg("time"), nb::arg("key"), nb::arg("tonality"))
    ;

    keysig_vec
        .def("numpy", to_numpy, kEventVecNumpyDoc)
        .def_static("from_numpy", &keysig_from_numpy<T>, kEventVecFromNumpyDoc)
    ;
    // clang-format on
    return std::make_tuple(keysig, keysig_vec);
}

template<TType T>
auto bind_timesig(nb::module_& m, const std::string& name_) {
    using unit       = typename T::unit;
    using self_t     = shared<TimeSignature<T>>;
    using self_inner = TimeSignature<T>;

    const std::string name      = "TimeSignature" + name_;
    auto [timesig, timesig_vec] = bind_time_stamp<TimeSignature<T>>(
        m, name, "TimeSignature", name_, time_signature_doc<T>()
    );

    auto to_numpy = &events_to_numpy<self_inner>;

    // clang-format off
    timesig.def_prop_rw(RW_COPY(u8, "numerator", numerator))
        .def_prop_rw(RW_COPY(u8, "denominator", denominator))
        .def("__init__", &pyinit<TimeSignature<T>, unit, u8, u8>,
            nb::arg("time"), nb::arg("numerator"), nb::arg("denominator"))
    ;

    timesig_vec
        .def("numpy", to_numpy, kEventVecNumpyDoc)
        .def_static("from_numpy", &timesig_from_numpy<T>, kEventVecFromNumpyDoc)
    ;
    // clang-format on
    return std::make_tuple(timesig, timesig_vec);
}

template<TType T>
auto bind_controlchange(nb::module_& m, const std::string& name_) {
    using unit       = typename T::unit;
    using self_t     = shared<ControlChange<T>>;
    using self_inner = ControlChange<T>;

    const std::string name                  = "ControlChange" + name_;
    auto [controlchange, controlchange_vec] = bind_time_stamp<ControlChange<T>>(
        m, name, "ControlChange", name_, control_change_doc<T>()
    );

    auto to_numpy = &events_to_numpy<self_inner>;

    // clang-format off
    controlchange
        .def_prop_rw(RW_COPY(u8, "number", number))
        .def_prop_rw(RW_COPY(u8, "value", value))
        .def("__init__", &pyinit<ControlChange<T>, unit, u8, u8>,
            nb::arg("time"), nb::arg("number"), nb::arg("value"))
    ;

    controlchange_vec
        .def("numpy", to_numpy, kEventVecNumpyDoc)
        .def_static("from_numpy", &controlchange_from_numpy<T>, kEventVecFromNumpyDoc)
    ;
    // clang-format on
    return std::make_tuple(controlchange, controlchange_vec);
}

template<TType T>
auto bind_pedal(nb::module_& m, const std::string& name_) {
    using unit       = typename T::unit;
    using self_t     = shared<Pedal<T>>;
    using self_inner = Pedal<T>;

    const std::string name  = "Pedal" + name_;
    auto [pedal, pedal_vec] = bind_time_stamp<Pedal<T>>(m, name, "Pedal", name_, pedal_doc<T>());

    auto to_numpy = &events_to_numpy<self_inner>;

    // clang-format off
    pedal
        .def_prop_rw(RW_COPY(unit, "start", time), "Alias of ``time``.")
        .def_prop_rw(RW_COPY(unit, "duration", duration))
        .def_prop_rw(
            "end",
            [](const self_t& self) { return self->end(); },
            [](self_t& self, unit value) { self->duration = symusic::detail::sub_time(value, self->time); },
            kEventEndDoc)
        .def("__init__", &pyinit<Pedal<T>, unit, unit>,
            nb::arg("time"), nb::arg("duration"))
    ;

    pedal_vec
        .def("numpy", to_numpy, kEventVecNumpyDoc)
        .def_static("from_numpy", &pedal_from_numpy<T>, kEventVecFromNumpyDoc)
    ;
    // clang-format on
    return std::make_tuple(pedal, pedal_vec);
}

template<TType T>
auto bind_tempo(nb::module_& m, const std::string& name_) {
    using unit       = typename T::unit;
    using self_t     = shared<Tempo<T>>;
    using self_inner = Tempo<T>;

    const std::string name  = "Tempo" + name_;
    auto [tempo, tempo_vec] = bind_time_stamp<Tempo<T>>(m, name, "Tempo", name_, tempo_doc<T>());

    auto to_numpy = &events_to_numpy<self_inner>;

    // clang-format off
    tempo
        .def_prop_rw(RW_COPY(i32, "mspq", mspq))
        .def("__init__", [](self_t* self, const unit time, std::optional<f64> qpm, std::optional<i32> mspq) {
            new (self) self_t(std::make_shared<Tempo<T>>());
            self_t& t = *self;
            t->time   = time;
            if (qpm.has_value()) {
                t->set_qpm(*qpm);
            } else if (mspq.has_value()) {
                t->mspq = *mspq;
            } else {
                throw std::invalid_argument("qpm or mspq must be specified");
            }
        }, nb::arg("time"), nb::arg("qpm") = nb::none(), nb::arg("mspq") = nb::none(),
            kEventTempoInitDoc)
        .def_prop_rw(
            "tempo",
            [](const self_t& self) { return self->qpm(); },
            [](self_t& self, f64 value) { self->set_qpm(value); },
            "Alias of ``qpm``.")
        .def_prop_rw(
            "qpm",
            [](const self_t& self) { return self->qpm(); },
            [](self_t& self, f64 value) { self->set_qpm(value); },
            kEventTempoQpmDoc)
    ;

    tempo_vec
        .def("numpy", to_numpy, kEventVecNumpyDoc)
        .def_static("from_numpy", &tempo_from_numpy<T>, kEventVecFromNumpyDoc)
    ;
    // clang-format on
    return std::make_tuple(tempo, tempo_vec);
}

template<TType T>
auto bind_pitchbend(nb::module_& m, const std::string& name_) {
    using unit       = typename T::unit;
    using self_t     = shared<PitchBend<T>>;
    using self_inner = PitchBend<T>;

    const std::string name = "PitchBend" + name_;
    auto [pitchbend, pitchbend_vec]
        = bind_time_stamp<PitchBend<T>>(m, name, "PitchBend", name_, pitch_bend_doc<T>());

    auto to_numpy = &events_to_numpy<self_inner>;

    // clang-format off
    pitchbend
        .def_prop_rw(RW_COPY(i32, "value", value))
        .def("__init__", &pyinit<PitchBend<T>, unit, i32>,
            nb::arg("time"), nb::arg("value"))
    ;

    pitchbend_vec
        .def("numpy", to_numpy, kEventVecNumpyDoc)
        .def_static("from_numpy", &pitchbend_from_numpy<T>, kEventVecFromNumpyDoc)
    ;
    // clang-format on
    return std::make_tuple(pitchbend, pitchbend_vec);
}

template<TType T>
auto bind_textmeta(nb::module_& m, const std::string& name_) {
    using unit   = typename T::unit;
    using self_t = shared<TextMeta<T>>;

    const std::string name = "TextMeta" + name_;
    auto [textmeta, textmeta_vec]
        = bind_time_stamp<TextMeta<T>>(m, name, "TextMeta", name_, text_meta_doc<T>());

    // clang-format off
    textmeta
        .def_prop_rw(RW_COPY(std::string, "text", text))
        .def("__init__", &pyinit<TextMeta<T>, unit, std::string>,
            nb::arg("time"), nb::arg("text"))
    ;

    textmeta_vec
        .def("numpy", [](const self_t&) {
            throw std::runtime_error("TextMeta does not support numpy");
        }, kEventTextMetaNumpyDoc)
        .def_static("from_numpy", []() {
            throw std::runtime_error("TextMeta does not support from_numpy");
        }, kEventTextMetaNumpyDoc)
    ;
    // clang-format on
    return std::make_tuple(textmeta, textmeta_vec);
}

}   // namespace symusic::py

//
// Bindings for the time based events (note, pedal, tempo, ...).
//

#pragma once

#include <optional>
#include <string>
#include <tuple>

#include "MetaMacro.h"
#include "../../utils/python_helpers.h"
#include "../../utils/vector_bindings.h"
#include <fmt/format.h>
#include "../docstring_helpers.h"

namespace pyutils {

namespace nb = nanobind;
using namespace symusic;

namespace event_docstrings {
constexpr std::string_view kTimedEventTemplate = R"pbdoc(
{signature}

{description}

Notes
-----
{time_notes}
)pbdoc";

inline std::string build_time_notes(
    const docstring_helpers::TimeFlavorInfo& flavor,
    const bool include_duration_note,
    std::string_view extra_note = {}
) {
    std::string notes
        = fmt::format("- {}\n- Optimized for {}", flavor.measurement_sentence, flavor.best_for);
    if (include_duration_note) {
        notes += fmt::format("\n- Durations use the same {} units", flavor.timeline_noun);
    }
    if (!extra_note.empty()) { notes += fmt::format("\n- {}", extra_note); }
    return notes;
}

inline std::string render_signature(
    const docstring_helpers::TimeFlavorInfo& flavor,
    std::string_view                         base,
    std::string_view                         signature_template
) {
    return fmt::format(
        fmt::runtime(signature_template),
        fmt::arg("class_name", fmt::format("{}{}", base, flavor.suffix)),
        fmt::arg("scalar", flavor.scalar_label)
    );
}

template<TType T>
const char* note_doc() {
    static const std::string rendered = [] {
        constexpr auto flavor = docstring_helpers::time_flavor<T>();
        const auto     signature
            = render_signature(flavor, "Note", "{class_name}(time: {scalar}, duration: {scalar}, pitch: int, velocity: int = 0)");
        const auto description = fmt::format(
            "Pitched event anchored to a {} timestamp with editable duration and velocity.",
            flavor.timeline_noun
        );
        const auto time_notes = build_time_notes(flavor, true);
        return fmt::format(
            fmt::runtime(kTimedEventTemplate),
            fmt::arg("signature", signature),
            fmt::arg("description", description),
            fmt::arg("time_notes", time_notes)
        );
    }();
    return rendered.c_str();
}

template<TType T>
const char* key_signature_doc() {
    static const std::string rendered = [] {
        constexpr auto flavor = docstring_helpers::time_flavor<T>();
        const auto     signature = render_signature(
            flavor, "KeySignature", "{class_name}(time: {scalar}, key: int, tonality: int)"
        );
        const auto description = fmt::format(
            "Key signature change scheduled on the {} timeline. ``key`` is the number of sharps/flats "
            "relative to C and ``tonality`` is ``0`` for major, ``1`` for minor.",
            flavor.timeline_noun
        );
        const auto time_notes = build_time_notes(flavor, false);
        return fmt::format(
            fmt::runtime(kTimedEventTemplate),
            fmt::arg("signature", signature),
            fmt::arg("description", description),
            fmt::arg("time_notes", time_notes)
        );
    }();
    return rendered.c_str();
}

template<TType T>
const char* time_signature_doc() {
    static const std::string rendered = [] {
        constexpr auto flavor = docstring_helpers::time_flavor<T>();
        const auto     signature = render_signature(
            flavor, "TimeSignature", "{class_name}(time: {scalar}, numerator: int, denominator: int)"
        );
        const auto description = fmt::format(
            "Meter change storing beats-per-bar and subdivision (e.g., ``3/4``) on the {} timeline. "
            "The denominator must be a power of two because it represents note lengths.",
            flavor.timeline_noun
        );
        const auto time_notes = build_time_notes(flavor, false);
        return fmt::format(
            fmt::runtime(kTimedEventTemplate),
            fmt::arg("signature", signature),
            fmt::arg("description", description),
            fmt::arg("time_notes", time_notes)
        );
    }();
    return rendered.c_str();
}

template<TType T>
const char* control_change_doc() {
    static const std::string rendered = [] {
        constexpr auto flavor = docstring_helpers::time_flavor<T>();
        const auto     signature = render_signature(
            flavor, "ControlChange", "{class_name}(time: {scalar}, number: int, value: int)"
        );
        const auto description = fmt::format(
            "Standard MIDI controller event carrying a controller number/value pair on the {} timeline.",
            flavor.timeline_noun
        );
        const auto time_notes = build_time_notes(flavor, false);
        return fmt::format(
            fmt::runtime(kTimedEventTemplate),
            fmt::arg("signature", signature),
            fmt::arg("description", description),
            fmt::arg("time_notes", time_notes)
        );
    }();
    return rendered.c_str();
}

template<TType T>
const char* pedal_doc() {
    static const std::string rendered = [] {
        constexpr auto flavor = docstring_helpers::time_flavor<T>();
        const auto     signature
            = render_signature(flavor, "Pedal", "{class_name}(time: {scalar}, duration: {scalar})");
        const auto description = fmt::format(
            "Sustain or soft pedal gesture represented as a timed range on the {} timeline.",
            flavor.timeline_noun
        );
        const auto time_notes = build_time_notes(flavor, true);
        return fmt::format(
            fmt::runtime(kTimedEventTemplate),
            fmt::arg("signature", signature),
            fmt::arg("description", description),
            fmt::arg("time_notes", time_notes)
        );
    }();
    return rendered.c_str();
}

template<TType T>
const char* tempo_doc() {
    static const std::string rendered = [] {
        constexpr auto flavor = docstring_helpers::time_flavor<T>();
        const auto     signature
            = render_signature(flavor, "Tempo", "{class_name}(time: {scalar}, mspq: int)");
        const auto description = fmt::format(
            "Tempo change stored as microseconds-per-quarter note at a {} timestamp. "
            "The ``qpm`` property mirrors beats-per-minute and mutating it updates ``mspq`` accordingly.",
            flavor.timeline_noun
        );
        const auto time_notes = build_time_notes(flavor, false);
        return fmt::format(
            fmt::runtime(kTimedEventTemplate),
            fmt::arg("signature", signature),
            fmt::arg("description", description),
            fmt::arg("time_notes", time_notes)
        );
    }();
    return rendered.c_str();
}

template<TType T>
const char* pitch_bend_doc() {
    static const std::string rendered = [] {
        constexpr auto flavor = docstring_helpers::time_flavor<T>();
        const auto     signature
            = render_signature(flavor, "PitchBend", "{class_name}(time: {scalar}, value: int)");
        const auto description = fmt::format(
            "Pitch-bend wheel value applied on the {} timeline. The raw 14-bit value matches the MIDI encoding.",
            flavor.timeline_noun
        );
        const auto time_notes = build_time_notes(flavor, false);
        return fmt::format(
            fmt::runtime(kTimedEventTemplate),
            fmt::arg("signature", signature),
            fmt::arg("description", description),
            fmt::arg("time_notes", time_notes)
        );
    }();
    return rendered.c_str();
}

template<TType T>
const char* text_meta_doc() {
    static const std::string rendered = [] {
        constexpr auto flavor = docstring_helpers::time_flavor<T>();
        const auto     signature
            = render_signature(flavor, "TextMeta", "{class_name}(time: {scalar}, text: str)");
        const auto description = fmt::format(
            "Free-form metadata event used for lyrics, markers, or comments on the {} timeline.",
            flavor.timeline_noun
        );
        const auto time_notes = build_time_notes(flavor, false);
        return fmt::format(
            fmt::runtime(kTimedEventTemplate),
            fmt::arg("signature", signature),
            fmt::arg("description", description),
            fmt::arg("time_notes", time_notes)
        );
    }();
    return rendered.c_str();
}

constexpr const char* kShiftTimeDoc
    = R"pbdoc(Shift the event's timestamp by *offset*. When *inplace* is False a cloned event is returned.)pbdoc";
constexpr const char* kCopyCtorDoc
    = R"pbdoc(Copy an existing event into this shared pointer wrapper.)pbdoc";
constexpr const char* kCopyDoc
    = R"pbdoc(Return a shallow or deep copy depending on *deep*. Deep copies duplicate underlying storage.)pbdoc";
constexpr const char* kCopyMethodDoc = R"pbdoc(Return a shallow copy of the event.)pbdoc";
constexpr const char* kDeepcopyDoc
    = R"pbdoc(Create a deep copy of the event suitable for Python's copy module helpers.)pbdoc";
constexpr const char* kReprDoc
    = R"pbdoc(Serialize the event into its human-friendly textual form.)pbdoc";
constexpr const char* kEqDoc
    = R"pbdoc(Events compare equal when they refer to the same pointer or when their values match.)pbdoc";
constexpr const char* kNeDoc
    = R"pbdoc(Events compare not-equal when their value representation differs.)pbdoc";
constexpr const char* kGetStateDoc
    = R"pbdoc(Serialize the event into ZPP bytes for pickling.)pbdoc";
constexpr const char* kSetStateDoc = R"pbdoc(Rehydrate the event from pickled ZPP bytes.)pbdoc";
constexpr const char* kUseCountDoc
    = R"pbdoc(Return the reference count of the underlying shared pointer. Helpful for debugging aliasing.)pbdoc";

constexpr const char* kVecSortDoc
    = R"pbdoc(Sort events in-place (unless *inplace* is False) by default key or a user supplied callable.)pbdoc";
constexpr const char* kVecFilterDoc
    = R"pbdoc(Keep only events for which the supplied Python callable returns True. Optionally operate on a copy.)pbdoc";
constexpr const char* kVecIsSortedDoc
    = R"pbdoc(Return True when the vector is already sorted by the default key or supplied callable.)pbdoc";
constexpr const char* kVecAdjustTimeDoc
    = R"pbdoc(Remap timestamps using two aligned lists of original/new time values. Returns copy unless *inplace*.)pbdoc";

constexpr const char* kNoteEmptyDoc = R"pbdoc(Return True when the note duration is zero.)pbdoc";
constexpr const char* kNoteEndDoc
    = R"pbdoc(Read or set the exclusive end time. Setting the value adjusts the duration.)pbdoc";
constexpr const char* kNoteShiftPitchDoc
    = R"pbdoc(Shift the MIDI pitch by *offset*. Returns a new note unless *inplace* is True.)pbdoc";
constexpr const char* kNoteShiftVelocityDoc
    = R"pbdoc(Offset the MIDI velocity by *offset*. Returns a new note unless *inplace* is True.)pbdoc";
constexpr const char* kNoteNumpyDoc
    = R"pbdoc(Expose the underlying SOA arrays as NumPy buffers for zero-copy inspection.)pbdoc";
constexpr const char* kNoteFromNumpyDoc
    = R"pbdoc(Build a note vector from NumPy arrays describing time/duration/pitch/velocity.)pbdoc";

constexpr const char* kKeySigInitDoc
    = R"pbdoc(Create a key signature event with *time*, *key* offset, and *tonality* (major/minor).)pbdoc";
constexpr const char* kTimeSigInitDoc
    = R"pbdoc(Create a time signature event with *time*, *numerator*, and *denominator*.)pbdoc";
constexpr const char* kCCInitDoc
    = R"pbdoc(Create a control change event with controller *number*/*value* emitted at *time*.)pbdoc";
constexpr const char* kPedalInitDoc
    = R"pbdoc(Create a sustain/soft pedal event given *time* and *duration*.)pbdoc";
constexpr const char* kTempoInitDoc
    = R"pbdoc(Create a tempo event at *time*. Provide either *qpm* (preferred) or *mspq*.)pbdoc";
constexpr const char* kPitchBendInitDoc
    = R"pbdoc(Create a pitch bend event with the raw wheel *value*.)pbdoc";
constexpr const char* kTextMetaInitDoc
    = R"pbdoc(Create a text metadata event with arbitrary UTF-8 text.)pbdoc";

constexpr const char* kTempoPropQpmDoc
    = R"pbdoc(Read or set the tempo in quarter-notes per minute. Setting recalculates the stored MSPQ value.)pbdoc";
constexpr const char* kTempoPropTempoDoc = kTempoPropQpmDoc;

constexpr const char* kVecFromNumpyDoc = R"pbdoc(Build an event vector from NumPy buffers.)pbdoc";
constexpr const char* kTempoNumpyDoc
    = R"pbdoc(Expose tempo MSPQ/time arrays as NumPy buffers for efficient analysis.)pbdoc";
constexpr const char* kPedalNumpyDoc
    = R"pbdoc(Expose pedal time/duration arrays as NumPy buffers.)pbdoc";
constexpr const char* kControlNumpyDoc
    = R"pbdoc(Expose control-change time/number/value arrays as NumPy buffers.)pbdoc";
constexpr const char* kKeySigNumpyDoc
    = R"pbdoc(Expose key signature time/key/tonality arrays as NumPy buffers.)pbdoc";
constexpr const char* kTimeSigNumpyDoc
    = R"pbdoc(Expose time signature time/numerator/denominator arrays as NumPy buffers.)pbdoc";
constexpr const char* kPitchBendNumpyDoc
    = R"pbdoc(Expose pitch bend time/value arrays as NumPy buffers.)pbdoc";
constexpr const char* kTextMetaNumpyDoc
    = R"pbdoc(Text metadata cannot be exported to NumPy because of variable-length strings.)pbdoc";
constexpr const char* kTextMetaFromNumpyDoc
    = R"pbdoc(Text metadata cannot be constructed from NumPy arrays; use regular constructors instead.)pbdoc";
}   // namespace event_docstrings

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

    auto event = class_doc ? nb::class_<shared<T>>(m, name.c_str(), nb::sig(event_sig.c_str()), class_doc) :
                             nb::class_<shared<T>>(m, name.c_str(), nb::sig(event_sig.c_str()));

    // clang-format off
    event
        .def_prop_rw(RW_COPY(unit, "time", time), "Event timestamp in chosen time unit")
        .def_prop_ro("ttype", [](const self_t&) { return typename T::ttype(); }, "Time unit type — Tick/Quarter/Second")
        .def(
            "shift_time",
            [](const self_t& self, const unit offset, const bool inplace) {
                self_t ans = inplace ? self : std::make_shared<T>(*self);
                ans->shift_time(offset);
                return ans;
            },
            nb::rv_policy::copy,
            nb::arg("offset"),
            nb::arg("inplace") = false,
            event_docstrings::kShiftTimeDoc
        )
        .def(
            "__init__",
            [](self_t* self, const self_t& other) { new (self) shared<T>(std::make_shared<T>(*other)); },
            nb::arg("other"),
            event_docstrings::kCopyCtorDoc
        )
        .def(
            "copy",
            [](const shared<T>& self, const bool deep) { return std::make_shared<T>(*self); },
            nb::arg("deep") = true,
            event_docstrings::kCopyDoc
        )
        .def("__copy__", copy_func, event_docstrings::kCopyMethodDoc)
        .def(
            "__deepcopy__",
            deepcopy_func,
            nb::arg("memo") = nb::none(),
            nb::arg("_nil") = nb::none(),
            event_docstrings::kDeepcopyDoc
        )
        .def("__repr__", [](const shared<T>& self) { return self->to_string(); }, event_docstrings::kReprDoc)
        .def("__eq__", [](const shared<T>& self, const shared<T>& other) { return *self == *other; }, event_docstrings::kEqDoc)
        .def("__eq__", [](const shared<T>&, const nb::object&) { return false; }, event_docstrings::kEqDoc)
        .def("__ne__", [](const shared<T>& self, const shared<T>& other) { return *self != *other; }, event_docstrings::kNeDoc)
        .def("__ne__", [](const shared<T>&, const nb::object&) { return true; }, event_docstrings::kNeDoc)
        .def(nb::self != nb::self) // NOLINT
        .def("__getstate__", &to_bytes<T>, event_docstrings::kGetStateDoc)
        .def("__setstate__", &from_bytes<T>, event_docstrings::kSetStateDoc)
        .def("__use_count", [](const shared<T>& self) { return self.use_count(); }, event_docstrings::kUseCountDoc)
    ;

    const auto list_name = name + "List";
    const auto vec_sig = fmt::format("class {}(PyVec[{}[{}]])", list_name, generic_base, unit_suffix);
    auto vec_class = bind_shared_pyvec<T>(m, list_name.c_str(), nb::sig(vec_sig.c_str()))
        .def_prop_ro("ttype", [](const vec_t&) { return typename T::ttype(); })
        .def("__getstate__", &vec_to_bytes<T>, event_docstrings::kGetStateDoc)
        .def("__setstate__", &vec_from_bytes<T>, event_docstrings::kSetStateDoc)
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
            event_docstrings::kVecSortDoc
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
            event_docstrings::kVecFilterDoc
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
            event_docstrings::kVecIsSortedDoc
        )
        .def(
            "adjust_time",
            [](vec_t& v, const vec<unit>& original_times, const vec<unit>& new_times, const bool inplace) -> vec_t {
                vec_t ans = inplace ? v : std::make_shared<pyvec<T>>(v->deepcopy());
                ops::adjust_time_inplace(*ans, original_times, new_times);
                return ans;
            },
            nb::rv_policy::copy,
            nb::arg("original_times"),
            nb::arg("new_times"),
            nb::arg("inplace") = false,
            event_docstrings::kVecAdjustTimeDoc
        )
    ;
    // clang-format on

    nb::implicitly_convertible<nb::list, shared<pyvec<T>>>();
    return std::make_tuple(event, vec_class);
}

#define HELPER(x) x

#define ADD_TO_DICT(__COUNT, NAME)                                                  \
    ans[#NAME] = nb::ndarray<nb::numpy, typename decltype(temp->NAME)::value_type>( \
        temp->NAME.data(), {size}, deleter                                          \
    );

#define TO_NUMPY_INNER(ARR_TYPE, ...)                                                             \
    [](const shared<pyvec<self_inner>>& self) {                                                   \
        auto*       temp = new ARR_TYPE<T>(*self);                                                \
        nb::capsule deleter(temp, [](void* p) noexcept { delete static_cast<ARR_TYPE<T>*>(p); }); \
        size_t      size = self->size();                                                          \
        nb::dict    ans{};                                                                        \
        REPEAT_ON(ADD_TO_DICT, __VA_ARGS__)                                                       \
        return ans;                                                                               \
    }

#define TO_NUMPY(...) HELPER(TO_NUMPY_INNER(__VA_ARGS__))

template<TType T>
auto bind_note(nb::module_& m, const std::string& name_) {
    using unit       = typename T::unit;
    using self_t     = shared<Note<T>>;
    using self_inner = Note<T>;

    const std::string name = "Note" + name_;
    auto [note, note_vec]
        = bind_time_stamp<Note<T>>(m, name, "Note", name_, event_docstrings::note_doc<T>());

    auto to_numpy = TO_NUMPY(NoteArr, time, duration, pitch, velocity);

    // clang-format off
    note.def_prop_rw(RW_COPY(unit,  "start",    time))
        .def_prop_rw(RW_COPY(unit,  "duration", duration))
        .def_prop_rw(RW_COPY(i8,    "pitch",    pitch))
        .def_prop_rw(RW_COPY(i8,    "velocity", velocity))
        .def("__init__", &pyinit<Note<T>, unit, unit, i8, i8>,
            nb::arg("time"), nb::arg("duration"), nb::arg("pitch"), nb::arg("velocity") = 0,
            "Create a note event from timing, duration, pitch, and velocity values."
        )
        .def_prop_rw(
            "end",
            [](const self_t& self) { return self->end(); },
            [](self_t& self, unit value) { self->duration = value - self->time;},
            event_docstrings::kNoteEndDoc)
        .def("empty", [](const self_t& self) { return self->empty(); }, event_docstrings::kNoteEmptyDoc)
        .def("ent_time", [](const self_t& self) { return self->end(); }, event_docstrings::kNoteEndDoc)
        .def(
            "shift_pitch",
            [](self_t& self, const int8_t offset, const bool inplace) {
                self_t ans = inplace ? self : std::make_shared<Note<T>>(*self);
                ans->shift_pitch_inplace(offset);
                return ans;
            }, nb::rv_policy::copy,
            nb::arg("offset"),
            nb::arg("inplace") = false,
            event_docstrings::kNoteShiftPitchDoc
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
            event_docstrings::kNoteShiftVelocityDoc
        )
    ;

    note_vec
        .def("numpy", to_numpy, event_docstrings::kNoteNumpyDoc)
        .def_static("from_numpy", &note_from_numpy<T>, event_docstrings::kNoteFromNumpyDoc)
    ;
    // clang-format on
    return std::make_tuple(note, note_vec);
}

template<TType T>
auto bind_keysig(nb::module_& m, const std::string& name_) {
    using unit       = typename T::unit;
    using self_t     = shared<KeySignature<T>>;
    using self_inner = KeySignature<T>;

    const std::string name    = "KeySignature" + name_;
    auto [keysig, keysig_vec] = bind_time_stamp<KeySignature<T>>(
        m, name, "KeySignature", name_, event_docstrings::key_signature_doc<T>()
    );

    auto to_numpy = TO_NUMPY(KeySignatureArr, time, key, tonality);

    // clang-format off
    keysig.def_prop_rw(RW_COPY(i8,    "key", key))
        .def_prop_rw(RW_COPY(u8,      "tonality", tonality))
        .def("__init__", &pyinit<KeySignature<T>, unit, i8, u8>,
            nb::arg("time"), nb::arg("key"), nb::arg("tonality"),
            event_docstrings::kKeySigInitDoc)
    ;

    keysig_vec
        .def("numpy", to_numpy, event_docstrings::kKeySigNumpyDoc)
        .def_static("from_numpy", &keysig_from_numpy<T>, event_docstrings::kVecFromNumpyDoc)
    ;
    // clang-format on
    return std::make_tuple(keysig, keysig_vec);
}

template<TType T>
auto bind_timesig(nb::module_& m, const std::string& name_) {
    using unit       = typename T::unit;
    using self_t     = shared<TimeSignature<T>>;
    using self_inner = TimeSignature<T>;

    const std::string name = "TimeSignature" + name_;
    auto [timesig, timesig_vec] = bind_time_stamp<TimeSignature<T>>(
        m, name, "TimeSignature", name_, event_docstrings::time_signature_doc<T>()
    );

    auto to_numpy = TO_NUMPY(TimeSignatureArr, time, numerator, denominator);

    // clang-format off
    timesig.def_prop_rw(RW_COPY(u8, "numerator", numerator))
        .def_prop_rw(RW_COPY(u8, "denominator", denominator))
        .def("__init__", &pyinit<TimeSignature<T>, unit, u8, u8>,
            nb::arg("time"), nb::arg("numerator"), nb::arg("denominator"),
            event_docstrings::kTimeSigInitDoc)
    ;

    timesig_vec
        .def("numpy", to_numpy, event_docstrings::kTimeSigNumpyDoc)
        .def_static("from_numpy", &timesig_from_numpy<T>, event_docstrings::kVecFromNumpyDoc)
    ;
    // clang-format on
    return std::make_tuple(timesig, timesig_vec);
}

template<TType T>
auto bind_controlchange(nb::module_& m, const std::string& name_) {
    using unit       = typename T::unit;
    using self_t     = shared<ControlChange<T>>;
    using self_inner = ControlChange<T>;

    const std::string name = "ControlChange" + name_;
    auto [controlchange, controlchange_vec] = bind_time_stamp<ControlChange<T>>(
        m, name, "ControlChange", name_, event_docstrings::control_change_doc<T>()
    );

    auto to_numpy = TO_NUMPY(ControlChangeArr, time, number, value);

    // clang-format off
    controlchange
        .def_prop_rw(RW_COPY(u8, "number", number))
        .def_prop_rw(RW_COPY(u8, "value", value))
        .def("__init__", &pyinit<ControlChange<T>, unit, u8, u8>,
            nb::arg("time"), nb::arg("number"), nb::arg("value"),
            event_docstrings::kCCInitDoc)
    ;

    controlchange_vec
        .def("numpy", to_numpy, event_docstrings::kControlNumpyDoc)
        .def_static("from_numpy", &controlchange_from_numpy<T>, event_docstrings::kVecFromNumpyDoc)
    ;
    // clang-format on
    return std::make_tuple(controlchange, controlchange_vec);
}

template<TType T>
auto bind_pedal(nb::module_& m, const std::string& name_) {
    using unit       = typename T::unit;
    using self_t     = shared<Pedal<T>>;
    using self_inner = Pedal<T>;

    const std::string name = "Pedal" + name_;
    auto [pedal, pedal_vec]
        = bind_time_stamp<Pedal<T>>(m, name, "Pedal", name_, event_docstrings::pedal_doc<T>());

    auto to_numpy = TO_NUMPY(PedalArr, time, duration);

    // clang-format off
    pedal
        .def_prop_rw(RW_COPY(unit, "start", time))
        .def_prop_rw(RW_COPY(unit, "duration", duration))
        .def_prop_rw(
            "end",
            [](const self_t& self) { return self->end(); },
            [](self_t& self, unit value) { self->duration = value - self->time; })
        .def("__init__", &pyinit<Pedal<T>, unit, unit>,
            nb::arg("time"), nb::arg("duration"),
            event_docstrings::kPedalInitDoc)
    ;

    pedal_vec
        .def("numpy", to_numpy, event_docstrings::kPedalNumpyDoc)
        .def_static("from_numpy", &pedal_from_numpy<T>, event_docstrings::kVecFromNumpyDoc)
    ;
    // clang-format on
    return std::make_tuple(pedal, pedal_vec);
}

template<TType T>
auto bind_tempo(nb::module_& m, const std::string& name_) {
    using unit       = typename T::unit;
    using self_t     = shared<Tempo<T>>;
    using self_inner = Tempo<T>;

    const std::string name = "Tempo" + name_;
    auto [tempo, tempo_vec]
        = bind_time_stamp<Tempo<T>>(m, name, "Tempo", name_, event_docstrings::tempo_doc<T>());

    auto to_numpy = TO_NUMPY(TempoArr, time, mspq);

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
            event_docstrings::kTempoInitDoc)
        .def_prop_rw(
            "tempo",
            [](const self_t& self) { return self->qpm(); },
            [](self_t& self, f64 value) { self->set_qpm(value); },
            event_docstrings::kTempoPropTempoDoc)
        .def_prop_rw(
            "qpm",
            [](const self_t& self) { return self->qpm(); },
            [](self_t& self, f64 value) { self->set_qpm(value); },
            event_docstrings::kTempoPropQpmDoc)
    ;

    tempo_vec
        .def("numpy", to_numpy, event_docstrings::kTempoNumpyDoc)
        .def_static("from_numpy", &tempo_from_numpy<T>, event_docstrings::kVecFromNumpyDoc)
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
    auto [pitchbend, pitchbend_vec] = bind_time_stamp<PitchBend<T>>(
        m, name, "PitchBend", name_, event_docstrings::pitch_bend_doc<T>()
    );

    auto to_numpy = TO_NUMPY(PitchBendArr, time, value);

    // clang-format off
    pitchbend
        .def_prop_rw(RW_COPY(i32, "value", value))
        .def("__init__", &pyinit<PitchBend<T>, unit, i32>,
            nb::arg("time"), nb::arg("value"),
            event_docstrings::kPitchBendInitDoc)
    ;

    pitchbend_vec
        .def("numpy", to_numpy, event_docstrings::kPitchBendNumpyDoc)
        .def_static("from_numpy", &pitchbend_from_numpy<T>, event_docstrings::kVecFromNumpyDoc)
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
        = bind_time_stamp<TextMeta<T>>(m, name, "TextMeta", name_, event_docstrings::text_meta_doc<T>());

    // clang-format off
    textmeta
        .def_prop_rw(RW_COPY(std::string, "text", text))
        .def("__init__", &pyinit<TextMeta<T>, unit, std::string>,
            nb::arg("time"), nb::arg("text"),
            event_docstrings::kTextMetaInitDoc)
    ;

    textmeta_vec
        .def("numpy", [](const self_t&) {
            throw std::runtime_error("TextMeta does not support numpy");
        }, event_docstrings::kTextMetaNumpyDoc)
        .def_static("from_numpy", []() {
            throw std::runtime_error("TextMeta does not support from_numpy");
        }, event_docstrings::kTextMetaFromNumpyDoc)
    ;
    // clang-format on
    return std::make_tuple(textmeta, textmeta_vec);
}

#undef TO_NUMPY
#undef TO_NUMPY_INNER
#undef ADD_TO_DICT
#undef HELPER

}   // namespace pyutils

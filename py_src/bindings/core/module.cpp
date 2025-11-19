//
// Entry point for the nanobind module. Keeps the high-level registration logic compact by
// delegating to the smaller binding units.
//

#include <random>
#include <string>

#include <nanobind/eigen/dense.h>
#include <nanobind/nanobind.h>
#include <nanobind/ndarray.h>
#include <nanobind/stl/filesystem.h>
#include <nanobind/stl/optional.h>
#include <nanobind/stl/string.h>
#include <nanobind/stl/vector.h>
#include <nanobind/typing.h>
#include <fmt/format.h>

#include "MetaMacro.h"
#include "../../utils/python_helpers.h"
#include "binding_common.h"
#include "../events/event_bindings.h"
#include "../score/score_bindings.h"
#include "../track/track_bindings.h"
#include "../synth/synthesizer_bindings.h"

#pragma warning(disable : 4996)

namespace nb = nanobind;

namespace symusic {

using namespace pyutils;

namespace time_unit_docstrings {
constexpr const char* kTickDoc
    = R"pbdoc(Discrete tick-based time unit that mirrors the raw MIDI resolution.)pbdoc";
constexpr const char* kQuarterDoc
    = R"pbdoc(Musical quarter-note time unit derived from the score's ticks-per-quarter setting.)pbdoc";
constexpr const char* kSecondDoc
    = R"pbdoc(Real-time second based time unit for wall-clock aligned workflows.)pbdoc";
}   // namespace time_unit_docstrings

namespace typing_detail {
struct ScoreTypingPlaceholder {};
struct PyVecTypingPlaceholder {};
struct TrackTypingPlaceholder {};
struct NoteTypingPlaceholder {};
struct KeySignatureTypingPlaceholder {};
struct TimeSignatureTypingPlaceholder {};
struct TempoTypingPlaceholder {};
struct ControlChangeTypingPlaceholder {};
struct PedalTypingPlaceholder {};
struct PitchBendTypingPlaceholder {};
struct TextMetaTypingPlaceholder {};
}   // namespace typing_detail

template<typename Placeholder>
void define_time_generic(nb::module_& m, const char* name) {
    const auto sig = fmt::format("class {}(typing.Generic[TimeUnitT])", name);
    const auto msg = fmt::format(
        "symusic.core.{0} is a typing helper. Instantiate {0}Tick, {0}Quarter, or {0}Second "
        "instead.",
        name
    );

    nb::class_<Placeholder>(m, name, nb::is_generic(), nb::sig(sig.c_str()))
        .def("__init__", [msg](Placeholder*) { throw nb::type_error(msg.c_str()); });
}
#define BIND_EVENT(__COUNT, BIND_FUNC) \
    BIND_FUNC<Tick>(m, "Tick");        \
    BIND_FUNC<Quarter>(m, "Quarter");  \
    BIND_FUNC<Second>(m, "Second");

NB_MODULE(core, m) {
#ifndef MEM_LEAK_WARNING
    nb::set_leak_warnings(false);
    m.attr("_nanobind_mem_leak_warning") = false;
#else
    m.attr("_nanobind_mem_leak_warning") = true;
#endif

    m.attr("_MIDI2ABC") = std::string("");

    auto time_unit_t    = nb::type_var("TimeUnitT");
    auto event_t        = nb::type_var("EventT");
    m.attr("TimeUnitT") = time_unit_t;
    m.attr("EventT")    = event_t;

    nb::class_<typing_detail::ScoreTypingPlaceholder>(
        m, "Score", nb::is_generic(), nb::sig("class Score(typing.Generic[TimeUnitT])")
    )
        .def("__init__", [](typing_detail::ScoreTypingPlaceholder*) {
            throw nb::type_error(
                "symusic.core.Score is a typing helper. Instantiate ScoreTick, "
                "ScoreQuarter, or ScoreSecond instead."
            );
        });

    nb::class_<typing_detail::PyVecTypingPlaceholder>(
        m, "PyVec", nb::is_generic(), nb::sig("class PyVec(typing.Generic[EventT])")
    )
        .def("__init__", [](typing_detail::PyVecTypingPlaceholder*) {
            throw nb::type_error(
                "symusic.core.PyVec is a typing helper. Instantiate concrete event list types "
                "(e.g., NoteTickList)."
            );
        });

    define_time_generic<typing_detail::TrackTypingPlaceholder>(m, "Track");
    define_time_generic<typing_detail::NoteTypingPlaceholder>(m, "Note");
    define_time_generic<typing_detail::KeySignatureTypingPlaceholder>(m, "KeySignature");
    define_time_generic<typing_detail::TimeSignatureTypingPlaceholder>(m, "TimeSignature");
    define_time_generic<typing_detail::TempoTypingPlaceholder>(m, "Tempo");
    define_time_generic<typing_detail::ControlChangeTypingPlaceholder>(m, "ControlChange");
    define_time_generic<typing_detail::PedalTypingPlaceholder>(m, "Pedal");
    define_time_generic<typing_detail::PitchBendTypingPlaceholder>(m, "PitchBend");
    define_time_generic<typing_detail::TextMetaTypingPlaceholder>(m, "TextMeta");

    auto tick = nb::class_<Tick>(m, "Tick", time_unit_docstrings::kTickDoc)
                    .def(nb::init<>())
                    .def("__repr__", [](const Tick&) { return "symusic.core.Tick"; })
                    .def("is_time_unit", [](const Tick&) { return true; })
                    .def("__getstate__", [](const Tick&) { return nb::bytes(""); })
                    .def("__setstate__", [](Tick& self, const nb::bytes&) { new (&self) Tick(); });

    auto quarter
        = nb::class_<Quarter>(m, "Quarter", time_unit_docstrings::kQuarterDoc)
              .def(nb::init<>())
              .def("__repr__", [](const Quarter&) { return "symusic.core.Quarter"; })
              .def("is_time_unit", [](const Quarter&) { return true; })
              .def("__getstate__", [](const Quarter&) { return nb::bytes(""); })
              .def("__setstate__", [](Quarter& self, const nb::bytes&) { new (&self) Quarter(); });

    auto second
        = nb::class_<Second>(m, "Second", time_unit_docstrings::kSecondDoc)
              .def(nb::init<>())
              .def("__repr__", [](const Second&) { return "symusic.core.Second"; })
              .def("is_time_unit", [](const Second&) { return true; })
              .def("__getstate__", [](const Second&) { return nb::bytes(""); })
              .def("__setstate__", [](Second& self, const nb::bytes&) { new (&self) Second(); });

    tick.def("__eq__", [](const Tick&, const nb::object& other) {
        return nb::isinstance<Tick>(other);
    });
    quarter.def("__eq__", [](const Quarter&, const nb::object& other) {
        return nb::isinstance<Quarter>(other);
    });
    second.def("__eq__", [](const Second&, const nb::object& other) {
        return nb::isinstance<Second>(other);
    });

    REPEAT_ON(
        BIND_EVENT,
        bind_note,
        bind_keysig,
        bind_timesig,
        bind_tempo,
        bind_controlchange,
        bind_pedal,
        bind_pitchbend,
        bind_textmeta,
        bind_track,
        bind_score
    )
#undef BIND_EVENT

    bind_synthesizer(m);
}

}   // namespace symusic

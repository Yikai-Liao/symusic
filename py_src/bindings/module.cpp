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

#include "MetaMacro.h"
#include "../utils/python_helpers.h"
#include "binding_common.h"
#include "event_bindings.h"
#include "score_bindings.h"
#include "track_bindings.h"
#include "synthesizer_bindings.h"

#pragma warning(disable : 4996)

namespace nb = nanobind;

namespace symusic {

using namespace pyutils;

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

    auto tick = nb::class_<Tick>(m, "Tick")
        .def(nb::init<>())
        .def("__repr__", [](const Tick&) { return "symusic.core.Tick"; })
        .def("is_time_unit", [](const Tick&) { return true; })
        .def("__getstate__", [](const Tick&) { return nb::bytes(""); })
        .def("__setstate__", [](Tick& self, const nb::bytes&) { new (&self) Tick(); });

    auto quarter = nb::class_<Quarter>(m, "Quarter")
        .def(nb::init<>())
        .def("__repr__", [](const Quarter&) { return "symusic.core.Quarter"; })
        .def("is_time_unit", [](const Quarter&) { return true; })
        .def("__getstate__", [](const Quarter&) { return nb::bytes(""); })
        .def("__setstate__", [](Quarter& self, const nb::bytes&) { new (&self) Quarter(); });

    auto second = nb::class_<Second>(m, "Second")
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
        bind_note, bind_keysig, bind_timesig, bind_tempo,
        bind_controlchange, bind_pedal, bind_pitchbend, bind_textmeta,
        bind_track, bind_score
    )
#undef BIND_EVENT

    bind_synthesizer(m);
}

}   // namespace symusic

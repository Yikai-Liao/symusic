//
// Entry point of the nanobind module symusic._core.
//

#include "binding_prelude.h"

#include "../../utils/filesystem_error.h"
#include "utils/storage.h"
#include "binding_registration.h"
#include "../synth/synthesizer_bindings.h"

#pragma warning(disable : 4996)

namespace nb = nanobind;

namespace symusic::py {

NB_MODULE(_core, m) {
    register_filesystem_error_translator();
#ifndef MEM_LEAK_WARNING
    nb::set_leak_warnings(false);
    m.attr("_nanobind_mem_leak_warning") = false;
#else
    m.attr("_nanobind_mem_leak_warning") = true;
#endif

    nb::class_<Tick>(m, "Tick", "Tick()\n\nMIDI ticks.").def(nb::init<>());
    nb::class_<Quarter>(m, "Quarter", "Quarter()\n\nQuarter notes.").def(nb::init<>());
    nb::class_<Second>(m, "Second", "Second()\n\nSeconds on the tempo map.").def(nb::init<>());
    bind_event_types(m);
    bind_track_types(m);
    bind_score_types(m);
    bind_synthesizer(m);
}

}   // namespace symusic::py

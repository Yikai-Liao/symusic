//
// Tempo, control change, pedal and pitch bend bindings; see register_events.cpp.
//
#include "../core/binding_prelude.h"

#include "MetaMacro.h"
#include "event_bindings.h"

namespace nb = nanobind;

namespace symusic {

void bind_controller_family_types(nb::module_& m) {
#define BIND_EVENT(__COUNT, BIND_FUNC) \
    BIND_FUNC<Tick>(m, "Tick");        \
    BIND_FUNC<Quarter>(m, "Quarter");  \
    BIND_FUNC<Second>(m, "Second");

    REPEAT_ON(
        BIND_EVENT,
        pyutils::bind_tempo,
        pyutils::bind_controlchange,
        pyutils::bind_pedal,
        pyutils::bind_pitchbend
    )

#undef BIND_EVENT
}

}   // namespace symusic

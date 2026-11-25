//
// Tempo, control change, pedal and pitch bend bindings; see register_events.cpp.
//
#include "../core/binding_prelude.h"

#include "../core/binding_common.h"
#include "event_bindings.h"

namespace nb = nanobind;

namespace symusic::py {

void bind_controller_family_types(nb::module_& m) {
    bind_units(m, []<TType T>(nb::module_& module, const std::string& name) {
        bind_tempo<T>(module, name);
    });
    bind_units(m, []<TType T>(nb::module_& module, const std::string& name) {
        bind_controlchange<T>(module, name);
    });
    bind_units(m, []<TType T>(nb::module_& module, const std::string& name) {
        bind_pedal<T>(module, name);
    });
    bind_units(m, []<TType T>(nb::module_& module, const std::string& name) {
        bind_pitchbend<T>(module, name);
    });
}

}   // namespace symusic::py

//
// Registers the event bindings. The two families compile in separate translation units, because
// one unit with all of them dominated the build time.
//
#include "../core/binding_prelude.h"

#include "../core/binding_registration.h"

namespace nb = nanobind;

namespace symusic {

void bind_note_family_types(nb::module_& m);
void bind_controller_family_types(nb::module_& m);

void bind_event_types(nb::module_& m) {
    bind_note_family_types(m);
    bind_controller_family_types(m);
}

}   // namespace symusic

//
// Aggregate time-event registration after splitting the heaviest nanobind bindings into smaller
// translation units. The original all-in-one registration path created a single dominant compile
// bottleneck during `pip install`, so this dispatcher preserves the public API while letting Ninja
// schedule the expensive families in parallel.
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

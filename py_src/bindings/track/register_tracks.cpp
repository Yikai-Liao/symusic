//
// Track bindings, in their own translation unit to shorten the build.
//
#include "../core/binding_prelude.h"

#include "../core/binding_common.h"
#include "../core/binding_registration.h"
#include "track_bindings.h"

namespace nb = nanobind;

namespace symusic::py {

void bind_track_types(nb::module_& m) {
    bind_units(m, []<TType T>(nb::module_& module, const std::string& name) {
        bind_track<T>(module, name);
    });
}

}   // namespace symusic::py

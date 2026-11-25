#include "../core/binding_prelude.h"

#include "track_bindings.h"

namespace symusic::py {

void bind_track_types(nb::module_& module) { bind_track(module); }

}   // namespace symusic::py

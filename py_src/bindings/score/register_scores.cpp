#include "../core/binding_prelude.h"

#include <nanobind/stl/filesystem.h>

#include "score_bindings.h"

namespace symusic::py {

void bind_score_types(nb::module_& module) { bind_score(module); }

}   // namespace symusic::py

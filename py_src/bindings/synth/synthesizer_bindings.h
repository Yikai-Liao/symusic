//
// Synthesizer binding helpers.
//

#pragma once

#include <nanobind/nanobind.h>

namespace symusic::py {

nanobind::module_& bind_synthesizer(nanobind::module_& m);

}   // namespace symusic::py

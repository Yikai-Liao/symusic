//
// Registration functions defined in the separate binding translation units.
//

#pragma once

#include <nanobind/nanobind.h>

namespace symusic {

void bind_event_types(nanobind::module_& m);
void bind_track_types(nanobind::module_& m);
void bind_score_types(nanobind::module_& m);

}   // namespace symusic

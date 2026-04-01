//
// Register track bindings in a dedicated translation unit so the large nanobind templates do not
// share one oversized compile unit with the rest of the module. This keeps the registration logic
// identical while improving parallel build throughput.
//
#include "../core/binding_prelude.h"

#include "MetaMacro.h"
#include "../core/binding_registration.h"
#include "track_bindings.h"

namespace nb = nanobind;

namespace symusic {

void bind_track_types(nb::module_& m) {
#define BIND_TRACK(__COUNT, BIND_FUNC) \
    BIND_FUNC<Tick>(m, "Tick");        \
    BIND_FUNC<Quarter>(m, "Quarter");  \
    BIND_FUNC<Second>(m, "Second");

    REPEAT_ON(BIND_TRACK, bind_track)

#undef BIND_TRACK
}

}   // namespace symusic

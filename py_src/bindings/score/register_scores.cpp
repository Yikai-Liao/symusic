//
// Register score bindings in a dedicated translation unit for the same reason as the other split
// registration files: reduce the size of the hottest compile units and expose more work to Ninja
// during wheel builds without altering runtime behavior.
//
#include "../core/binding_prelude.h"

#include "MetaMacro.h"
#include "../core/binding_registration.h"
#include "score_bindings.h"

namespace nb = nanobind;

namespace symusic {

void bind_score_types(nb::module_& m) {
#define BIND_SCORE(__COUNT, BIND_FUNC) \
    BIND_FUNC<Tick>(m, "Tick");        \
    BIND_FUNC<Quarter>(m, "Quarter");  \
    BIND_FUNC<Second>(m, "Second");

    REPEAT_ON(BIND_SCORE, bind_score)

#undef BIND_SCORE
}

}   // namespace symusic

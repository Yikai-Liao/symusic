//
// Score bindings, in their own translation unit to shorten the build.
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

#pragma once

#include <limits>
#include <stdexcept>

#include "symusic/synth.h"

TEST_CASE("Synthesizer validates settings before opening the runtime", "[symusic][synth]") {
    using symusic::Synthesizer;
    CHECK_THROWS_AS(Synthesizer({}, {}, {.sample_rate = 0}), std::invalid_argument);
    CHECK_THROWS_AS(Synthesizer({}, {}, {.polyphony = 0}), std::invalid_argument);
    CHECK_THROWS_AS(
        Synthesizer({}, {}, {.tail_seconds = std::numeric_limits<double>::quiet_NaN()}),
        std::invalid_argument
    );
}

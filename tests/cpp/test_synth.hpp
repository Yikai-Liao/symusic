#pragma once

#include <array>
#include <functional>
#include <future>
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

#ifdef SYMUSIC_TEST_FLUID_RUNTIME
TEST_CASE("Concurrent renders serialize FluidSynth initialization", "[symusic][synth]") {
    using namespace symusic;
    // The fixture ignores SoundFont contents. The constructor still checks that it exists.
    const Synthesizer  first(SYMUSIC_TEST_FLUID_RUNTIME, SYMUSIC_TEST_FLUID_RUNTIME);
    const Synthesizer  second(SYMUSIC_TEST_FLUID_RUNTIME, SYMUSIC_TEST_FLUID_RUNTIME);
    std::promise<void> start;
    const auto         ready  = start.get_future().share();
    const auto         render = [&](const Synthesizer& synth) {
        ready.wait();
        return synth.render(Score<Tick>{480}).frames();
    };
    std::array workers{
        std::async(std::launch::async, render, std::cref(first)),
        std::async(std::launch::async, render, std::cref(second)),
        std::async(std::launch::async, render, std::cref(first)),
        std::async(std::launch::async, render, std::cref(second)),
    };
    start.set_value();
    for (auto& worker : workers) { CHECK(worker.get() == 44100); }
}
#endif

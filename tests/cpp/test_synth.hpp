#pragma once

#include <array>
#include <functional>
#include <future>
#include <limits>
#include <stdexcept>

#include "symusic/synth.h"

TEST_CASE("An empty AudioBuffer has zero frames", "[symusic][synth]") {
    const auto audio = symusic::AudioBuffer{};
    CHECK(audio.frames() == 0);
}

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
TEST_CASE("Concurrent renders serialize FluidSynth sample-cache access", "[symusic][synth]") {
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
    auto       failing_render = std::async(std::launch::async, [&] {
        Score<Tick> score{480};
        score.tracks.emplace_back();
        score.tracks.back().notes.push_back(detail::make_event<Note<Tick>>(0, 480, 60, 100));
        score.tracks.back().notes.back().pitch = -1;
        ready.wait();
        try {
            (void)first.render(score);
        } catch (const std::invalid_argument&) { return true; }
        return false;
    });
    std::array workers{
        std::async(std::launch::async, render, std::cref(first)),
        std::async(std::launch::async, render, std::cref(second)),
        std::async(std::launch::async, render, std::cref(first)),
        std::async(std::launch::async, render, std::cref(second)),
    };
    start.set_value();
    for (auto& worker : workers) { CHECK(worker.get() == 44100); }
    CHECK(failing_render.get());
}
#endif

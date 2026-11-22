#include "../core/binding_prelude.h"
#include "synthesizer_bindings.h"
#include "utils/storage.h"

#include <nanobind/ndarray.h>
#include <nanobind/stl/filesystem.h>

#include "symusic/synth.h"

namespace nb = nanobind;

namespace symusic::py {
namespace {

std::filesystem::path fluid_library_path() {
    const auto runtime = nb::module_::import_("symusic._vendor.fluidsynth._library");
    return nb::cast<std::filesystem::path>(runtime.attr("library_path")());
}

using AudioArray = nb::ndarray<nb::numpy, float, nb::ndim<2>, nb::c_contig, nb::device::cpu>;

template<TType T>
AudioArray render(const Synthesizer& synth, const shared<Score<T>>& score, bool stereo) {
    auto        audio = std::make_unique<AudioBuffer>(synth.render(*score, stereo));
    nb::capsule owner(audio.get(), [](void* data) noexcept {
        delete static_cast<AudioBuffer*>(data);
    });
    auto*       buffer = audio.release();
    // NumPy must distinguish an empty buffer from a null pointer.
    static float empty_sample = 0;
    auto*        samples      = buffer->samples.empty() ? &empty_sample : buffer->samples.data();
    return {samples, {buffer->frames(), buffer->channels}, owner};
}

constexpr auto kRenderDoc = R"pbdoc(
Render a score in any time unit to a C-contiguous float32 NumPy array of shape
(frames, 2) in stereo or (frames, 1) in mono. MIDI events are aligned to FluidSynth
blocks. The configured release tail follows the final event block.
)pbdoc";

}   // namespace

nb::module_& bind_synthesizer(nb::module_& m) {
    nb::class_<Synthesizer>(
        m, "Synthesizer", "Render scores offline with a local SF2 or SF3 SoundFont."
    )
        .def(
            "__init__",
            [](Synthesizer*                 self,
               const std::filesystem::path& soundfont,
               int                          sample_rate,
               int                          polyphony,
               bool                         reverb,
               bool                         chorus,
               double                       tail_seconds) {
                const auto library_path = fluid_library_path();
                new (self) Synthesizer(
                    library_path, soundfont, {sample_rate, polyphony, reverb, chorus, tail_seconds}
                );
            },
            nb::arg("soundfont"),
            nb::kw_only(),
            nb::arg("sample_rate")  = 44100,
            nb::arg("polyphony")    = 256,
            nb::arg("reverb")       = false,
            nb::arg("chorus")       = false,
            nb::arg("tail_seconds") = 1.0,
            "Configure offline SoundFont rendering."
        )
        .def(
            "render",
            render<Tick>,
            nb::arg("score"),
            nb::kw_only(),
            nb::arg("stereo") = true,
            kRenderDoc
        )
        .def(
            "render",
            render<Quarter>,
            nb::arg("score"),
            nb::kw_only(),
            nb::arg("stereo") = true,
            kRenderDoc
        )
        .def(
            "render",
            render<Second>,
            nb::arg("score"),
            nb::kw_only(),
            nb::arg("stereo") = true,
            kRenderDoc
        );
    return m;
}

}   // namespace symusic::py

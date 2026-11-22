//
// Implementation of the Synthesizer bindings.
//

#include "utils/storage.h"
#include "../core/binding_prelude.h"
#include "synthesizer_bindings.h"

#include <nanobind/eigen/dense.h>
#include <nanobind/ndarray.h>
#include <nanobind/stl/filesystem.h>

#include "prestosynth/wav.h"
#include "symusic.h"

namespace nb = nanobind;

namespace symusic {

namespace synth_docstrings {
constexpr const char* kSynthDoc   = R"pbdoc(
Renders scores to audio with an SF2 or SF3 SoundFont, using prestosynth.

*sample_rate* is the output rate in Hz. *quality*, 0-4, selects how SoundFont samples are resampled
to it: 0 is linear interpolation, 1 zero-order hold, 2-4 sinc from fastest to best. Other values
raise ``ValueError``. Loading reads the whole SoundFont, so reuse one synthesizer for many scores.
)pbdoc";
constexpr const char* kRenderDoc  = R"pbdoc(
Render *score*, in any time unit, to a ``float32`` array of shape ``(channels, samples)``: 2
channels with *stereo*, otherwise 1. Only notes are rendered; pedals, pitch bends and control
changes have no effect. Samples are not normalized and can exceed 1.
)pbdoc";
constexpr const char* kDumpWavDoc = R"pbdoc(
Write *data*, a ``float32`` array of shape ``(channels, samples)``, to a WAV file. With *use_int16*
the file holds 16-bit PCM, which clips samples beyond 1; otherwise 32-bit float.
)pbdoc";
}   // namespace synth_docstrings

nb::module_& bind_synthesizer(nb::module_& m) {
    nb::class_<Synthesizer>(m, "Synthesizer", synth_docstrings::kSynthDoc)
        .def(
            nb::init<const std::string&, u32, u8>(),
            nb::arg("sf_path"),
            nb::arg("sample_rate"),
            nb::arg("quality")
        )
        .def(
            nb::init<const std::filesystem::path&, u32, u8>(),
            nb::arg("sf_path"),
            nb::arg("sample_rate"),
            nb::arg("quality")
        )
        .def(
            "render",
            [](Synthesizer& self, const shared<py::Score<Tick>>& score, const bool stereo) {
                return self.render(py::copy_score_values(*score), stereo);
            },
            nb::arg("score"),
            nb::arg("stereo") = true,
            synth_docstrings::kRenderDoc
        )
        .def(
            "render",
            [](Synthesizer& self, const shared<py::Score<Quarter>>& score, const bool stereo) {
                return self.render(py::copy_score_values(*score), stereo);
            },
            nb::arg("score"),
            nb::arg("stereo") = true,
            synth_docstrings::kRenderDoc
        )
        .def(
            "render",
            [](Synthesizer& self, const shared<py::Score<Second>>& score, const bool stereo) {
                return self.render(py::copy_score_values(*score), stereo);
            },
            nb::arg("score"),
            nb::arg("stereo") = true,
            synth_docstrings::kRenderDoc
        );

    m.def(
        "dump_wav",
        [](const std::string&                                                        path,
           const nb::ndarray<f32, nb::shape<-1, -1>, nb::device::cpu, nb::f_contig>& data,
           const i32                                                                 sample_rate,
           const bool                                                                use_int16) {
            psynth::WAVE_write(
                path, data.shape(0), data.shape(1), sample_rate, data.data(), use_int16
            );
        },
        nb::arg("path"),
        nb::arg("data"),
        nb::arg("sample_rate"),
        nb::arg("use_int16") = true,
        synth_docstrings::kDumpWavDoc
    );
    return m;
}

}   // namespace symusic

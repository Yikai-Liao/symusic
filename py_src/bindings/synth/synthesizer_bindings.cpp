//
// Implementation of the Synthesizer bindings.
//

#include "synthesizer_bindings.h"

#include <nanobind/ndarray.h>
#include <nanobind/stl/filesystem.h>
#include <nanobind/stl/string.h>

#include "prestosynth/wav.h"
#include "symusic.h"

namespace nb = nanobind;

namespace symusic {

namespace synth_docstrings {
constexpr const char* kSynthDoc
    = R"pbdoc(High-level Prestosynth wrapper that renders Scores with SF2/SF3 SoundFonts. Reuse a single synthesizer per SoundFont to avoid disk I/O.)pbdoc";
constexpr const char* kSynthInitStringDoc
    = R"pbdoc(Create a synthesizer from a string path to an SF2/SF3 SoundFont.)pbdoc";
constexpr const char* kSynthInitPathDoc
    = R"pbdoc(Create a synthesizer from a pathlib.Path pointing to an SF2/SF3 SoundFont.)pbdoc";
constexpr const char* kSynthRenderDoc
    = R"pbdoc(Render a Score into a float32 waveform. When *stereo* is True, two channels are emitted.)pbdoc";
constexpr const char* kDumpWavDoc
    = R"pbdoc(Write a NumPy buffer to a WAV file using Prestosynth's writer. Set *use_int16* to quantize to PCM16.)pbdoc";
}   // namespace synth_docstrings

nb::module_& bind_synthesizer(nb::module_& m) {
    nb::class_<Synthesizer>(m, "Synthesizer", synth_docstrings::kSynthDoc)
        .def(
            nb::init<const std::string&, u32, u8>(),
            nb::arg("sf_path"),
            nb::arg("sample_rate"),
            nb::arg("quality"),
            synth_docstrings::kSynthInitStringDoc
        )
        .def(
            nb::init<const std::filesystem::path&, u32, u8>(),
            nb::arg("sf_path"),
            nb::arg("sample_rate"),
            nb::arg("quality"),
            synth_docstrings::kSynthInitPathDoc
        )
        .def(
            "render",
            [](Synthesizer& self, const shared<Score<Tick>>& score, const bool stereo) {
                return self.render(*score, stereo);
            },
            nb::arg("score"),
            nb::arg("stereo") = true,
            synth_docstrings::kSynthRenderDoc
        )
        .def(
            "render",
            [](Synthesizer& self, const shared<Score<Quarter>>& score, const bool stereo) {
                return self.render(*score, stereo);
            },
            nb::arg("score"),
            nb::arg("stereo") = true,
            synth_docstrings::kSynthRenderDoc
        )
        .def(
            "render",
            [](Synthesizer& self, const shared<Score<Second>>& score, const bool stereo) {
                return self.render(*score, stereo);
            },
            nb::arg("score"),
            nb::arg("stereo") = true,
            synth_docstrings::kSynthRenderDoc
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

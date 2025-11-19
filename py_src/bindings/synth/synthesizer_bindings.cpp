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
constexpr const char* kSynthDoc = R"pbdoc(
Synthesizer(sf_path: str | Path, sample_rate: int = 44100, quality: int = 0)
============================================================================

Prestosynth-powered renderer that turns scores into audio buffers using SF2/SF3 soundfonts. Reuse
a single synthesizer per soundfont to avoid repeated disk I/O.

Parameters
----------
sf_path:
    Filesystem path to an SF2/SF3 soundfont (either ``str`` or ``pathlib.Path``).
sample_rate:
    Output sample rate in Hz.
quality:
    Prestosynth render quality hint (``0`` = default).

Examples
--------
>>> from symusic import Score, Synthesizer
>>> score = Score.from_file("example.mid")
>>> synth = Synthesizer("MuseScore_General.sf3", sample_rate=48000)
>>> buffer = synth.render(score, stereo=True)
>>> Synthesizer.dump_wav("example.wav", buffer, 48000, use_int16=True)
)pbdoc";
constexpr const char* kSynthInitStringDoc
    = R"pbdoc(Create a synthesizer from a string path to an SF2/SF3 soundfont.)pbdoc";
constexpr const char* kSynthInitPathDoc
    = R"pbdoc(Create a synthesizer from a ``pathlib.Path`` pointing to an SF2/SF3 soundfont.)pbdoc";
constexpr const char* kSynthRenderDoc = R"pbdoc(
Render a :class:`symusic.Score` into a float32 waveform.

Parameters
----------
score:
    Tick/quarter/second score to render.
stereo:
    When ``True`` two channels are emitted; otherwise mono.
)pbdoc";
constexpr const char* kDumpWavDoc = R"pbdoc(
Write a NumPy buffer to a WAV file using Prestosynth's writer.

Parameters
----------
path:
    Destination path.
buffer:
    NumPy ndarray with shape ``(samples,)`` or ``(channels, samples)``.
sample_rate:
    Sample rate in Hz.
use_int16:
    Set to ``True`` to quantize the float32 buffer to PCM16.
)pbdoc";
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

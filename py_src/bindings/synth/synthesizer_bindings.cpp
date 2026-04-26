//
// Implementation of the Synthesizer bindings.
//

#include "../core/binding_prelude.h"
#include "synthesizer_bindings.h"

#include <vector>

#include <nanobind/ndarray.h>
#include <nanobind/stl/filesystem.h>

#include "symusic.h"

namespace nb = nanobind;

namespace symusic {

namespace {

void dump_wav_1d(
    const std::filesystem::path& path,
    const nb::ndarray<const f32, nb::ndim<1>, nb::device::cpu>& data,
    const i32                    sampleRate,
    const bool                   useInt16
) {
    write_wav(path, data.data(), 1, data.shape(0), sampleRate, useInt16);
}

void dump_wav_2d(
    const std::filesystem::path& path,
    const nb::ndarray<const f32, nb::ndim<2>, nb::device::cpu>& data,
    const i32                    sampleRate,
    const bool                   useInt16
) {
    const auto channels = static_cast<size_t>(data.shape(0));
    const auto frames = static_cast<size_t>(data.shape(1));
    std::vector<float> contiguous(channels * frames);

    const auto rowStride = static_cast<size_t>(data.stride(0) / static_cast<int64_t>(sizeof(float)));
    const auto colStride = static_cast<size_t>(data.stride(1) / static_cast<int64_t>(sizeof(float)));
    const auto* base = data.data();
    for (size_t channel = 0; channel < channels; ++channel) {
        for (size_t frame = 0; frame < frames; ++frame) {
            contiguous[channel * frames + frame] = base[channel * rowStride + frame * colStride];
        }
    }

    write_wav(path, contiguous.data(), channels, frames, sampleRate, useInt16);
}

template<TType T>
nb::ndarray<nb::numpy, float> render_score(
    Synthesizer&          self,
    const shared<Score<T>>& score,
    const bool            stereo
) {
    auto buffer = self.render(*score, stereo);
    auto* data = buffer.release();
    nb::capsule owner(data, [](void* ptr) noexcept { delete[] static_cast<float*>(ptr); });
    const auto [channels, frames] = buffer.dims();
    return nb::ndarray<nb::numpy, float>(data, {channels, frames}, owner);
}

}   // namespace

namespace synth_docstrings {
constexpr const char* kSynthDoc = R"pbdoc(
Synthesizer(
    sf_path: str | pathlib.Path,
    sample_rate: int = 44100,
    *,
    gain: float = 0.2,
    polyphony: int = 256,
    interpolation: int = 4,
    reverb: bool = True,
    reverb_roomsize: float = 0.2,
    reverb_damp: float = 0.0,
    reverb_width: float = 0.5,
    reverb_level: float = 0.9,
    chorus: bool = True,
    chorus_nr: int = 3,
    chorus_level: float = 2.0,
    chorus_speed: float = 0.3,
    chorus_depth: float = 8.0,
    chorus_waveform: int = 0,
)
============================================================================

FluidLite-powered renderer that turns scores into float32 audio buffers using SF2/SF3 soundfonts.
Reuse a single synthesizer instance to render multiple scores with the same loaded soundfont and
settings.
)pbdoc";
constexpr const char* kSynthRenderDoc = R"pbdoc(
Render a :class:`symusic.Score` into a float32 waveform with shape ``(channels, frames)``.

Parameters
----------
score:
    Tick/quarter/second score to render.
stereo:
    When ``True`` two channels are emitted; otherwise a mono buffer is returned.
)pbdoc";
constexpr const char* kDumpWavDoc = R"pbdoc(
Write a float32 waveform to a WAV file.

Parameters
----------
path :
    Destination path.
data :
    :class:`numpy.ndarray` with shape ``(frames,)`` or ``(channels, frames)``.
sample_rate :
    Sample rate in Hz.
use_int16 :
    Set to ``True`` to quantize the float32 buffer to PCM16. When ``False``,
    the file is written as float32 PCM.
)pbdoc";
}   // namespace synth_docstrings

nb::module_& bind_synthesizer(nb::module_& m) {
    nb::enum_<SynthInterpolation>(m, "SynthInterpolation")
        .value("none", SynthInterpolation::none)
        .value("linear", SynthInterpolation::linear)
        .value("fourth_order", SynthInterpolation::fourth_order)
        .value("seventh_order", SynthInterpolation::seventh_order);

    nb::enum_<ChorusWaveform>(m, "ChorusWaveform")
        .value("sine", ChorusWaveform::sine)
        .value("triangle", ChorusWaveform::triangle);

    nb::class_<Synthesizer>(m, "Synthesizer", synth_docstrings::kSynthDoc)
        .def(
            nb::init<
                const std::string&,
                u32,
                f64,
                u32,
                SynthInterpolation,
                bool,
                f64,
                f64,
                f64,
                f64,
                bool,
                u32,
                f64,
                f64,
                f64,
                ChorusWaveform
            >(),
            nb::arg("sf_path"),
            nb::arg("sample_rate") = 44100,
            nb::kw_only(),
            nb::arg("gain") = 0.2,
            nb::arg("polyphony") = 256,
            nb::arg("interpolation") = SynthInterpolation::fourth_order,
            nb::arg("reverb") = true,
            nb::arg("reverb_roomsize") = 0.2,
            nb::arg("reverb_damp") = 0.0,
            nb::arg("reverb_width") = 0.5,
            nb::arg("reverb_level") = 0.9,
            nb::arg("chorus") = true,
            nb::arg("chorus_nr") = 3,
            nb::arg("chorus_level") = 2.0,
            nb::arg("chorus_speed") = 0.3,
            nb::arg("chorus_depth") = 8.0,
            nb::arg("chorus_waveform") = ChorusWaveform::sine
        )
        .def(
            nb::init<
                const std::filesystem::path&,
                u32,
                f64,
                u32,
                SynthInterpolation,
                bool,
                f64,
                f64,
                f64,
                f64,
                bool,
                u32,
                f64,
                f64,
                f64,
                ChorusWaveform
            >(),
            nb::arg("sf_path"),
            nb::arg("sample_rate") = 44100,
            nb::kw_only(),
            nb::arg("gain") = 0.2,
            nb::arg("polyphony") = 256,
            nb::arg("interpolation") = SynthInterpolation::fourth_order,
            nb::arg("reverb") = true,
            nb::arg("reverb_roomsize") = 0.2,
            nb::arg("reverb_damp") = 0.0,
            nb::arg("reverb_width") = 0.5,
            nb::arg("reverb_level") = 0.9,
            nb::arg("chorus") = true,
            nb::arg("chorus_nr") = 3,
            nb::arg("chorus_level") = 2.0,
            nb::arg("chorus_speed") = 0.3,
            nb::arg("chorus_depth") = 8.0,
            nb::arg("chorus_waveform") = ChorusWaveform::sine
        )
        .def("render", &render_score<Tick>, nb::arg("score"), nb::arg("stereo") = true, synth_docstrings::kSynthRenderDoc)
        .def("render", &render_score<Quarter>, nb::arg("score"), nb::arg("stereo") = true, synth_docstrings::kSynthRenderDoc)
        .def("render", &render_score<Second>, nb::arg("score"), nb::arg("stereo") = true, synth_docstrings::kSynthRenderDoc);

    m.def(
        "dump_wav",
        &dump_wav_1d,
        nb::arg("path"),
        nb::arg("data"),
        nb::arg("sample_rate"),
        nb::arg("use_int16") = true,
        synth_docstrings::kDumpWavDoc
    );
    m.def(
        "dump_wav",
        &dump_wav_2d,
        nb::arg("path"),
        nb::arg("data"),
        nb::arg("sample_rate"),
        nb::arg("use_int16") = true,
        synth_docstrings::kDumpWavDoc
    );
    return m;
}

}   // namespace symusic

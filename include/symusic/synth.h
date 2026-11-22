#pragma once

#include <cmath>
#include <filesystem>
#include <stdexcept>

#include <fmt/format.h>

#include "symusic/detail/time_conversion.h"

namespace symusic {

/// Rendered audio as 32-bit float samples, interleaved by frame.
/// Stereo buffers store left then right for each frame; mono buffers store one sample per frame.
struct AudioBuffer {
    vec<f32> samples;
    size_t   channels = 0;

    [[nodiscard]] size_t frames() const { return channels == 0 ? 0 : samples.size() / channels; }
};

/// Rendering options, checked when a Synthesizer is constructed.
/// sample_rate is in Hz, from 8000 to 96000. polyphony is the most voices FluidSynth plays at
/// once, from 1 to 65535. tail_seconds is the finite, nonnegative time rendered after the last
/// event so that releases and effects can decay.
struct SynthSettings {
    int  sample_rate  = 44100;
    int  polyphony    = 256;
    bool reverb       = false;
    bool chorus       = false;
    f64  tail_seconds = 1.0;
};

class Synthesizer {
    struct Backend;

    shared<const Backend> backend;
    std::filesystem::path soundfont;
    SynthSettings         settings;

    [[nodiscard]] AudioBuffer render_seconds(const Score<Second>& score, bool stereo) const;

public:
    /// Load FluidSynth from library_path, which must be absolute, and keep soundfont for rendering.
    /// The SoundFont path is made absolute and must exist now; FluidSynth loads it on each render,
    /// and a file it cannot read or parse fails that render.
    /// Throws std::invalid_argument for invalid settings or a relative library path,
    /// std::filesystem::filesystem_error for a missing SoundFont, and std::runtime_error when the
    /// library cannot be loaded or lacks a required function.
    Synthesizer(
        const std::filesystem::path& library_path,
        const std::filesystem::path& soundfont,
        SynthSettings                settings = {}
    );

    /// Render notes, controls and pitch bends using the score's tempo map.
    /// Pedals and lyrics are ignored. Each track that has notes gets its own FluidSynth channel,
    /// up to 256. The buffer is stereo by default and mono averages the two channels. It ends
    /// tail_seconds after the last event; a score without notes renders zero frames.
    /// Throws std::invalid_argument for a negative or non-finite note duration or event time, a
    /// control or pitch bend value outside its MIDI range, or more than 256 tracks with notes;
    /// std::overflow_error when the buffer would be too large; std::filesystem::filesystem_error
    /// or std::runtime_error when the SoundFont cannot be loaded; and std::runtime_error when
    /// FluidSynth fails.
    template<TType T, typename S>
    [[nodiscard]] AudioBuffer render(const Score<T, S>& score, bool stereo = true) const {
        if constexpr (!std::is_same_v<T, Second>) {
            // Conversion can clamp a negative duration before the scheduler sees it.
            for (const auto& handle : S::get(score.tracks)) {
                for (const auto& note : S::get(S::get(handle).notes)) {
                    if (!std::isfinite(note.duration) || note.duration < 0) {
                        throw std::invalid_argument(
                            fmt::format(
                                "Note duration must be finite and nonnegative, got {}",
                                note.duration
                            )
                        );
                    }
                }
            }
        }
        if constexpr (std::is_same_v<T, Second> && std::is_same_v<S, ValueStorage>) {
            return render_seconds(score, stereo);
        } else {
            const auto    converter = detail::make_time_converter<Second>(score);
            Score<Second> rendering_score;
            const auto&   tracks = S::get(score.tracks);
            rendering_score.tracks.reserve(tracks.size());
            for (const auto& handle : tracks) {
                const auto& track = S::get(handle);
                const auto& notes = S::get(track.notes);
                if (notes.empty()) continue;
                vec<Note<Second>> seconds;
                if constexpr (std::is_same_v<T, Second>) {
                    seconds.assign(notes.begin(), notes.end());
                } else {
                    seconds = converter.template duration_vec<Note>(notes, 0);
                }
                rendering_score.tracks.emplace_back(
                    std::string{},
                    track.program,
                    track.is_drum,
                    std::move(seconds),
                    converter.template time_vec<ControlChange>(S::get(track.controls)),
                    converter.template time_vec<PitchBend>(S::get(track.pitch_bends)),
                    vec<TextMeta<Second>>{}
                );
            }
            return render_seconds(rendering_score, stereo);
        }
    }
};

}   // namespace symusic

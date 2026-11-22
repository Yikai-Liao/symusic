#pragma once

#include <cmath>
#include <filesystem>
#include <stdexcept>

#include "symusic/detail/time_conversion.h"

namespace symusic {

struct AudioBuffer {
    vec<float> samples;
    size_t     channels = 0;

    [[nodiscard]] size_t frames() const { return channels == 0 ? 0 : samples.size() / channels; }
};

struct SynthSettings {
    int    sample_rate  = 44100;
    int    polyphony    = 256;
    bool   reverb       = false;
    bool   chorus       = false;
    double tail_seconds = 1.0;
};

class Synthesizer {
    struct Backend;

    std::shared_ptr<const Backend> backend;
    std::filesystem::path          soundfont;
    SynthSettings                  settings;

    [[nodiscard]] AudioBuffer render_seconds(const Score<Second>& score, bool stereo) const;

public:
    Synthesizer(
        const std::filesystem::path& library_path,
        const std::filesystem::path& soundfont,
        SynthSettings                settings = {}
    );

    /// Render notes, controls and pitch bends using the score's tempo map.
    template<TType T, typename S>
    [[nodiscard]] AudioBuffer render(const Score<T, S>& score, bool stereo = true) const {
        if constexpr (!std::is_same_v<T, Second>) {
            // Conversion can clamp a negative duration before the scheduler sees it.
            for (const auto& handle : S::get(score.tracks)) {
                for (const auto& note : S::get(S::get(handle).notes)) {
                    if (!std::isfinite(note.duration) || note.duration < 0) {
                        throw std::invalid_argument("Note duration must be finite and nonnegative");
                    }
                }
            }
        }
        if constexpr (std::is_same_v<T, Second> && std::is_same_v<S, ValueStorage>) {
            return render_seconds(score, stereo);
        } else {
            const auto converter = detail::make_time_converter<Second>(score);
            Score<Second> rendering_score;
            const auto& tracks = S::get(score.tracks);
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
                    std::string{}, track.program, track.is_drum, std::move(seconds),
                    converter.template time_vec<ControlChange>(S::get(track.controls)),
                    converter.template time_vec<PitchBend>(S::get(track.pitch_bends)),
                    vec<Pedal<Second>>{}, vec<TextMeta<Second>>{}
                );
            }
            return render_seconds(rendering_score, stereo);
        }
    }
};

}   // namespace symusic

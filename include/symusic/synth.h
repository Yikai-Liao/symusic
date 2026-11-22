#pragma once

#include <filesystem>

#include "symusic/score.h"

namespace symusic {

struct AudioBuffer {
    vec<float> samples;
    size_t     channels;

    [[nodiscard]] size_t frames() const { return samples.size() / channels; }
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

public:
    Synthesizer(
        const std::filesystem::path& library_path,
        const std::filesystem::path& soundfont,
        SynthSettings                settings = {}
    );

    template<TType T>
    [[nodiscard]] AudioBuffer render(const Score<T>& score, bool stereo = true) const;
};

}   // namespace symusic

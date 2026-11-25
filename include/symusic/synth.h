#pragma once
#include "prestosynth/synthesizer.h"
#include "symusic/score.h"

#include <filesystem>
#include <stdexcept>
#include <string>

namespace symusic {
class Synthesizer {
private:
    psynth::Synthesizer synthesizer;

    static u8 checked_quality(const u8 quality) {
        if (quality > 4) {
            throw std::invalid_argument(
                "Synthesizer quality must be between 0 and 4, got " + std::to_string(quality)
            );
        }
        return quality;
    }

public:
    /// `quality` selects the libsamplerate converter as `4 - quality`, so only 0-4 are valid;
    /// larger values make every resampled sample silent.
    Synthesizer(const std::string& soundfont_path, u32 sample_rate, u8 quality) :
        synthesizer(soundfont_path, sample_rate, checked_quality(quality), 1) {}

    Synthesizer(const std::filesystem::path& soundfont_path, u32 sample_rate, u8 quality) :
        Synthesizer(soundfont_path.string(), sample_rate, quality) {}

    template<TType T>
    psynth::AudioData render(const Score<T>& score, bool stereo);
};

namespace detail {
psynth::Sequence to_sequence(const Score<Second>& score);
}

}   // namespace symusic

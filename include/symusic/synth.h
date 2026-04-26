#pragma once

#ifndef SYNTH_H
#define SYNTH_H

#include "symusic/score.h"

#include <filesystem>
#include <mutex>
#include <memory>
#include <tuple>

namespace symusic {

enum class SynthInterpolation : i32 {
    none = 0,
    linear = 1,
    fourth_order = 4,
    seventh_order = 7,
};

enum class ChorusWaveform : i32 {
    sine = 0,
    triangle = 1,
};

class AudioBuffer {
private:
    size_t                  channelCount;
    size_t                  frameCount;
    std::unique_ptr<float[]> dataPtr;

public:
    AudioBuffer(size_t channels, size_t frames);
    AudioBuffer(const AudioBuffer&) = delete;
    AudioBuffer& operator=(const AudioBuffer&) = delete;
    AudioBuffer(AudioBuffer&&) noexcept = default;
    AudioBuffer& operator=(AudioBuffer&&) noexcept = default;
    ~AudioBuffer() = default;

    float* data();
    [[nodiscard]] const float* data() const;
    float* release();
    [[nodiscard]] std::tuple<size_t, size_t> dims() const;
};

void write_wav(
    const std::filesystem::path& path,
    const float*                 data,
    size_t                       channels,
    size_t                       frames,
    i32                          sampleRate,
    bool                         useInt16 = true
);

class Synthesizer {
private:
    class Impl;
    std::unique_ptr<Impl> impl;

public:
    Synthesizer(
        const std::string&     sfPath,
        u32                    sampleRate = 44100,
        f64                    gain = 0.2,
        u32                    polyphony = 256,
        SynthInterpolation     interpolation = SynthInterpolation::fourth_order,
        bool                   reverb = true,
        f64                    reverbRoomsize = 0.2,
        f64                    reverbDamp = 0.0,
        f64                    reverbWidth = 0.5,
        f64                    reverbLevel = 0.9,
        bool                   chorus = true,
        u32                    chorusNr = 3,
        f64                    chorusLevel = 2.0,
        f64                    chorusSpeed = 0.3,
        f64                    chorusDepth = 8.0,
        ChorusWaveform         chorusWaveform = ChorusWaveform::sine
    );

    Synthesizer(
        const std::filesystem::path& sfPath,
        u32                          sampleRate = 44100,
        f64                          gain = 0.2,
        u32                          polyphony = 256,
        SynthInterpolation           interpolation = SynthInterpolation::fourth_order,
        bool                         reverb = true,
        f64                          reverbRoomsize = 0.2,
        f64                          reverbDamp = 0.0,
        f64                          reverbWidth = 0.5,
        f64                          reverbLevel = 0.9,
        bool                         chorus = true,
        u32                          chorusNr = 3,
        f64                          chorusLevel = 2.0,
        f64                          chorusSpeed = 0.3,
        f64                          chorusDepth = 8.0,
        ChorusWaveform               chorusWaveform = ChorusWaveform::sine
    );

    Synthesizer(const Synthesizer&) = delete;
    Synthesizer& operator=(const Synthesizer&) = delete;
    Synthesizer(Synthesizer&&) noexcept;
    Synthesizer& operator=(Synthesizer&&) noexcept;
    ~Synthesizer();

    template<TType T>
    AudioBuffer render(const Score<T>& score, bool stereo);
};

namespace details {

struct BankSelect {
    Second::unit time;
    u8           bank;

    auto operator<=>(const BankSelect&) const = default;
};

vec<BankSelect> getBanks(const Track<Second>& track);

}   // namespace details

}   // namespace symusic

#endif   // SYNTH_H

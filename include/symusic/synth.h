//
// Created by nhy on 2024/2/12.
//
#pragma once
#ifndef SYNTH_H
#define SYNTH_H

#include "prestosynth/synthesizer.h"
#include "symusic/score.h"


namespace symusic {
class Synthesizer {
private:
    psynth::Synthesizer synthesizer;

public:
    Synthesizer(const std::string &sfPath, const uint32_t sampleRate, const uint8_t quality):
        synthesizer(sfPath, sampleRate, quality) {}

    psynth::AudioData render_single_thread(const Score<Second> & score, bool stereo);
    psynth::AudioData render(const Score<Second> & score, bool stereo, uint8_t workers);
};

}
#endif //SYNTH_H

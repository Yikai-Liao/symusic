#pragma once
#ifndef SYMUSIC_TEST_SYNTH_HPP
#define SYMUSIC_TEST_SYNTH_HPP

#include <catch2/catch_all.hpp>

#include "symusic/conversion.h"
#include "symusic/synth.h"

using namespace symusic;

TEST_CASE("AudioBuffer owns contiguous audio memory", "[symusic][synth]") {
    AudioBuffer buffer(2, 8);
    const auto [channels, frames] = buffer.dims();

    REQUIRE(channels == 2);
    REQUIRE(frames == 8);

    auto* data = buffer.data();
    data[0] = 0.5f;
    data[7] = -0.25f;
    data[8] = 0.125f;

    REQUIRE(buffer.data()[0] == Catch::Approx(0.5f));
    REQUIRE(buffer.data()[7] == Catch::Approx(-0.25f));
    REQUIRE(buffer.data()[8] == Catch::Approx(0.125f));
}

TEST_CASE("AudioBuffer release transfers ownership", "[symusic][synth]") {
    AudioBuffer buffer(1, 4);
    auto* raw = buffer.release();

    REQUIRE(raw != nullptr);
    REQUIRE_THROWS_AS(buffer.data(), std::runtime_error);

    delete[] raw;
}

TEST_CASE("Bank routing follows drum and CC0 semantics", "[symusic][synth]") {
    Track<Second> track("Piano", 0, false);
    track.controls->push_back(ControlChange<Second>(0.5, 0, 8));
    track.controls->push_back(ControlChange<Second>(1.0, 32, 99));

    const auto banks = details::getBanks(track);
    REQUIRE(banks.size() == 3);
    REQUIRE(banks[0].time == Catch::Approx(0.0));
    REQUIRE(banks[0].bank == 0);
    REQUIRE(banks[1].time == Catch::Approx(0.5));
    REQUIRE(banks[1].bank == 8);

    Track<Second> drumTrack("Drums", 0, true);
    const auto drumBanks = details::getBanks(drumTrack);
    REQUIRE(drumBanks.size() == 2);
    REQUIRE(drumBanks[0].bank == 128);
    REQUIRE(drumBanks[1].bank == 128);
}

TEST_CASE("Track routing reserves channel 9 for drums", "[symusic][synth]") {
    Score<Second> score;

    auto drumA = std::make_shared<Track<Second>>("Drum A", 0, true);
    auto melodicA = std::make_shared<Track<Second>>("Lead", 10, false);
    auto drumB = std::make_shared<Track<Second>>("Drum B", 0, true);
    auto melodicB = std::make_shared<Track<Second>>("Bass", 32, false);

    score.tracks->push_back(drumA);
    score.tracks->push_back(melodicA);
    score.tracks->push_back(drumB);
    score.tracks->push_back(melodicB);

    const auto channels = details::route_track_channels(score);
    REQUIRE(channels.size() == 4);
    REQUIRE(channels[0] == 9);
    REQUIRE(channels[1] == 0);
    REQUIRE(channels[2] == 9);
    REQUIRE(channels[3] == 1);
}

TEST_CASE("Track routing rejects the 256th melodic track", "[symusic][synth]") {
    Score<Second> score;
    for (int i = 0; i < 256; ++i) {
        score.tracks->push_back(std::make_shared<Track<Second>>("Melodic", 0, false));
    }

    REQUIRE_THROWS_WITH(
        details::route_track_channels(score),
        Catch::Matchers::ContainsSubstring("reserves channel 9 for drum tracks")
            && Catch::Matchers::ContainsSubstring("255 internal channels")
            && Catch::Matchers::ContainsSubstring("does not reuse channels")
    );
}

TEST_CASE("Track routing allows more than 256 total tracks when melodic tracks fit", "[symusic][synth]") {
    Score<Second> score;
    for (int i = 0; i < 255; ++i) {
        score.tracks->push_back(std::make_shared<Track<Second>>("Melodic", 0, false));
    }
    for (int i = 0; i < 4; ++i) {
        score.tracks->push_back(std::make_shared<Track<Second>>("Drum", 0, true));
    }

    const auto channels = details::route_track_channels(score);
    REQUIRE(channels.size() == 259);
    REQUIRE(channels[0] == 0);
    REQUIRE(channels[8] == 8);
    REQUIRE(channels[9] == 10);
    REQUIRE(channels[254] == 255);
    REQUIRE(channels[255] == 9);
    REQUIRE(channels[258] == 9);
}

TEST_CASE("Tick scores convert to seconds before rendering", "[symusic][synth]") {
    Score<Tick> tickScore(480);
    tickScore.tempos->push_back(Tempo<Tick>(0, 500000));
    tickScore.tempos->push_back(Tempo<Tick>(960, 1000000));

    auto track = std::make_shared<Track<Tick>>("Piano", 0, false);
    track->notes->push_back(Note<Tick>(0, 480, 60, 80));
    track->notes->push_back(Note<Tick>(960, 480, 64, 80));
    track->notes->push_back(Note<Tick>(1440, 480, 67, 80));
    tickScore.tracks->push_back(track);

    const auto secondScore = convert<Second>(tickScore);
    REQUIRE(secondScore.tracks->size() == 1);
    REQUIRE(std::abs(secondScore.tracks->at(0)->notes->at(0).start() - 0.0) < 0.01);
    REQUIRE(std::abs(secondScore.tracks->at(0)->notes->at(1).start() - 1.0) < 0.01);
    REQUIRE(std::abs(secondScore.tracks->at(0)->notes->at(2).start() - 2.0) < 0.01);
    REQUIRE(std::abs(secondScore.tracks->at(0)->notes->at(0).duration - 0.5) < 0.01);
    REQUIRE(std::abs(secondScore.tracks->at(0)->notes->at(1).duration - 1.0) < 0.01);
    REQUIRE(std::abs(secondScore.tracks->at(0)->notes->at(2).duration - 1.0) < 0.01);
}

#endif   // SYMUSIC_TEST_SYNTH_HPP

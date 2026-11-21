#pragma once

#include <array>
#include <stdexcept>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_exception.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include "symusic/io/abc.h"

namespace symusic::abc_test {

inline const std::array<std::string, 3> kVersionLines{"", "%abc-2.0\n", "%abc-2.1\n"};
inline const std::string                kHeader       = "X:1\nT:\nM:4/4\nL:1/4\nK:C\n";
inline const std::string                kStrictHeader = "%abc-2.1\n" + kHeader;
inline constexpr std::string_view       kStressProfile
    = "%%MIDI ptstress 4 112 1.4 75 0.6 102 1.2 65 0.8\n";

inline void rejects(std::string_view source, const std::string& diagnostic) {
    CHECK_THROWS_MATCHES(
        abc::read(source, "case.abc"),
        std::invalid_argument,
        Catch::Matchers::MessageMatches(Catch::Matchers::StartsWith("case.abc:" + diagnostic))
    );
}

using Notes  = vec<std::tuple<i32, i32, i32>>;   // onset, duration, pitch
using Onsets = vec<std::pair<i32, i32>>;         // onset, pitch
using Lyrics = vec<std::pair<i32, std::string>>;

struct Passage {
    const char* music;
    Notes       notes;
};

inline Notes note_spans(const Track<Tick>& track) {
    Notes result;
    for (const auto& note : *track.notes) result.emplace_back(note.time, note.duration, note.pitch);
    return result;
}

inline Notes melody(const Score<Tick>& score) {
    REQUIRE(score.tracks->size() == 1);
    return note_spans(*score.tracks->front());
}

inline vec<i32> pitches(const Track<Tick>& track) {
    vec<i32> result;
    for (const auto& note : *track.notes) result.push_back(note.pitch);
    return result;
}

inline vec<i32> pitches(const Score<Tick>& score) {
    REQUIRE(score.tracks->size() == 1);
    return pitches(*score.tracks->front());
}

inline Onsets onsets(const Track<Tick>& track) {
    Onsets result;
    for (const auto& note : *track.notes) result.emplace_back(note.time, note.pitch);
    return result;
}

inline Lyrics sung(const Score<Tick>& score) {
    REQUIRE_FALSE(score.tracks->empty());
    Lyrics result;
    for (const auto& lyric : *score.tracks->front()->lyrics)
        result.emplace_back(lyric.time, lyric.text);
    return result;
}

inline vec<i32> velocities(const Score<Tick>& score, size_t track = 0) {
    REQUIRE(track < score.tracks->size());
    vec<i32> result;
    for (const auto& note : *score.tracks->at(track)->notes) result.push_back(note.velocity);
    return result;
}

}   // namespace symusic::abc_test

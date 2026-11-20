#include <algorithm>
#include <fstream>
#include <string>

#include <catch2/catch_test_macros.hpp>
#include <picojson/picojson.h>

#include "symusic/io/abc.h"

namespace {
using namespace symusic;
using Json = picojson::value;
using Rows = vec<vec<int>>;

Json fixture(const char* name) {
    std::ifstream input(std::string("testcases/") + name);
    REQUIRE(input.good());
    Json data;
    REQUIRE(picojson::parse(data, input).empty());
    return data;
}

Rows expected_rows(const Json& field) {
    Rows rows;
    if (field.is<picojson::null>()) return rows;
    for (const auto& row : field.get<picojson::array>()) {
        auto& values = rows.emplace_back();
        for (const auto& n : row.get<picojson::array>())
            values.push_back(static_cast<int>(n.get<double>()));
    }
    return rows;
}

template<class Events, class... Fields>
Rows project(const Events& events, Fields... fields) {
    Rows rows;
    for (const auto& e : events) rows.push_back({e.time, static_cast<int>(e.*fields)...});
    return rows;
}

void check_track(const Track<Tick>& track, const Json& expected) {
    using N = Note<Tick>;
    using C = ControlChange<Tick>;
    CHECK(track.program == expected.get("program").get<double>());
    CHECK(track.is_drum == expected.get("drums").get<bool>());
    auto notes = project(*track.notes, &N::duration, &N::pitch, &N::velocity);
    std::ranges::sort(notes);
    CHECK(notes == expected_rows(expected.get("notes")));
    CHECK(
        project(*track.controls, &C::number, &C::value) == expected_rows(expected.get("controls"))
    );
    CHECK(
        project(*track.pitch_bends, &PitchBend<Tick>::value) == expected_rows(expected.get("bends"))
    );
    CHECK(project(*track.pedals, &Pedal<Tick>::duration) == expected_rows(expected.get("pedals")));
}

void check_score(const Score<Tick>& score, const Json& expected, const Json& tempos) {
    using M = TimeSignature<Tick>;
    using K = KeySignature<Tick>;
    CHECK(score.ticks_per_quarter == expected.get("tpq").get<double>());
    CHECK(project(*score.tempos, &Tempo<Tick>::mspq) == expected_rows(tempos));
    auto meters = project(*score.time_signatures, &M::numerator, &M::denominator);
    meters.erase(std::unique(meters.begin(), meters.end()), meters.end());
    CHECK(meters == expected_rows(expected.get("meters")));
    CHECK(
        project(*score.key_signatures, &K::key, &K::tonality) == expected_rows(expected.get("keys"))
    );
    const auto& tracks = expected.get("tracks").get<picojson::array>();
    REQUIRE(score.tracks->size() == tracks.size());
    for (size_t i = 0; i < tracks.size(); ++i) check_track(*(*score.tracks)[i], tracks[i]);
}

TEST_CASE("Playback snapshots", "[abc][oracle]") {
    const auto  inputs = fixture("abc_inputs.json");
    const auto  golden = fixture("abc_oracle.json");
    const auto& cases  = inputs.get<picojson::object>();
    REQUIRE(cases.size() == golden.get("cases").get<picojson::object>().size());
    for (const auto& [name, sample] : cases) {
        CAPTURE(name);
        auto [score, warnings] = abc::read(sample.get("abc").get<std::string>(), name);
        if (sample.contains("warning")) {
            const auto code = "[" + sample.get("warning").get<std::string>() + "]";
            CHECK(std::ranges::any_of(warnings, [&](const auto& w) {
                return w.find(code) != w.npos;
            }));
        } else
            CHECK(warnings.empty());
        REQUIRE(golden.get("cases").contains(name));
        const auto& expected = golden.get("cases").get(name);
        const auto& tempos   = sample.contains("native_tempos") ? sample.get("native_tempos") :
                                                                  expected.get("tempos");
        check_score(score, expected, tempos);
    }
}
}   // namespace

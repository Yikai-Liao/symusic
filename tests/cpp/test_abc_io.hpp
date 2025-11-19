#pragma once

#include <catch2/catch_all.hpp>
#include <filesystem>
#include <sstream>
#include <cstdlib>

#include "symusic/io/abc.h"
#include "symusic/io/midi.h"
#include "symusic/io/common.h"

TEST_CASE("ABC round-trip matches for bundled fixtures", "[abc]") {
    namespace fs = std::filesystem;
    const fs::path root = fs::path("tests/testcases/abc_files");
    REQUIRE(fs::exists(root));
    for (const auto& entry : fs::directory_iterator(root)) {
        if (!entry.is_regular_file()) { continue; }
        const auto path = entry.path();
        const auto data = symusic::read_file(path.string());
        auto score = symusic::Score<symusic::Tick>::parse<symusic::DataFormat::ABC>(data);
        auto dumped = score.dumps<symusic::DataFormat::ABC>();
        auto reparsed = symusic::Score<symusic::Tick>::parse<symusic::DataFormat::ABC>(dumped);
        INFO("ABC file: " << path.string());
        REQUIRE(score == reparsed);

        const char* abc2midi = std::getenv("SYMUSIC_ABC2MIDI");
        if (!abc2midi || std::string(abc2midi).empty()) { continue; }
        const fs::path temp = fs::temp_directory_path() / (path.stem().string() + "_reference.mid");
        std::ostringstream cmd;
        cmd << "\"" << abc2midi << "\" \"" << path.string() << "\" -o \"" << temp.string() << "\" -silent";
        const int ret = std::system(cmd.str().c_str());
        if (ret != 0 || !fs::exists(temp)) {
            if (fs::exists(temp)) { fs::remove(temp); }
            continue;
        }
        auto midi_bytes = symusic::read_file(temp.string());
        auto midi_score = symusic::Score<symusic::Tick>::parse<symusic::DataFormat::MIDI>(midi_bytes);
        fs::remove(temp);
        REQUIRE(score == midi_score);
    }
}

//
// Created by nhy on 2024/2/14.
//

#include "symusic.h"
#include "argparse.h"
using namespace symusic;

int main(const int argc, const char *argv[]) {
    argparse::ArgumentParser program("adjust_time");
    program.add_argument("midi_path").help("Path to the MIDI file");
    try {
        program.parse_args(argc, argv);
    } catch (const std::exception& err) {
        std::cerr << err.what() << std::endl;
        std::cout << program;
        std::exit(1);
    }
    const auto midi_path = program.get<std::string>("midi_path");
    const auto buffer = read_file(midi_path);
    const auto s = Score<Tick>::parse<DataFormat::MIDI>(buffer);
    fmt::println("{}", s.tracks[0].notes);
    auto end = s.end();
    const auto s2 = ops::adjust_time(s, {0, end / 2, end}, {0, end, end + end/2}, true);
    fmt::println("\n{}", s2.tracks[0].notes);
}

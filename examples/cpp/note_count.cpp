//
// Created by lyk on 23-9-21.
//
#include "symusic.h"
#include "fmt/ranges.h"
#include "argparse.h"
using namespace symusic;


int main(const int argc, const char *argv[]) {
    argparse::ArgumentParser program("note_count");
    program.add_argument("midi_path").help("Path to the MIDI file");
    program.add_argument("-n")
        .default_value(100)
        .help("Number of iterations, for profiling")
        .action([](const std::string &value) { return std::stoi(value); });

    try {
        program.parse_args(argc, argv);
    } catch (const std::exception& err) {
        std::cerr << err.what() << std::endl;
        std::cout << program;
        std::exit(1);
    }

    const auto filename = program.get<std::string>("midi_path");
    const size_t n = program.get<int>("-n");
    fmt::println("Filename: {}", filename);
    fmt::println("Number of iterations: {}", n);

    const auto data = read_file(filename);
    for(size_t i=0; i<n; ++i) {
        const auto s = Score<Tick>::parse<DataFormat::MIDI>(data);
        fmt::println("Score notes: {}", s.note_num());
    }
    return 0;
}

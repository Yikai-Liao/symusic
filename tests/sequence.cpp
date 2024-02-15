//
// Created by nhy on 2024/2/12.
//
#include "symusic.h"
#include "argparse.h"
#include <filesystem>
using namespace symusic;

int main(const int argc, const char *argv[]) {
    argparse::ArgumentParser program("sequence");
    program.add_argument("midi_path").help("Path to the MIDI file");
    program.add_argument("-o", "--output").default_value("./output.txt").help("Path to the output txt file");
    try {
        program.parse_args(argc, argv);
    } catch (const std::exception& err) {
        std::cerr << err.what() << std::endl;
        std::cout << program;
        std::exit(1);
    }

    const auto midi_path = program.get<std::string>("midi_path");
    const auto out_path = program.get<std::string>("output");

    const auto buffer = read_file(midi_path);
    const auto s = Score<Second>::parse<DataFormat::MIDI>(buffer);
    const psynth::Sequence seq = details::toSequence(s);

    std::string out;
    for(auto i=0; i<seq.tracks.size(); ++i) {
        out += "-----Track " + std::to_string(i) + "-----\n";
        out += "Preset: " + std::to_string(seq.tracks[i].preset) + "\tBank: " + std::to_string(seq.tracks[i].bank) + "\n";
        out += "Notes:\n";
        out += "{\n";
        for(const auto & note: seq.tracks[i].notes) {
            out += fmt::format("  {{{}, {}, {}, {}}},\n", note.start, note.duration, note.pitch, note.velocity);
        }
        out += "}\n\n";
    }
    write_file(out_path, std::span(reinterpret_cast<const u8*>(out.data()), out.size()));
    // using filesystem to find the absolute path
    fmt::println("Sequence written to {}", std::filesystem::absolute(out_path).string());
}

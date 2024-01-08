//
// Created by lyk on 23-9-21.
//
#include "symusic.h"
#include "fmt/ranges.h"
using namespace symusic;


int main(const int argc, const char *argv[]) {
    if(argc == 2) {
        const std::string filename{argv[1]};
        std::cout << "Filename: " << filename << std::endl;
        const auto data = read_file(filename);
        auto s = Score<Tick>::parse<DataFormat::MIDI>(data);
        auto s2 = convert<Second>(s, 0);
        fmt::println("{}", s2.tracks[0].notes);
        // for(size_t i=0; i<100; ++i) {
        //     const auto s = Score<Tick>::parse<DataFormat::MIDI>(data);
        //     fmt::println("Score notes: {}", s.note_num());
        // }
    } else {
        std::cout << "Usage: ./note_count <midi_file_name>" << std::endl;
    }
    return 0;
}

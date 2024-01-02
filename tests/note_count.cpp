//
// Created by lyk on 23-9-21.
//
#include "symusic.h"
#include "cassert"
using namespace symusic;


int main(int argc, char *argv[]) {
    if(argc == 2) {
        std::string filename = std::string(argv[1]);
        std::cout << "Filename: " << filename << std::endl;
        try {
            // auto midi = file::MidiFile::from_file(filename);
            auto data = read_file(filename);
            auto s = Score<Tick>::parse<DataFormat::MIDI>(data);
            auto score_notes = s.note_num();
            std::cout << "Score notes: " << score_notes << std::endl;
            assert(midi_notes == score_notes);
        } catch(const char* e) {
            std::cout << e << std::endl;
            exit(EXIT_FAILURE);
        };
    } else {
        std::cout << "Usage: ./note_count <midi_file_name>" << std::endl;
    }
    return 0;
}

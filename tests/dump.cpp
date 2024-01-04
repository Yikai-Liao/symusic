#include "symusic.h"
#include "cassert"
using namespace symusic;

int main(int argc, char *argv[]) {
    if(argc == 2) {
        std::string filename = std::string(argv[1]);
        std::cout << "Filename: " << filename << std::endl;
        try {
            auto s = Score<Tick>::parse<DataFormat::MIDI>(read_file(filename));
            std::cout << "Score1: " << s.to_string() << std::endl;
            // dump to midi bytes
            auto midi_bytes = s.dumps<DataFormat::MIDI>();
            // reload from midi bytes
            auto s2 = Score<Tick>::parse<DataFormat::MIDI>(midi_bytes);
            std::cout << "Score2: " << s2.to_string() << std::endl;
        } catch(const char* e) {
            std::cout << e << std::endl;
            exit(EXIT_FAILURE);
        };
    } else {
        std::cout << "Usage: ./note_count <midi_file_name>" << std::endl;
    }
    return 0;
}

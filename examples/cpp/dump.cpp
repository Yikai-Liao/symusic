#include "symusic.h"
#include "cassert"
#include "argparse.h"
using namespace symusic;

int main(int argc, char *argv[]) {
    argparse::ArgumentParser program("dump");
    program.add_argument("midi_path").help("Path to the MIDI file");
    try {
        program.parse_args(argc, argv);
    } catch (const std::exception& err) {
        std::cerr << err.what() << std::endl;
        std::cout << program;
        std::exit(1);
    }

    const auto filename = program.get<std::string>("midi_path");
    try {
        auto s = Score<Tick>::parse<DataFormat::MIDI>(read_file(filename));
        std::cout << "Score1: " << s.to_string() << std::endl;
		auto rogueCtrlMessage = (*s.tracks)[2]->controls;
		int time = (*rogueCtrlMessage)[0]->time;
		int number = (*rogueCtrlMessage)[0]->number;
		int value = (*rogueCtrlMessage)[0]->value;
		std::cout << "time: " << std::to_string(time) << ", number: " <<std::to_string(number) << ", value: " << std::to_string(value) << std::endl;
        // dump to midi bytes
        auto midi_bytes = s.dumps<DataFormat::MIDI>();
        // reload from midi bytes
        auto s2 = Score<Tick>::parse<DataFormat::MIDI>(midi_bytes);
        std::cout << "Score2: " << s2.to_string() << std::endl;
    } catch(const char* e) {
        std::cout << e << std::endl;
        exit(EXIT_FAILURE);
    };
    return 0;
}

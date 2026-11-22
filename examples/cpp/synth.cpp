#include "symusic.h"
#include "argparse.h"
using namespace symusic;

int main(const int argc, const char* argv[]) {
    argparse::ArgumentParser program("synth");
    program.add_argument("midi_path").help("Path to the MIDI file");
    program.add_argument("--library").required().help("Absolute path to libfluidsynth");
    program.add_argument("-sf", "--soundfont").required().help("Path to the soundfont(2/3) file");
    program.add_argument("-o", "--output")
        .default_value("./output.f32")
        .help("Path to interleaved native-endian float32 output");
    program.add_argument("-r", "--sample-rate")
        .default_value(44100)
        .action([](const std::string& value) { return std::stoi(value); })
        .help("Sample rate of the output audio file");
    program.add_argument("-s", "--stereo").help("Output stereo audio").flag();
    try {
        program.parse_args(argc, argv);
    } catch (const std::exception& err) {
        std::cerr << err.what() << std::endl;
        std::cout << program;
        std::exit(1);
    }

    auto midi_path    = program.get<std::string>("midi_path");
    auto output_path  = program.get<std::string>("output");
    auto sf_path      = program.get<std::string>("soundfont");
    auto sample_rate  = program.get<int>("--sample-rate");
    auto stereo       = program.get<bool>("--stereo");
    auto library_path = program.get<std::string>("--library");

    const auto        data  = read_file(midi_path);
    const auto        score = Score<Second>::parse<DataFormat::MIDI>(data);
    const Synthesizer synth(library_path, sf_path, {.sample_rate = sample_rate});
    const auto        audio = synth.render(score, stereo);
    const auto        bytes = std::span(
        reinterpret_cast<const u8*>(audio.samples.data()), audio.samples.size() * sizeof(float)
    );
    write_file(output_path, bytes);
    return 0;
}

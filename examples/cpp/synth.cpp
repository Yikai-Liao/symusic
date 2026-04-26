//
// Created by nhy on 2024/2/12.
//
#include "symusic.h"
#include "argparse.h"
using namespace symusic;

int main(const int argc, const char *argv[]) {
    argparse::ArgumentParser program("synth");
    program.add_argument("midi_path").help("Path to the MIDI file");
    program.add_argument("-sf", "--soundfont").required().help("Path to the soundfont(2/3) file");
    program.add_argument("-o", "--output").default_value("./output.wav").help("Path to the output audio file");
    program.add_argument("-r", "--sample-rate")
        .default_value(44100)
        .action([](const std::string &value) { return std::stoi(value); })
        .help("Sample rate of the output audio file");
    program.add_argument("-s", "--stereo").help("Output stereo audio").flag();

    try {
        program.parse_args(argc, argv);
    } catch (const std::exception& err) {
        std::cerr << err.what() << std::endl;
        std::cout << program;
        std::exit(1);
    }

    auto midi_path = program.get<std::string>("midi_path");
    auto output_path = program.get<std::string>("output");
    auto sf_path = program.get<std::string>("soundfont");
    auto sample_rate = program.get<int>("--sample-rate");
    auto stereo = program.get<bool>("--stereo");


    // show all the arguments
    fmt::println("Input MIDI:   \"{}\"", midi_path);
    fmt::println("Output Audio: \"{}\"", output_path);
    fmt::println("SoundFont:    \"{}\"", sf_path);
    fmt::println("Sample Rate:  {}", sample_rate);
    fmt::println("Stereo:       {}", stereo);


    const auto data = read_file(midi_path);
    const auto s = Score<Second>::parse<DataFormat::MIDI>(data);
    Synthesizer synth(sf_path, sample_rate);
    auto audio = synth.render(s, stereo);
    const auto [channels, frames] = audio.dims();
    write_wav(output_path, audio.data(), channels, frames, sample_rate, true);
    return 0;
}

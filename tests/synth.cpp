//
// Created by nhy on 2024/2/12.
//
#include "symusic.h"
#include "argparse.h"
using namespace symusic;

int main(const int argc, const char *argv[]) {
    auto args = util::argparser("MIDI to Audio Synthesizer, based on prestosynth and symusic");
    args.set_program_name("synth")
        .add_argument<std::string>("midi_path", "Path to the MIDI file")
        .add_named_argument<std::string>("sf" , "Path to the soundfont(2/3) file")
        .add_named_argument<std::string>("o", "Path to the output audio file")
        .add_option<int>("-r", "--sample-rate", "Sample rate of the output audio file", 44100)
        .add_option<bool>("-s", "--stereo", "Output stereo audio", true)
        .add_option<int>("-q", "--quality", "Quality of the synthesizer", 0)
        .add_option<int>("-w", "--worker-num", "Number of workers for the synthesizer", 1)
        .add_help_option()
        .parse(argc, argv);

    auto midi_path = args.get_argument<std::string>("midi_path");
    auto output_path = args.get_argument<std::string>("o");
    auto sf_path = args.get_argument<std::string>("sf");
    auto sample_rate = args.get_option<int>("-r");
    auto stereo = args.get_option<bool>("-s");
    auto quality = args.get_option<int>("-q");
    auto worker_num = args.get_option<int>("-w");

    // show all the arguments
    fmt::println("Input MIDI:   \"{}\"", midi_path);
    fmt::println("Output Audio: \"{}\"", output_path);
    fmt::println("SoundFont:    \"{}\"", sf_path);
    fmt::println("Sample Rate:  {}", sample_rate);
    fmt::println("Stereo:       {}", stereo);
    fmt::println("Quality:      {}", quality);
    fmt::println("Num Workers:  {}", worker_num);

    const auto data = read_file(midi_path);
    const auto s = Score<Second>::parse<DataFormat::MIDI>(data);
    Synthesizer synth(sf_path, sample_rate, quality, worker_num);
    auto audio = synth.render(s, true);
    psynth::write_audio(output_path, audio, sample_rate);
    return 0;
}

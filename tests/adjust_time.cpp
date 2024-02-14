//
// Created by nhy on 2024/2/14.
//

#include "symusic.h"
#include "argparse.h"
using namespace symusic;

int main(const int argc, const char *argv[]) {
    auto args = util::argparser("MIDI to Audio Synthesizer, based on prestosynth and symusic");
    args.set_program_name("sequence")
        .add_argument<std::string>("midi_path", "Path to the MIDI file")
        .add_help_option()
        .parse(argc, argv);

    const auto midi_path = args.get_argument<std::string>("midi_path");
    const auto buffer = read_file(midi_path);
    const auto s = Score<Tick>::parse<DataFormat::MIDI>(buffer);
    fmt::println("{}", s.tracks[0].notes);
    auto end = s.end();
    const auto s2 = ops::adjust_time(s, {0, end / 2, end}, {0, end, end + end/2}, true);
    fmt::println("\n{}", s2.tracks[0].notes);
}

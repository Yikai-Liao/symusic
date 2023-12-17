//
// Created by lyk on 23-9-21.
//
#include "Score.hpp"
#include "cassert"
using namespace score;
using namespace minimidi;

size_t count_note_num(file::MidiFile& midifile)
{
    size_t num = 0;

    for (int t_idx = 0; t_idx < midifile.track_num(); ++t_idx)
    {
        auto& track = midifile.track(t_idx);

        for (int m_idx = 0; m_idx < track.message_num(); ++m_idx)
        {
            auto& msg = track.message(m_idx);

            if(msg.get_type() == minimidi::message::MessageType::NoteOn &&
                msg.get_velocity() != 0 &&msg.get_pitch() != 0)
            {
                num++;
            }
        }
    }

    return num;
};

int main(int argc, char *argv[]) {
    if(argc == 2) {
        std::string filename = std::string(argv[1]);
        std::cout << "Filename: " << filename << std::endl;
        try {
            auto midi = file::MidiFile::from_file(filename);
            auto s = Score<Tick>(midi);
            auto midi_notes = count_note_num(midi);
            auto score_notes = s.note_num();
            std::cout << "Midi notes: " << midi_notes << std::endl;
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
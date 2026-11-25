#include <symusic/ops.h>
#include <symusic/score.h>
#include <symusic/synth.h>

int main() {
    symusic::Score<symusic::Tick> score(480);
    auto&                         track = score.tracks.emplace_back();
    auto&                         note  = track.notes.emplace_back();
    note.time                           = 120;
    note.duration                       = 240;
    note.pitch                          = 60;
    note.velocity                       = 90;
    // Force a link to the out-of-line render specialization without requiring a SoundFont.
    const auto volatile render
        = &symusic::Synthesizer::render<symusic::Tick, symusic::ValueStorage>;
    return score.end() == 360 && render != nullptr ? 0 : 1;
}

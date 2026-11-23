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
    return score.end() == 360 ? 0 : 1;
}

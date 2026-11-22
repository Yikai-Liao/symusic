#include "utils/storage.h"
#include "symusic/detail/pianoroll_impl.h"

namespace symusic {

template TrackPianoroll TrackPianoroll::from_track(const Track<Tick, py::SharedStorage>&, const vec<PianorollMode>&, std::pair<u8, u8>, bool);
template ScorePianoroll ScorePianoroll::from_score(const Score<Tick, py::SharedStorage>&, const vec<PianorollMode>&, std::pair<u8, u8>, bool);

}  // namespace symusic

#include "symusic/pianoroll.h"

namespace symusic {

TrackPianoroll::TrackPianoroll(size_t modeDim, size_t pitchDim, size_t timeDim) {
    this->modeDim = modeDim;
    this->pitchDim = pitchDim;
    this->timeDim = timeDim;
    this->data = new pianoroll_t[modeDim * pitchDim * timeDim];

    this->clear();
};

TrackPianoroll::~TrackPianoroll() {
    delete this->data;
};

TrackPianoroll TrackPianoroll::from_track(const Track<Tick>& track,
        const std::vector<PianorollMode>& modes,
        std::pair<uint8_t, uint8_t> pitchRange,
        bool encodeVelocity,
        bool deOverlap) {
    TrackPianoroll pianoroll(modes.size(), pitchRange.second - pitchRange.first, track.end() + 1);

    // TODO: de-overlap

    for(const auto &note : track.notes) {
        for (int modeIdx = 0; modeIdx < modes.size(); ++modeIdx) {
            pianoroll.set(modeIdx,
                note.pitch,
                modes[modeIdx] != PianorollMode::Offset ? note.start() : note.end(),
                modes[modeIdx] != PianorollMode::Frame ? note.duration : 1,
                encodeVelocity ? 1 : note.velocity);
        }
    }

    return pianoroll;
};

pianoroll_t* TrackPianoroll::get_data() {
    assert(this->data != nullptr);

    return this->data;
};

pianoroll_t* TrackPianoroll::release_data() {
    assert(this->data != nullptr);

    return std::move(this->data);
};

pianoroll_t* TrackPianoroll::operator()(size_t mode, size_t pitch, size_t time) {
    return this->data + mode * this->pitchDim * this->timeDim + pitch * this->timeDim + time;
};

void TrackPianoroll::set(size_t mode, size_t pitch, size_t start, size_t duration, pianoroll_t value) {
    std::fill_n((*this)(mode, pitch, start),
                duration,
                value);
};

pianoroll_t TrackPianoroll::get(size_t mode, size_t pitch, size_t time) {
    return *((*this)(mode, pitch, time));
};

void TrackPianoroll::clear() {
    std::fill_n(this->data,
                this->modeDim * this->pitchDim * this->timeDim,
                0);
};

const std::tuple<size_t, size_t, size_t> TrackPianoroll::dims() const {
    return {this->modeDim, this->pitchDim, this->timeDim};
};


ScorePianoroll::ScorePianoroll(size_t modeDim, size_t trackDim, size_t pitchDim, size_t timeDim) {
    this->modeDim = modeDim;
    this->trackDim = trackDim;
    this->pitchDim = pitchDim;
    this->timeDim = timeDim;
    this->data = new pianoroll_t[modeDim * trackDim * pitchDim * timeDim];

    this->clear();
};

ScorePianoroll::~ScorePianoroll() {
    delete this->get_data();
};

ScorePianoroll ScorePianoroll::from_track(const Score<Tick>& score,
        const std::vector<PianorollMode>& modes,
        std::pair<uint8_t, uint8_t> pitchRange,
        bool encodeVelocity,
        bool deOverlap) {
    ScorePianoroll pianoroll(modes.size(), score.tracks.size(), pitchRange.second - pitchRange.first, score.end() + 1);

    // TODO: de-overlap

    for(int trackIdx = 0; trackIdx < score.tracks.size(), ++trackIdx;) {
        const auto &track = score.tracks[trackIdx];
        for(const auto &note : track.notes) {
            for (int modeIdx = 0; modeIdx < modes.size(); ++modeIdx) {
                pianoroll.set(modeIdx,
                    trackIdx,
                    note.pitch,
                    modes[modeIdx] != PianorollMode::Offset ? note.start() : note.end(),
                    modes[modeIdx] != PianorollMode::Frame ? note.duration : 1,
                    encodeVelocity ? 1 : note.velocity);
            }
        }
    }

    return pianoroll;
}

pianoroll_t* ScorePianoroll::get_data() {
    assert(this->data != nullptr);

    return this->data;
};

pianoroll_t* ScorePianoroll::release_data() {
    assert(this->data != nullptr);

    return std::move(this->data);
};

pianoroll_t* ScorePianoroll::operator()(size_t mode, size_t track, size_t pitch, size_t time) {
    return get_data() +
        mode * this->trackDim * this->pitchDim * this->timeDim +
        track * this->pitchDim * this->timeDim +
        pitch * this->timeDim +
        time;
};

void ScorePianoroll::set(size_t mode, size_t track, size_t pitch, size_t start, size_t duration, pianoroll_t value) {
    std::fill_n((*this)(mode, track, pitch, start),
                duration,
                value);
};

pianoroll_t ScorePianoroll::get(size_t mode, size_t track, size_t pitch, size_t time) {
    return *((*this)(mode, track, pitch, time));
};

void ScorePianoroll::clear() {
    std::fill_n(this->data,
                this->modeDim * this->trackDim * this->pitchDim * this->timeDim,
                0);
};

const std::tuple<size_t, size_t, size_t, size_t> ScorePianoroll::dims() const {
    return {this->modeDim, this->trackDim, this->pitchDim, this->timeDim};
};

}

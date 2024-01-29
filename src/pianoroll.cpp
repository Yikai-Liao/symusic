#include "symusic/pianoroll.h"

#include <iostream>

namespace symusic {

TrackPianoroll::TrackPianoroll(size_t modeDim, size_t pitchDim, size_t timeDim) {
    this->modeDim = modeDim;
    this->pitchDim = pitchDim;
    this->timeDim = timeDim;
    this->dataPtr = new pianoroll_t[modeDim * pitchDim * timeDim];

    this->clear();
};

TrackPianoroll::~TrackPianoroll() {
    delete this->dataPtr;
};

TrackPianoroll TrackPianoroll::from_track(
    const Track<Tick>& track,
    const std::vector<PianorollMode>& modes,
    const std::pair<uint8_t, uint8_t> pitchRange,
    const bool encodeVelocity) {

    TrackPianoroll pianoroll(modes.size(), pitchRange.second - pitchRange.first, track.end() + 1);

    for(const auto &note : track.notes) {
        for (int modeIdx = 0; modeIdx < modes.size(); ++modeIdx) {
            pianoroll.set(modeIdx,
                note.pitch - pitchRange.first,
                modes[modeIdx] != PianorollMode::offset ? note.start() : note.end(),
                modes[modeIdx] == PianorollMode::frame ? note.duration : 1,
                encodeVelocity ? note.velocity: 1);
        }
    }

    return pianoroll;
};

pianoroll_t* TrackPianoroll::data() {
    if(this->dataPtr == nullptr)
        throw std::runtime_error("Data in TrackPianoroll is moved.");

    return this->dataPtr;
};

pianoroll_t* TrackPianoroll::release() {
    if(this->dataPtr == nullptr)
        throw std::runtime_error("Data in TrackPianoroll is moved.");

    pianoroll_t* ptr = this->dataPtr;
    this->dataPtr = nullptr;

    return ptr;
};

pianoroll_t* TrackPianoroll::operator()(size_t mode, size_t pitch, size_t time) {
    return this->data() + mode * this->pitchDim * this->timeDim + pitch * this->timeDim + time;
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
    std::fill_n(this->data(),
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
    this->dataPtr = new pianoroll_t[modeDim * trackDim * pitchDim * timeDim];

    this->clear();
};

ScorePianoroll::~ScorePianoroll() {
    delete this->dataPtr;
};

ScorePianoroll ScorePianoroll::from_score(
    const Score<Tick>& score,
    const std::vector<PianorollMode>& modes,
    const std::pair<uint8_t, uint8_t> pitchRange,
    const bool encodeVelocity) {

    ScorePianoroll pianoroll(modes.size(), score.tracks.size(), pitchRange.second - pitchRange.first, score.end() + 1);

    for(int trackIdx = 0; trackIdx < score.tracks.size(); ++trackIdx) {
        const auto &track = score.tracks[trackIdx];
        for(const auto &note : track.notes) {
            for (int modeIdx = 0; modeIdx < modes.size(); ++modeIdx) {
                pianoroll.set(modeIdx,
                    trackIdx,
                    note.pitch - pitchRange.first,
                    modes[modeIdx] != PianorollMode::offset ? note.start() : note.end(),
                    modes[modeIdx] == PianorollMode::frame ? note.duration : 1,
                    encodeVelocity ? note.velocity: 1);
            }
        }
    }

    return pianoroll;
}

pianoroll_t* ScorePianoroll::data() {
    if(this->dataPtr == nullptr)
        throw std::runtime_error("Data in ScorePianoroll is moved.");

    return this->dataPtr;
};

pianoroll_t* ScorePianoroll::release() {
    if(this->dataPtr == nullptr)
        throw std::runtime_error("Data in ScorePianoroll is moved.");

    pianoroll_t* ptr = this->dataPtr;
    this->dataPtr = nullptr;

    return ptr;
};

pianoroll_t* ScorePianoroll::operator()(size_t mode, size_t track, size_t pitch, size_t time) {
    return data() +
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
    std::fill_n(this->dataPtr,
                this->modeDim * this->trackDim * this->pitchDim * this->timeDim,
                0);
};

const std::tuple<size_t, size_t, size_t, size_t> ScorePianoroll::dims() const {
    return {this->modeDim, this->trackDim, this->pitchDim, this->timeDim};
};

}

#include "symusic/detail/pianoroll_impl.h"

#include <initializer_list>
#include <limits>
#include <memory>
#include <stdexcept>
#include <string>

namespace symusic {



TrackPianoroll::TrackPianoroll(size_t modeDim, size_t pitchDim, size_t timeDim) :
    modeDim(modeDim), pitchDim(pitchDim), timeDim(timeDim),
    dataPtr(std::make_unique<pianoroll_t[]>(detail::checked_volume({modeDim, pitchDim, timeDim}))) {}


pianoroll_t* TrackPianoroll::data() {
    if (this->dataPtr == nullptr) throw std::runtime_error("Data in TrackPianoroll is moved.");
    return this->dataPtr.get();
};

const pianoroll_t* TrackPianoroll::data() const {
    if (this->dataPtr == nullptr) throw std::runtime_error("Data in TrackPianoroll is moved.");
    return this->dataPtr.get();
};

pianoroll_t* TrackPianoroll::release() {
    if (this->dataPtr == nullptr) throw std::runtime_error("Data in TrackPianoroll is moved.");
    return this->dataPtr.release();
};

pianoroll_t* TrackPianoroll::operator()(const size_t mode, const size_t pitch, const size_t time) {
    detail::check_index("mode", mode, this->modeDim);
    detail::check_index("pitch", pitch, this->pitchDim);
    detail::check_index("time", time, this->timeDim);
    return this->data() + mode * this->pitchDim * this->timeDim + pitch * this->timeDim + time;
};

void TrackPianoroll::fill_unchecked(
    const size_t      mode,
    const size_t      pitch,
    const size_t      start,
    const size_t      duration,
    const pianoroll_t value
) noexcept {
    std::fill_n(
        this->dataPtr.get() + (mode * this->pitchDim + pitch) * this->timeDim + start,
        duration,
        value
    );
}

void TrackPianoroll::set(
    const size_t      mode,
    const size_t      pitch,
    const size_t      start,
    const size_t      duration,
    const pianoroll_t value
) {
    detail::check_time_span(start, duration, this->timeDim);
    if (duration == 0) {
        detail::check_index("mode", mode, this->modeDim);
        detail::check_index("pitch", pitch, this->pitchDim);
        return;
    }
    std::fill_n((*this)(mode, pitch, start), duration, value);
};

pianoroll_t TrackPianoroll::get(size_t mode, size_t pitch, size_t time) {
    return *((*this)(mode, pitch, time));
};

void TrackPianoroll::clear() {
    std::fill_n(this->data(), this->modeDim * this->pitchDim * this->timeDim, 0);
};

std::tuple<size_t, size_t, size_t> TrackPianoroll::dims() const {
    return {this->modeDim, this->pitchDim, this->timeDim};
};

ScorePianoroll::ScorePianoroll(size_t modeDim, size_t trackDim, size_t pitchDim, size_t timeDim) :
    modeDim(modeDim), trackDim(trackDim), pitchDim(pitchDim), timeDim(timeDim),
    dataPtr(
        std::make_unique<pianoroll_t[]>(detail::checked_volume({modeDim, trackDim, pitchDim, timeDim}))
    ) {}


pianoroll_t* ScorePianoroll::data() {
    if (this->dataPtr == nullptr) throw std::runtime_error("Data in ScorePianoroll is moved.");
    return this->dataPtr.get();
};

const pianoroll_t* ScorePianoroll::data() const {
    if (this->dataPtr == nullptr) throw std::runtime_error("Data in ScorePianoroll is moved.");
    return this->dataPtr.get();
};

pianoroll_t* ScorePianoroll::release() {
    if (this->dataPtr == nullptr) throw std::runtime_error("Data in ScorePianoroll is moved.");
    return this->dataPtr.release();
};

pianoroll_t* ScorePianoroll::operator()(size_t mode, size_t track, size_t pitch, size_t time) {
    detail::check_index("mode", mode, this->modeDim);
    detail::check_index("track", track, this->trackDim);
    detail::check_index("pitch", pitch, this->pitchDim);
    detail::check_index("time", time, this->timeDim);
    return data() + mode * this->trackDim * this->pitchDim * this->timeDim
           + track * this->pitchDim * this->timeDim + pitch * this->timeDim + time;
};

void ScorePianoroll::fill_unchecked(
    const size_t      mode,
    const size_t      track,
    const size_t      pitch,
    const size_t      start,
    const size_t      duration,
    const pianoroll_t value
) noexcept {
    std::fill_n(
        this->dataPtr.get()
            + ((mode * this->trackDim + track) * this->pitchDim + pitch) * this->timeDim + start,
        duration,
        value
    );
}

void ScorePianoroll::set(
    size_t mode, size_t track, size_t pitch, size_t start, size_t duration, pianoroll_t value
) {
    detail::check_time_span(start, duration, this->timeDim);
    if (duration == 0) {
        detail::check_index("mode", mode, this->modeDim);
        detail::check_index("track", track, this->trackDim);
        detail::check_index("pitch", pitch, this->pitchDim);
        return;
    }
    std::fill_n((*this)(mode, track, pitch, start), duration, value);
};

pianoroll_t ScorePianoroll::get(size_t mode, size_t track, size_t pitch, size_t time) {
    return *((*this)(mode, track, pitch, time));
};

void ScorePianoroll::clear() {
    std::fill_n(this->data(), this->modeDim * this->trackDim * this->pitchDim * this->timeDim, 0);
};

std::tuple<size_t, size_t, size_t, size_t> ScorePianoroll::dims() const {
    return {this->modeDim, this->trackDim, this->pitchDim, this->timeDim};
};

template TrackPianoroll TrackPianoroll::from_track(const Track<Tick, ValueStorage>&, const vec<PianorollMode>&, std::pair<u8, u8>, bool);
template ScorePianoroll ScorePianoroll::from_score(const Score<Tick, ValueStorage>&, const vec<PianorollMode>&, std::pair<u8, u8>, bool);

}   // namespace symusic

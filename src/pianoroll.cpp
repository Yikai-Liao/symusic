#include "symusic/detail/pianoroll_impl.h"

#include <initializer_list>
#include <limits>
#include <memory>
#include <stdexcept>
#include <string>

namespace symusic {

TrackPianoroll::TrackPianoroll(size_t mode_dim, size_t pitch_dim, size_t time_dim) :
    mode_dim(mode_dim), pitch_dim(pitch_dim), time_dim(time_dim),
    data_ptr(std::make_unique<pianoroll_t[]>(detail::checked_volume({mode_dim, pitch_dim, time_dim}))) {}

pianoroll_t* TrackPianoroll::data() {
    if (this->data_ptr == nullptr) throw std::runtime_error("Data in TrackPianoroll is moved.");
    return this->data_ptr.get();
};

const pianoroll_t* TrackPianoroll::data() const {
    if (this->data_ptr == nullptr) throw std::runtime_error("Data in TrackPianoroll is moved.");
    return this->data_ptr.get();
};

pianoroll_t* TrackPianoroll::release() {
    if (this->data_ptr == nullptr) throw std::runtime_error("Data in TrackPianoroll is moved.");
    return this->data_ptr.release();
};

pianoroll_t* TrackPianoroll::operator()(const size_t mode, const size_t pitch, const size_t time) {
    detail::check_index("mode", mode, this->mode_dim);
    detail::check_index("pitch", pitch, this->pitch_dim);
    detail::check_index("time", time, this->time_dim);
    return this->data() + mode * this->pitch_dim * this->time_dim + pitch * this->time_dim + time;
};

void TrackPianoroll::fill_unchecked(
    const size_t      mode,
    const size_t      pitch,
    const size_t      start,
    const size_t      duration,
    const pianoroll_t value
) noexcept {
    std::fill_n(
        this->data_ptr.get() + (mode * this->pitch_dim + pitch) * this->time_dim + start,
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
    detail::check_time_span(start, duration, this->time_dim);
    if (duration == 0) {
        detail::check_index("mode", mode, this->mode_dim);
        detail::check_index("pitch", pitch, this->pitch_dim);
        return;
    }
    std::fill_n((*this)(mode, pitch, start), duration, value);
};

pianoroll_t TrackPianoroll::get(size_t mode, size_t pitch, size_t time) {
    return *((*this)(mode, pitch, time));
};

void TrackPianoroll::clear() {
    std::fill_n(this->data(), this->mode_dim * this->pitch_dim * this->time_dim, 0);
};

std::tuple<size_t, size_t, size_t> TrackPianoroll::dims() const {
    return {this->mode_dim, this->pitch_dim, this->time_dim};
};

ScorePianoroll::ScorePianoroll(size_t mode_dim, size_t track_dim, size_t pitch_dim, size_t time_dim) :
    mode_dim(mode_dim), track_dim(track_dim), pitch_dim(pitch_dim), time_dim(time_dim),
    data_ptr(
        std::make_unique<pianoroll_t[]>(detail::checked_volume({mode_dim, track_dim, pitch_dim, time_dim}))
    ) {}

pianoroll_t* ScorePianoroll::data() {
    if (this->data_ptr == nullptr) throw std::runtime_error("Data in ScorePianoroll is moved.");
    return this->data_ptr.get();
};

const pianoroll_t* ScorePianoroll::data() const {
    if (this->data_ptr == nullptr) throw std::runtime_error("Data in ScorePianoroll is moved.");
    return this->data_ptr.get();
};

pianoroll_t* ScorePianoroll::release() {
    if (this->data_ptr == nullptr) throw std::runtime_error("Data in ScorePianoroll is moved.");
    return this->data_ptr.release();
};

pianoroll_t* ScorePianoroll::operator()(size_t mode, size_t track, size_t pitch, size_t time) {
    detail::check_index("mode", mode, this->mode_dim);
    detail::check_index("track", track, this->track_dim);
    detail::check_index("pitch", pitch, this->pitch_dim);
    detail::check_index("time", time, this->time_dim);
    return data() + mode * this->track_dim * this->pitch_dim * this->time_dim
           + track * this->pitch_dim * this->time_dim + pitch * this->time_dim + time;
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
        this->data_ptr.get()
            + ((mode * this->track_dim + track) * this->pitch_dim + pitch) * this->time_dim + start,
        duration,
        value
    );
}

void ScorePianoroll::set(
    size_t mode, size_t track, size_t pitch, size_t start, size_t duration, pianoroll_t value
) {
    detail::check_time_span(start, duration, this->time_dim);
    if (duration == 0) {
        detail::check_index("mode", mode, this->mode_dim);
        detail::check_index("track", track, this->track_dim);
        detail::check_index("pitch", pitch, this->pitch_dim);
        return;
    }
    std::fill_n((*this)(mode, track, pitch, start), duration, value);
};

pianoroll_t ScorePianoroll::get(size_t mode, size_t track, size_t pitch, size_t time) {
    return *((*this)(mode, track, pitch, time));
};

void ScorePianoroll::clear() {
    std::fill_n(this->data(), this->mode_dim * this->track_dim * this->pitch_dim * this->time_dim, 0);
};

std::tuple<size_t, size_t, size_t, size_t> ScorePianoroll::dims() const {
    return {this->mode_dim, this->track_dim, this->pitch_dim, this->time_dim};
};

template TrackPianoroll TrackPianoroll::from_track(const Track<Tick, ValueStorage>&, const vec<PianorollMode>&, std::pair<u8, u8>, bool);
template ScorePianoroll ScorePianoroll::from_score(const Score<Tick, ValueStorage>&, const vec<PianorollMode>&, std::pair<u8, u8>, bool);

}   // namespace symusic

#include "symusic/pianoroll.h"

#include <initializer_list>
#include <limits>
#include <memory>
#include <stdexcept>
#include <string>

namespace symusic {

namespace {

/// Multiply dimensions, throwing std::length_error (ValueError in Python) when the element count
/// would not fit in a signed object size.
size_t checked_volume(std::initializer_list<size_t> dims) {
    size_t volume = 1;
    for (const size_t dim : dims) {
        if (dim != 0 && volume > static_cast<size_t>(std::numeric_limits<ptrdiff_t>::max()) / dim) {
            throw std::length_error("Pianoroll dimensions are too large.");
        }
        volume *= dim;
    }
    return volume;
}

/// Time-axis length for a track or score ending at `end` (exclusive end plus one slot).
/// The sum is done in 64 bits so `end == INT32_MAX` cannot overflow.
size_t time_dim_from_end(const int64_t end) {
    if (end < 0) throw std::invalid_argument("Cannot build a pianoroll: the end time is negative.");
    return static_cast<size_t>(end) + 1;
}

/// Validate one note against a time axis of `timeDim` slots and return its start and end.
/// Throws std::invalid_argument for a negative time or duration, or a note that runs past the axis.
/// A zero duration is valid: frame mode writes nothing for it.
std::pair<size_t, size_t> checked_note_span(
    const int64_t time, const int64_t duration, const size_t timeDim
) {
    if (time < 0) {
        throw std::invalid_argument(
            "Cannot build a pianoroll: note time " + std::to_string(time) + " is negative."
        );
    }
    if (duration < 0) {
        throw std::invalid_argument(
            "Cannot build a pianoroll: note duration " + std::to_string(duration) + " is negative."
        );
    }
    const int64_t end = time + duration;   // both are non-negative i32 values, so no overflow
    if (static_cast<uint64_t>(end) >= timeDim) {
        throw std::invalid_argument(
            "Cannot build a pianoroll: note end " + std::to_string(end)
            + " lies beyond the track end."
        );
    }
    return {static_cast<size_t>(time), static_cast<size_t>(end)};
}

void check_index(const char* name, const size_t index, const size_t dim) {
    if (index >= dim) {
        throw std::out_of_range(
            std::string("Pianoroll ") + name + " index " + std::to_string(index)
            + " is out of range for size " + std::to_string(dim) + "."
        );
    }
}

/// Check that `duration` slots starting at `start` fit inside `timeDim` without overflowing.
void check_time_span(const size_t start, const size_t duration, const size_t timeDim) {
    if (start > timeDim || duration > timeDim - start) {
        throw std::out_of_range(
            "Pianoroll time span [" + std::to_string(start) + ", +" + std::to_string(duration)
            + ") is out of range for size " + std::to_string(timeDim) + "."
        );
    }
}

}   // namespace

TrackPianoroll::TrackPianoroll(size_t modeDim, size_t pitchDim, size_t timeDim) :
    modeDim(modeDim), pitchDim(pitchDim), timeDim(timeDim),
    dataPtr(std::make_unique<pianoroll_t[]>(checked_volume({modeDim, pitchDim, timeDim}))) {}

TrackPianoroll TrackPianoroll::from_track(
    const Track<Tick>&                track,
    const std::vector<PianorollMode>& modes,
    const std::pair<uint8_t, uint8_t> pitchRange,
    const bool                        encodeVelocity
) {
    if (pitchRange.second < pitchRange.first) {
        throw std::invalid_argument("Pianoroll pitch range is reversed.");
    }
    const size_t   timeDim = time_dim_from_end(int64_t{track.end()});
    TrackPianoroll pianoroll(modes.size(), pitchRange.second - pitchRange.first, timeDim);

    for (const auto& note : *track.notes) {
        if (note.pitch >= pitchRange.first && note.pitch < pitchRange.second) {
            const auto [noteStart, noteEnd] = checked_note_span(note.time, note.duration, timeDim);
            for (size_t modeIdx = 0; modeIdx < modes.size(); ++modeIdx) {
                // Invariant: modeIdx < modes.size(), the pitch offset is below the pitch range
                // size, and checked_note_span proved the span lies inside the time axis.
                pianoroll.fill_unchecked(
                    modeIdx,
                    note->pitch - pitchRange.first,
                    modes[modeIdx] != PianorollMode::offset ? noteStart : noteEnd,
                    modes[modeIdx] == PianorollMode::frame ? noteEnd - noteStart : 1,
                    encodeVelocity ? note->velocity : 1
                );
            }
        }
    }

    return pianoroll;
};

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
    check_index("mode", mode, this->modeDim);
    check_index("pitch", pitch, this->pitchDim);
    check_index("time", time, this->timeDim);
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
    check_time_span(start, duration, this->timeDim);
    if (duration == 0) {
        check_index("mode", mode, this->modeDim);
        check_index("pitch", pitch, this->pitchDim);
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
        std::make_unique<pianoroll_t[]>(checked_volume({modeDim, trackDim, pitchDim, timeDim}))
    ) {}

ScorePianoroll ScorePianoroll::from_score(
    const Score<Tick>&                score,
    const std::vector<PianorollMode>& modes,
    const std::pair<uint8_t, uint8_t> pitchRange,
    const bool                        encodeVelocity
) {
    if (pitchRange.second < pitchRange.first) {
        throw std::invalid_argument("Pianoroll pitch range is reversed.");
    }
    const auto&    tracks  = *score.tracks;
    const size_t   timeDim = time_dim_from_end(int64_t{score.end()});
    ScorePianoroll pianoroll(
        modes.size(), tracks.size(), pitchRange.second - pitchRange.first, timeDim
    );

    for (size_t trackIdx = 0; trackIdx < tracks.size(); ++trackIdx) {
        for (const Track<Tick>& track = *tracks[trackIdx]; const auto& note : *track.notes) {
            if (note.pitch >= pitchRange.first && note.pitch < pitchRange.second) {
                const auto [noteStart, noteEnd]
                    = checked_note_span(note.time, note.duration, timeDim);
                for (size_t modeIdx = 0; modeIdx < modes.size(); ++modeIdx) {
                    // Invariant: modeIdx < modes.size(), trackIdx < tracks.size(), the pitch
                    // offset is below the pitch range size, and checked_note_span proved the
                    // span lies inside the time axis.
                    pianoroll.fill_unchecked(
                        modeIdx,
                        trackIdx,
                        note->pitch - pitchRange.first,
                        modes[modeIdx] != PianorollMode::offset ? noteStart : noteEnd,
                        modes[modeIdx] == PianorollMode::frame ? noteEnd - noteStart : 1,
                        encodeVelocity ? note->velocity : 1
                    );
                }
            }
        }
    }

    return pianoroll;
}

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
    check_index("mode", mode, this->modeDim);
    check_index("track", track, this->trackDim);
    check_index("pitch", pitch, this->pitchDim);
    check_index("time", time, this->timeDim);
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
    check_time_span(start, duration, this->timeDim);
    if (duration == 0) {
        check_index("mode", mode, this->modeDim);
        check_index("track", track, this->trackDim);
        check_index("pitch", pitch, this->pitchDim);
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

}   // namespace symusic

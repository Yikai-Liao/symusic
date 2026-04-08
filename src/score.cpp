//
// Created by lyk on 23-12-25.
//
#include <algorithm>
#include <cmath>
#include <optional>

#include "MetaMacro.h"
#include "symusic/conversion.h"
#include "symusic/score.h"
#include "symusic/ops.h"

namespace symusic {

// extern to_string and summary
#define EXTERN_REPR(__COUNT, T)                              \
    extern template std::string Score<T>::to_string() const; \
    extern template std::string Score<T>::summary() const;

REPEAT_ON(EXTERN_REPR, Tick, Quarter, Second)
#undef EXTERN_REPR

namespace details {

constexpr Quarter::unit kQuarterTimeTolerance = 1e-4f;

inline bool close_enough(const Quarter::unit lhs, const Quarter::unit rhs) {
    return std::fabs(lhs - rhs) <= kQuarterTimeTolerance;
}

inline bool ge_or_close(const Quarter::unit lhs, const Quarter::unit rhs) {
    return lhs > rhs || close_enough(lhs, rhs);
}

inline Quarter::unit beat_step_for(
    const std::optional<TimeSignature<Quarter>>& time_signature
) {
    if (!time_signature.has_value() || time_signature->denominator == 0) {
        return static_cast<Quarter::unit>(1);
    }
    if (time_signature->numerator % 3 == 0 && time_signature->numerator != 3) {
        return static_cast<Quarter::unit>(12.f / time_signature->denominator);
    }
    return static_cast<Quarter::unit>(4.f / time_signature->denominator);
}

inline size_t beats_per_bar_for(const TimeSignature<Quarter>& time_signature) {
    if (time_signature.numerator == 0) { return 1; }
    if (time_signature.numerator % 3 == 0 && time_signature.numerator != 3) {
        return std::max<size_t>(1, time_signature.numerator / 3);
    }
    return std::max<size_t>(1, time_signature.numerator);
}

template<TType From, TType To>
vec<typename To::unit> convert_times_via_markers(
    const vec<typename From::unit>& times, const Score<From>& context
) {
    if constexpr (std::is_same_v<From, To>) {
        return vec<typename To::unit>(times.begin(), times.end());
    } else {
        Score<From> marker_score(context.ticks_per_quarter);
        marker_score.tempos = context.tempos;

        vec<TextMeta<From>> markers;
        markers.reserve(times.size());
        for (const auto time : times) { markers.emplace_back(time, ""); }
        marker_score.markers = std::make_shared<pyvec<TextMeta<From>>>(std::move(markers));

        auto converted_markers = convert<To>(marker_score).markers->collect();
        vec<typename To::unit> converted_times;
        converted_times.reserve(converted_markers.size());
        for (const auto& marker : converted_markers) { converted_times.push_back(marker.time); }
        return converted_times;
    }
}

inline Score<Quarter> to_quarter_score(const Score<Quarter>& score) { return score; }

template<TType T>
    requires(!std::is_same_v<T, Quarter>)
Score<Quarter> to_quarter_score(const Score<T>& score) {
    return convert<Quarter>(score);
}

inline size_t find_time_index(
    const vec<Quarter::unit>& values, const Quarter::unit target, const size_t default_index
) {
    for (size_t index = 0; index < values.size(); ++index) {
        if (close_enough(values[index], target)) { return index; }
    }
    return default_index;
}

inline vec<Quarter::unit> compute_beats_quarter(
    const Score<Quarter>& score, const Quarter::unit start_time
) {
    const auto end_time = score.end();
    if (!std::isfinite(start_time) || !std::isfinite(end_time) || start_time >= end_time) {
        return {};
    }

    auto time_signatures = score.time_signatures->collect();
    std::sort(time_signatures.begin(), time_signatures.end(), [](const auto& lhs, const auto& rhs) {
        return lhs.time < rhs.time;
    });

    vec<Quarter::unit> beats{start_time};
    size_t next_ts_index = 0;
    std::optional<TimeSignature<Quarter>> current_time_signature = std::nullopt;
    while (next_ts_index < time_signatures.size()
           && ge_or_close(beats.back(), time_signatures[next_ts_index].time)) {
        current_time_signature = time_signatures[next_ts_index];
        ++next_ts_index;
    }

    while (beats.back() < end_time) {
        auto next_beat = beats.back() + beat_step_for(current_time_signature);
        if (next_ts_index < time_signatures.size()
            && ge_or_close(next_beat, time_signatures[next_ts_index].time)) {
            next_beat = time_signatures[next_ts_index].time;
            current_time_signature = time_signatures[next_ts_index];
            ++next_ts_index;
            while (next_ts_index < time_signatures.size()
                   && close_enough(time_signatures[next_ts_index].time, next_beat)) {
                current_time_signature = time_signatures[next_ts_index];
                ++next_ts_index;
            }
        }
        beats.push_back(next_beat);
    }

    beats.pop_back();
    return beats;
}

inline vec<Quarter::unit> compute_downbeats_quarter(
    const Score<Quarter>& score, const Quarter::unit start_time
) {
    auto beats = compute_beats_quarter(score, start_time);
    if (beats.empty()) { return {}; }

    auto time_signatures = score.time_signatures->collect();
    std::sort(time_signatures.begin(), time_signatures.end(), [](const auto& lhs, const auto& rhs) {
        return lhs.time < rhs.time;
    });
    if (time_signatures.empty() || time_signatures.front().time > start_time) {
        time_signatures.insert(time_signatures.begin(), TimeSignature<Quarter>(start_time, 4, 4));
    }

    vec<Quarter::unit> downbeats;
    size_t             end_beat_index = 0;
    for (size_t index = 0; index + 1 < time_signatures.size(); ++index) {
        const auto& start_ts = time_signatures[index];
        const auto& end_ts   = time_signatures[index + 1];
        const auto  start_beat_index = find_time_index(beats, start_ts.time, 0);
        end_beat_index = find_time_index(beats, end_ts.time, start_beat_index);
        const auto stride = beats_per_bar_for(start_ts);
        for (size_t beat_index = start_beat_index; beat_index < end_beat_index;
             beat_index += stride) {
            downbeats.push_back(beats[beat_index]);
        }
    }

    const auto& final_time_signature = time_signatures.back();
    const auto  start_beat_index
        = find_time_index(beats, final_time_signature.time, end_beat_index);
    const auto stride = beats_per_bar_for(final_time_signature);
    for (size_t beat_index = start_beat_index; beat_index < beats.size(); beat_index += stride) {
        downbeats.push_back(beats[beat_index]);
    }

    return downbeats;
}

}   // namespace details

template<TType T>
typename T::unit Score<T>::start() const {
    // if (this->empty()) return 0; // Old check is insufficient
    typename T::unit ans = std::numeric_limits<typename T::unit>::max();
    for (const auto& track : *tracks) { ans = std::min(ans, track->start()); }
    ans = std::min(ans, ops::start(*time_signatures));
    ans = std::min(ans, ops::start(*key_signatures));
    ans = std::min(ans, ops::start(*tempos));
    // ans = std::min(ans, ops::start(*lyrics)); // Lyrics are track-level
    ans = std::min(ans, ops::start(*markers));
    // If ans is still max(), it means all lists were empty or ops::start returned max()
    return ans == std::numeric_limits<typename T::unit>::max() ? 0 : ans;
}

template<TType T>
typename T::unit Score<T>::end() const {
    if (this->empty()) return 0;

    typename T::unit ans = std::numeric_limits<typename T::unit>::lowest();
    for (const shared<Track<T>>& track : *tracks) { ans = std::max(ans, track->end()); }
    ans = std::max(ans, ops::end(*time_signatures));
    ans = std::max(ans, ops::end(*key_signatures));
    ans = std::max(ans, ops::end(*tempos));
    // ans = std::max(ans, ops::end(*lyrics));
    ans = std::max(ans, ops::end(*markers));
    return ans;
}

template<TType T>
size_t Score<T>::note_num() const {
    size_t ans = 0;
    for (const shared<Track<T>>& track : *tracks) { ans += track->note_num(); }
    return ans;
}

template<TType T>
bool Score<T>::empty() const {
    return tracks->empty()
        && time_signatures->empty()
        && key_signatures->empty()
        && tempos->empty()
        // && lyrics->empty()
        && markers->empty();
}

template<TType T>
size_t Score<T>::track_num() const {
    return tracks->size();
}

template<TType T>
vec<typename T::unit> Score<T>::get_beats(const unit start_time) const {
    const auto quarter_score = details::to_quarter_score(*this);
    const auto quarter_start
        = details::convert_times_via_markers<T, Quarter>(vec<unit>{start_time}, *this).front();
    const auto quarter_beats = details::compute_beats_quarter(quarter_score, quarter_start);
    return details::convert_times_via_markers<Quarter, T>(quarter_beats, quarter_score);
}

template<TType T>
vec<typename T::unit> Score<T>::get_downbeats(const unit start_time) const {
    const auto quarter_score = details::to_quarter_score(*this);
    const auto quarter_start
        = details::convert_times_via_markers<T, Quarter>(vec<unit>{start_time}, *this).front();
    const auto quarter_downbeats
        = details::compute_downbeats_quarter(quarter_score, quarter_start);
    return details::convert_times_via_markers<Quarter, T>(quarter_downbeats, quarter_score);
}

template<TType T>
void Score<T>::sort_inplace(const bool reverse) {

    for (auto& track : *tracks) track->sort_inplace(reverse);
    auto key = [](const auto& event) { return event.default_key(); };
    time_signatures->sort(key, reverse);
    key_signatures->sort(key, reverse);
    tempos->sort(key, reverse);
    // lyrics->sort(key, reverse);
    markers->sort(key, reverse);
}

template<TType T>
Score<T> Score<T>::sort(const bool reverse) const {
    auto ans = deepcopy();
    ans.sort_inplace(reverse);
    return ans;
}

template<TType T>
void Score<T>::clip_inplace(unit start, unit end, bool clip_end) {
    for (auto& track : *tracks) track->clip_inplace(start, end, clip_end);
    ops::clip_with_sentinel_inplace(*time_signatures, start, end);
    ops::clip_with_sentinel_inplace(*key_signatures, start, end);
    ops::clip_with_sentinel_inplace(*tempos, start, end);
    // ops::clip_inplace(*lyrics, start, end);
    ops::clip_inplace(*markers, start, end);
}


template<TType T>
Score<T> Score<T>::clip(unit start, unit end, bool clip_end) const {
    auto ans = deepcopy();
    ans.clip_inplace(start, end, clip_end);
    return ans;
}

template<TType T>
void Score<T>::trim_inplace(unit start, unit end, unit min_overlap, const std::string &start_mode, const std::string &end_mode) {
    for (auto& track : *tracks) track->trim_inplace(start, end, min_overlap, start_mode, end_mode);;
    ops::clip_with_sentinel_inplace(*time_signatures, start, end);
    ops::clip_with_sentinel_inplace(*key_signatures, start, end);
    ops::clip_with_sentinel_inplace(*tempos, start, end);
    // ops::clip_inplace(*lyrics, start, end);
    ops::clip_inplace(*markers, start, end);
}

template<TType T>
Score<T> Score<T>::trim(unit start, unit end, unit min_overlap, const std::string &start_mode, const std::string &end_mode) const {
    auto ans = deepcopy();
    ans.trim_inplace(start, end, min_overlap, start_mode, end_mode);
    return ans;
}

// time shift
template<TType T>
void Score<T>::shift_time_inplace(const unit offset) {
    for (auto& track : *tracks) track->shift_time_inplace(offset);
    for (auto& time_signature : *time_signatures) time_signature->shift_time_inplace(offset);
    for (auto& key_signature : *key_signatures) key_signature->shift_time_inplace(offset);
    for (auto& tempo : *tempos) tempo->shift_time_inplace(offset);
    // for (auto& lyric : *lyrics) lyric->shift_time_inplace(offset);
    for (auto& marker : *markers) marker->shift_time_inplace(offset);
}

template<TType T>
Score<T> Score<T>::shift_time(const unit offset) const {
    auto ans = deepcopy();
    ans.shift_time_inplace(offset);
    return ans;
}

// pitch shift
template<TType T>
void Score<T>::shift_pitch_inplace(const i8 offset) {
    for (auto& track : *tracks) track->shift_pitch_inplace(offset);
}

template<TType T>
Score<T> Score<T>::shift_pitch(const i8 offset) const {
    auto ans = deepcopy();
    ans.shift_pitch_inplace(offset);
    return ans;
}

// velocity shift
template<TType T>
void Score<T>::shift_velocity_inplace(const i8 offset) {
    for (auto& track : *tracks) track->shift_velocity_inplace(offset);
}

template<TType T>
Score<T> Score<T>::shift_velocity(const i8 offset) const {
    auto ans = deepcopy();
    ans.shift_velocity_inplace(offset);
    return ans;
}

#define INSTANTIATE_SCORE(__COUNT, T) template struct Score<T>;
REPEAT_ON(INSTANTIATE_SCORE, Tick, Quarter, Second)
#undef INSTANTIATE_SCORE

}   // namespace symusic

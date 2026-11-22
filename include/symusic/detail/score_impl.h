#pragma once

#include <algorithm>
#include <cmath>
#include <optional>

#include "symusic/conversion.h"
#include "symusic/score.h"
#include "symusic/ops.h"

#include "symusic/repr.h"

namespace symusic {

namespace detail {

struct BeatTimelineQuarter {
    Quarter::unit               end_time;
    vec<TimeSignature<Quarter>> time_signatures;
};

constexpr Quarter::unit kQuarterTimeTolerance = 1e-4f;

inline bool close_enough(const Quarter::unit lhs, const Quarter::unit rhs) {
    return std::fabs(lhs - rhs) <= kQuarterTimeTolerance;
}

inline bool ge_or_close(const Quarter::unit lhs, const Quarter::unit rhs) {
    return lhs > rhs || close_enough(lhs, rhs);
}

inline Quarter::unit beat_step_for(const std::optional<TimeSignature<Quarter>>& time_signature) {
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

template<TType T, typename S>
BeatTimelineQuarter make_beat_timeline_quarter(const Score<T, S>& score) {
    return {
        .end_time        = convert_time<Quarter>(score.end(), score),
        .time_signatures = convert_time_events<Quarter, TimeSignature>(S::get(score.time_signatures), score)
    };
}

template<TType T, typename S>
Score<Quarter> make_quarter_tempo_context(const Score<T, S>& score) {
    Score<Quarter> quarter_context(score.ticks_per_quarter);
    quarter_context.tempos = convert_time_events<Quarter, Tempo>(S::get(score.tempos), score);
    return quarter_context;
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
    const BeatTimelineQuarter& timeline, const Quarter::unit start_time
) {
    const auto end_time = timeline.end_time;
    if (!std::isfinite(start_time) || !std::isfinite(end_time) || start_time >= end_time) {
        return {};
    }

    auto time_signatures = timeline.time_signatures;
    std::sort(time_signatures.begin(), time_signatures.end(), [](const auto& lhs, const auto& rhs) {
        return lhs.time < rhs.time;
    });

    vec<Quarter::unit>                    beats{start_time};
    size_t                                next_ts_index          = 0;
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
            next_beat              = time_signatures[next_ts_index].time;
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
    const BeatTimelineQuarter& timeline, const Quarter::unit start_time
) {
    auto beats = compute_beats_quarter(timeline, start_time);
    if (beats.empty()) { return {}; }

    auto time_signatures = timeline.time_signatures;
    std::sort(time_signatures.begin(), time_signatures.end(), [](const auto& lhs, const auto& rhs) {
        return lhs.time < rhs.time;
    });
    if (time_signatures.empty() || time_signatures.front().time > start_time) {
        time_signatures.insert(time_signatures.begin(), symusic::detail::make_event<TimeSignature<Quarter>>(start_time, 4, 4));
    }

    vec<Quarter::unit> downbeats;
    size_t             end_beat_index = 0;
    for (size_t index = 0; index + 1 < time_signatures.size(); ++index) {
        const auto& start_ts         = time_signatures[index];
        const auto& end_ts           = time_signatures[index + 1];
        const auto  start_beat_index = find_time_index(beats, start_ts.time, 0);
        end_beat_index               = find_time_index(beats, end_ts.time, start_beat_index);
        const auto stride            = beats_per_bar_for(start_ts);
        for (size_t beat_index = start_beat_index; beat_index < end_beat_index;
             beat_index += stride) {
            downbeats.push_back(beats[beat_index]);
        }
    }

    const auto& final_time_signature = time_signatures.back();
    const auto start_beat_index = find_time_index(beats, final_time_signature.time, end_beat_index);
    const auto stride           = beats_per_bar_for(final_time_signature);
    for (size_t beat_index = start_beat_index; beat_index < beats.size(); beat_index += stride) {
        downbeats.push_back(beats[beat_index]);
    }

    return downbeats;
}

}   // namespace detail

template<TType T, typename S>
typename T::unit Score<T, S>::start() const {
    typename T::unit ans = std::numeric_limits<typename T::unit>::max();
    for (const auto& track : S::get(tracks)) { ans = std::min(ans, S::get(track).start()); }
    ans = std::min(ans, ops::start(S::get(time_signatures)));
    ans = std::min(ans, ops::start(S::get(key_signatures)));
    ans = std::min(ans, ops::start(S::get(tempos)));
    ans = std::min(ans, ops::start(S::get(markers)));
    // If ans is still max(), it means all lists were empty or ops::start returned max()
    return ans == std::numeric_limits<typename T::unit>::max() ? 0 : ans;
}

template<TType T, typename S>
typename T::unit Score<T, S>::end() const {
    if (this->empty()) return 0;

    typename T::unit ans = std::numeric_limits<typename T::unit>::lowest();
    for (const auto& track : S::get(tracks)) { ans = std::max(ans, S::get(track).end()); }
    ans = std::max(ans, ops::end(S::get(time_signatures)));
    ans = std::max(ans, ops::end(S::get(key_signatures)));
    ans = std::max(ans, ops::end(S::get(tempos)));
    ans = std::max(ans, ops::end(S::get(markers)));
    return ans;
}

template<TType T, typename S>
size_t Score<T, S>::note_num() const {
    size_t ans = 0;
    for (const auto& track : S::get(tracks)) { ans += S::get(track).note_num(); }
    return ans;
}

template<TType T, typename S>
bool Score<T, S>::empty() const {
    return S::get(tracks).empty() && S::get(time_signatures).empty() && S::get(key_signatures).empty() && S::get(tempos).empty()
           && S::get(markers).empty();
}

template<TType T, typename S>
size_t Score<T, S>::track_num() const {
    return S::get(tracks).size();
}

template<TType T, typename S>
vec<typename T::unit> Score<T, S>::get_beats(const unit start_time) const {
    const auto timeline              = detail::make_beat_timeline_quarter(*this);
    const auto quarter_tempo_context = detail::make_quarter_tempo_context(*this);
    const auto quarter_start         = convert_time<Quarter>(start_time, *this);
    const auto quarter_beats         = detail::compute_beats_quarter(timeline, quarter_start);
    return convert_times<T, Quarter>(
        std::span<const Quarter::unit>(quarter_beats), quarter_tempo_context
    );
}

template<TType T, typename S>
vec<typename T::unit> Score<T, S>::get_downbeats(const unit start_time) const {
    const auto timeline              = detail::make_beat_timeline_quarter(*this);
    const auto quarter_tempo_context = detail::make_quarter_tempo_context(*this);
    const auto quarter_start         = convert_time<Quarter>(start_time, *this);
    const auto quarter_downbeats     = detail::compute_downbeats_quarter(timeline, quarter_start);
    return convert_times<T, Quarter>(
        std::span<const Quarter::unit>(quarter_downbeats), quarter_tempo_context
    );
}

template<TType T, typename S>
void Score<T, S>::sort_inplace(const bool reverse) {

    for (auto& track : S::get(tracks)) S::get(track).sort_inplace(reverse);
    auto key = [](const auto& event) { return event.default_key(); };
    S::sort(S::get(time_signatures), key, reverse);
    S::sort(S::get(key_signatures), key, reverse);
    S::sort(S::get(tempos), key, reverse);
    S::sort(S::get(markers), key, reverse);
}

template<TType T, typename S>
Score<T, S> Score<T, S>::sort(const bool reverse) const {
    auto ans = deepcopy();
    ans.sort_inplace(reverse);
    return ans;
}

template<TType T, typename S>
void Score<T, S>::clip_inplace(unit start, unit end, bool clip_end) {
    for (auto& track : S::get(tracks)) S::get(track).clip_inplace(start, end, clip_end);
    ops::clip_with_sentinel_inplace<S>(S::get(time_signatures), start, end);
    ops::clip_with_sentinel_inplace<S>(S::get(key_signatures), start, end);
    ops::clip_with_sentinel_inplace<S>(S::get(tempos), start, end);
    ops::clip_inplace<S>(S::get(markers), start, end);
}

template<TType T, typename S>
Score<T, S> Score<T, S>::clip(unit start, unit end, bool clip_end) const {
    auto ans = deepcopy();
    ans.clip_inplace(start, end, clip_end);
    return ans;
}

template<TType T, typename S>
void Score<T, S>::trim_inplace(
    unit               start,
    unit               end,
    unit               min_overlap,
    const std::string& start_mode,
    const std::string& end_mode
) {
    detail::validate_trim_modes(start_mode, end_mode);
    for (auto& track : S::get(tracks)) S::get(track).trim_inplace(start, end, min_overlap, start_mode, end_mode);
    ops::clip_with_sentinel_inplace<S>(S::get(time_signatures), start, end);
    ops::clip_with_sentinel_inplace<S>(S::get(key_signatures), start, end);
    ops::clip_with_sentinel_inplace<S>(S::get(tempos), start, end);
    ops::clip_inplace<S>(S::get(markers), start, end);
}

template<TType T, typename S>
Score<T, S> Score<T, S>::trim(
    unit               start,
    unit               end,
    unit               min_overlap,
    const std::string& start_mode,
    const std::string& end_mode
) const {
    auto ans = deepcopy();
    ans.trim_inplace(start, end, min_overlap, start_mode, end_mode);
    return ans;
}

// time shift
template<TType T, typename S>
void Score<T, S>::shift_time_inplace(const unit offset) {
    for (auto& track : S::get(tracks)) S::get(track).shift_time_inplace(offset);
    for (auto& time_signature : S::get(time_signatures)) symusic::shift_time(time_signature, offset);
    for (auto& key_signature : S::get(key_signatures)) symusic::shift_time(key_signature, offset);
    for (auto& tempo : S::get(tempos)) symusic::shift_time(tempo, offset);
    for (auto& marker : S::get(markers)) symusic::shift_time(marker, offset);
}

template<TType T, typename S>
Score<T, S> Score<T, S>::shift_time(const unit offset) const {
    auto ans = deepcopy();
    ans.shift_time_inplace(offset);
    return ans;
}

// pitch shift
template<TType T, typename S>
void Score<T, S>::shift_pitch_inplace(const i8 offset) {
    for (auto& track : S::get(tracks)) S::get(track).shift_pitch_inplace(offset);
}

template<TType T, typename S>
Score<T, S> Score<T, S>::shift_pitch(const i8 offset) const {
    auto ans = deepcopy();
    ans.shift_pitch_inplace(offset);
    return ans;
}

// velocity shift
template<TType T, typename S>
void Score<T, S>::shift_velocity_inplace(const i8 offset) {
    for (auto& track : S::get(tracks)) S::get(track).shift_velocity_inplace(offset);
}

template<TType T, typename S>
Score<T, S> Score<T, S>::shift_velocity(const i8 offset) const {
    auto ans = deepcopy();
    ans.shift_velocity_inplace(offset);
    return ans;
}

template<TType T, typename S>
bool Score<T, S>::operator==(const Score& other) const {
    const auto equal_tracks = [](const auto& a, const auto& b) {
        return S::get(a) == S::get(b);
    };
    return ticks_per_quarter == other.ticks_per_quarter
        && std::ranges::equal(S::get(tracks), S::get(other.tracks), equal_tracks)
        && S::get(time_signatures) == S::get(other.time_signatures)
        && S::get(key_signatures) == S::get(other.key_signatures)
        && S::get(tempos) == S::get(other.tempos)
        && S::get(markers) == S::get(other.markers);
}

template<TType T, typename S>
Score<T, S> Score<T, S>::deepcopy() const {
    const auto clone = [](const auto& field) {
        const auto& values = S::get(field);
        using E = typename std::remove_cvref_t<decltype(values)>::value_type;
        return S::adopt(vec<E>(values.begin(), values.end()));
    };
    vec<track_handle> copied_tracks;
    copied_tracks.reserve(S::get(tracks).size());
    for (const auto& track : S::get(tracks)) {
        copied_tracks.push_back(S::adopt_track(S::get(track).deepcopy()));
    }
    return {ticks_per_quarter, S::adopt_tracks(std::move(copied_tracks)), clone(time_signatures),
            clone(key_signatures), clone(tempos), clone(markers)};
}

template<TType T, typename S>
std::string Score<T, S>::summary() const {
    return fmt::format("Score(ttype={}, tpq={}, begin={}, end={}, tracks={}, notes={}, time_sig={}, key_sig={}, markers={})",
                       T(), ticks_per_quarter, start(), end(), track_num(), note_num(),
                       S::get(time_signatures).size(), S::get(key_signatures).size(), S::get(markers).size());
}

template<TType T, typename S>
std::string Score<T, S>::to_string() const {
    return summary();
}

}  // namespace symusic

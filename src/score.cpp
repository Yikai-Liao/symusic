#include <algorithm>
#include <cmath>
#include <span>

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

inline u8 default_beat_units(const TimeSignature<Quarter>& meter) {
    return meter.numerator > 3 && meter.numerator % 3 == 0 ? 3 : 1;
}

inline size_t beats_per_bar_for(const TimeSignature<Quarter>& meter) {
    if (!meter.beat_groups.empty() && meter.valid_beat_groups()) return meter.beat_groups.size();
    return std::max<size_t>(1, meter.numerator / default_beat_units(meter));
}

// A meter change starts its cycle at the first written beat group.
class BeatCycle {
    Quarter::unit       step_ = 1;
    std::span<const u8> groups_;
    std::span<const u8> remaining_;

public:
    BeatCycle() = default;

    explicit BeatCycle(const TimeSignature<Quarter>& meter) {
        if (meter.denominator == 0) return;
        if (!meter.beat_groups.empty() && meter.valid_beat_groups()) {
            step_   = 4.f / meter.denominator;
            groups_ = meter.beat_groups;
        } else {
            step_ = (4.f * default_beat_units(meter)) / meter.denominator;
        }
    }

    Quarter::unit next_step() {
        if (groups_.empty()) return step_;
        if (remaining_.empty()) remaining_ = groups_;
        const auto length = remaining_.front();
        remaining_        = remaining_.subspan(1);
        return length * step_;
    }
};

template<TType T>
BeatTimelineQuarter make_beat_timeline_quarter(const Score<T>& score) {
    return {
        .end_time        = convert_time<Quarter>(score.end(), score),
        .time_signatures = convert_time_events<Quarter>(*score.time_signatures, score).collect()
    };
}

template<TType T>
Score<Quarter> make_quarter_tempo_context(const Score<T>& score) {
    Score<Quarter> quarter_context(score.ticks_per_quarter);
    quarter_context.tempos = std::make_shared<pyvec<Tempo<Quarter>>>(
        convert_time_events<Quarter>(*score.tempos, score)
    );
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

    vec<Quarter::unit> beats{start_time};
    BeatCycle          cycle;
    auto               next_meter = time_signatures.cbegin();
    const auto         meters_end = time_signatures.cend();
    while (next_meter != meters_end && ge_or_close(start_time, next_meter->time)) {
        cycle = BeatCycle(*next_meter++);
    }

    while (beats.back() < end_time) {
        auto next_beat = beats.back() + cycle.next_step();
        if (next_meter != meters_end && ge_or_close(next_beat, next_meter->time)) {
            next_beat = next_meter->time;
            do {
                cycle = BeatCycle(*next_meter++);
            } while (next_meter != meters_end && close_enough(next_meter->time, next_beat));
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
        time_signatures.insert(time_signatures.begin(), TimeSignature<Quarter>(start_time, 4, 4));
    }

    vec<Quarter::unit> downbeats;
    size_t             end_beat_index = 0;
    for (size_t index = 0; index + 1 < time_signatures.size(); ++index) {
        const auto& start_ts         = time_signatures[index];
        const auto& end_ts           = time_signatures[index + 1];
        const auto  start_beat_index = find_time_index(beats, start_ts.time, 0);
        end_beat_index               = find_time_index(beats, end_ts.time, start_beat_index);
        if (start_ts.is_free_meter()) continue;
        const auto stride = beats_per_bar_for(start_ts);
        for (size_t beat_index = start_beat_index; beat_index < end_beat_index;
             beat_index += stride) {
            downbeats.push_back(beats[beat_index]);
        }
    }

    const auto& final_time_signature = time_signatures.back();
    if (final_time_signature.is_free_meter()) return downbeats;
    const auto start_beat_index = find_time_index(beats, final_time_signature.time, end_beat_index);
    const auto stride           = beats_per_bar_for(final_time_signature);
    for (size_t beat_index = start_beat_index; beat_index < beats.size(); beat_index += stride) {
        downbeats.push_back(beats[beat_index]);
    }

    return downbeats;
}

}   // namespace details

template<TType T>
typename T::unit Score<T>::start() const {
    typename T::unit ans = std::numeric_limits<typename T::unit>::max();
    for (const auto& track : *tracks) { ans = std::min(ans, track->start()); }
    ans = std::min(ans, ops::start(*time_signatures));
    ans = std::min(ans, ops::start(*key_signatures));
    ans = std::min(ans, ops::start(*tempos));
    ans = std::min(ans, ops::start(*markers));
    ans = std::min(ans, ops::start(*note_tunings));
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
    ans = std::max(ans, ops::end(*markers));
    ans = std::max(ans, ops::end(*note_tunings));
    if (explicit_end) ans = std::max(ans, *explicit_end);
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
    return tracks->empty() && time_signatures->empty() && key_signatures->empty() && tempos->empty()
           && markers->empty() && note_tunings->empty() && !explicit_end;
}

template<TType T>
size_t Score<T>::track_num() const {
    return tracks->size();
}

template<TType T>
vec<typename T::unit> Score<T>::get_beats(const unit start_time) const {
    const auto timeline              = details::make_beat_timeline_quarter(*this);
    const auto quarter_tempo_context = details::make_quarter_tempo_context(*this);
    const auto quarter_start         = convert_time<Quarter>(start_time, *this);
    const auto quarter_beats         = details::compute_beats_quarter(timeline, quarter_start);
    return convert_times<T, Quarter>(
        std::span<const Quarter::unit>(quarter_beats), quarter_tempo_context
    );
}

template<TType T>
vec<typename T::unit> Score<T>::get_downbeats(const unit start_time) const {
    const auto timeline              = details::make_beat_timeline_quarter(*this);
    const auto quarter_tempo_context = details::make_quarter_tempo_context(*this);
    const auto quarter_start         = convert_time<Quarter>(start_time, *this);
    const auto quarter_downbeats     = details::compute_downbeats_quarter(timeline, quarter_start);
    return convert_times<T, Quarter>(
        std::span<const Quarter::unit>(quarter_downbeats), quarter_tempo_context
    );
}

template<TType T>
void Score<T>::sort_inplace(const bool reverse) {

    for (auto& track : *tracks) track->sort_inplace(reverse);
    auto key = [](const auto& event) { return event.default_key(); };
    time_signatures->sort(key, reverse);
    key_signatures->sort(key, reverse);
    tempos->sort(key, reverse);
    markers->sort(key, reverse);
    note_tunings->sort(key, reverse);
}

template<TType T>
Score<T> Score<T>::sort(const bool reverse) const {
    auto ans = deepcopy();
    ans.sort_inplace(reverse);
    return ans;
}

template<class U>
void clip_explicit_end(std::optional<U>& extent, U start, U end) {
    if (!extent) return;
    if (const auto clipped = std::min(*extent, end); clipped > start)
        extent = clipped;
    else
        extent.reset();
}

template<TType T>
void Score<T>::clip_inplace(unit start, unit end, bool clip_end) {
    for (auto& track : *tracks) track->clip_inplace(start, end, clip_end);
    ops::clip_with_sentinel_inplace(*time_signatures, start, end);
    ops::clip_with_sentinel_inplace(*key_signatures, start, end);
    ops::clip_with_sentinel_inplace(*tempos, start, end);
    ops::clip_inplace(*markers, start, end);
    ops::clip_inplace(*note_tunings, start, end);
    clip_explicit_end(explicit_end, start, end);
}


template<TType T>
Score<T> Score<T>::clip(unit start, unit end, bool clip_end) const {
    auto ans = deepcopy();
    ans.clip_inplace(start, end, clip_end);
    return ans;
}

template<TType T>
void Score<T>::trim_inplace(
    unit               start,
    unit               end,
    unit               min_overlap,
    const std::string& start_mode,
    const std::string& end_mode
) {
    for (auto& track : *tracks) track->trim_inplace(start, end, min_overlap, start_mode, end_mode);
    ;
    ops::clip_with_sentinel_inplace(*time_signatures, start, end);
    ops::clip_with_sentinel_inplace(*key_signatures, start, end);
    ops::clip_with_sentinel_inplace(*tempos, start, end);
    ops::clip_inplace(*markers, start, end);
    ops::clip_inplace(*note_tunings, start, end);
    clip_explicit_end(explicit_end, start, end);
}

template<TType T>
Score<T> Score<T>::trim(
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
template<TType T>
void Score<T>::shift_time_inplace(const unit offset) {
    if (explicit_end) explicit_end = details::add_time(*explicit_end, offset);
    for (auto& track : *tracks) track->shift_time_inplace(offset);
    for (auto& time_signature : *time_signatures) time_signature->shift_time_inplace(offset);
    for (auto& key_signature : *key_signatures) key_signature->shift_time_inplace(offset);
    for (auto& tempo : *tempos) tempo->shift_time_inplace(offset);
    for (auto& marker : *markers) marker->shift_time_inplace(offset);
    for (auto& tuning : *note_tunings) tuning->shift_time_inplace(offset);
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

#pragma once

#include "symusic/track.h"
#include "symusic/ops.h"

#include "symusic/repr.h"

namespace symusic {

template<TType T, typename S>
typename T::unit Track<T, S>::start() const {
    typename T::unit ans = std::numeric_limits<typename T::unit>::max();
    ans                  = std::min(ans, ops::start(S::get(notes)));
    ans                  = std::min(ans, ops::start(S::get(controls)));
    ans                  = std::min(ans, ops::start(S::get(pitch_bends)));
    ans                  = std::min(ans, ops::start(S::get(pedals)));
    ans                  = std::min(ans, ops::start(S::get(lyrics)));
    return ans == std::numeric_limits<typename T::unit>::max() ? 0 : ans;
}

template<TType T, typename S>
typename T::unit Track<T, S>::end() const {
    if (this->empty()) return 0;
    typename T::unit ans = std::numeric_limits<typename T::unit>::min();
    ans                  = std::max(ans, ops::end(S::get(notes)));
    ans                  = std::max(ans, ops::end(S::get(controls)));
    ans                  = std::max(ans, ops::end(S::get(pitch_bends)));
    ans                  = std::max(ans, ops::end(S::get(pedals)));
    ans                  = std::max(ans, ops::end(S::get(lyrics)));
    return ans;
}

template<TType T, typename S>
size_t Track<T, S>::note_num() const {
    return S::get(notes).size();
}

template<TType T, typename S>
bool Track<T, S>::empty() const {
    return S::get(notes).empty() && S::get(controls).empty() && S::get(pitch_bends).empty() && S::get(pedals).empty()
           && S::get(lyrics).empty();
}

template<TType T, typename S>
void Track<T, S>::sort_inplace(const bool reverse) {
    auto key = [](const auto& event) { return event.default_key(); };
    S::sort(S::get(notes), key, reverse);
    S::sort(S::get(controls), key, reverse);
    S::sort(S::get(pitch_bends), key, reverse);
    S::sort(S::get(pedals), key, reverse);
    S::sort(S::get(lyrics), key, reverse);
}

template<TType T, typename S>
Track<T, S> Track<T, S>::sort(const bool reverse) {
    auto ans = deepcopy();
    ans.sort_inplace(reverse);
    return ans;
}

template<TType T, typename S>
void Track<T, S>::clip_inplace(const unit start, const unit end, const bool clip_end) {
    ops::clip_inplace<S>(S::get(notes), start, end, clip_end);
    ops::clip_inplace<S>(S::get(controls), start, end);
    ops::clip_inplace<S>(S::get(pitch_bends), start, end);
    ops::clip_inplace<S>(S::get(pedals), start, end, clip_end);
    ops::clip_inplace<S>(S::get(lyrics), start, end, clip_end);
}

template<TType T, typename S>
Track<T, S> Track<T, S>::clip(const unit start, const unit end, const bool clip_end) const {
    auto ans = deepcopy();
    ans.clip_inplace(start, end, clip_end);
    return ans;
}

template<TType T, typename S>
void Track<T, S>::trim_inplace(
    unit               start,
    unit               end,
    unit               min_overlap,
    const std::string& start_mode,
    const std::string& end_mode
) {
    ops::trim_inplace<S>(S::get(notes), start, end, min_overlap, start_mode, end_mode);
    ops::clip_inplace<S>(S::get(controls), start, end);
    ops::clip_inplace<S>(S::get(pitch_bends), start, end);
    ops::trim_inplace<S>(S::get(pedals), start, end, min_overlap, start_mode, end_mode);
    ops::clip_inplace<S>(S::get(lyrics), start, end);
}

template<TType T, typename S>
Track<T, S> Track<T, S>::trim(
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

template<TType T, typename S>
void Track<T, S>::shift_time_inplace(const unit offset) {
    for (auto& note : S::get(notes)) note->shift_time_inplace(offset);
    for (auto& control : S::get(controls)) control->shift_time_inplace(offset);
    for (auto& pitch_bend : S::get(pitch_bends)) pitch_bend->shift_time_inplace(offset);
    for (auto& pedal : S::get(pedals)) pedal->shift_time_inplace(offset);
    for (auto& lyric : S::get(lyrics)) lyric->shift_time_inplace(offset);
}

template<TType T, typename S>
Track<T, S> Track<T, S>::shift_time(const unit offset) const {
    auto ans = deepcopy();
    ans.shift_time_inplace(offset);
    return ans;
}

template<TType T, typename S>
void Track<T, S>::shift_pitch_inplace(const i8 offset) {
    for (auto& note : S::get(notes)) note->shift_pitch_inplace(offset);
}

template<TType T, typename S>
Track<T, S> Track<T, S>::shift_pitch(const i8 offset) const {
    auto ans = deepcopy();
    ans.shift_pitch_inplace(offset);
    return ans;
}

template<TType T, typename S>
void Track<T, S>::shift_velocity_inplace(const i8 offset) {
    for (auto& note : S::get(notes)) note->shift_velocity_inplace(offset);
}

template<TType T, typename S>
Track<T, S> Track<T, S>::shift_velocity(const i8 offset) const {
    auto ans = deepcopy();
    ans.shift_velocity_inplace(offset);
    return ans;
}

template<TType T, typename S>
bool Track<T, S>::operator==(const Track& other) const {
    return name == other.name && program == other.program && is_drum == other.is_drum
        && S::get(notes) == S::get(other.notes)
        && S::get(controls) == S::get(other.controls)
        && S::get(pitch_bends) == S::get(other.pitch_bends)
        && S::get(pedals) == S::get(other.pedals)
        && S::get(lyrics) == S::get(other.lyrics);
}

template<TType T, typename S>
Track<T, S> Track<T, S>::deepcopy() const {
    const auto clone = [](const auto& field) {
        const auto& values = S::get(field);
        using E = typename std::remove_cvref_t<decltype(values)>::value_type;
        return S::adopt(vec<E>(values.begin(), values.end()));
    };
    return {name, program, is_drum, clone(notes), clone(controls), clone(pitch_bends),
            clone(pedals), clone(lyrics)};
}

template<TType T, typename S>
std::string Track<T, S>::summary() const {
    return fmt::format("Track(ttype={}, program={}, is_drum={}, name={}, notes={}, lyrics={})",
                       T(), program, is_drum, name, note_num(), S::get(lyrics).size());
}

template<TType T, typename S>
std::string Track<T, S>::to_string() const {
    return summary();
}

}  // namespace symusic

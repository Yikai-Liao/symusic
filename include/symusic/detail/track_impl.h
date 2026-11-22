#pragma once

#include <optional>

#include "symusic/track.h"
#include "symusic/ops.h"

#include "symusic/repr.h"

namespace symusic {

template<TType T, typename S>
vec<Pedal<T>> Track<T, S>::pedals() const {
    vec<const ControlChange<T>*> sustain;
    for (const auto& control : S::get(controls)) {
        if (control.number == 64) sustain.push_back(&control);
    }
    std::stable_sort(sustain.begin(), sustain.end(), [](const auto* left, const auto* right) {
        return left->time < right->time;
    });
    vec<Pedal<T>> result;
    std::optional<unit> start;
    for (const auto* control : sustain) {
        if (control->value >= 64) {
            if (!start) start = control->time;
        } else if (start) {
            result.push_back(detail::make_event<Pedal<T>>(
                *start, detail::sub_time(control->time, *start)
            ));
            start.reset();
        }
    }
    return result;
}

template<TType T, typename S>
typename T::unit Track<T, S>::start() const {
    typename T::unit ans = std::numeric_limits<typename T::unit>::max();
    ans                  = std::min(ans, ops::start(S::get(notes)));
    ans                  = std::min(ans, ops::start(S::get(controls)));
    ans                  = std::min(ans, ops::start(S::get(pitch_bends)));
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
    ans                  = std::max(ans, ops::end(S::get(lyrics)));
    return ans;
}

template<TType T, typename S>
bool Track<T, S>::empty() const {
    return S::get(notes).empty() && S::get(controls).empty() && S::get(pitch_bends).empty()
           && S::get(lyrics).empty();
}

template<TType T, typename S>
void Track<T, S>::sort(const bool reverse) {
    auto key = [](const auto& event) { return event.default_key(); };
    S::sort(S::get(notes), key, reverse);
    S::stable_sort(S::get(controls), key, reverse);
    S::sort(S::get(pitch_bends), key, reverse);
    S::sort(S::get(lyrics), key, reverse);
}

template<TType T, typename S>
Track<T, S> Track<T, S>::clip(const unit start, const unit end, const bool clip_end) const {
    auto result = copy();
    ops::clip<S>(S::get(result.notes), start, end, clip_end);
    ops::clip<S>(S::get(result.controls), start, end);
    ops::clip<S>(S::get(result.pitch_bends), start, end);
    ops::clip<S>(S::get(result.lyrics), start, end);
    return result;
}

template<TType T, typename S>
Track<T, S> Track<T, S>::trim(
    unit               start,
    unit               end,
    unit               min_overlap,
    const std::string& start_mode,
    const std::string& end_mode
) const {
    auto result = copy();
    ops::trim<S>(S::get(result.notes), start, end, min_overlap, start_mode, end_mode);
    ops::clip<S>(S::get(result.controls), start, end);
    ops::clip<S>(S::get(result.pitch_bends), start, end);
    ops::clip<S>(S::get(result.lyrics), start, end);
    return result;
}

template<TType T, typename S>
void Track<T, S>::shift_time(const unit offset) {
    detail::FieldUpdates<unit> changes;
    detail::collect_time_shifts(*this, offset, changes);
    changes.apply();
}

template<TType T, typename S>
void Track<T, S>::shift_pitch(const i32 offset) {
    detail::FieldUpdates<i8> changes;
    detail::collect_note_shifts<2>(*this, offset, changes);
    changes.apply();
}

template<TType T, typename S>
void Track<T, S>::shift_velocity(const i32 offset) {
    detail::FieldUpdates<i8> changes;
    detail::collect_note_shifts<3>(*this, offset, changes);
    changes.apply();
}

template<TType T, typename S>
bool Track<T, S>::operator==(const Track& other) const {
    return name == other.name && program == other.program && is_drum == other.is_drum
        && S::get(notes) == S::get(other.notes)
        && S::get(controls) == S::get(other.controls)
        && S::get(pitch_bends) == S::get(other.pitch_bends)
        && S::get(lyrics) == S::get(other.lyrics);
}

template<TType T, typename S>
Track<T, S> Track<T, S>::copy() const {
    const auto clone = [](const auto& field) {
        const auto& values = S::get(field);
        using E = typename std::remove_cvref_t<decltype(values)>::value_type;
        return S::adopt(vec<E>(values.begin(), values.end()));
    };
    return {name, program, is_drum, clone(notes), clone(controls), clone(pitch_bends),
            clone(lyrics)};
}

namespace detail {
inline std::string quote_track_name(std::string_view name) {
    const auto escaped = fmt::format("{:?}", name);
    if (name.find('\'') != std::string_view::npos
        && name.find('"') == std::string_view::npos) {
        return escaped;
    }

    // Reuse fmt's escaping, changing only escapes affected by the delimiter.
    std::string quoted = "'";
    const std::string_view contents(escaped.data() + 1, escaped.size() - 2);
    for (size_t i = 0; i < contents.size(); ++i) {
        const char ch = contents[i];
        if (ch == '\\') {
            const char next = contents[++i];
            if (next != '"') quoted += '\\';
            quoted += next;
        } else {
            if (ch == '\'') quoted += '\\';
            quoted += ch;
        }
    }
    quoted += '\'';
    return quoted;
}
}  // namespace detail

template<TType T, typename S>
std::string Track<T, S>::summary() const {
    return fmt::format("Track(ttype='{}', program={}, is_drum={}, name={}, notes={}, lyrics={})",
                       T(), program, is_drum ? "True" : "False", detail::quote_track_name(name),
                       S::get(notes).size(), S::get(lyrics).size());
}

template<TType T, typename S>
std::string Track<T, S>::to_string() const {
    return summary();
}

}  // namespace symusic

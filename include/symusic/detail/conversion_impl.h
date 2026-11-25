#pragma once

#include "symusic/detail/event_fields.h"
#include "symusic/conversion.h"

namespace symusic {

template<TType To, TType From, typename S>
Score<To, S> convert(const Score<From, S>& score, typename To::unit min_duration) {
    const auto                               converter  = detail::make_time_converter<To>(score);
    const auto&                              old_tracks = S::get(score.tracks);
    vec<typename Score<To, S>::track_handle> tracks;
    tracks.reserve(old_tracks.size());
    for (const auto& handle : old_tracks) {
        const auto& track = S::get(handle);
        tracks.push_back(
            S::adopt_track(
                Track<To, S>{
                    track.name,
                    track.program,
                    track.is_drum,
                    S::adopt(
                        converter.template duration_vec<Note>(S::get(track.notes), min_duration)
                    ),
                    S::adopt(converter.template time_vec<ControlChange>(S::get(track.controls))),
                    S::adopt(converter.template time_vec<PitchBend>(S::get(track.pitch_bends))),
                    S::adopt(converter.template time_vec<TextMeta>(S::get(track.lyrics)))
                }
            )
        );
    }
    return {
        score.ticks_per_quarter,
        S::adopt_tracks(std::move(tracks)),
        S::adopt(converter.template time_vec<TimeSignature>(S::get(score.time_signatures))),
        S::adopt(converter.template time_vec<KeySignature>(S::get(score.key_signatures))),
        S::adopt(converter.template time_vec<Tempo>(S::get(score.tempos))),
        S::adopt(converter.template time_vec<TextMeta>(S::get(score.markers)))
    };
}

namespace detail {

template<bool HasDuration = false, typename Range, typename Convert>
auto resample_events(const Range& input, Convert convert, Tick::unit min_duration = 0) {
    using E = typename Range::value_type;
    vec<E> output;
    output.reserve(input.size());
    for (const auto& event : input) {
        if constexpr (HasDuration) {
            output.push_back(
                retime_event<E>(
                    convert(event.time), std::max(convert(event.duration), min_duration), event
                )
            );
        } else {
            output.push_back(retime_event<E>(convert(event.time), event));
        }
    }
    return output;
}

template<typename S>
Score<Tick, S> resample_ticks(
    const Score<Tick, S>& score, i32 ticks_per_quarter, i32 min_duration
) {
    if (ticks_per_quarter <= 0) {
        throw std::invalid_argument("symusic::resample: ticks_per_quarter must be positive");
    }
    if (min_duration < 0) {
        throw std::invalid_argument("symusic::resample: min_duration must be non-negative");
    }
    const auto scale
        = static_cast<f64>(ticks_per_quarter) / static_cast<f64>(score.ticks_per_quarter);
    auto convert_time
        = [scale](Tick::unit time) { return detail::round_to_i32(scale * static_cast<f64>(time)); };
    const auto&                                old_tracks = S::get(score.tracks);
    vec<typename Score<Tick, S>::track_handle> tracks;
    tracks.reserve(old_tracks.size());
    for (const auto& handle : old_tracks) {
        const auto& track = S::get(handle);
        tracks.push_back(
            S::adopt_track(
                Track<Tick, S>{
                    track.name,
                    track.program,
                    track.is_drum,
                    S::adopt(
                        resample_events<true>(S::get(track.notes), convert_time, min_duration)
                    ),
                    S::adopt(resample_events(S::get(track.controls), convert_time)),
                    S::adopt(resample_events(S::get(track.pitch_bends), convert_time)),
                    S::adopt(resample_events(S::get(track.lyrics), convert_time))
                }
            )
        );
    }
    return {
        ticks_per_quarter,
        S::adopt_tracks(std::move(tracks)),
        S::adopt(resample_events(S::get(score.time_signatures), convert_time)),
        S::adopt(resample_events(S::get(score.key_signatures), convert_time)),
        S::adopt(resample_events(S::get(score.tempos), convert_time)),
        S::adopt(resample_events(S::get(score.markers), convert_time))
    };
}

}   // namespace detail

template<TType T, typename S>
Score<Tick, S> resample(const Score<T, S>& score, i32 ticks_per_quarter, Tick::unit min_duration) {
    if constexpr (std::same_as<T, Tick>) {
        return detail::resample_ticks(score, ticks_per_quarter, min_duration);
    } else {
        return detail::resample_ticks(convert<Tick>(score), ticks_per_quarter, min_duration);
    }
}

}   // namespace symusic

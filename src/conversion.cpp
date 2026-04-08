//
// Created by lyk on 23-12-16.
// This file is now much too confusing to read.
// Refactor it later.
//
#include <cmath>
#include "symusic/conversion.h"
#include "symusic/ops.h"

namespace symusic {

namespace details {

template<TType To, TType From, typename Converter>
Score<To> convertInner(
    const Score<From>& score, const Converter& converter, const typename To::unit min_dur
) {
    Score<To> new_s(score.ticks_per_quarter);
    // new_s.lyrics   = std::make_shared<pyvec<TextMeta<To>>>(
    //     std::move(converter.time_vec(*score.lyrics))
    // );
    new_s.markers  = std::make_shared<pyvec<TextMeta<To>>>(
        std::move(converter.time_vec(*score.markers))
    );
    new_s.tempos   = std::make_shared<pyvec<Tempo<To>>>(
        std::move(converter.time_vec(*score.tempos))
    );
    new_s.time_signatures = std::make_shared<pyvec<TimeSignature<To>>>(
        std::move(converter.time_vec(*score.time_signatures))
    );
    new_s.key_signatures = std::make_shared<pyvec<KeySignature<To>>>(
        std::move(converter.time_vec(*score.key_signatures))
    );

    new_s.tracks->reserve(score.tracks->size());
    for (const shared<Track<From>>& track : *score.tracks) {
        auto new_t   = std::make_shared<Track<To>>(track->name, track->program, track->is_drum);
        new_t->notes = std::make_shared<pyvec<Note<To>>>(
            std::move(converter.duration_vec(*track->notes, min_dur))
        );
        new_t->pedals = std::make_shared<pyvec<Pedal<To>>>(
            std::move(converter.duration_vec(*track->pedals, min_dur))
        );
        new_t->pitch_bends = std::make_shared<pyvec<PitchBend<To>>>(
            std::move(converter.time_vec(*track->pitch_bends))
        );
        new_t->controls = std::make_shared<pyvec<ControlChange<To>>>(
            std::move(converter.time_vec(*track->controls))
        );
        new_t->lyrics = std::make_shared<pyvec<TextMeta<To>>>(
            std::move(converter.time_vec(*track->lyrics))
        );
        new_s.tracks->push_back(std::move(new_t));
    }
    return new_s;
}

}   // namespace details

#define IMPLEMENT_CONVERT(To, From)                                                          \
    template<>                                                                               \
    Score<To> convert<To, From>(const Score<From>& score, To::unit min_dur) {                \
        return details::convertInner<To, From>(score, details::make_time_converter<To>(score), min_dur); \
    }

//                To        From
IMPLEMENT_CONVERT(Tick, Tick)
IMPLEMENT_CONVERT(Quarter, Quarter)
IMPLEMENT_CONVERT(Second, Second)

IMPLEMENT_CONVERT(Tick, Quarter)
IMPLEMENT_CONVERT(Quarter, Tick)

IMPLEMENT_CONVERT(Second, Tick)
IMPLEMENT_CONVERT(Tick, Second)

IMPLEMENT_CONVERT(Second, Quarter)
IMPLEMENT_CONVERT(Quarter, Second)

#undef IMPLEMENT_CONVERT

namespace details {

template<class T, class Convert>
shared<pyvec<T>> resample_time(const pyvec<T>& data, Convert convert) {
    vec<T> capsule;
    capsule.reserve(data.size());
    for (const auto& item : data) { capsule.emplace_back(convert(item->time), item); }
    return std::make_shared<pyvec<T>>(std::move(capsule));
}

template<class T, class Convert>
shared<pyvec<T>> resample_dur(
    const pyvec<T>& data, Convert convert, const typename T::unit min_dur
) {
    vec<T> capsule;
    capsule.reserve(data.size());

    for (const auto& item : data) {
        capsule.emplace_back(convert(item->time), std::max(convert(item->duration), min_dur), item);
    }
    return std::make_shared<pyvec<T>>(std::move(capsule));
}

Score<Tick> resample_inner(const Score<Tick>& score, const i32 tpq, const i32 min_dur) {
    if (tpq <= 0) {
        throw std::invalid_argument("symusic::resample: ticks_per_quarter must be positive");
    }
    if (min_dur < 0) {
        throw std::invalid_argument("symusic::resample: min_dur must be non-negative");
    }
    Score<Tick> ans(tpq);
    const f64   scale_rate = static_cast<f64>(tpq) / static_cast<f64>(score.ticks_per_quarter);

    auto f64toi32 = [](const f64 x) {
        if (x > static_cast<f64>(std::numeric_limits<i32>::max())) {
            throw std::overflow_error(
                "symusic::resample: time after resample (" + std::to_string(x)
                + ") is out of int32 range"
            );
        }
        return static_cast<i32>(std::round(x));
    };

    auto convert = [f64toi32, scale_rate](const Tick::unit t) -> Tick::unit {
        return f64toi32(scale_rate * static_cast<f64>(t));
    };

    // REPEAT_ON(RESAMPLE_GENERAL, time_signatures, key_signatures, tempos, lyrics, markers)
    ans.time_signatures = resample_time(*score.time_signatures, convert);
    ans.key_signatures  = resample_time(*score.key_signatures, convert);
    ans.tempos          = resample_time(*score.tempos, convert);
    // ans.lyrics          = resample_time(*score.lyrics, convert);
    ans.markers         = resample_time(*score.markers, convert);

    const size_t track_num = score.tracks->size();
    ans.tracks             = std::make_shared<vec<shared<Track<Tick>>>>();
    ans.tracks->reserve(track_num);
    for (const auto& old_track : *score.tracks) {
        auto new_track = std::make_shared<Track<Tick>>(
            old_track->name, old_track->program, old_track->is_drum
        );
        new_track->notes       = resample_dur(*old_track->notes, convert, min_dur);
        new_track->pedals      = resample_dur(*old_track->pedals, convert, min_dur);
        new_track->pitch_bends = resample_time(*old_track->pitch_bends, convert);
        new_track->controls    = resample_time(*old_track->controls, convert);
        new_track->lyrics      = resample_time(*old_track->lyrics, convert);
        ans.tracks->push_back(std::move(new_track));
    }
    return ans;
}

}   // namespace details

template<>
Score<Tick> resample(const Score<Quarter>& score, const i32 tpq, const i32 min_dur) {
    return details::resample_inner(convert<Tick>(score), tpq, min_dur);
}

template<>
Score<Tick> resample(const Score<Tick>& score, const i32 tpq, const i32 min_dur) {
    return details::resample_inner(score, tpq, min_dur);
}
template<>
Score<Tick> resample(const Score<Second>& score, const i32 tpq, const i32 min_dur) {
    return details::resample_inner(convert<Tick>(score), tpq, min_dur);
}
}   // namespace symusic

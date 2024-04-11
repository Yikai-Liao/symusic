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
    new_s.time_signatures = std::make_shared<vec<shared<TimeSignature<To>>>>(
        std::move(converter.time_vec(*score.time_signatures))
    );
    new_s.key_signatures = std::make_shared<vec<shared<KeySignature<To>>>>(
        std::move(converter.time_vec(*score.key_signatures))
    );
    new_s.tempos =
        std::make_shared<vec<shared<Tempo<To>>>>(std::move(converter.time_vec(*score.tempos)));
    new_s.lyrics =
        std::make_shared<vec<shared<TextMeta<To>>>>(std::move(converter.time_vec(*score.lyrics)));
    new_s.markers =
        std::make_shared<vec<shared<TextMeta<To>>>>(std::move(converter.time_vec(*score.markers)));

    new_s.tracks->reserve(score.tracks->size());
    for (const shared<Track<From>>& track : *score.tracks) {
        Track<To> new_t(track->name, track->program, track->is_drum);
        new_t.notes = std::make_shared<vec<shared<Note<To>>>>(
            std::move(converter.duration_vec(*track->notes, min_dur))
        );
        new_t.pedals = std::make_shared<vec<shared<Pedal<To>>>>(
            std::move(converter.duration_vec(*track->pedals, min_dur))
        );
        new_t.pitch_bends = std::make_shared<vec<shared<PitchBend<To>>>>(
            std::move(converter.time_vec(*track->pitch_bends))
        );
        new_t.controls = std::make_shared<vec<shared<ControlChange<To>>>>(
            std::move(converter.time_vec(*track->controls))
        );

        new_s.tracks->push_back(std::make_shared<Track<To>>(std::move(new_t)));
    }
    return new_s;
}

template<typename Converter, TType To, TType From>
struct SimpleConverter {
    template<template<typename> typename T>
    [[nodiscard]] vec<shared<T<To>>> time_vec(const vec<shared<T<From>>>& data) const {
        const auto self = static_cast<const Converter*>(this);

        shared<vec<T<To>>> capsule = std::make_shared<vec<T<To>>>();
        capsule->reserve(data.size());

        vec<shared<T<To>>> ans;
        ans.reserve(data.size());

        for (const shared<T<From>>& d : data) {
            capsule->emplace_back(self->time(d->time), *d);
            ans.emplace_back(capsule, &capsule->back());
        }
        return ans;
    }

    template<template<class> class T>
    [[nodiscard]] vec<shared<T<To>>> duration_vec(const vec<shared<T<From>>>& data, typename To::unit min_dur)
        const {
        const auto self = static_cast<const Converter*>(this);
        min_dur         = std::max(min_dur, static_cast<typename To::unit>(0));

        shared<vec<T<To>>> capsule = std::make_shared<vec<T<To>>>();
        capsule->reserve(data.size());

        vec<shared<T<To>>> ans;
        ans.reserve(data.size());

        for (const shared<T<From>>& d : data) {
            capsule->emplace_back(
                self->time(d->time), std::max(min_dur, self->time(d->duration)), *d
            );
            ans.emplace_back(capsule, &capsule->back());
        }
        return ans;
    }
};

struct Tick2Tick : SimpleConverter<Tick2Tick, Tick, Tick> {
    typedef Tick From;
    typedef Tick To;

    explicit        Tick2Tick(const Score<From>& score) {}
    static To::unit time(const From::unit t) { return t; }
};

struct Quarter2Quarter : SimpleConverter<Quarter2Quarter, Quarter, Quarter> {
    typedef Quarter From;
    typedef Quarter To;

    explicit        Quarter2Quarter(const Score<From>& score) {}
    static To::unit time(const From::unit t) { return t; }
};

struct Second2Second : SimpleConverter<Second2Second, Second, Second> {
    typedef Second From;
    typedef Second To;

    explicit        Second2Second(const Score<From>& score) {}
    static To::unit time(const From::unit t) { return t; }
};

struct Tick2Quarter : SimpleConverter<Tick2Quarter, Quarter, Tick> {
    typedef Tick    From;
    typedef Quarter To;
    f32             tpq;

    explicit Tick2Quarter(const Score<From>& score) :
        tpq(static_cast<f32>(score.ticks_per_quarter)) {}
    [[nodiscard]] To::unit time(const From::unit t) const { return static_cast<To::unit>(t) / tpq; }
};

struct Quarter2Tick : SimpleConverter<Quarter2Tick, Tick, Quarter> {
    typedef Quarter From;
    typedef Tick    To;
    f32             tpq;

    explicit Quarter2Tick(const Score<From>& score) :
        tpq(static_cast<f32>(score.ticks_per_quarter)) {}
    [[nodiscard]] To::unit time(const From::unit t) const {
        return static_cast<To::unit>(std::round(t * tpq));
    }
};

template<typename Converter, TType To, TType From>
struct SecondConverter {
    f64                      tpq;
    vec<typename To::unit>   to_times;
    vec<typename From::unit> from_times;
    vec<f64>                 factors;

    explicit SecondConverter(const Score<From>& score) :
        tpq(static_cast<f64>(score.ticks_per_quarter)) {
        vec<Tempo<From>> tempos;
        if (score.tempos->empty()) {
            // 120 qpm
            tempos = {{0, 500000}, {std::numeric_limits<typename From::unit>::max(), 500000}};
        } else {
            tempos.reserve(score.tempos->size() + 2);
            for (const shared<Tempo<From>>& tempo : *(score.tempos)) {
                tempos.emplace_back(*tempo);
            }
            ops::sort_by_time(tempos);
            if (tempos.empty() || tempos[0].time != static_cast<typename From::unit>(0)) {
                tempos.insert(tempos.begin(), Tempo<From>(0, 500000));
            }
            // add a guard at the end
            tempos.emplace_back(
                std::numeric_limits<typename From::unit>::max(), tempos.back().mspq
            );
        }

        const auto self = static_cast<const Converter*>(this);

        typename To::unit   pivot_to   = 0;
        typename From::unit pivot_from = 0;
        double              cur_factor = self->get_factor(tempos[0]);

        to_times.reserve(tempos.size());
        from_times.reserve(tempos.size());
        factors.reserve(tempos.size());

        to_times.emplace_back(pivot_to);
        from_times.emplace_back(pivot_from);
        factors.emplace_back(cur_factor);

        for (auto i = 1; i < tempos.size(); ++i) {
            pivot_to   = self->get_time(tempos[i].time, pivot_to, pivot_from, cur_factor);
            pivot_from = tempos[i].time;
            cur_factor = self->get_factor(tempos[i]);
            to_times.emplace_back(pivot_to);
            from_times.emplace_back(pivot_from);
            factors.emplace_back(cur_factor);
        }
    }

    template<template<class> class T>
    [[nodiscard]] vec<shared<T<To>>> time_vec(const vec<shared<T<From>>>& data) const {
        const auto         self    = static_cast<const Converter*>(this);
        shared<vec<T<To>>> capsule = std::make_shared<vec<T<To>>>();
        capsule->reserve(data.size());

        vec<shared<T<To>>> ans;
        ans.reserve(data.size());

        auto cur_range  = std::make_pair(from_times[0], from_times[1]);
        auto pivot_to   = to_times[0];
        auto cur_factor = factors[0];

        for (const shared<T<From>>& event : data) {
            if ((event->time) < cur_range.first | (event->time) >= cur_range.second) {
                auto i = std::upper_bound(from_times.begin(), from_times.end(), event->time) -
                         from_times.begin() - 1;
                cur_range  = std::make_pair(from_times[i], from_times[i + 1]);
                pivot_to   = to_times[i];
                cur_factor = factors[i];
            }
            capsule->emplace_back(
                self->get_time(event->time, pivot_to, cur_range.first, cur_factor), *event
            );
            ans.emplace_back(capsule, &capsule->back());
        }
        return ans;
    }

    template<template<class> class T>
    [[nodiscard]] vec<shared<T<To>>> duration_vec(const vec<shared<T<From>>>& data, typename To::unit min_dur)
        const {
        const auto self = static_cast<const Converter*>(this);
        min_dur         = std::max(min_dur, static_cast<typename To::unit>(0));

        shared<vec<T<To>>> capsule = std::make_shared<vec<T<To>>>();
        capsule->reserve(data.size());

        vec<shared<T<To>>> ans;
        ans.reserve(data.size());

        auto cur_range  = std::make_pair(from_times[0], from_times[1]);
        auto pivot_to   = to_times[0];
        auto cur_factor = factors[0];
        for (const shared<T<From>>& event : data) {
            if ((event->time) < cur_range.first | (event->time) >= cur_range.second) {
                auto i = std::upper_bound(from_times.begin(), from_times.end(), event->time) -
                         from_times.begin() - 1;
                cur_range  = std::make_pair(from_times[i], from_times[i + 1]);
                pivot_to   = to_times[i];
                cur_factor = factors[i];
            }
            auto start = self->get_time(event->time, pivot_to, cur_range.first, cur_factor);
            if (auto end = event->end(); end < cur_range.first | end >= cur_range.second) {
                auto i = std::upper_bound(from_times.begin(), from_times.end(), end) -
                         from_times.begin() - 1;
                cur_range  = std::make_pair(from_times[i], from_times[i + 1]);
                pivot_to   = to_times[i];
                cur_factor = factors[i];
            }
            auto end      = self->get_time(event->end(), pivot_to, cur_range.first, cur_factor);
            auto duration = std::max(min_dur, end - start);
            capsule->emplace_back(start, duration, *event);
            ans.emplace_back(capsule, &capsule->back());
        }
        return ans;
    }
};

struct Tick2Second : SecondConverter<Tick2Second, Second, Tick> {
    typedef Tick   From;
    typedef Second To;

    explicit Tick2Second(const Score<From>& score) : SecondConverter{score} {}

    [[nodiscard]] f64 get_factor(const Tempo<From>& tempo) const {
        return static_cast<f64>(tempo.mspq) / 1000000. / tpq;
    }

    [[nodiscard]] To::unit static get_time(
        const From::unit t,
        const To::unit   pivot_to,
        const From::unit pivot_from,
        const f64        cur_factor
    ) {
        return pivot_to + static_cast<To::unit>(cur_factor * (t - pivot_from));
    }
};

struct Second2Tick : SecondConverter<Second2Tick, Tick, Second> {
    typedef Second From;
    typedef Tick   To;

    explicit Second2Tick(const Score<From>& score) : SecondConverter{score} {}

    [[nodiscard]] f64 get_factor(const Tempo<From>& tempo) const {
        return 1000000. * tpq / static_cast<f64>(tempo.mspq);
    }

    [[nodiscard]] To::unit static get_time(
        const From::unit t,
        const To::unit   pivot_to,
        const From::unit pivot_from,
        const f64        cur_factor
    ) {
        return pivot_to + static_cast<To::unit>(std::round(cur_factor * (t - pivot_from)));
    }
};

struct Quarter2Second : SecondConverter<Quarter2Second, Second, Quarter> {
    typedef Quarter From;
    typedef Second  To;

    explicit Quarter2Second(const Score<From>& score) : SecondConverter{score} {}

    [[nodiscard]] static f64 get_factor(const Tempo<From>& tempo) {
        return static_cast<f64>(tempo.mspq) / 1000000.;
    }

    [[nodiscard]] To::unit static get_time(
        const From::unit t,
        const To::unit   pivot_to,
        const From::unit pivot_from,
        const f64        cur_factor
    ) {
        return pivot_to + static_cast<To::unit>(cur_factor * (t - pivot_from));
    }
};

struct Second2Quarter : SecondConverter<Second2Quarter, Quarter, Second> {
    typedef Second  From;
    typedef Quarter To;

    explicit Second2Quarter(const Score<From>& score) : SecondConverter{score} {}

    [[nodiscard]] static f64 get_factor(const Tempo<From>& tempo) {
        return 1000000. / static_cast<f64>(tempo.mspq);
    }

    [[nodiscard]] To::unit static get_time(
        const From::unit t,
        const To::unit   pivot_to,
        const From::unit pivot_from,
        const f64        cur_factor
    ) {
        return pivot_to + static_cast<To::unit>(cur_factor * (t - pivot_from));
    }
};

}   // namespace details

#define IMPLEMENT_CONVERT(To, From)                                                          \
    template<>                                                                               \
    Score<To> convert<To, From>(const Score<From>& score, To::unit min_dur) {                \
        return details::convertInner<To, From>(score, details::From##2##To(score), min_dur); \
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
shared<vec<shared<T>>> resample_time(const vec<shared<T>>& data, Convert convert) {
    auto capsule = std::make_shared<vec<T>>();
    capsule->reserve(data.size());
    vec<shared<T>> ans;
    ans.reserve(data.size());
    for (const auto& item : data) {
        capsule->emplace_back(convert(item->time), *item);
        ans.emplace_back(capsule, &capsule->back());
    }
    return std::make_shared<vec<shared<T>>>(std::move(ans));
}

template<class T, class Convert>
shared<vec<shared<T>>> resample_dur(
    const vec<shared<T>>& data, Convert convert, const typename T::unit min_dur
) {
    auto capsule = std::make_shared<vec<T>>();
    capsule->reserve(data.size());
    vec<shared<T>> ans;
    ans.reserve(data.size());
    for (const auto& item : data) {
        capsule->emplace_back(
            convert(item->time), std::max(convert(item->duration), min_dur), *item
        );
        ans.emplace_back(capsule, &capsule->back());
    }
    return std::make_shared<vec<shared<T>>>(std::move(ans));
}

Score<Tick> resample_inner(const Score<Tick>& score, const i32 tpq, const i32 min_dur) {
    Score<Tick> ans(tpq);
    const auto  from_tpq = static_cast<double>(score.ticks_per_quarter);
    auto        convert  = [tpq, from_tpq](const Tick::unit t) -> Tick::unit {
        return static_cast<Tick::unit>(std::round(static_cast<double>(tpq * t) / from_tpq));
    };

    // REPEAT_ON(RESAMPLE_GENERAL, time_signatures, key_signatures, tempos, lyrics, markers)
    ans.time_signatures = resample_time(*score.time_signatures, convert);
    ans.key_signatures  = resample_time(*score.key_signatures, convert);
    ans.tempos          = resample_time(*score.tempos, convert);
    ans.lyrics          = resample_time(*score.lyrics, convert);
    ans.markers         = resample_time(*score.markers, convert);

    const size_t track_num = score.tracks->size();
    ans.tracks             = std::make_shared<vec<shared<Track<Tick>>>>();
    ans.tracks->reserve(track_num);
    for (const auto& old_track : *score.tracks) {
        auto new_track =
            std::make_shared<Track<Tick>>(old_track->name, old_track->program, old_track->is_drum);
        new_track->notes       = resample_dur(*old_track->notes, convert, min_dur);
        new_track->pedals      = resample_dur(*old_track->pedals, convert, min_dur);
        new_track->pitch_bends = resample_time(*old_track->pitch_bends, convert);
        new_track->controls    = resample_time(*old_track->controls, convert);
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

// /*
//  *  Conversion between shared and native
//  */
//
// template<TType T>
// Track<T> to_shared(TrackNative<T>&& track) {
//     Track<T> new_track{std::move(track.name), track.program, track.is_drum};
//     new_track.notes       = details::to_shared_vec(std::move(track.notes));
//     new_track.controls    = details::to_shared_vec(std::move(track.controls));
//     new_track.pitch_bends = details::to_shared_vec(std::move(track.pitch_bends));
//     new_track.pedals      = details::to_shared_vec(std::move(track.pedals));
//     return new_track;
// }
//
// template<TType T>
// Score<T> to_shared(ScoreNative<T>&& score) {
//     Score<T> new_score{score.ticks_per_quarter};
//     new_score.time_signatures = details::to_shared_vec(std::move(score.time_signatures));
//     new_score.key_signatures  = details::to_shared_vec(std::move(score.key_signatures));
//     new_score.tempos          = details::to_shared_vec(std::move(score.tempos));
//     new_score.lyrics          = details::to_shared_vec(std::move(score.lyrics));
//     new_score.markers         = details::to_shared_vec(std::move(score.markers));
//     new_score.tracks.reserve(score.tracks.size());
//     for (TrackNative<T>&& track : std::move(score.tracks)) {
//         new_score.tracks.push_back(std::move(to_shared(std::move(track))));
//     }
//     return new_score;
// }
//
// template<TType T>
// TrackNative<T> to_native(const Track<T>& track) {
//     TrackNative<T> new_track{track.name, track.program, track.is_drum};
//     new_track.notes       = details::to_native_vec(track.notes);
//     new_track.controls    = details::to_native_vec(track.controls);
//     new_track.pitch_bends = details::to_native_vec(track.pitch_bends);
//     new_track.pedals      = details::to_native_vec(track.pedals);
//     return new_track;
// }
//
// template<TType T>
// ScoreNative<T> to_native(const Score<T>& score) {
//     ScoreNative<T> new_score{score.ticks_per_quarter};
//     new_score.time_signatures = details::to_native_vec(score.time_signatures);
//     new_score.key_signatures  = details::to_native_vec(score.key_signatures);
//     new_score.tempos          = details::to_native_vec(score.tempos);
//     new_score.lyrics          = details::to_native_vec(score.lyrics);
//     new_score.markers         = details::to_native_vec(score.markers);
//     new_score.tracks.reserve(score.tracks.size());
//     for (const shared<Track<T>>& track : *score.tracks) {
//         new_score.tracks.push_back(std::move(to_native(*track)));
//     }
//     return new_score;
// }

// Explicit instantiation to_shared and to_native
// #define INSTANTIATE_TO_SHARED_AND_NATIVE(T)                     \
//     extern template Track<T>       to_shared(TrackNative<T>&&); \
//     extern template Score<T>       to_shared(ScoreNative<T>&&); \
//     extern template TrackNative<T> to_native(const Track<T>&);  \
//     extern template ScoreNative<T> to_native(const Score<T>&);
//
// INSTANTIATE_TO_SHARED_AND_NATIVE(Tick)
// INSTANTIATE_TO_SHARED_AND_NATIVE(Quarter)
// INSTANTIATE_TO_SHARED_AND_NATIVE(Second)
//
// #undef INSTANTIATE_TO_SHARED_AND_NATIVE

}   // namespace symusic

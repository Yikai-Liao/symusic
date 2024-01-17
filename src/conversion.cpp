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

struct Tick2Quarter {
    float tpq;

    explicit Tick2Quarter(const Score<Tick> &score): tpq(static_cast<float>(score.ticks_per_quarter)) {}

    Quarter::unit operator()(const Tick::unit tick) const {
        return static_cast<Quarter::unit>(tick) / tpq;
    }
};

struct Quarter2Tick {
    f32 tpq;

    explicit Quarter2Tick(const Score<Quarter> &score): tpq(static_cast<float>(score.ticks_per_quarter)) {}

    Tick::unit operator()(const float quarter) const {
        return static_cast<Tick::unit>(std::round(quarter * tpq));
    }
};

struct Quarter2Quarter {
    explicit Quarter2Quarter(const Score<Quarter> &) {}
    Quarter::unit operator()(const Quarter::unit quarter) const { return quarter; }
};

struct Tick2Tick {
    explicit Tick2Tick(const Score<Tick> &) {}
    Tick::unit operator()(const Tick::unit tick) const { return tick; }
};

struct Second2Second {
    explicit Second2Second(const Score<Second> &) {}
    Second::unit operator()(const Second::unit s) const { return s; }
};

struct Tick2Second {
    f32 tpq;
    vec<Tempo<Tick>> tempos;

    explicit Tick2Second(const Score<Tick> & score): tpq(static_cast<float>(score.ticks_per_quarter)){
        if(score.tempos.empty()) {
            // 120 qpm
            tempos = {{0, 500000}, {std::numeric_limits<Tick::unit>::max(), 500000}};
        } else {
            // vec<Tempo<Tick>> origin(score.tempos);
            // ops::sort_by_time(origin);  // sort it
            tempos.reserve(score.tempos.size() + 2);
            std::copy(score.tempos.begin(), score.tempos.end(), std::back_inserter(tempos));
            ops::sort_by_time(tempos);
            if(tempos.empty() || tempos[0].time != 0) {
                tempos.insert(tempos.begin(), Tempo<Tick>(0, 500000));
            }
            // add a guard at the end
            tempos.emplace_back(std::numeric_limits<Tick::unit>::max(), tempos.back().mspq);
        }
    }

    template<template<class> class T>
    [[nodiscard]] vec<T<Second>> time_vec(const vec<T<Tick>> & data) const {
        vec<T<Tick>> origin(data);
        ops::sort_by_time(origin);  // sort it
        vec<T<Second>> ans;
        ans.reserve(origin.size());
        auto t_iter = tempos.begin() + 1;
        i32 pivot_tick = 0;
        f32 pivot_time = 0;
        double cur_factor = static_cast<double>(tempos[0].mspq) / 1000000. / static_cast<double>(tpq);
        for(const auto & event : origin) {
            while(event.time > t_iter->time) {
                pivot_time += static_cast<float>(cur_factor * (t_iter->time - pivot_tick));
                pivot_tick = t_iter->time;
                cur_factor = static_cast<double>(t_iter->mspq) / 1000000. / static_cast<double>(tpq);
                ++t_iter;
            }
            ans.emplace_back(
                pivot_time + static_cast<float>(cur_factor * (event.time - pivot_tick)),
                event
            );
        }
        return ans;
    }

    template<template<class> class T>
     [[nodiscard]] vec<T<Second>> duration_vec(const vec<T<Tick>> & data) const {
        vec<T<Tick>> origin(data);
        ops::sort_by_time(origin);  // sort it
        vec<T<Second>> ans;
        ans.reserve(origin.size());
        auto t_iter = tempos.begin() + 1;
        i32 pivot_tick = 0;
        f32 pivot_time = 0;
        double cur_factor = static_cast<double>(tempos[0].mspq) / 1000000. / static_cast<double>(tpq);
        for(const auto & event : origin) {
            while(event.time > t_iter->time) {
                pivot_time += static_cast<float>(cur_factor * (t_iter->time - pivot_tick));
                pivot_tick = t_iter->time;
                cur_factor = static_cast<double>(t_iter->mspq) / 1000000. / static_cast<double>(tpq);
                ++t_iter;
            }
            ans.emplace_back(
                pivot_time + static_cast<float>(cur_factor * (event.time - pivot_tick)),
                0, event
            );
        }

        vec<std::pair<Tick::unit, u32>> end_times; // .end(), i
        end_times.reserve(origin.size());
        for(size_t i = 0; i < origin.size(); ++i) {
            end_times.emplace_back(origin[i].end(), i);
        }
        pdqsort_detail::insertion_sort(end_times.begin(), end_times.end(), [](const auto & a, const auto & b) {
            return a.first < b.first;
        });
        pivot_tick = 0;
        pivot_time = 0;
        t_iter = tempos.begin() + 1;
        cur_factor = static_cast<double>(tempos[0].mspq) / 1000000. / static_cast<double>(tpq);
        for(const auto & event : end_times) {
            while(event.first > t_iter->time) {
                pivot_time += static_cast<float>(cur_factor * (t_iter->time - pivot_tick));
                pivot_tick = t_iter->time;
                cur_factor = static_cast<double>(t_iter->mspq) / 1000000. / static_cast<double>(tpq);
                ++t_iter;
            }
            ans[event.second].duration =
                pivot_time
                + static_cast<float>(cur_factor * (event.first - pivot_tick))
                - ans[event.second].time;
        }
        return ans;
    }
};

struct Second2Tick {
    f32 tpq;
    vec<Tempo<Second>> tempos;

    explicit Second2Tick(const Score<Second> & score): tpq(static_cast<float>(score.ticks_per_quarter)){
        if(score.tempos.empty()) {
            // 120 qpm
            tempos = {{0, 500000}, {std::numeric_limits<Second::unit>::max(), 500000}};
        } else {
            // vec<Tempo<Second>> origin(score.tempos);
            // ops::sort_by_time(origin);  // sort it
            tempos.reserve(score.tempos.size() + 2);
            std::copy(score.tempos.begin(), score.tempos.end(), std::back_inserter(tempos));
            ops::sort_by_time(tempos);
            if(tempos.empty() || tempos[0].time != 0) {
                tempos.insert(tempos.begin(), Tempo<Second>(0, 500000));
            }
            // add a guard at the end
            tempos.emplace_back(std::numeric_limits<Second::unit>::max(), tempos.back().mspq);
        }
    }

    template<template<class> class T>
    [[nodiscard]] vec<T<Tick>> time_vec(const vec<T<Second>> & data) const {
        vec<T<Second>> origin(data);
        ops::sort_by_time(origin);  // sort it
        vec<T<Tick>> ans;
        ans.reserve(origin.size());
        auto t_iter = tempos.begin() + 1;
        i32 pivot_tick = 0;
        f32 pivot_time = 0;
        double cur_factor = 1000000. * static_cast<double>(tpq) / static_cast<double>(tempos[0].mspq);
        for(const auto & event : origin) {
            while(event.time > t_iter->time) {
                pivot_tick += static_cast<i32>(std::round(cur_factor * (t_iter->time - pivot_time)));
                pivot_time = t_iter->time;
                cur_factor = 1000000. * static_cast<double>(tpq) / static_cast<double>(t_iter->mspq);
                ++t_iter;
            }
            ans.emplace_back(
                pivot_tick + static_cast<i32>(std::round(cur_factor * (event.time - pivot_time))),
                event
            );
        }
        return ans;
    }

    template<template<class> class T>
    [[nodiscard]] vec<T<Tick>> duration_vec(const vec<T<Second>> & data) const {
        vec<T<Second>> origin(data);
        ops::sort_by_time(origin);  // sort it
        vec<T<Tick>> ans;
        ans.reserve(origin.size());
        auto t_iter = tempos.begin() + 1;
        i32 pivot_tick = 0;
        f32 pivot_time = 0;
        double cur_factor = 1000000. * static_cast<double>(tpq) / static_cast<double>(tempos[0].mspq);
        for(const auto & event : origin) {
            while(event.time > t_iter->time) {
                pivot_tick += static_cast<i32>(std::round(cur_factor * (t_iter->time - pivot_time)));
                pivot_time = t_iter->time;
                cur_factor = 1000000. * static_cast<double>(tpq) / static_cast<double>(t_iter->mspq);
                ++t_iter;
            }
            ans.emplace_back(
                pivot_tick + static_cast<i32>(std::round(cur_factor * (event.time - pivot_time))),
                0, event
            );
        }

        vec<std::pair<Second::unit, u32>> end_times; // .end(), i
        end_times.reserve(origin.size());
        for(size_t i = 0; i < origin.size(); ++i) {
            end_times.emplace_back(origin[i].end(), i);
        }
        pdqsort_detail::insertion_sort(end_times.begin(), end_times.end(), [](const auto & a, const auto & b) {
            return a.first < b.first;
        });
        pivot_tick = 0;
        pivot_time = 0;
        t_iter = tempos.begin() + 1;
        cur_factor = 1000000. * static_cast<double>(tpq) / static_cast<double>(tempos[0].mspq);
        for(const auto & event : end_times) {
            while(event.first > t_iter->time) {
                pivot_tick += static_cast<i32>(std::round(cur_factor * (t_iter->time - pivot_time)));
                pivot_time = t_iter->time;
                cur_factor = 1000000. * static_cast<double>(tpq) / static_cast<double>(t_iter->mspq);
                ++t_iter;
            }
            ans[event.second].duration =
                pivot_tick
                + static_cast<i32>(std::round(cur_factor * (event.first - pivot_time)))
                - ans[event.second].time;
        }
        return ans;
    }
};

} // namespace details

#define HELPER_NAME(From, To) details::From##2##To

#define IMPLEMENT_CONVERT(To, From)                                                             \
    template<> Score<To> convert<To, From> (COMVERT_ARGUMENTS(To, From)) {                      \
        HELPER_NAME(From, To) helper(score);                                                    \
        Score<To> new_s(score.ticks_per_quarter);                                               \
        CONVERT_VEC_TIME(key_signatures, new_s, score, helper);                                 \
        CONVERT_VEC_TIME(time_signatures, new_s, score, helper);                                \
        CONVERT_VEC_TIME(tempos, new_s, score, helper);                                         \
        CONVERT_VEC_TIME(lyrics, new_s, score, helper);                                         \
        CONVERT_VEC_TIME(markers, new_s, score, helper);                                        \
        new_s.tracks.reserve(score.tracks.size());                                              \
        for(const Track<From> &track: score.tracks) {                                           \
            Track<To> new_t(track.name, track.program, track.is_drum);                          \
            CONVERT_VEC_TIME_DUR(notes, new_t, track, helper);                                  \
            CONVERT_VEC_TIME_DUR(pedals, new_t, track, helper);                                 \
            CONVERT_VEC_TIME(pitch_bends, new_t, track, helper);                                \
            CONVERT_VEC_TIME(controls, new_t, track, helper);                                   \
            new_s.tracks.emplace_back(std::move(new_t));                                        \
        }                                                                                       \
        return new_s;                                                                           \
    }

#define COMVERT_ARGUMENTS(To, From) const Score<From> & score

#define CONVERT_VEC_TIME(Name, In, Out, Convert)        \
    In.Name.reserve(Out.Name.size());                   \
    for(const auto & d : Out.Name) {                    \
        In.Name.emplace_back(                           \
            Convert(d.time), d);                        \
    }

#define CONVERT_VEC_TIME_DUR(Name, In, Out, Convert)    \
    In.Name.reserve(Out.Name.size());                   \
    for(const auto & d : Out.Name) {                    \
        In.Name.emplace_back(                           \
            Convert(d.time), Convert(d.duration), d     \
        );                                              \
    }

//                To    From
IMPLEMENT_CONVERT(Tick, Quarter)
IMPLEMENT_CONVERT(Quarter, Tick)

#undef CONVERT_VEC_TIME_DUR
#undef COMVERT_ARGUMENTS

// Directly copy for the same type
template<> Score<Tick> convert<Tick, Tick> (const Score<Tick> & score) { return score; }
template<> Score<Quarter> convert<Quarter, Quarter> (const Score<Quarter> & score) { return score; }
template<> Score<Second> convert<Second, Second> (const Score<Second> & score) { return score; }

// #undef CONVERT_VEC_TIME_DUR
#define MAX(A, B) ((A) > (B) ? (A) : (B))

#define COMVERT_ARGUMENTS(To, From)                     \
    const Score<From> & score, const typename To::unit min_dur

#define CONVERT_VEC_TIME_DUR(Name, In, Out, Convert)    \
    In.Name.reserve(Out.Name.size());                   \
    for(const auto & d : Out.Name) {                    \
        In.Name.emplace_back(                           \
            Convert(d.time),                            \
            MAX(Convert(d.duration), min_dur), d        \
        );                                              \
    }

//               To    From
IMPLEMENT_CONVERT(Tick, Tick)
IMPLEMENT_CONVERT(Tick, Quarter)
IMPLEMENT_CONVERT(Quarter, Tick)
IMPLEMENT_CONVERT(Quarter, Quarter)
IMPLEMENT_CONVERT(Second, Second)

#undef CONVERT_VEC_TIME
#undef CONVERT_VEC_TIME_DUR
#undef COMVERT_ARGUMENTS

#define COMVERT_ARGUMENTS(To, From) const Score<From> & score
#define CONVERT_VEC_TIME(Name, In, Out, Convert)    In.Name = Convert.time_vec(Out.Name);
#define CONVERT_VEC_TIME_DUR(Name, In, Out, Convert)    In.Name = Convert.duration_vec(Out.Name);

//               To    From
IMPLEMENT_CONVERT(Second, Tick) // Score<Second> convert<Second, Tick>(const Score<Tick>& score)
IMPLEMENT_CONVERT(Tick, Second) // Score<Second> convert<Second, Quarter>(const Score<Quarter>& score)

#undef CONVERT_VEC_TIME_DUR
#undef COMVERT_ARGUMENTS

#define COMVERT_ARGUMENTS(To, From)                     \
    const Score<From> & score, const typename To::unit min_dur

#define CONVERT_VEC_TIME_DUR(Name, In, Out, Convert)    \
    In.Name = Convert.duration_vec(Out.Name);           \
    for(auto & d : In.Name) {                           \
        d.duration = MAX(d.duration, min_dur);          \
    }                                                   \

//               To    From
IMPLEMENT_CONVERT(Second, Tick) // Score<Second> convert<Second, Tick>(const Score<Tick>& score, const typename Second::unit min_dur)
IMPLEMENT_CONVERT(Tick, Second) // Score<Second> convert<Second, Quarter>(const Score<Quarter>& score, const typename Second::unit min_dur)

template<>
Score<Second> convert<Second, Quarter>(const Score<Quarter> & score, const Second::unit min_dur) {
    const Score<Tick> new_score = convert<Tick>(score);
    return convert<Second>(new_score, min_dur);
}

template<>
Score<Second> convert<Second, Quarter>(const Score<Quarter> & score) {
    const Score<Tick> new_score = convert<Tick>(score);
    return convert<Second>(new_score);
}

template<>
Score<Quarter> convert<Quarter, Second>(const Score<Second> & score, const Quarter::unit min_dur) {
    const Score<Tick> new_score = convert<Tick>(score);
    return convert<Quarter>(new_score, min_dur);
}

template<>
Score<Quarter> convert<Quarter, Second>(const Score<Second> & score) {
    const Score<Tick> new_score = convert<Tick>(score);
    return convert<Quarter>(new_score);
}

#undef CONVERT_VEC_TIME_DUR
#undef COMVERT_ARGUMENTS
#undef HELPER_NAME
#undef IMPLEMENT_CONVERT

// Implementation of to_note_arr
// template<TType T>
// NoteArr<T> to_note_arr(const Track<T> & track) {
//     NoteArr<T> note_arr{track.name, track.program, track.is_drum};
//     note_arr.reserve(track.note_num());
//     for (const auto & note: track.notes) {
//         note_arr.push_back(note);
//     }   return note_arr;
// }
//
// template<TType T>
// Track<T> to_track(const NoteArr<T> & note_arr) {
//     Track<T> track{note_arr.name, note_arr.program, note_arr.is_drum};
//     const size_t note_num = note_arr.note_num();
//     track.notes.reserve(note_num);
//     for(size_t i = 0; i < note_num; ++i) {
//         track.notes.emplace_back(
//             note_arr.time[i],
//             note_arr.duration[i],
//             note_arr.pitch[i],
//             note_arr.velocity[i]
//         );
//     }
//     return track;
// }
//
// // Instantiation of to_note_arr and to_track
// #define INSTANTIATE(__COUNT, T) \
//     template NoteArr<T> to_note_arr(const Track<T> & track); \
//     template Track<T> to_track(const NoteArr<T> & note_arr);
//
// REPEAT_ON(INSTANTIATE, Tick, Quarter, Second)
//
// #undef INSTANTIATE

// Resample Score<Tick> to Score<Tick> with given new tpq and min_dur
Score<Tick> resample_inner(const Score<Tick> & score, const i32 tpq, const i32 min_dur) {
    Score<Tick> ans(tpq);

#define CONVERT_TIME(VALUE) \
static_cast<i32>(std::round(static_cast<double>(tpq * VALUE) / static_cast<double>(score.ticks_per_quarter)))

#define RESAMPLE_GENERAL(__COUNT, VEC)                      \
    ans.VEC.reserve(score.VEC.size());                      \
    for(const auto &item: score.VEC)                        \
        ans.VEC.emplace_back(CONVERT_TIME(item.time), item);

#define RESAMPLE_DUR(__COUNT, VEC)                          \
    ans.VEC.reserve(score.VEC.size());                      \
    for(const auto &item: score.VEC)                        \
        ans.VEC.emplace_back(                               \
            CONVERT_TIME(item.time),                        \
            MAX(                                            \
                CONVERT_TIME(item.duration), min_dur        \
            ), item                                         \
        );

    REPEAT_ON(RESAMPLE_GENERAL, time_signatures, key_signatures, tempos, lyrics, markers)
    const size_t track_num = score.tracks.size();
    ans.tracks = vec<Track<Tick>>(track_num);
    for(size_t i = 0; i < track_num; ++i) {
        const auto &track = score.tracks[i];
        auto &new_track = ans.tracks[i];
        new_track.name = track.name;
        new_track.program = track.program;
        new_track.is_drum = track.is_drum;
        REPEAT_ON(RESAMPLE_DUR, tracks[i].notes, tracks[i].pedals)
        REPEAT_ON(RESAMPLE_GENERAL, tracks[i].controls, tracks[i].pitch_bends)
    }

#undef CONVERT_TIME
#undef RESAMPLE_GENERAL
#undef RESAMPLE_DUR

    return ans;
}

template<>
Score<Tick> resample(const Score<Quarter> & score, const i32 tpq, const i32 min_dur) {
    return resample_inner(convert<Tick>(score), tpq, min_dur);
}

template<>
Score<Tick> resample(const Score<Tick> & score, const i32 tpq, const i32 min_dur) {
    return resample_inner(score, tpq, min_dur);
}
template<>
Score<Tick> resample(const Score<Second> & score, const i32 tpq, const i32 min_dur) {
    return resample_inner(convert<Tick>(score), tpq, min_dur);
}

} // namespace symusic

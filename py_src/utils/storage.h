#pragma once

#include <utility>

#include <pyvec.hpp>

#include "symusic/score.h"

namespace symusic::py {

using pycontainer::pyvec;

// Python lists own reference-stable event chunks. Track handles also keep
// tracks alive independently of their position in a score.
struct SharedStorage {
    template<typename E>
    using List = shared<pyvec<E>>;

    template<typename Track>
    using TrackHandle = shared<Track>;

    template<typename Track>
    using Tracks = shared<vec<shared<Track>>>;

    static decltype(auto) get(auto& field) {
        if constexpr (std::is_const_v<std::remove_reference_t<decltype(field)>>) {
            return std::as_const(*field);
        } else {
            return (*field);
        }
    }

    template<typename E>
    static List<E> adopt(vec<E>&& values) {
        return std::make_shared<pyvec<E>>(std::move(values));
    }

    template<typename Track>
    static shared<Track> adopt_track(Track&& track) {
        return std::make_shared<Track>(std::move(track));
    }

    template<typename Track>
    static shared<vec<Track>> adopt_tracks(vec<Track>&& tracks) {
        return std::make_shared<vec<Track>>(std::move(tracks));
    }

    static void erase_if(auto& events, auto predicate) {
        events.filter([&](const auto& event) { return !predicate(event); });
    }

    static void sort(auto& events, auto projection, bool reverse) {
        events.sort(projection, reverse);
    }

    static void stable_sort(auto& events, auto projection, bool reverse) {
        sort(events, projection, reverse);
    }

    static void keep(auto& events, size_t destination, size_t source) {
        *(events.pbegin() + destination) = *(events.pbegin() + source);
    }
};

template<TType T>
using Score = symusic::Score<T, SharedStorage>;

template<TType T>
using Track = symusic::Track<T, SharedStorage>;

}  // namespace symusic::py

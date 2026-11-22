#pragma once

#include <algorithm>
#include <functional>
#include <ranges>
#include <utility>

#include "symusic/mtype.h"

namespace symusic {

// The default policy owns events and tracks by value. Policies may change
// ownership while keeping algorithms over the same ordered event ranges.
struct ValueStorage {
    template<typename E>
    using List = vec<E>;

    template<typename Track>
    using TrackHandle = Track;

    template<typename Track>
    using Tracks = vec<Track>;

    static decltype(auto) get(auto& field) { return (field); }

    template<typename E>
    static vec<E> adopt(vec<E>&& values) { return std::move(values); }

    template<typename Track>
    static Track adopt_track(Track&& track) { return std::move(track); }

    template<typename Track>
    static vec<Track> adopt_tracks(vec<Track>&& tracks) { return std::move(tracks); }

    static void erase_if(auto& events, auto predicate) { std::erase_if(events, predicate); }

    static void sort(auto& events, auto projection, bool reverse) {
        if (reverse) {
            std::ranges::sort(events, std::ranges::greater{}, projection);
        } else {
            std::ranges::sort(events, std::ranges::less{}, projection);
        }
    }

    static void stable_sort(auto& events, auto projection, bool reverse) {
        std::stable_sort(events.begin(), events.end(), [&](const auto& left, const auto& right) {
            return reverse ? projection(right) < projection(left) : projection(left) < projection(right);
        });
    }

    static void keep(auto& events, size_t destination, size_t source) {
        if (destination != source) events[destination] = std::move(events[source]);
    }
};

}  // namespace symusic

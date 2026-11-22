#pragma once

#include <algorithm>
#include <map>


#include <pdqsort.h>

#include "symusic/score.h"
#include "symusic/utils.h"

namespace symusic::detail {

template<typename T>
void sort_by_time(vec<T>& data) {
    pdqsort_branchless(data.begin(), data.end(), [](const auto& a, const auto& b) {
        return (a.time) < (b.time);
    });
}

// Builds a score from ordered event streams. The importer supplies each track key
// and decides when to promote a pending track into the result.
template<TType T, typename Key>
class ScoreBuilder {
    using unit = typename T::unit;

    ScoreNative<T>                               score;
    std::map<Key, TrackNative<T>>                tracks;
    Key                                          last_key{};
    TrackNative<T>*                              last_track = nullptr;
    vec<TrackNative<T>*>                         note_tracks;
    vec<std::pair<TrackNative<T>*, TextMeta<T>>> lyrics;

public:
    struct NoteHandle {
        vec<Note<T>>* notes = nullptr;
        size_t        index = 0;
    };

    explicit ScoreBuilder(i32 tpq) : score(tpq) {}

    TrackNative<T>& track(
        const Key& key, TrackNative<T>& pending, bool promote, size_t note_capacity
    ) {
        if (last_track && last_key == key) return *last_track;
        if (auto it = tracks.find(key); it != tracks.end()) {
            last_key   = key;
            last_track = &it->second;
            return *last_track;
        }
        if (!promote) return pending;

        auto& result = tracks.try_emplace(key, std::move(pending)).first->second;
        pending      = {};
        result.notes.reserve(note_capacity);
        last_key   = key;
        last_track = &result;
        return result;
    }

    NoteHandle begin_note(TrackNative<T>& track, unit time, i8 pitch, i8 velocity) {
        if (track.notes.empty()) note_tracks.push_back(&track);
        track.notes.emplace_back(time, -1, pitch, velocity);
        return {&track.notes, track.notes.size() - 1};
    }

    void end_note(NoteHandle handle, unit time) {
        auto& note    = (*handle.notes)[handle.index];
        note.duration = details::sub_time(time, note.time);
    }

    void add_lyric(TrackNative<T>& fallback, TextMeta<T> lyric) {
        lyrics.emplace_back(&fallback, std::move(lyric));
    }

    void add(TimeSignature<T> event) { score.time_signatures.push_back(event); }
    void add(KeySignature<T> event) { score.key_signatures.push_back(event); }
    void add(Tempo<T> event) { score.tempos.push_back(event); }
    void add(TextMeta<T> event) { score.markers.push_back(std::move(event)); }

    void finish_group(const std::string& name) {
        TrackNative<T>* lyric_owner = nullptr;
        for (auto* track : note_tracks) {
            std::erase_if(track->notes, [](const Note<T>& note) { return note.duration < 0; });
            if (!lyric_owner && !track->notes.empty()) lyric_owner = track;
        }
        for (auto& [fallback, lyric] : lyrics) {
            (lyric_owner ? lyric_owner : fallback)->lyrics.push_back(std::move(lyric));
        }
        for (auto& [key, track] : tracks) {
            if (track.empty()) continue;
            track.name = name;
            score.tracks.push_back(std::move(track));
        }
        lyrics.clear();
        note_tracks.clear();
        tracks.clear();
        last_track = nullptr;
    }

    [[nodiscard]] ScoreNative<T> finish() && {
        sort_by_time(score.time_signatures);
        sort_by_time(score.key_signatures);
        sort_by_time(score.tempos);
        sort_by_time(score.markers);
        return std::move(score);
    }
};

}   // namespace symusic::detail

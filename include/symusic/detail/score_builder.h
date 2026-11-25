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

    struct PendingLyric {
        Key         key;
        u8          program;
        bool        is_drum;
        TextMeta<T> lyric;
    };

    Score<T>                score;
    std::map<Key, Track<T>> tracks;
    Key                     last_key{};
    Track<T>*               last_track = nullptr;
    vec<Track<T>*>          note_tracks;
    vec<PendingLyric>       lyrics;

public:
    struct NoteHandle {
        vec<Note<T>>* notes = nullptr;
        size_t        index = 0;
    };

    explicit ScoreBuilder(i32 ticks_per_quarter) : score(ticks_per_quarter) {}

    Track<T>* find_track(const Key& key) {
        if (last_track && last_key == key) return last_track;
        if (auto it = tracks.find(key); it != tracks.end()) {
            last_key   = key;
            last_track = &it->second;
            return last_track;
        }
        return nullptr;
    }

    Track<T>& create_track(const Key& key, Track<T>&& initial) {
        auto& result = tracks.try_emplace(key, std::move(initial)).first->second;
        last_key     = key;
        last_track   = &result;
        return result;
    }

    NoteHandle begin_note(Track<T>& track, unit time, i8 pitch, i8 velocity) {
        if (track.notes.empty()) note_tracks.push_back(&track);
        auto& note = track.notes.emplace_back(time, 0, pitch, velocity);
        // An unpaired note stays private to the builder and is discarded at finish_group().
        note.duration = -1;
        return {&track.notes, track.notes.size() - 1};
    }

    void end_note(NoteHandle handle, unit time) {
        auto& note    = (*handle.notes)[handle.index];
        note.duration = detail::sub_time(time, note.time);
    }

    void add_lyric(const Key& key, u8 program, bool is_drum, TextMeta<T> lyric) {
        lyrics.push_back({key, program, is_drum, std::move(lyric)});
    }

    void add(TimeSignature<T> event) { score.time_signatures.push_back(event); }
    void add(KeySignature<T> event) { score.key_signatures.push_back(event); }
    void add(Tempo<T> event) { score.tempos.push_back(event); }
    void add(TextMeta<T> event) { score.markers.push_back(std::move(event)); }

    void finish_group(const std::string& name) {
        Track<T>* lyric_owner = nullptr;
        for (auto* track : note_tracks) {
            std::erase_if(track->notes, [](const Note<T>& note) { return note.duration < 0; });
            if (!lyric_owner && !track->notes.empty()) lyric_owner = track;
        }
        for (auto& pending : lyrics) {
            auto& owner = lyric_owner ?
                              *lyric_owner :
                              tracks.try_emplace(pending.key, "", pending.program, pending.is_drum)
                                  .first->second;
            owner.lyrics.push_back(std::move(pending.lyric));
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

    [[nodiscard]] Score<T> finish() && {
        sort_by_time(score.time_signatures);
        sort_by_time(score.key_signatures);
        sort_by_time(score.tempos);
        sort_by_time(score.markers);
        return std::move(score);
    }
};

}   // namespace symusic::detail

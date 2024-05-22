//
// Created by nhy on 2024/2/12.
//
#include "symusic/synth.h"
#include "symusic/ops.h"
#include "symusic/conversion.h"

// #include "unordered_dense.h"
#include <algorithm>
#include <map>

namespace symusic {

namespace details {

struct BankSelect {
    Second::unit time;
    u8           bank;
                 BankSelect(const Second::unit time, const u8 bank) : time(time), bank(bank) {}
};

struct TrackIdx {
    u8   program;
    u8   bank;
    auto operator<=>(const TrackIdx&) const = default;
};

typedef std::map<TrackIdx, psynth::Track> TrackMapper;

vec<BankSelect> getBanks(const Track<Second>& track) {
    vec<BankSelect> banks;
    // Drum Track always get bank=128
    if (track.is_drum) {
        banks.emplace_back(0, 128);
        banks.emplace_back(std::numeric_limits<Second::unit>::max(), 128);
        return banks;
    }
    // filter bank select events
    for (const ControlChange<Second>& control : *track.controls) {
        if (control->number == 0) { banks.emplace_back(control->time, control->value); }
    }
    // if no bank select event, add default bank 0
    if (banks.empty()) {
        banks.emplace_back(0, 0);
        banks.emplace_back(std::numeric_limits<Second::unit>::max(), 0);
        return banks;
    }
    // add default bank 0 at the beginning and last bank at the end
    if (banks.front().time != 0) { banks.emplace(banks.begin(), 0, 0); }
    banks.emplace_back(std::numeric_limits<Second::unit>::max(), banks.back().bank);

    return banks;
}

vec<psynth::Note> toPNotes(const pyvec<Note<Second>>& notes) {
    static_assert(std::is_same_v<Second::unit, decltype(psynth::Note::start)>);
    vec<psynth::Note> pnotes;
    pnotes.resize(notes.size());
    for (size_t i = 0; i < notes.size(); ++i) {
        const Note<Second>& note  = notes[i];
        psynth::Note&       pnote = pnotes[i];
        pnote.start               = note.time;
        pnote.duration            = note.duration;
        pnote.pitch               = note.pitch;
        pnote.velocity            = note.velocity;
    }
    std::sort(pnotes.begin(), pnotes.end(), [](const psynth::Note& a, const psynth::Note& b) {
        return a.start < b.start;
    });
    return pnotes;
}

void divideNotes(
    const vec<psynth::Note>& notes,
    const vec<BankSelect>&   banks,
    TrackMapper&             mapper,
    const u8                 program
) {
    // notes and banks are sorted
    const auto getIdx   = [program](const BankSelect& x) { return TrackIdx{program, x.bank}; };
    auto       getTrack = [&mapper](const TrackIdx& idx) -> psynth::Track& {
        auto [it, inserted] = mapper.try_emplace(idx, psynth::Track{});
        return it->second;
    };

    auto  bank_it   = banks.begin() + 1;
    auto  cur_idx   = getIdx(*banks.begin());
    auto& cur_track = getTrack(cur_idx);

    for (const auto& note : notes) {
        for (; bank_it->time < note.start; ++bank_it) {
            cur_idx   = getIdx(*bank_it);
            cur_track = getTrack(cur_idx);
        }
        cur_track.notes.push_back(note);
    }
}

psynth::Sequence toSequence(const Score<Second>& score) {
    psynth::Sequence sequence;
    TrackMapper      track_mapper{};
    for (const shared<Track<Second>>& score_track : *(score.tracks)) {
        auto banks  = getBanks(*score_track);
        auto pnotes = toPNotes(*score_track->notes);
        divideNotes(pnotes, banks, track_mapper, score_track->program);
    }
    for (auto& [idx, track] : track_mapper) {
        if (track.notes.empty()) continue;
        track.preset = idx.program;
        track.bank   = idx.bank;
        sequence.tracks.push_back(std::move(track));
    }
    if (sequence.tracks.empty()) { throw std::runtime_error("No valid tracks in the score"); }
    return sequence;
}
}   // namespace details

template<TType T>
psynth::AudioData Synthesizer::render(const Score<T>& score, const bool stereo) {
    if constexpr (std::is_same_v<T, Second>) {
        return synthesizer.render(details::toSequence(score), stereo);
    } else {
        return synthesizer.render(details::toSequence(convert<Second>(score)), stereo);
    }
}

// explicit instantiation
template psynth::AudioData Synthesizer::render(const Score<Tick>&, bool);
template psynth::AudioData Synthesizer::render(const Score<Quarter>&, bool);
template psynth::AudioData Synthesizer::render(const Score<Second>&, bool);

}   // namespace symusic

#include "symusic/io/abc.h"

#include <algorithm>
#include <bit>
#include <cmath>
#include <limits>
#include <map>
#include <numeric>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>

#include <fmt/format.h>
#include <utf8.h>

#include "settings.h"
#include "symusic/detail/time_conversion.h"
#include "symusic/utils.h"
#include "write_lanes.h"
#include "write_voice.h"

namespace symusic::abc {
namespace {

using namespace detail;

constexpr i32 kMidiTempoMax = (1U << 24U) - 1U;
constexpr i32 kMidiDeltaMax = (1U << 28U) - 1U;

void check_tempo(i32 mspq) {
    if (mspq < 1 || mspq > kMidiTempoMax)
        invalid_export("tempo", "microseconds per quarter must fit the 24-bit MIDI range");
}

std::string tempo_text(i32 mspq) {
    check_tempo(mspq);
    // ABC 2.1 section 3.1.8 permits fractional beats. Try a quarter first,
    // then halve the beat until the integer interval rate/(mspq+1) < bpm <= rate/mspq
    // contains a BPM. At denominator 2^25 the interval is wider than one
    // for every 24-bit tempo, and the first match fits abc2midi's BPM * 4.
    i64 rate = 60'000'000;
    for (i32 denominator = 4;; denominator *= 2, rate *= 2) {
        const auto bpm = rate / mspq;
        if (rate / bpm == mspq) return fmt::format("1/{}={}", denominator, bpm);
    }
}

template<TType T>
bool is_audible(const Note<T>& note) {
    return note.duration > 0 && note.velocity > 0;
}

// Validate times in the input unit, then round each endpoint to the ABC grid.
// Second-based scores use the input tempo map. For an interval, convert the
// original end rather than rounding a standalone duration; the caller decides
// what to do if an originally positive interval collapses to one grid point.
template<TType T>
class ExportClock {
public:
    ExportClock(const Score<T>& score, const ExportLocation& at) :
        source_tpq(score.ticks_per_quarter) {
        if (source_tpq <= 0) invalid_export("score", "ticks per quarter must be positive");
        if constexpr (std::is_same_v<T, Second>) {
            for (const auto& tempo : *score.tempos) {
                if (!std::isfinite(tempo.time) || tempo.time < 0)
                    invalid_export("tempo", "time must be finite and nonnegative");
                check_tempo(tempo.mspq);
            }
            seconds.emplace(score);
        }
        // Probe only audible onsets. Each event is validated below with its own location.
        for (const auto& track : *score.tracks) {
            for (const auto& note : *track->notes) {
                if (is_audible(note) && std::isfinite(note.time) && note.time >= 0
                    && std::round(on_grid(note.time)) == 0) {
                    at.warn(
                        "note-start-offset",
                        "shifted all note onsets one tick because a note starts at tick zero"
                    );
                    onset_offset = 0;
                    return;
                }
            }
        }
    }

    [[nodiscard]] i32 note_tick(i32 tick) const { return details::sub_time(tick, onset_offset); }

    template<class Event>
    [[nodiscard]] std::pair<i32, i32> interval(
        const Event& event, std::string_view kind, const ExportLocation& at
    ) const {
        if (!std::isfinite(event.duration))
            at.invalid(fmt::format("{} duration must be finite", kind));
        if (event.duration < 0) at.invalid(fmt::format("{} duration cannot be negative", kind));
        return {scale(event.time, at), scale(static_cast<f64>(event.time) + event.duration, at)};
    }

    [[nodiscard]] i32 scale(f64 time, const ExportLocation& at) const {
        if (!std::isfinite(time) || time < 0)
            at.invalid("event time must be finite and nonnegative");
        if constexpr (std::is_same_v<T, Tick>) {
            if (time > std::numeric_limits<i32>::max())
                at.invalid("event time is outside the score's tick range");
        }
        const auto exact   = on_grid(time);
        const auto rounded = std::round(exact);
        if (!(rounded >= 0 && rounded <= std::numeric_limits<i32>::max() - 2))
            at.invalid("rescaled event leaves no room for the ABC note release");
        if (rounded > kMidiDeltaMax)
            at.warn("midi-delta-range", "very late events may exceed the MIDI delta-time range");
        if (std::abs(rounded - exact) > grid_noise(exact))
            at.warn(
                "resampled-time",
                "rounded {:.8g} ABC ticks to {:.0f} on the 480-TPQ grid",
                exact,
                rounded
            );
        // The bounded, integral result is exact in f64.
        return details::f64_to_i32(static_cast<f64>(rounded));
    }

private:
    i32                                          source_tpq;
    i32                                          onset_offset = PlaybackGrid::kAttackOffset;
    std::optional<details::SecondToQuarter<f64>> seconds;

    [[nodiscard]] f64 on_grid(f64 time) const {
        if constexpr (std::is_same_v<T, Tick>) {
            return time * kTpq / source_tpq;
        } else if constexpr (std::is_same_v<T, Quarter>) {
            return time * kTpq;
        } else {
            return static_cast<f64>(seconds.value().time_value(static_cast<f64>(time))) * kTpq;
        }
    }

    static f64 grid_noise(f64 ticks) {
        if constexpr (std::is_same_v<T, Tick>) return 0;
        // Ignore floating storage noise near ordinary grid positions, without
        // hiding larger rounding errors in long floating-point scores.
        return std::min(0.001, std::numeric_limits<typename T::unit>::epsilon() * ticks);
    }
};

template<TType T>
vec<WrittenNote> written_notes(
    const Track<T>& track, const ExportClock<T>& clock, const ExportLocation& at
) {
    vec<WrittenNote> result;
    result.reserve(track.notes->size());
    for (const auto& note : *track.notes) {
        if (note.pitch < 0 || note.velocity < 0)
            at.invalid("note pitch and velocity must be in MIDI range 0–127");
        auto [start, end] = clock.interval(note, "note", at);
        if (!is_audible(note)) {
            at.warn("silent-note", "omitted a zero-velocity or zero-duration note");
            continue;
        }
        if (end == start) {
            ++end;
            at.warn("short-note", "stretched a resampled note to one ABC tick");
        }
        const auto duration = end - start;
        // abc2midi joins tied atoms, then multiplies the reduced numerator by its tick division.
        if (duration / std::gcd(duration, kTpq) > std::numeric_limits<i32>::max() / kTpq)
            at.warn("midi-sustain-range", "abc2midi may shorten an unusually long held note");
        if (track.is_drum && duration == 1)
            at.warn(
                "midi-short-drum",
                "abc2midi can play a one-tick drum hit at tick {} with zero duration; use MIDI "
                "export to keep the release",
                clock.note_tick(start) + PlaybackGrid::kAttackOffset
            );
        result.push_back({clock.note_tick(start), clock.note_tick(end), note.pitch, note.velocity});
    }
    return result;
}

template<TType T>
std::string meter_text(const TimeSignature<T>& event) {
    if (!event.numerator || !event.denominator || !std::has_single_bit(event.denominator)) {
        invalid_export("meter", "expected a positive numerator and a power-of-two denominator");
    }
    return fmt::format("{}/{}", event.numerator, event.denominator);
}

template<TType T>
std::string key_text(const KeySignature<T>& event) {
    if (event.key < -7 || event.key > 7 || event.tonality < 0 || event.tonality > 1) {
        invalid_export("key", "expected −7 to 7 fifths and major or minor tonality");
    }
    return std::string(detail::key_name(event.key, event.tonality != 0));
}

// The last change in source time wins when the grid merges field changes. Extract
// tick zero for the header and retain the remaining inline fields for all voices.
template<TType T>
std::pair<std::string, Fields> tune_header(
    const Score<T>&       score,
    const ExportClock<T>& clock,
    std::string_view      title,
    ExportWarnings&       warnings
) {
    Fields     global;
    const auto field = [&](char key, const auto& events, std::string initial, auto format) {
        struct Change {
            typename T::unit time;
            std::string      text;
        };
        std::map<i32, Change> changes;
        const ExportLocation  at{fmt::format("{}: field", key), warnings};
        for (const auto& event : events) {
            const auto tick     = clock.scale(event.time, at);
            auto       text     = format(event);
            auto [it, inserted] = changes.try_emplace(tick, event.time, std::move(text));
            if (inserted) continue;
            auto& last = it->second;
            if (last.text != text)
                at.warn("duplicate-time", "multiple changes at one tick; kept the last value");
            if (event.time >= last.time) last = {event.time, std::move(text)};
        }
        if (auto zero = changes.extract(0); !zero.empty()) initial = std::move(zero.mapped().text);
        if (key != 'Q' && !changes.empty())
            at.warn(
                "midi-field-timing",
                "abc2midi may move the change at tick {} to earlier MIDI events in different "
                "voices; use MIDI export for exact timing",
                changes.begin()->first
            );
        for (const auto& [tick, change] : changes)
            global[tick].push_back(fmt::format("[{}:{}]", key, change.text));
        return initial;
    };
    const auto meter = field('M', *score.time_signatures, "none", meter_text<T>);
    const auto key   = field('K', *score.key_signatures, "C", key_text<T>);
    const auto tempo
        = field('Q', *score.tempos, "1/4=120", [](const auto& e) { return tempo_text(e.mspq); });
    return {
        fmt::format(
            "%abc-2.1\nX:1\nT:{}\nI:abc-creator symusic-" SYMUSIC_VERSION
            "\nI:linebreak <none>\nM:{}\nL:1/4\nQ:{}\nK:{}\n",
            encode_text(title, TextContext::Title),
            meter,
            tempo,
            key
        ),
        std::move(global)
    };
}

template<TType T>
void place_lyrics(
    PlannedTrack&             plan,
    const pyvec<TextMeta<T>>& lyrics,
    const ExportClock<T>&     clock,
    ExportBudget&             budget
) {
    const auto& at = plan.at;
    budget.spend(lyrics.size(), at);
    std::multimap<i32, const std::string*> pending;
    for (const auto& lyric : lyrics) {
        const auto tick = clock.note_tick(clock.scale(lyric.time, at));
        if (!utf8::is_valid(lyric.text.begin(), lyric.text.end()))
            at.invalid("lyric must be valid UTF-8");
        if (lyric.text.empty())
            at.warn("omitted-empty-lyric", "ABC blank syllables do not produce lyric events");
        else
            pending.emplace(tick, &lyric.text);
    }
    budget.spend(pending.size(), at, plan.lanes.size());

    // At a given tick, fill attack slots on every lane before splitting any sustain.
    for (auto& lane : plan.lanes) {
        std::erase_if(pending, [&](const auto& word) {
            const auto event = lane.find(word.first);
            return event != lane.end() && !event->second.attacks.empty()
                   && event->second.try_lyric(*word.second);
        });
    }
    for (auto& lane : plan.lanes) {
        auto   event    = lane.begin();
        size_t sounding = 0;
        std::erase_if(pending, [&](const auto& word) {
            while (event != lane.end() && event->first <= word.first) {
                sounding -= event->second.releases;
                sounding += event->second.attacks.size();
                ++event;
            }
            return sounding && lane[word.first].try_lyric(*word.second);
        });
    }
    if (!pending.empty())
        at.warn(
            "omitted-lyrics",
            "omitted {} lyric event(s) without a free ABC syllable slot (first at tick {})",
            pending.size(),
            pending.begin()->first + PlaybackGrid::kAttackOffset
        );
}

vec<std::pair<i32, i32>> pedal_intervals(vec<ControlChange<Tick>> controls) {
    std::ranges::stable_sort(controls, {}, &ControlChange<Tick>::time);
    vec<std::pair<i32, i32>> intervals;
    std::optional<i32>       start;
    for (const auto& event : controls) {
        if (event.value >= 64) {
            if (!start) start = event.time;
        } else if (start) {
            intervals.emplace_back(*start, event.time);
            start.reset();
        }
    }
    return intervals;
}

std::string bend_field(i32 value, const ExportLocation& at) {
    if (value < -detail::kMidiBendCenter || value >= detail::kMidiBendCenter)
        at.invalid("pitch bend must be in MIDI range −8192–8191");
    // The range check above guarantees a nonnegative 14-bit wire value.
    const auto wire = static_cast<u32>(value + detail::kMidiBendCenter);
    return fmt::format("[I:MIDI pitchbend {} {}]", wire & static_cast<u32>(kMidiMax), wire >> 7U);
}

template<TType T>
Fields channel_fields(
    const Track<T>&       track,
    const ExportClock<T>& clock,
    const ExportLocation& at,
    size_t                copies,
    ExportBudget&         budget
) {
    // Each effect emits a token per lane, even when all effects share a tick.
    budget.spend(track.controls->size(), at, copies);
    budget.spend(track.pitch_bends->size(), at, copies);
    Fields     fields;
    const auto control = [&](i32 tick, i32 number, i32 value) {
        fields[tick].push_back(fmt::format("[I:MIDI control {} {}]", number, value));
        at.warn(
            "midi-control-timing",
            "abc2midi may move CC {} at tick {} or write it after a same-tick attack; use MIDI "
            "export for exact timing and order",
            number,
            tick
        );
    };
    vec<ControlChange<Tick>> sustain;
    for (const auto& event : *track.controls) {
        if (event.number > kMidiMax || event.value > kMidiMax)
            at.invalid("control number and value must be in MIDI range 0–127");
        const auto tick = clock.scale(event.time, at);
        if (event.number == 64) sustain.emplace_back(tick, event.number, event.value);
        control(tick, event.number, event.value);
    }
    for (const auto& bend : *track.pitch_bends) {
        const auto tick = clock.scale(bend.time, at);
        fields[tick].push_back(bend_field(bend.value, at));
        if (tick > 0)
            at.warn(
                "midi-bend-timing",
                "abc2midi may emit timed pitch bends early and shift later notes; use MIDI export "
                "for interoperable bend timing"
            );
    }

    vec<std::pair<i32, i32>> pedals;
    for (const auto& pedal : *track.pedals) {
        const auto interval = clock.interval(pedal, "pedal", at);
        if (interval.first != interval.second)
            pedals.push_back(interval);
        else if (pedal.duration > 0)
            at.warn("short-pedal", "omitted a pedal interval shorter than the ABC grid");
    }
    if (pedals.empty()) return fields;
    std::ranges::sort(pedals);
    // Explicit CC64 can carry half-pedal values. Compare only on the playback grid.
    if (!sustain.empty()) {
        if (pedals != pedal_intervals(std::move(sustain)))
            at.warn(
                "pedal-conflict", "pedal intervals disagree with CC64; kept the explicit controls"
            );
        return fields;
    }
    for (auto pedal = pedals.begin(); pedal != pedals.end();) {
        auto [start, end] = *pedal++;
        while (pedal != pedals.end() && pedal->first < end) {
            at.warn("overlapping-pedals", "merged overlapping sustain intervals");
            end = std::max(end, pedal++->second);
        }
        budget.spend(2 * PlaybackWork::kToken, at, copies);
        control(start, 64, 127);
        control(end, 64, 0);
    }
    return fields;
}

template<TType T>
vec<PlannedTrack> plan_tracks(
    const Score<T>& score, const ExportClock<T>& clock, const ExportLocation& song
) {
    vec<PlannedTrack> tracks;
    VoiceAllocation   voices;
    ExportBudget      budget;
    for (size_t i = 0; i < score.tracks->size(); ++i) {
        const auto& track = *(*score.tracks)[i];
        if (!utf8::is_valid(track.name.begin(), track.name.end()))
            invalid_export(fmt::format("track {}", i + 1), "track name must be valid UTF-8");
        ExportLocation at{
            fmt::format("track {} ('{}')", i + 1, encode_text(track.name, TextContext::Title)),
            song.warnings
        };
        if (track.program > kMidiMax) at.invalid("program must be in MIDI range 0–127");
        if (track.is_drum && track.program != 0)
            at.warn("drum-program", "other ABC readers may ignore the drum program");
        const auto limit = voices.available(track.is_drum);
        auto       lanes = partition_lanes(written_notes(track, clock, at), limit, at);
        const auto count = lanes.size();
        if (count > 1) at.warn("split-voice", "used multiple ABC voices to preserve note events");
        auto         channel = channel_fields(track, clock, at, count, budget);
        PlannedTrack plan{
            std::move(at),
            track.name,
            track.program,
            track.is_drum,
            std::move(lanes),
            std::move(channel)
        };
        place_lyrics(plan, *track.lyrics, clock, budget);
        if (plan.lanes.front().empty() && plan.channel.empty()) continue;
        for (const auto& lane : plan.lanes)
            budget.spend(lane.size() * PlaybackWork::kToken, plan.at);
        // A control-only track still claims one voice, even without an onset to place.
        if (count > limit.count)
            plan.at.invalid(fmt::format("export requires more than {}", limit.name));
        voices.total += count;
        if (!track.is_drum) voices.melodic += count;
        if (track.is_drum && voices.total - voices.melodic > 1)
            song.warn(
                "shared-drum-channel",
                "abc2midi uses channel 10 for all percussion voices; their notes and settings can "
                "interact"
            );
        tracks.push_back(std::move(plan));
    }
    return tracks;
}

template<TType T>
Result<std::string> write_score(const Score<T>& score) {
    ExportWarnings       warnings;
    const ExportLocation at{"score", warnings};
    const ExportClock    clock(score, at);
    if (!score.markers->empty())
        at.warn("omitted-markers", "ABC export does not represent arbitrary timed markers");
    auto tracks = plan_tracks(score, clock, at);
    auto [text, global]
        = tune_header(score, clock, tracks.empty() ? "Untitled" : tracks.front().name, warnings);
    if (tracks.empty()) tracks.push_back({at, "", 0, false, {Timeline{}}, {}});
    ExportBudget budget;
    i32          id = 0;
    for (const auto& track : tracks)
        for (const auto& lane : track.lanes) write_voice(text, budget, ++id, track, lane, global);
    vec<std::string> messages;
    for (const auto& [label, message] : warnings)
        messages.push_back(fmt::format("{}: {}", label, message));
    return {std::move(text), std::move(messages)};
}

}   // namespace

Result<std::string> write(const Score<Tick>& score) { return write_score(score); }
Result<std::string> write(const Score<Quarter>& score) { return write_score(score); }
Result<std::string> write(const Score<Second>& score) { return write_score(score); }

}   // namespace symusic::abc

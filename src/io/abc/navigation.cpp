#include "navigation.h"

#include <algorithm>
#include <iterator>
#include <map>
#include <optional>
#include <ranges>
#include <span>
#include <string>
#include <string_view>
#include <utility>

#include <fmt/format.h>

namespace symusic::abc::detail {
namespace {

// Entries mark a gap before a token. A bar action consumes the bar it replaces.
// Charge the full visit before publishing a range, including every repeated token.
struct Schedule {
    std::span<const Token> voice;
    Diagnostics&           diagnostics;
    PlayOrder              order;
    size_t                 next_written = 0;
    size_t                 remaining    = PlaybackWork::kLimit;

    void require(size_t count, size_t at) const {
        if (count > remaining)
            diagnostics.error(
                voice[at].at, "expansion-limit", "expanded music exceeds the playback token limit"
            );
    }

    void append(size_t first, size_t end, Flow flow = Flow::Token, u32 occurrence = 0) {
        if (first == end && flow == Flow::Token) return;
        const auto count = std::max<size_t>(1, end - first);
        require(count, first);
        remaining -= count;
        const bool restore
            = first > next_written || (flow == Flow::EnterPart && first < next_written);
        next_written = end;
        if (flow == Flow::Token && !order.empty() && order.back().flow == Flow::Token
            && order.back().last == first)
            order.back().last += count;
        else
            order.push_back({first, first + count, flow, restore, false, occurrence});
    }
};

struct Boundary {
    size_t     token;
    const Bar& bar;
    size_t     repeat_end;   // Includes an inferred close before a new repeat start.
    bool       ending_at_previous_close;
    // Distance to the next routing boundary, or to the end of the boundary list.
    std::ptrdiff_t route_stride = 1;
};

bool ending_prefix(const Token& token) {
    // The meter field before [2 in the section 13.1 example is part of the
    // ending. Settings between the repeat bar and its ending do not separate them.
    return std::holds_alternative<Field>(token.value) || token.is_voice_setting();
}

vec<Boundary> boundaries(
    std::span<const Token> voice, size_t first, size_t last, Diagnostics& diagnostics
) {
    vec<Boundary> result;
    bool          opened      = false;
    bool          after_close = false;
    for (auto i = first; i < last; ++i) {
        const auto& token = voice[i];
        const auto* bar   = std::get_if<Bar>(&token.value);
        if (!bar) {
            if (after_close && !ending_prefix(token)) after_close = false;
            continue;
        }
        auto repeat_end = bar->repeat_end;
        if (bar->repeat_start && !repeat_end && opened) {
            diagnostics.warn(
                token.at,
                "nested-repeat",
                "treated a nested repeat start as an end followed by a new start"
            );
            repeat_end = 2;
        }
        if (repeat_end) opened = false;
        if (bar->repeat_start) opened = true;
        const bool joined = after_close && !bar->resets_measure && !bar->endings.empty();
        result.push_back({i, *bar, repeat_end, joined});
        after_close = repeat_end != 0;
    }
    std::ptrdiff_t stride = 1;
    for (auto& boundary : result | std::views::reverse) {
        boundary.route_stride = stride;
        stride = boundary.repeat_end || !boundary.bar.endings.empty() ? 1 : stride + 1;
    }
    return result;
}

using BarIterator = vec<Boundary>::const_iterator;

// The next written token and the first bar at or after it. A trailing ending
// can resume on either side of a bar.
struct Cursor {
    size_t      token;
    BarIterator bar;
};

Cursor after(BarIterator bar) { return {bar->token + 1, std::next(bar)}; }

struct Repeat {
    Cursor                first;
    BarIterator           last_close;
    const RepeatEndings*  trailing;
    Cursor                resume;
    std::optional<size_t> local_passes;   // Otherwise P: selects the pass for this region.
};

class WrittenPart {
public:
    WrittenPart(size_t begin, size_t end, Schedule& schedule) :
        begin(begin), end(end), schedule(schedule) {}

    // A named part emits a verse selection on entry. An anonymous section
    // (the prelude or the entire voice) starts directly with its music.
    void append(size_t scheduled_visits = 0);

private:
    size_t        begin, end;
    Schedule&     schedule;
    vec<Boundary> bars;
    vec<Repeat>   repeats;
    u32           visits = 0;

    void                      compile(size_t scheduled_visits);
    [[nodiscard]] BarIterator ending_at_close(BarIterator close) const;
    [[nodiscard]] Repeat      repeat_from(Cursor first, BarIterator close) const;
    [[nodiscard]] Cursor      after_ending(BarIterator marker) const;
    void                      play_repeat(const Repeat& repeat);
    [[nodiscard]] bool jumps_inside_music_line(BarIterator first_bar, size_t played_from) const;
    [[nodiscard]] bool ending_starts_at_bar(const Boundary& boundary) const;
    void               play_pass(const Repeat& repeat, size_t pass);
};

bool WrittenPart::ending_starts_at_bar(const Boundary& boundary) const {
    if (boundary.bar.resets_measure) return true;
    if (boundary.token == begin) return false;
    const auto* previous = std::get_if<Bar>(&schedule.voice[boundary.token - 1].value);
    return previous && previous->resets_measure && !previous->repeat_end;
}

BarIterator WrittenPart::ending_at_close(BarIterator close) const {
    if (!close->bar.endings.empty()) return close;
    const auto next = std::next(close);
    return next != bars.end() && next->ending_at_previous_close ? next : bars.end();
}

Cursor WrittenPart::after_ending(BarIterator marker) const {
    for (auto bar = std::next(marker); bar != bars.end(); ++bar) {
        if (bar->bar.repeat_start || bar->repeat_end || !bar->bar.endings.empty())
            return {bar->token, bar};
        // ABC 2.1 section 4.10 includes the terminating double bar in the ending.
        if (bar->bar.section) return after(bar);
    }
    return {end, bars.end()};
}

Repeat WrittenPart::repeat_from(Cursor first, BarIterator close) const {
    // Successive numbered endings may each have their own closing repeat.
    // A section or new repeat start separates the next region.
    auto ending = bars.end();
    while (!close->bar.repeat_start) {
        ending = ending_at_close(close);
        if (ending == bars.end()) break;
        const auto next = std::find_if(std::next(ending), bars.end(), [](const Boundary& bar) {
            return bar.repeat_end || bar.bar.repeat_start || bar.bar.section;
        });
        if (next == bars.end() || next->bar.repeat_start || next->bar.section) break;
        close = next;
    }
    const auto* trailing = ending == bars.end() ? nullptr : &ending->bar.endings;
    size_t      passes   = first.bar == bars.begin() ? 0 : std::prev(first.bar)->bar.repeat_start;
    for (const auto& bar : std::ranges::subrange(first.bar, std::next(close)))
        passes = std::max({passes, bar.repeat_end, bar.bar.endings.last_pass()});
    if (trailing) passes = std::max(passes, trailing->last_pass());
    return {first, close, trailing, trailing ? after_ending(ending) : after(close), passes};
}

void WrittenPart::play_pass(const Repeat& repeat, size_t pass) {
    auto first                 = repeat.first.token;
    bool playing               = true;
    bool skipped_ending_at_bar = false;
    for (auto current = repeat.first.bar; current <= repeat.last_close;) {
        const auto& boundary = *current;
        if (playing)
            schedule.append(first, boundary.token);
        else
            // Unlike emitted steps, skipped routing decisions are not charged in playback.
            schedule.diagnostics.spend(PlaybackWork::kToken, schedule.voice[boundary.token].at);
        first                = boundary.token + 1;
        const auto& endings  = boundary.bar.endings;
        const bool  selected = endings.contains(pass);
        if (boundary.repeat_end) {
            const auto next_ending = ending_at_close(current);
            const bool enters_ending
                = next_ending != bars.end() && next_ending->bar.endings.contains(pass);
            if (enters_ending || playing) {
                auto flow = Flow::SoftBar;
                if (enters_ending && skipped_ending_at_bar)
                    flow = Flow::HardBar;
                else if (!playing)
                    flow = Flow::InlineEnding;
                // On a skip, use the original ending's measure boundary and the
                // selected ending's lyric line.
                const auto bar = playing ? boundary.token : next_ending->token;
                schedule.append(bar, bar + 1, flow);
            }
            if (playing && current != repeat.last_close) return;
            if (enters_ending && current != repeat.last_close && next_ending != current)
                schedule.append(first, next_ending->token);
        } else {
            if (selected || (playing && endings.empty())) schedule.append(boundary.token, first);
            if (playing && !selected && !endings.empty())
                skipped_ending_at_bar = ending_starts_at_bar(boundary);
        }
        if (!endings.empty()) playing = selected;
        current += boundary.route_stride;
    }
    if (repeat.trailing && repeat.trailing->contains(pass))
        schedule.append(repeat.last_close->token + 1, repeat.resume.token);
}

bool WrittenPart::jumps_inside_music_line(BarIterator first_bar, size_t played_from) const {
    // An implicit opening bar is inserted after the initial music-line entry.
    if (first_bar == bars.begin()) return true;
    const auto& opening = *std::prev(first_bar);
    if (!opening.bar.repeat_start && !opening.bar.section) return false;
    for (const auto& range : std::span(schedule.order).subspan(played_from)) {
        const auto tokens = range.tokens(schedule.voice);
        const auto anchor = std::ranges::find_if(tokens, &Token::anchors_lyrics);
        if (anchor != tokens.end())
            return anchor->music_line == schedule.voice[opening.token].music_line;
    }
    return false;
}

void WrittenPart::play_repeat(const Repeat& repeat) {
    if (!repeat.local_passes) {
        play_pass(repeat, visits);
        return;
    }
    const auto first = repeat.first.token;
    // Each pass emits an entry or jump, even if all its notes are skipped.
    schedule.require(*repeat.local_passes, first);
    schedule.append(first, first, first == begin ? Flow::EnterImplicitRepeat : Flow::EnterRepeat);
    play_pass(repeat, 1);
    for (size_t pass = 2; pass <= *repeat.local_passes; ++pass) {
        const auto jump = schedule.order.size();
        schedule.append(first, first, Flow::JumpRepeat);
        play_pass(repeat, pass);
        schedule.order[jump].continues_lyrics = jumps_inside_music_line(repeat.first.bar, jump + 1);
    }
}

void WrittenPart::compile(size_t scheduled_visits) {
    bars = boundaries(schedule.voice, begin, end, schedule.diagnostics);
    Cursor first{begin, bars.cbegin()};
    auto   latest_section = bars.cend();
    for (auto marker = first.bar; marker != bars.cend();) {
        if (marker->repeat_end) {
            // Leading endings in a repeated part select by part visit (§4.10).
            const bool by_visit = scheduled_visits > 1 && first.token == begin
                                  && std::any_of(first.bar, marker, [](const Boundary& bar) {
                                         return !bar.bar.endings.empty();
                                     });
            const bool explicit_start
                = first.bar != bars.cbegin() && std::prev(first.bar)->bar.repeat_start;
            // An unmatched close repeats from the latest double bar (§4.8).
            if (!by_visit && !explicit_start && latest_section != bars.cend())
                first = after(latest_section);
            auto repeat = repeat_from(first, marker);
            if (by_visit) repeat.local_passes.reset();
            first = repeat.resume;
            repeats.push_back(repeat);
        } else if (marker->bar.repeat_start) {
            first = after(marker);
        } else {
            if (marker->bar.section && marker->bar.endings.empty()) latest_section = marker;
            ++marker;
            continue;
        }
        marker         = first.bar;
        latest_section = bars.cend();
    }
    if (first.bar == bars.cbegin() || !std::prev(first.bar)->repeat_end) {
        for (const auto& boundary : std::ranges::subrange(first.bar, bars.cend())) {
            if (!boundary.bar.endings.empty())
                schedule.diagnostics.warn(
                    schedule.voice[boundary.token].at,
                    "orphan-ending",
                    "played a numbered ending outside a closed repeat in written order"
                );
        }
    }
}

void WrittenPart::append(size_t scheduled_visits) {
    // Compile only parts that play. An omitted section cannot issue repeat warnings.
    if (visits == 0) compile(scheduled_visits);
    auto first = begin;
    if (scheduled_visits) schedule.append(first, first, Flow::EnterPart, visits);
    ++visits;
    for (const auto& repeat : repeats) {
        schedule.append(first, repeat.first.token);
        play_repeat(repeat);
        first = repeat.resume.token;
    }
    schedule.append(first, end);
}

using Parts = std::map<char, WrittenPart>;

Parts split_parts(Schedule& schedule) {
    const auto source = schedule.voice;
    Parts      parts;
    char       label = '\0';
    size_t     start = 0;
    const auto save  = [&](size_t end) {
        if (!parts.try_emplace(label, start, end, schedule).second) {
            const auto& marker = source[end == source.size() ? start : end];
            schedule.diagnostics.error(
                marker.at, "duplicate-part", "part label is repeated in this voice"
            );
        }
    };
    for (size_t i = 0; i < source.size(); ++i) {
        const auto& marker = source[i];
        const auto* field  = std::get_if<Field>(&marker.value);
        if (!field || field->kind != 'P') continue;
        if (field->value.size() != 1 || !letter(field->value.front())) {
            schedule.diagnostics.warn(
                marker.at, "invalid-part-label", "ignored a P: label that is not a single letter"
            );
            continue;
        }
        save(i);
        label = upper(field->value.front());
        start = i;
    }
    save(source.size());
    return parts;
}

}   // namespace

PartItinerary::PartItinerary(std::string_view order, Location at, Diagnostics& diagnostics) :
    at(at), diagnostics(diagnostics) {
    if (order.empty()) return;
    labels = part_order(order, PlaybackWork::kLimit, at, diagnostics);
    for (char label : *labels) ++visits[label - 'A'];
}

PlayOrder PartItinerary::arrange(const vec<Token>& voice) const {
    Schedule schedule{voice, diagnostics};
    if (!labels) {
        WrittenPart(0, voice.size(), schedule).append();
    } else {
        auto parts = split_parts(schedule);
        parts.at('\0').append();
        for (char label : *labels) {
            const auto part = parts.find(label);
            if (part == parts.end())
                diagnostics.warn(
                    at, "missing-part", fmt::format("no music found for part {}", label)
                );
            else
                part->second.append(visits[label - 'A']);
        }
    }
    return std::move(schedule.order);
}

}   // namespace symusic::abc::detail

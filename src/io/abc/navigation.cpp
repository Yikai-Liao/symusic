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
#include <variant>

namespace symusic::abc::detail {
namespace {

// Bound navigation independently of how many events each step emits.
constexpr size_t kMaxMaterializedSteps = 1'000'000;
using Section                          = std::span<const Token>;

struct Boundary {
    Section::iterator token;
    const Bar&        bar;
    size_t            repeat_end;   // Includes an inferred close before a new repeat start.
    bool              ending_at_previous_close;
    // Distance to the next routing boundary, or to the end of the boundary list.
    std::ptrdiff_t route_stride = 1;
};

bool ending_prefix(const Token& token) {
    // The meter field before [2 in the section 13.1 example is part of the
    // ending. Settings between the repeat bar and its ending do not separate them.
    return std::holds_alternative<Field>(token.value)
           || std::holds_alternative<TempoNotation>(token.value)
           || std::holds_alternative<PitchChange>(token.value)
           || std::holds_alternative<Directive>(token.value)
           || std::holds_alternative<MidiTrackCommand>(token.value)
           || std::holds_alternative<VoiceInstrument>(token.value);
}

vec<Boundary> boundaries(Section source, Diagnostics& diagnostics) {
    vec<Boundary> result;
    bool          opened      = false;
    bool          after_close = false;
    for (auto token = source.begin(); token != source.end(); ++token) {
        const auto* bar = std::get_if<Bar>(&token->value);
        if (!bar) {
            if (after_close && !ending_prefix(*token)) after_close = false;
            continue;
        }
        auto repeat_end = bar->repeat_end;
        if (bar->repeat_start && !repeat_end && opened) {
            diagnostics.warn(
                token->at,
                "nested-repeat",
                "treated a nested repeat start as an end followed by a new start"
            );
            repeat_end = 2;
        }
        if (repeat_end) opened = false;
        if (bar->repeat_start) opened = true;
        const bool joined = after_close && !bar->resets_measure && !bar->endings.empty();
        result.push_back({token, *bar, repeat_end, joined});
        after_close = repeat_end != 0;
    }
    std::ptrdiff_t stride = 1;
    for (auto& boundary : result | std::views::reverse) {
        boundary.route_stride = stride;
        stride = boundary.repeat_end || !boundary.bar.endings.empty() ? 1 : stride + 1;
    }
    return result;
}

// A repeat region may have several closing bars in successive numbered endings.
// A region is replayed once per selected pass; intermediate endings are skipped.
class RepeatOrder {
public:
    RepeatOrder(Section source, PlayOrder& output, Diagnostics& diagnostics) :
        source(source), bars(boundaries(source, diagnostics)), output(output),
        diagnostics(diagnostics) {}
    void append(std::optional<size_t> part_pass = {});
    void append_part(u32 occurrence, size_t visits);

private:
    Section       source;
    vec<Boundary> bars;
    PlayOrder&    output;
    Diagnostics&  diagnostics;

    using BarIterator   = vec<Boundary>::const_iterator;
    using TokenIterator = Section::iterator;

    struct TrailingEnding {
        BarIterator   marker;
        TokenIterator end;
        BarIterator   resume;
    };

    struct Closing {
        BarIterator                   last;
        size_t                        passes;
        std::optional<TrailingEnding> trailing;
    };

    BarIterator                   next_ending_close(BarIterator from) const;
    BarIterator                   ending_at_close(BarIterator close) const;
    std::optional<TrailingEnding> trailing_ending(BarIterator close) const;
    Closing                       closing(BarIterator first_close, BarIterator first_bar) const;
    void                          reserve_steps(size_t count, TokenIterator token) const;
    void                          step(TokenIterator token, Flow flow);
    void                          slice(TokenIterator first, TokenIterator end);
    void region(TokenIterator first, BarIterator first_bar, const Closing& close);
    bool jumps_inside_music_line(BarIterator first_bar, size_t played_from) const;
    void play_pass(TokenIterator first, BarIterator first_bar, const Closing& close, size_t pass);
};

bool ending_starts_at_bar(const Boundary& boundary, Section source) {
    if (boundary.bar.resets_measure) return true;
    if (boundary.token == source.begin()) return false;
    const auto* previous = std::get_if<Bar>(&std::prev(boundary.token)->value);
    return previous && previous->resets_measure && !previous->repeat_end;
}

RepeatOrder::BarIterator RepeatOrder::next_ending_close(BarIterator from) const {
    const auto ending = ending_at_close(from);
    if (ending == bars.end()) return bars.end();
    for (auto next = std::next(ending); next != bars.end(); ++next) {
        if (next->bar.repeat_start || next->bar.section) break;
        if (next->repeat_end) return next;
    }
    return bars.end();
}

RepeatOrder::Closing RepeatOrder::closing(BarIterator first_close, BarIterator first_bar) const {
    const auto opening = first_bar != bars.begin() ? std::prev(first_bar)->bar.repeat_start : 0;
    // Both delimiters and numbered endings can request passes. Honor the largest request.
    Closing result{first_close, opening, {}};
    auto    first = first_bar;
    for (;;) {
        const auto end = std::next(result.last);
        for (const auto& boundary : std::ranges::subrange(first, end))
            result.passes = std::max(result.passes, boundary.bar.endings.last_pass());
        result.passes = std::max(result.passes, result.last->repeat_end);
        if (result.last->bar.repeat_start) break;
        const auto next = next_ending_close(result.last);
        if (next == bars.end()) break;
        first       = end;
        result.last = next;
    }
    result.trailing = trailing_ending(result.last);
    if (result.trailing)
        result.passes = std::max(result.passes, result.trailing->marker->bar.endings.last_pass());
    return result;
}

RepeatOrder::BarIterator RepeatOrder::ending_at_close(BarIterator close) const {
    if (!close->bar.endings.empty()) return close;
    const auto next = std::next(close);
    return next != bars.end() && next->ending_at_previous_close ? next : bars.end();
}

std::optional<RepeatOrder::TrailingEnding> RepeatOrder::trailing_ending(BarIterator close) const {
    if (close->bar.repeat_start) return {};
    const auto marker = ending_at_close(close);
    if (marker == bars.end()) return {};
    for (auto end = std::next(marker); end != bars.end(); ++end) {
        // ABC 2.1 section 4.10 closes the ending at the next double bar.
        // Incomplete notation does not consume the next repeat or ending.
        if (end->bar.repeat_start || end->repeat_end || !end->bar.endings.empty())
            return TrailingEnding{marker, end->token, end};
        if (end->bar.section) return TrailingEnding{marker, std::next(end->token), std::next(end)};
    }
    return TrailingEnding{marker, source.end(), bars.end()};
}

void RepeatOrder::reserve_steps(size_t count, TokenIterator token) const {
    if (count > kMaxMaterializedSteps - output.size()) {
        diagnostics.error(
            token->at, "expansion-limit", "expanded music exceeds the playback token limit"
        );
    }
}

void RepeatOrder::step(TokenIterator token, Flow flow) {
    reserve_steps(1, token);
    output.push_back({&*token, flow});
}

void RepeatOrder::slice(TokenIterator first, TokenIterator end) {
    if (first == end) return;
    reserve_steps(static_cast<size_t>(end - first), first);
    for (const auto& token : std::ranges::subrange(first, end)) output.push_back({&token});
}

void RepeatOrder::play_pass(
    TokenIterator first, BarIterator first_bar, const Closing& close, size_t pass
) {
    bool playing               = true;
    bool skipped_ending_at_bar = false;
    for (auto current = first_bar; current <= close.last;) {
        const auto& boundary = *current;
        if (playing)
            slice(first, boundary.token);
        else
            // Unlike emitted steps, skipped routing decisions are not charged in playback.
            diagnostics.spend(PlaybackWork::token, boundary.token->at);
        first                = std::next(boundary.token);
        const auto& endings  = boundary.bar.endings;
        const bool  selected = endings.contains(pass);
        if (boundary.repeat_end) {
            const auto next_ending = ending_at_close(current);
            const bool enters_ending
                = next_ending != bars.end() && next_ending->bar.endings.contains(pass);
            if (enters_ending || playing) {
                auto flow = Flow::soft_bar;
                if (enters_ending && skipped_ending_at_bar)
                    flow = Flow::hard_bar;
                else if (!playing)
                    flow = Flow::inline_ending;
                // On a skip, use the original ending's measure boundary and the
                // selected ending's lyric line.
                step(playing ? boundary.token : next_ending->token, flow);
            }
            if (playing && current != close.last) return;
            if (enters_ending && current != close.last && next_ending != current)
                slice(first, next_ending->token);
        } else {
            if (selected || (playing && endings.empty())) slice(boundary.token, first);
            if (playing && !selected && !endings.empty())
                skipped_ending_at_bar = ending_starts_at_bar(boundary, source);
        }
        if (!endings.empty()) playing = selected;
        current += playing ? 1 : boundary.route_stride;
    }
    if (close.trailing && close.trailing->marker->bar.endings.contains(pass))
        slice(std::next(close.last->token), close.trailing->end);
}

bool RepeatOrder::jumps_inside_music_line(BarIterator first_bar, size_t played_from) const {
    // An implicit opening bar is inserted after the initial music-line entry.
    if (first_bar == bars.begin()) return true;
    const auto& opening = *std::prev(first_bar);
    if (!opening.bar.repeat_start && !opening.bar.section) return false;
    const auto played = std::span(output).subspan(played_from);
    const auto entry  = std::ranges::find_if(played, [](const PlayStep& step) {
        return step.token->anchors_lyrics();
    });
    return entry != played.end() && entry->token->music_line == opening.token->music_line;
}

void RepeatOrder::region(TokenIterator first, BarIterator first_bar, const Closing& close) {
    // Each pass emits at least an entry or jump, even when all its notes are skipped.
    reserve_steps(close.passes, first);
    step(first, first == source.begin() ? Flow::enter_implicit_repeat : Flow::enter_repeat);
    play_pass(first, first_bar, close, 1);
    for (size_t pass = 2; pass <= close.passes; ++pass) {
        const auto jump = output.size();
        step(first, Flow::jump_repeat);
        play_pass(first, first_bar, close, pass);
        output[jump].continues_lyrics = jumps_inside_music_line(first_bar, jump + 1);
    }
}

void RepeatOrder::append_part(u32 occurrence, size_t visits) {
    reserve_steps(1, source.begin());
    output.push_back({&source.front(), Flow::enter_part, false, occurrence});
    append(visits > 1 ? std::optional<size_t>{occurrence + 1} : std::nullopt);
}

void RepeatOrder::append(std::optional<size_t> part_pass) {
    auto first          = source.begin();
    auto first_bar      = bars.cbegin();
    auto latest_section = bars.cend();
    for (auto marker = first_bar; marker != bars.cend();) {
        if (marker->repeat_end) {
            // ABC 2.1 section 4.10: numbered endings at the start of a
            // repeated part, without an opening repeat bar, select by visit.
            const bool part_endings = part_pass && first == source.begin()
                                      && std::ranges::any_of(
                                          std::ranges::subrange(first_bar, marker),
                                          [](const auto& bar) { return !bar.bar.endings.empty(); }
                                      );
            const bool explicit_start
                = first_bar != bars.cbegin() && std::prev(first_bar)->bar.repeat_start;
            // ABC 2.1 section 4.8: an unmatched close can repeat from the latest double bar.
            if (!part_endings && !explicit_start && latest_section != bars.cend()) {
                const auto after = std::next(latest_section->token);
                slice(first, after);
                first     = after;
                first_bar = std::next(latest_section);
            }
            const auto close = closing(marker, first_bar);
            if (part_endings)
                play_pass(first, first_bar, close, *part_pass);
            else
                region(first, first_bar, close);
            first  = close.trailing ? close.trailing->end : std::next(close.last->token);
            marker = close.trailing ? close.trailing->resume : std::next(close.last);
        } else if (marker->bar.repeat_start) {
            const auto after = std::next(marker->token);
            slice(first, after);
            first = after;
            ++marker;
        } else {
            if (marker->bar.section && marker->bar.endings.empty())
                latest_section = marker;
            ++marker;
            continue;
        }
        first_bar      = marker;
        latest_section = bars.cend();
    }
    if (first_bar == bars.cbegin() || !std::prev(first_bar)->repeat_end) {
        for (const auto& boundary : std::ranges::subrange(first_bar, bars.cend())) {
            if (!boundary.bar.endings.empty())
                diagnostics.warn(
                    boundary.token->at,
                    "orphan-ending",
                    "played a numbered ending outside a closed repeat in written order"
                );
        }
    }
    slice(first, source.end());
}

using Parts = std::map<char, Section>;

Parts split_parts(Section source, Diagnostics& diagnostics) {
    Parts parts;
    char  label = '\0';
    auto  start = source.begin();
    for (auto marker = start; marker != source.end(); ++marker) {
        const auto* field = std::get_if<Field>(&marker->value);
        if (!field || field->kind != 'P') continue;
        if (field->value.size() != 1 || !letter(field->value.front())) {
            diagnostics.warn(
                marker->at, "invalid-part-label", "ignored a P: label that is not a single letter"
            );
            continue;
        }
        if (!parts.try_emplace(label, start, marker).second) {
            diagnostics.error(marker->at, "duplicate-part", "part label is repeated in this voice");
        }
        label = upper(field->value.front());
        start = marker;
    }
    if (!parts.try_emplace(label, start, source.end()).second) {
        diagnostics.error(start->at, "duplicate-part", "part label is repeated in this voice");
    }
    return parts;
}

}   // namespace

PartItinerary::PartItinerary(
    std::string_view order, Location at, Diagnostics& diagnostics
) : at(at), diagnostics(diagnostics) {
    if (!order.empty()) labels = part_order(order, kMaxMaterializedSteps, at, diagnostics);
}

PlayOrder PartItinerary::arrange(const vec<Token>& voice) const {
    PlayOrder tokens;
    if (!labels) {
        RepeatOrder(voice, tokens, diagnostics).append();
        return tokens;
    }
    const auto sections = split_parts(voice, diagnostics);
    RepeatOrder(sections.at('\0'), tokens, diagnostics).append();
    std::map<char, RepeatOrder> repeats;
    std::array<size_t, 26>      visits{};
    std::array<u32, 26>         occurrences{};
    for (const auto label : *labels) ++visits[label - 'A'];
    for (const auto label : *labels) {
        const auto section = sections.find(label);
        if (section == sections.end()) {
            diagnostics.warn(at, "missing-part", fmt::format("no music found for part {}", label));
            continue;
        }
        auto [repeat, inserted] = repeats.try_emplace(
            label, section->second, tokens, diagnostics
        );
        repeat->second.append_part(occurrences[label - 'A']++, visits[label - 'A']);
    }
    return tokens;
}

}   // namespace symusic::abc::detail

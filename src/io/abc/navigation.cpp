#include "navigation.h"

#include <map>
#include <span>

namespace symusic::abc::detail {
namespace {

constexpr int max_passes = 32;
// Bound navigation independently of how many events each step emits.
constexpr size_t max_materialized_steps = 1'000'000;
using Section                           = std::span<const Token>;

class PartOrder {
public:
    PartOrder(std::string_view text, Location at, Diagnostics& diagnostics) :
        text(text), at(at), diagnostics(diagnostics) {}
    std::string parse(int depth = 0);

private:
    std::string_view text;
    Location         at;
    Diagnostics&     diagnostics;

    char pop();
    int  count();
};

char PartOrder::pop() {
    const auto next = text.front();
    text.remove_prefix(1);
    return next;
}

int PartOrder::count() {
    const auto begin = text;
    while (!text.empty() && digit(text.front())) pop();
    if (begin.size() == text.size()) return 1;
    const auto count = integer(begin.substr(0, begin.size() - text.size()));
    if (!count || *count < 1 || *count > static_cast<int>(max_materialized_steps)) {
        diagnostics.error(
            at, "part-count-range", "part repeat count must be between 1 and 1000000"
        );
    }
    return *count;
}

std::string PartOrder::parse(int depth) {
    if (depth > 32) diagnostics.error(at, "part-depth", "too many nested groups in P: order");
    std::string labels;
    while (!text.empty()) {
        const auto c = pop();
        if (c == ')' && depth != 0) return labels;
        if (whitespace.find(c) != whitespace.npos || c == '.') continue;
        if (!letter(c) && c != '(')
            diagnostics.error(
                at, "invalid-parts", "P: order needs part letters, groups or repeat counts"
            );
        const auto group  = c == '(' ? parse(depth + 1) : std::string(1, upper(c));
        const auto copies = count();
        if (group.empty()) continue;
        if (group.size() * copies > max_materialized_steps - labels.size()) {
            diagnostics.error(
                at, "expansion-limit", "part order exceeds the playback expansion limit"
            );
        }
        for (int n = 0; n < copies; ++n) labels += group;
    }
    if (depth) diagnostics.error(at, "invalid-parts", "missing ')' in P: order");
    return labels;
}

struct Endings {
    uint64_t mask      = 0;
    int      last_pass = 0;
    bool     has(int pass) const { return (mask & (uint64_t{1} << pass)) != 0; }
};

int ending_pass(std::string_view text, const Token& token, Diagnostics& diagnostics) {
    const auto n = integer(text);
    if (!n || *n < 1 || *n > max_passes) {
        diagnostics.error(
            token.at, "ending-range", "repeat ending number must be between 1 and 32"
        );
    }
    return *n;
}

Endings endings(const Token& token, std::string_view text, Diagnostics& diagnostics) {
    Endings result;
    while (!text.empty()) {
        const auto comma  = text.find(',');
        const auto range  = text.substr(0, comma);
        const auto hyphen = range.find('-');
        const auto first  = ending_pass(range.substr(0, hyphen), token, diagnostics);
        const auto last   = hyphen == range.npos ?
                                first :
                                ending_pass(range.substr(hyphen + 1), token, diagnostics);
        if (first > last)
            diagnostics.error(token.at, "invalid-ending", "repeat ending range is reversed");
        for (int i = first; i <= last; ++i) result.mask |= uint64_t{1} << i;
        result.last_pass = std::max(result.last_pass, last);
        text.remove_prefix(comma == text.npos ? text.size() : comma + 1);
    }
    return result;
}

struct Boundary {
    size_t  index;
    Bar     bar;
    Endings endings;
};

vec<Boundary> boundaries(Section source, Diagnostics& diagnostics) {
    vec<Boundary> result;
    bool          opened = false;
    for (size_t i = 0; i < source.size(); ++i) {
        const auto* bar = std::get_if<Bar>(&source[i].value);
        if (!bar) continue;
        auto normalized = *bar;
        if (bar->repeat_start && !bar->repeat_end && opened) {
            diagnostics.warn(
                source[i].at,
                "nested-repeat",
                "treated a nested repeat start as an end followed by a new start"
            );
            normalized.repeat_end = true;
        }
        if (normalized.repeat_end) opened = false;
        if (normalized.repeat_start) opened = true;
        result.push_back({i, normalized, endings(source[i], bar->endings, diagnostics)});
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
    void append();

private:
    Section       source;
    vec<Boundary> bars;
    PlayOrder&    output;
    Diagnostics&  diagnostics;

    struct Closing {
        size_t last;
        int    passes;
    };

    std::optional<size_t> next_ending_close(size_t from) const;
    Closing               closing(size_t first, size_t first_bar) const;
    int                   final_pass(size_t first_bar, size_t last_bar) const;
    void                  reserve_steps(size_t count, size_t index) const;
    void                  step(size_t index, Flow flow);
    void                  slice(size_t first, size_t end);
    void                  region(size_t first, size_t first_bar, size_t last_bar, int passes);
    void                  play_pass(size_t first, size_t first_bar, size_t last_bar, int pass);
};

std::optional<size_t> RepeatOrder::next_ending_close(size_t from) const {
    bool numbered = bars[from].endings.mask != 0;
    for (size_t i = from + 1; i < bars.size(); ++i) {
        const auto& next = bars[i];
        if (next.bar.repeat_start || next.bar.section) break;
        if (next.bar.repeat_end) return numbered ? std::optional(i) : std::nullopt;
        if (next.endings.mask != 0) numbered = true;
    }
    return {};
}

RepeatOrder::Closing RepeatOrder::closing(size_t first, size_t first_bar) const {
    Closing result{first, std::max(2, bars[first].endings.last_pass)};
    while (!bars[result.last].bar.repeat_start) {
        const auto next = next_ending_close(result.last);
        if (!next) break;
        for (size_t i = result.last + 1; i <= *next; ++i)
            result.passes = std::max(result.passes, bars[i].endings.last_pass);
        result.last   = *next;
        result.passes = std::max(result.passes, 2 + static_cast<int>(*next != first));
    }
    result.passes = std::max(result.passes, final_pass(first_bar, result.last));
    return result;
}

int RepeatOrder::final_pass(size_t first_bar, size_t last_bar) const {
    if (last_bar + 1 == bars.size() || bars[last_bar].bar.repeat_start) return 0;
    const auto& tail = bars[last_bar + 1];
    if (tail.index != bars[last_bar].index + 1 || !tail.endings.mask) return 0;
    // A trailing ending can request the next pass after the contiguous endings
    // already covered by the closing repeat bars.
    uint64_t covered = 0;
    for (size_t i = first_bar; i <= last_bar; ++i) covered |= bars[i].endings.mask;
    int next = 1;
    while (next < max_passes && (covered & (uint64_t{1} << next))) ++next;
    return tail.endings.has(next) ? next : 0;
}

void RepeatOrder::reserve_steps(size_t count, size_t index) const {
    if (count > max_materialized_steps - output.size()) {
        diagnostics.error(
            source[index].at, "expansion-limit", "expanded music exceeds the playback token limit"
        );
    }
}

void RepeatOrder::step(size_t index, Flow flow) {
    reserve_steps(1, index);
    output.push_back({&source[index], flow});
}

void RepeatOrder::slice(size_t first, size_t end) {
    if (first == end) return;
    reserve_steps(end - first, first);
    for (size_t i = first; i < end; ++i) output.push_back({&source[i]});
}

void RepeatOrder::play_pass(size_t first, size_t first_bar, size_t last_bar, int pass) {
    bool playing = true;
    for (size_t b = first_bar; b <= last_bar; ++b) {
        const auto& boundary = bars[b];
        if (playing) slice(first, boundary.index);
        first               = boundary.index + 1;
        const bool selected = boundary.endings.has(pass);
        if (boundary.bar.repeat_end) {
            if (playing || selected) {
                const auto mode = selected ? Flow::hard_bar : Flow::soft_bar;
                step(boundary.index, mode);
            }
            if (playing && b != last_bar) return;
        } else if (selected || (playing && boundary.endings.mask == 0))
            slice(boundary.index, first);
        if (boundary.endings.mask) playing = selected;
    }
}

void RepeatOrder::region(size_t first, size_t first_bar, size_t last_bar, int passes) {
    step(first, Flow::enter_repeat);
    for (int pass = 1; pass <= passes; ++pass) {
        if (pass > 1) step(first, Flow::jump_repeat);
        play_pass(first, first_bar, last_bar, pass);
    }
}

void RepeatOrder::append() {
    size_t first     = 0;
    size_t first_bar = 0;
    for (size_t b = 0; b < bars.size(); ++b) {
        const auto& marker = bars[b];
        if (marker.bar.repeat_end) {
            const auto close = closing(b, first_bar);
            region(first, first_bar, close.last, close.passes);
            b = close.last;
        } else if (marker.bar.repeat_start) {
            slice(first, marker.index + 1);
        } else
            continue;
        first     = bars[b].index + 1;
        first_bar = b + 1;
    }
    const bool outside_repeat = first_bar == 0 || !bars[first_bar - 1].bar.repeat_end;
    for (size_t b = first_bar; outside_repeat && b < bars.size(); ++b) {
        if (bars[b].endings.mask != 0)
            diagnostics.warn(
                source[bars[b].index].at,
                "orphan-ending",
                "played a numbered ending outside a closed repeat in written order"
            );
    }
    slice(first, source.size());
}

using Parts = std::map<char, Section>;

Parts split_parts(Section source, Diagnostics& diagnostics) {
    Parts  parts;
    char   label = '\0';
    size_t start = 0;
    for (size_t i = 0; i < source.size(); ++i) {
        const auto* field = std::get_if<Field>(&source[i].value);
        if (!field || field->kind != 'P') continue;
        if (field->value.size() != 1 || !letter(field->value.front())) {
            diagnostics.warn(
                source[i].at, "invalid-part-label", "ignored a P: label that is not a single letter"
            );
            continue;
        }
        if (!parts.try_emplace(label, source.subspan(start, i - start)).second) {
            diagnostics.error(
                source[i].at, "duplicate-part", "part label is repeated in this voice"
            );
        }
        label = upper(field->value.front());
        start = i;
    }
    if (!parts.try_emplace(label, source.subspan(start)).second) {
        diagnostics.error(
            source[start].at, "duplicate-part", "part label is repeated in this voice"
        );
    }
    return parts;
}

}   // namespace

PlayOrder play_order(
    const vec<Token>& source, std::string_view parts, Location at, Diagnostics& diagnostics
) {
    PlayOrder tokens;
    if (parts.empty()) {
        RepeatOrder(source, tokens, diagnostics).append();
        return tokens;
    }
    const auto sections = split_parts(source, diagnostics);
    RepeatOrder(sections.at('\0'), tokens, diagnostics).append();
    for (const auto label : PartOrder(parts, at, diagnostics).parse()) {
        const auto section = sections.find(label);
        if (section == sections.end())
            diagnostics.warn(at, "missing-part", fmt::format("no music found for part {}", label));
        else
            RepeatOrder(section->second, tokens, diagnostics).append();
    }
    return tokens;
}

}   // namespace symusic::abc::detail

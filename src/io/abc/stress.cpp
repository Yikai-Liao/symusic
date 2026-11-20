#include "stress.h"

#include <algorithm>
#include <cmath>
#include <locale>
#include <sstream>

#include <tao/pegtl.hpp>

namespace symusic::abc::detail {
namespace {

namespace peg                          = tao::pegtl;
constexpr size_t      max_segments     = 31;
constexpr long double boundary_epsilon = 1e-10L;

using sign   = peg::one<'+', '-'>;
using digits = peg::plus<peg::digit>;
using dot    = peg::one<'.'>;
using mantissa
    = peg::sor<peg::seq<digits, peg::opt<dot, peg::star<peg::digit>>>, peg::seq<dot, digits>>;
struct count : peg::seq<peg::opt<sign>, digits> {};
struct velocity : digits {};
struct stretch
    : peg::seq<peg::opt<sign>, mantissa, peg::opt<peg::one<'e', 'E'>, peg::opt<sign>, digits>> {};
using spaces  = peg::plus<peg::space>;
using segment = peg::seq<spaces, velocity, spaces, stretch>;
using profile = peg::seq<
    peg::star<peg::space>,
    count,
    peg::rep_min_max<1, max_segments, segment>,
    peg::star<peg::space>,
    peg::eof>;

struct Values {
    int                              count    = 0;
    int                              velocity = 0;
    vec<std::pair<int, long double>> segments;
};

template<class Rule>
struct Read : peg::nothing<Rule> {};

template<>
struct Read<count> {
    static bool apply(const auto& input, Values& values) {
        values.count = integer(input.string_view()).value_or(0);
        return values.count > 0 && values.count <= static_cast<int>(max_segments);
    }
};

template<>
struct Read<velocity> {
    static bool apply(const auto& input, Values& values) {
        values.velocity = integer(input.string_view()).value_or(-1);
        return values.velocity >= 0 && values.velocity <= midi_max;
    }
};

template<>
struct Read<stretch> {
    static bool apply(const auto& input, Values& values) {
        std::istringstream number{input.string()};
        number.imbue(std::locale::classic());
        long double factor = 0;
        if (!(number >> factor) || factor < 0 || factor > values.count) return false;
        values.segments.emplace_back(values.velocity, factor);
        return true;
    }
};

long double segment_position(long double ticks, Meter meter, size_t segments) {
    const auto position = ticks / meter.measure() * segments;
    // Durations such as three triplets can arrive infinitesimally below the beat.
    const auto boundary = std::round(position);
    return std::abs(position - boundary) < boundary_epsilon ? boundary : position;
}

}   // namespace

std::optional<TuneStress::Profile> TuneStress::parse(std::string_view text) {
    peg::view_input<> input(text);
    Values            values;
    if (!peg::parse<profile, Read>(input, values)
        || values.segments.size() != static_cast<size_t>(values.count))
        return {};
    Profile     result;
    long double start = 0;
    for (const auto& [level, factor] : values.segments) {
        result.segments.push_back({level, factor, start});
        result.widest = std::max(result.widest, factor);
        start += factor;
    }
    if (start == 0) return {};
    // Keep segment proportions when factors do not sum to their count.
    // Forcing only the last endpoint can erase a positive final segment.
    for (auto& segment : result.segments)
        segment.warped_start = segment.warped_start / start * result.segments.size();
    return result;
}

bool TuneStress::handles(std::string_view name) {
    return name == "ptstress" || name == "stressmodel";
}

TuneStress::TuneStress(const Tune& tune, const TextLoader& files, Diagnostics& diagnostics) {
    for (const auto& token : tune.header) configure(token, opening, files, diagnostics);
    auto state = opening;
    for (const auto& token : tune.body) {
        if (configure(token, state, files, diagnostics)) changes.emplace_back(token.at, state);
    }
}

bool TuneStress::loads(Location at, std::string_view command) const {
    return loaded_profiles.contains({at, trim(command)});
}

bool TuneStress::configure(
    const Token& token, Settings& state, const TextLoader& files, Diagnostics& diagnostics
) {
    auto args = token.information().value_or(std::string_view{});
    if (word(args) != "MIDI") return false;
    const auto key  = std::pair{token.at, trim(args)};
    const auto name = word(args);
    if (name == "stressmodel") {
        const auto value = integer(trim(args));
        if (value == 1)
            state.model = Model::articulation;
        else if (value == 2)
            state.model = Model::timing;
        else {
            diagnostics.warn(token.at, "invalid-stress-model", "MIDI stressmodel must be 1 or 2");
            return false;
        }
        return true;
    }
    if (name != "ptstress") return false;
    // MIDIx can expand different commands at the same source location.
    if (const auto loaded = loaded_profiles.find(key); loaded != loaded_profiles.end()) {
        state.profile = loaded->second;
        return true;
    }

    args                          = trim(args);
    auto              next        = parse(args);
    auto              fields      = args;
    const auto        count_field = word(fields);
    peg::view_input<> count_input(count_field);
    if (!next && !count_field.empty() && !peg::parse<peg::seq<count, peg::eof>>(count_input)) {
        if (args.size() >= 2 && args.front() == '"' && args.back() == '"')
            args = args.substr(1, args.size() - 2);
        const auto  external = files ? files(args, {}) : TextRead{};
        const auto* profile  = std::get_if<ExternalText>(&external);
        if (!profile) {
            diagnostics.warn(token.at, "stress-file", "could not read MIDI stress profile");
            return false;
        }
        next = parse(profile->text);
    }
    if (!next) {
        diagnostics.warn(
            token.at,
            "invalid-stress-profile",
            "expected a segment count and velocity/stretch pairs"
        );
        return false;
    }
    state.profile = profiles.size();
    profiles.push_back(std::move(*next));
    loaded_profiles.emplace(key, state.profile);
    return true;
}

long double TuneStress::Profile::warped_tick(long double position, Meter meter) const {
    const auto count   = static_cast<long double>(segments.size());
    const auto bars    = std::floor(position / count);
    const auto within  = std::fmod(position, count);
    const auto index   = static_cast<size_t>(within);
    const auto begin   = segments[index].warped_start;
    const auto end     = index + 1 == segments.size() ? count : segments[index + 1].warped_start;
    const auto elapsed = std::lerp(begin, end, within - index) * meter.measure() / count;
    // abc2midi quantizes endpoints (not individual lengths) to .001 quarters.
    return bars * meter.measure() + std::round(elapsed * 1000 / tpq) * tpq / 1000;
}

const TuneStress::Settings& TuneStress::settings_at(Location at) const {
    const auto next
        = std::ranges::upper_bound(changes, at, {}, &decltype(changes)::value_type::first);
    return next == changes.begin() ? opening : std::prev(next)->second;
}

bool TuneStress::articulates(Location at, Meter meter) const {
    return configured() && !meter.free && settings_at(at).model == Model::articulation;
}

StressNote TuneStress::note(Location at, long double start, long double length, Meter meter) const {
    if (!configured() || meter.free || length <= 0) return {length, 0, {}};
    const auto& settings = settings_at(at);
    const auto& profile  = profiles[settings.profile];
    const auto& segments = profile.segments;
    const auto  begin    = segment_position(start, meter, segments.size());
    const auto  end      = segment_position(start + length, meter, segments.size());
    const auto  index    = std::floor(begin);
    if (settings.model == Model::timing) {
        const auto slot = static_cast<size_t>(std::fmod(index, segments.size()));
        const auto warped
            = std::max(0.L, profile.warped_tick(end, meter) - profile.warped_tick(begin, meter));
        // Before an explicit barline, the profile's terminal boundary has no gain.
        return {warped, index == segments.size() ? 0 : segments[slot].velocity, {}};
    }
    // The articulation pass leaves long notes alone. At a segment boundary
    // the reference counts the endpoint when deciding whether to apply trim.
    if (end <= begin || end > segments.size() || std::floor(end) - index > 2)
        return {length, 0, {}};
    return {
        length,
        segments[static_cast<size_t>(index)].velocity,
        profile.gate_ratio(begin, end, length)
    };
}

long double TuneStress::Profile::gate_ratio(
    long double begin, long double end, long double length
) const {
    auto        segment  = segments.begin() + static_cast<size_t>(begin);
    auto        boundary = std::floor(begin) + 1;
    long double weighted = 0;
    for (auto cursor = begin; cursor < end; ++segment, ++boundary) {
        const auto stop = std::min(boundary, end);
        weighted += (stop - cursor) * segment->stretch;
        cursor = stop;
    }
    const auto shrink = 1 - std::clamp(weighted / ((end - begin) * widest), 0.L, 1.L);
    // The reference stores trim in hundredths of the reduced duration numerator.
    const auto quarters = length / tpq;
    auto       n        = 0.L;
    auto       d        = 1.L;
    auto       prev_n   = 1.L;
    auto       prev_d   = 0.L;
    auto       fraction = quarters;
    for (int i = 0; i < 20; ++i) {
        const auto term   = std::floor(fraction);
        const auto next_n = term * prev_n + n;
        const auto next_d = term * prev_d + d;
        if (next_n > 1'000'000 || next_d > 1'000'000) break;
        n = std::exchange(prev_n, next_n);
        d = std::exchange(prev_d, next_d);
        if (std::abs(next_n / next_d - quarters) < boundary_epsilon) {
            const auto trim
                = std::floor(next_n * 100 * shrink + boundary_epsilon) / (next_d * 100) * tpq;
            return trim < length ? std::floor(length - trim + boundary_epsilon) / length : 1;
        }
        fraction = 1 / (fraction - term);
    }
    return std::floor(length * (1 - shrink) + boundary_epsilon) / length;
}

}   // namespace symusic::abc::detail

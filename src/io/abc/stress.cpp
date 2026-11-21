#include "stress.h"

#include <algorithm>
#include <cmath>
#include <locale>
#include <map>
#include <sstream>

namespace symusic::abc::detail {
namespace {

constexpr i32 kMaxSegments     = 31;
constexpr f64 kBoundaryEpsilon = 1e-10;

// Profile files can spread the fields of one measure across several lines.
std::string_view profile_field(std::string_view& text) {
    constexpr std::string_view kSpace = " \t\r\n\f\v";
    const auto                 start  = text.find_first_not_of(kSpace);
    if (start == text.npos) return {};
    text.remove_prefix(start);
    const auto field = text.substr(0, text.find_first_of(kSpace));
    text.remove_prefix(field.size());
    return field;
}

bool spells_integer(std::string_view field) {
    if (field.starts_with('+') || field.starts_with('-')) field.remove_prefix(1);
    return !field.empty() && std::ranges::all_of(field, digit);
}

f64 segment_position(f64 ticks, f64 measure, f64 segments) {
    const auto position = ticks / measure * segments;
    // Durations such as three triplets can arrive infinitesimally below the beat.
    const auto boundary = std::round(position);
    return std::abs(position - boundary) < kBoundaryEpsilon ? boundary : position;
}

}   // namespace

std::optional<TuneStress::Profile> TuneStress::parse(std::string_view text) {
    const auto count = integer(profile_field(text)).value_or(0);
    if (count < 1 || count > kMaxSegments) return {};

    Profile result;
    f64     total = 0;
    for (i32 i = 0; i < count; ++i) {
        const auto level = profile_field(text);
        if (level.empty() || !digit(level.front())) return {};
        const auto velocity = integer(level).value_or(-1);
        if (velocity < 0 || velocity > kMidiMax) return {};
        const auto factor = profile_field(text);
        if (factor.find_first_not_of("0123456789+-.eE") != factor.npos) return {};
        std::istringstream number{std::string(factor)};
        number.imbue(std::locale::classic());
        f64 stretch;
        if (!(number >> stretch) || !number.eof() || stretch < 0 || stretch > count) return {};
        result.segments.push_back({velocity, stretch, total});
        result.widest = std::max(result.widest, stretch);
        total += stretch;
    }
    if (!profile_field(text).empty() || total == 0) return {};
    // Fit the entire profile to the measure, preserving every segment's proportion.
    for (auto& segment : result.segments)
        segment.warped_start = segment.warped_start / total * count;
    return result;
}

TuneStress::TuneStress(Tune& tune, const TextLoader& files, Diagnostics& diagnostics) {
    // Repeated aliases at one MIDIx site resolve a profile only once, even on failure.
    std::map<std::tuple<Location, size_t, std::string_view>, std::optional<size_t>> loaded;
    Settings                                                                        state;
    const auto resolve = [&](Token& token) {
        const Instruction command(token);
        if (command.midi != "ptstress" && command.midi != "stressmodel") return false;
        const auto work      = PlaybackWork::text(*token.information());
        auto&      performed = token.value.emplace<StressCommand>(StressCommand{false, work});
        if (command.midi == "stressmodel") {
            const auto value = integer(trim(command.args));
            if (value == 1)
                state.model = Model::Articulation;
            else if (value == 2)
                state.model = Model::Timing;
            else {
                diagnostics.warn(
                    token.at, "invalid-stress-model", "MIDI stressmodel must be 1 or 2"
                );
                return false;
            }
            return true;
        }

        // Aliases from one invocation share a site; separate macro tokens have distinct offsets.
        const auto key         = std::tuple{token.at, token.at.expansion_offset, command.args};
        auto [entry, inserted] = loaded.try_emplace(key);
        if (inserted) {
            auto profile = read_profile(command.args, token.at, files, diagnostics);
            if (profile) {
                entry->second = profiles.size();
                profiles.push_back(std::move(*profile));
            }
        }
        if (!entry->second) return false;
        state.profile            = *entry->second;
        performed.profile_loaded = true;
        return true;
    };
    for (auto& token : tune.header) resolve(token);
    settings = WrittenSetting<Settings>(state);
    for (auto& token : tune.body) {
        if (resolve(token)) settings.record(token.at.written, state);
    }
}

std::optional<TuneStress::Profile> TuneStress::read_profile(
    std::string_view args, Location at, const TextLoader& files, Diagnostics& diagnostics
) {
    args              = trim(args);
    auto       next   = parse(args);
    auto       fields = args;
    const auto first  = word(fields);
    // A malformed inline profile (including an oversized count) is never a file name.
    if (!next && !first.empty() && !spells_integer(first)) {
        if (args.size() >= 2 && args.front() == '"' && args.back() == '"')
            args = args.substr(1, args.size() - 2);
        const auto  external = files ? files(args, {}) : TextRead{};
        const auto* profile  = std::get_if<ExternalText>(&external);
        if (!profile) {
            diagnostics.warn(at, "stress-file", "could not read MIDI stress profile");
            return {};
        }
        next = parse(profile->text);
    }
    if (!next) {
        diagnostics.warn(
            at, "invalid-stress-profile", "expected a segment count and velocity/stretch pairs"
        );
    }
    return next;
}

f64 TuneStress::Profile::warped_tick(f64 position, f64 measure) const {
    const auto count   = static_cast<f64>(segments.size());
    const auto bars    = std::floor(position / count);
    const auto within  = std::fmod(position, count);
    const auto index   = static_cast<size_t>(within);
    const auto begin   = segments[index].warped_start;
    const auto end     = index + 1 == segments.size() ? count : segments[index + 1].warped_start;
    const auto elapsed = std::lerp(begin, end, within - std::floor(within)) * measure / count;
    // abc2midi quantizes endpoints (not individual lengths) to .001 quarters.
    return bars * measure + std::round(elapsed * 1000 / kTpq) * kTpq / 1000;
}

bool TuneStress::articulates(Location at, Meter meter) const {
    return configured() && !meter.free && settings.at(at.written).model == Model::Articulation;
}

StressNote TuneStress::note(Location at, f64 start, f64 length, Meter meter) const {
    if (!configured() || meter.free || length <= 0) return {length, 0, {}};
    const auto& state    = settings.at(at.written);
    const auto& profile  = profiles[state.profile];
    const auto& segments = profile.segments;
    const auto  count    = static_cast<f64>(segments.size());   // One to kMaxSegments per measure.
    const auto  measure  = meter.measure();
    const auto  begin    = segment_position(start, measure, count);
    const auto  end      = segment_position(start + length, measure, count);
    const auto  index    = std::floor(begin);
    if (state.model == Model::Timing) {
        const auto slot   = static_cast<size_t>(std::fmod(index, count));
        const auto warped = std::max(
            0.0, profile.warped_tick(end, measure) - profile.warped_tick(begin, measure)
        );
        // Before an explicit barline, the profile's terminal boundary has no gain.
        return {warped, index == count ? 0 : segments[slot].velocity, {}};
    }
    // The articulation pass leaves long notes alone. At a segment boundary
    // the reference counts the endpoint when deciding whether to apply trim.
    if (end <= begin || end > count || std::floor(end) - index > 2) return {length, 0, {}};
    return {
        length,
        segments[static_cast<size_t>(index)].velocity,
        profile.gate_ratio(begin, end, length)
    };
}

f64 TuneStress::Profile::gate_ratio(f64 begin, f64 end, f64 length) const {
    f64 weighted = 0;
    for (auto cursor = begin; cursor < end;) {
        const auto segment = std::floor(cursor);
        const auto stop    = std::min(segment + 1, end);
        weighted += (stop - cursor) * segments[static_cast<size_t>(segment)].stretch;
        cursor = stop;
    }
    const auto shrink = 1 - std::clamp(weighted / ((end - begin) * widest), 0.0, 1.0);
    // The reference stores trim in hundredths of the reduced duration numerator.
    const auto quarters = length / kTpq;
    auto       n        = 0.0;
    auto       d        = 1.0;
    auto       prev_n   = 1.0;
    auto       prev_d   = 0.0;
    auto       fraction = quarters;
    for (i32 i = 0; i < 20; ++i) {
        const auto term   = std::floor(fraction);
        const auto next_n = term * prev_n + n;
        const auto next_d = term * prev_d + d;
        if (next_n > 1'000'000 || next_d > 1'000'000) break;
        n = std::exchange(prev_n, next_n);
        d = std::exchange(prev_d, next_d);
        if (std::abs(next_n / next_d - quarters) < kBoundaryEpsilon) {
            const auto trim
                = std::floor(next_n * 100 * shrink + kBoundaryEpsilon) / (next_d * 100) * kTpq;
            return trim < length ? std::floor(length - trim + kBoundaryEpsilon) / length : 1;
        }
        fraction = 1 / (fraction - term);
    }
    return std::floor(length * (1 - shrink) + kBoundaryEpsilon) / length;
}

}   // namespace symusic::abc::detail

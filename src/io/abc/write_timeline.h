#pragma once

#include <map>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

#include <fmt/format.h>

#include "playback_rules.h"
#include "symusic/mtype.h"

namespace symusic::abc::detail {

[[noreturn]] inline void invalid_export(std::string_view where, std::string_view message) {
    throw std::invalid_argument(fmt::format("ABC export {}: {}", where, message));
}

// Keep the first example per location and code, in diagnostic label order.
using ExportWarnings = std::map<std::string, std::string>;

struct ExportLocation {
    std::string     label;
    ExportWarnings& warnings;

    template<class... Args>
    void warn(std::string_view code, fmt::format_string<Args...> message, Args&&... args) const {
        auto [it, inserted] = warnings.try_emplace(fmt::format("ABC export {} [{}]", label, code));
        if (inserted) it->second = fmt::format(message, std::forward<Args>(args)...);
    }

    [[noreturn]] void invalid(std::string_view message) const { invalid_export(label, message); }
};

class ExportBudget {
public:
    void spend(size_t work, const ExportLocation& at, size_t copies = 1) {
        if (copies && work > remaining / copies)
            at.invalid("[expansion-limit] output exceeds the ABC playback work limit");
        remaining -= work * copies;
    }

private:
    size_t remaining = PlaybackWork::kLimit;
};

struct WrittenNote {
    i32 start;
    i32 end;
    i8  pitch;
    i8  velocity;
};

// Written ticks include every attack, release and placed syllable. Within one
// lane, simultaneous attacks share a velocity and sounding pitches are unique.
struct Moment {
    vec<WrittenNote>   attacks;
    size_t             releases = 0;
    const std::string* lyric    = nullptr;   // Borrowed from the score for the duration of write().

    bool try_lyric(const std::string& word) {
        if (lyric) return false;
        lyric = &word;
        return true;
    }
};

using Timeline = std::map<i32, Moment>;
using Fields   = std::map<i32, vec<std::string>>;

struct PlannedTrack {
    ExportLocation at;
    std::string    name;
    i32            program;
    bool           drums;
    vec<Timeline>  lanes;
    Fields         channel;
};

}   // namespace symusic::abc::detail

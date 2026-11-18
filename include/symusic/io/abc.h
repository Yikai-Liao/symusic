#pragma once

#include <string_view>

#include "symusic/conversion.h"

namespace symusic::abc {

// Recoverable problems are returned in source order, not printed. An operation
// that cannot produce a score or valid ABC throws std::invalid_argument instead.
template<class T>
struct Result {
    T                value;
    vec<std::string> warnings;
};

// Read the first tune as performed music, ignoring layout and print annotations.
// source is a label for diagnostics; it is not opened as a file.
[[nodiscard]] Result<Score<Tick>> read(std::string_view text, std::string_view source = "<string>");

// Write playable ABC, diagnosing score events that cannot be represented.
[[nodiscard]] Result<std::string> write(const Score<Tick>& score);

}   // namespace symusic::abc

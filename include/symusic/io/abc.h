#pragma once

#include <string_view>

#include "symusic/score.h"

namespace symusic::abc {

// Recoverable problems are returned as diagnostics, not printed. An operation
// that cannot produce a score or valid ABC throws std::invalid_argument instead.
template<class T>
struct Result {
    T                value;
    vec<std::string> warnings;
};

// Read the first UTF-8 tune as performed music, ignoring layout and print annotations.
// source is a label for diagnostics; it is not opened as a file.
[[nodiscard]] Result<Score<Tick>> read(std::string_view text, std::string_view source = "<string>");

// Write playable ABC. Validate score times before rounding to the playback grid.
[[nodiscard]] Result<std::string> write(const Score<Tick>& score);
[[nodiscard]] Result<std::string> write(const Score<Quarter>& score);
[[nodiscard]] Result<std::string> write(const Score<Second>& score);

}   // namespace symusic::abc

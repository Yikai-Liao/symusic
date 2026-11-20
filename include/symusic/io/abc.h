#pragma once

#include <filesystem>
#include <string_view>

#include "symusic/score.h"

namespace symusic::abc {

// A completed import or export, with recoverable diagnostics. The core never
// prints these warnings. Read warnings carry a source name, physical line,
// UTF-8 byte column and diagnostic code; write warnings describe losses and
// compatibility limits in the generated text.
template<class T>
struct Result {
    T                value;
    vec<std::string> warnings;
};

// Read the first tune from UTF-8 or US-ASCII text as a performed score at
// 480 ticks per quarter note. Parts and repeats are expanded; engraving is not retained.
// The result owns its data independently of text. source labels diagnostics;
// neither it nor external references in the text are opened as files.
// Throws std::invalid_argument with a located diagnostic if invalid input
// prevents recovery or the tune exceeds playback limits.
[[nodiscard]] Result<Score<Tick>> read(std::string_view text, std::string_view source = "<string>");

// Read the first tune with the same score and diagnostic contract as read().
// Relative header includes and stress profiles may read regular files whose
// canonical paths stay under the score's directory. Unavailable references
// warn; failure to open or read the main file throws std::runtime_error.
[[nodiscard]] Result<Score<Tick>> read_file(const std::filesystem::path& path);

// Encode one score as an ABC 2.1 tune without modifying it. Validate and
// escape UTF-8 text and convert time from the score's unit to the 480-tick playback grid.
// The output does not reconstruct engraving or guarantee identical playback
// in other players. Recoverable losses are in warnings; invalid event values
// or a score that exceeds the export limits throw std::invalid_argument.
// The overloads differ only in their input time unit.
[[nodiscard]] Result<std::string> write(const Score<Tick>& score);
[[nodiscard]] Result<std::string> write(const Score<Quarter>& score);
[[nodiscard]] Result<std::string> write(const Score<Second>& score);

}   // namespace symusic::abc

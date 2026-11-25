#pragma once

#include <functional>
#include <optional>

#include "symusic/io/common.h"
#include "symusic/score.h"

namespace symusic {

// Read a score using its extension or the explicit format. An ABC decoder may
// be supplied by clients that bundle an ABC converter.
template<TType T = Tick, typename S = ValueStorage>
[[nodiscard]] Score<T, S> read_score(
    const std::filesystem::path&                    path,
    std::optional<DataFormat>                       format     = std::nullopt,
    std::function<Score<T, S>(std::span<const u8>)> decode_abc = {}
) {
    switch (format ? *format : score_file_format(path)) {
    case DataFormat::MIDI: return Score<T, S>::from_midi(read_score_file(path));
    case DataFormat::ABC:
        if (decode_abc) return decode_abc(read_score_file(path));
        break;
    default: throw std::invalid_argument("Unknown score format");
    }
    throw std::invalid_argument("An ABC decoder is required to read an ABC score");
}

}   // namespace symusic

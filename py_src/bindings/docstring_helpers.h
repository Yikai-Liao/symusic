//
// Shared helpers for generating nanobind docstrings across Tick/Quarter/Second flavors.
//

#pragma once

#include <string_view>

#include "symusic/time_unit.h"

namespace symusic::docstring_helpers {

struct TimeFlavorInfo {
    std::string_view suffix;
    std::string_view timeline_sentence;
    std::string_view measurement_sentence;
    std::string_view best_for;
    std::string_view scalar_label;
    std::string_view timeline_noun;
    std::string_view tpq_parameter;
};

template<typename T>
constexpr TimeFlavorInfo time_flavor();

template<>
constexpr TimeFlavorInfo time_flavor<Tick>() {
    return TimeFlavorInfo{
        .suffix               = "Tick",
        .timeline_sentence    = "stores timestamps as discrete MIDI ticks so round-trips remain lossless.",
        .measurement_sentence = "Time values are signed 32-bit tick offsets derived from the stored ticks-per-quarter (``tpq``).",
        .best_for             = "lossless MIDI editing or workflows that must match the original resolution",
        .scalar_label         = "int",
        .timeline_noun        = "tick",
        .tpq_parameter        = "Ticks-per-quarter resolution that defines how many integer steps make a quarter note (usually ``480`` or ``960``).",
    };
}

template<>
constexpr TimeFlavorInfo time_flavor<Quarter>() {
    return TimeFlavorInfo{
        .suffix               = "Quarter",
        .timeline_sentence    = "stores timestamps as floating-point quarter-note counts (``1.0`` equals one beat).",
        .measurement_sentence = "Time values are 32-bit floats measured in musical quarter notes relative to the stored ``tpq``.",
        .best_for             = "beat-aware editing, tuplets, swing adjustments, or tempo-relative transforms",
        .scalar_label         = "float",
        .timeline_noun        = "quarter note",
        .tpq_parameter        = "Reference ticks-per-quarter carried from the source score so conversions to ticks stay deterministic.",
    };
}

template<>
constexpr TimeFlavorInfo time_flavor<Second>() {
    return TimeFlavorInfo{
        .suffix               = "Second",
        .timeline_sentence    = "stores timestamps as floating-point seconds aligned with the tempo map.",
        .measurement_sentence = "Time values are 32-bit floats measured in seconds and derived from the tempo/key signatures.",
        .best_for             = "audio alignment, wall-clock automation, or duration-based editing",
        .scalar_label         = "float",
        .timeline_noun        = "second",
        .tpq_parameter        = "Reference ticks-per-quarter retained for precise conversion back to tick or beat timelines.",
    };
}

}   // namespace symusic::docstring_helpers


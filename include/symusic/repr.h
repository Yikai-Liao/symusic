//
// Created by lyk on 23-12-14.
//
#pragma once

#ifndef LIBSYMUSIC_REPR_H
#define LIBSYMUSIC_REPR_H

#include <string>
#include <iostream>

#include "fmt/core.h"
#include "fmt/ranges.h"

#include "symusic/event.h"
#include "symusic/track.h"
#include "symusic/score.h"


// define fmt::formatter for symusic::Note
// support different format for Note
// :d for details Note(time=0.22, duration=0.51, pitch=65, velocity=0.5)
// :s for short Note(0.22, 0.51, 65, 0.5)
// if nothing, use short format, i.e. "{}" is the same as "{:s}"

namespace symusic::details {struct BaseParser{};}

namespace fmt {

#define TAG_FORMAT(__COUNT, NAME)                                                   \
template<>                                                                          \
struct formatter<symusic::NAME> {                                                   \
    constexpr auto parse(const format_parse_context &ctx) { return ctx.begin(); }   \
    template<typename FmtCtx>                                                       \
    constexpr auto format(const symusic::NAME & _, FmtCtx &ctx) const {             \
        return fmt::format_to(ctx.out(), #NAME);                                    \
    }                                                                               \
};

REPEAT_ON(TAG_FORMAT, Tick, Quarter, Second)

#undef TAG_FORMAT

template<>
struct formatter<symusic::DataFormat> {
    static constexpr auto parse(const format_parse_context &ctx) {
        return ctx.begin();
    }

    template<typename FmtCtx>
    constexpr auto format(const symusic::DataFormat &data_format, FmtCtx &ctx) const {
        using symusic::DataFormat;
        // TODO using x-macro to generate this verbose code
        switch (data_format) {
            case DataFormat::MIDI:
                return fmt::format_to(ctx.out(), "MIDI");
            case DataFormat::MusicXML:
                return fmt::format_to(ctx.out(), "MusicXML");
            case DataFormat::ABC:
                return fmt::format_to(ctx.out(), "ABC");
            case DataFormat::ZPP:
                return fmt::format_to(ctx.out(), "ZPP");
            case DataFormat::ALPACA:
                return fmt::format_to(ctx.out(), "ALPACA");
            case DataFormat::CEREAL:
                return fmt::format_to(ctx.out(), "CEREAL");
        }
    }
};

template<>
struct formatter<symusic::details::BaseParser> {
    char presentation = 's';

    constexpr auto parse(const format_parse_context &ctx) {
        auto it = ctx.begin();
        auto const end = ctx.end();
        if (it != end && (*it == 'd' || *it == 's')) {
            presentation = *it++;
        }
        if (it != end && *it != '}') {
            throw format_error("invalid format");
        }
        return it;
    }
};

#define INNER_FORMATTER(                                                            \
    STRUCT_NAME, ARG_NAME, DETAIL_FORMAT, SHORT_FORMAT, ...)                        \
    template<symusic::TType T>                                                      \
    struct formatter<symusic::STRUCT_NAME<T>>:                                      \
        formatter<symusic::details::BaseParser> {                                   \
        template<typename FmtCtx>                                                   \
        auto format(const symusic::STRUCT_NAME<T> &ARG_NAME, FmtCtx &ctx) const {   \
            if (this->presentation == 'd') {                                        \
                return fmt::format_to( ctx.out(), DETAIL_FORMAT, ##__VA_ARGS__ );   \
            } else {                                                                \
                return fmt::format_to( ctx.out(), SHORT_FORMAT, ##__VA_ARGS__ );    \
            }                                                                       \
        }                                                                           \
    };

// this is to make msvc work
#define HELPER(X) X
#define FORMATTER(...) HELPER(INNER_FORMATTER(__VA_ARGS__))

// T is numeric type
template<typename T> requires std::is_arithmetic_v<T>
std::string fix_float(const T value) {
    if constexpr (std::is_same_v<T, float> || std::is_same_v<T, double>) {
        return fmt::format("{:.2f}", value);
    }   return fmt::format("{}", value);
}

FORMATTER(
    Note, d,
    "Note(time={}, duration={}, pitch={}, velocity={})",
    "Note({}, {}, {}, {})",
    fix_float(d.time), fix_float(d.duration), d.pitch, d.velocity
)

FORMATTER(
    Pedal, d,
    "Pedal(time={}, duration={})",
    "Pedal({}, {})",
    fix_float(d.time), d.duration
)

FORMATTER(
    ControlChange, d,
    "ControlChange(time={}, number={}, value={})",
    "ControlChange({}, {}, {})",
    fix_float(d.time), d.number, d.value
)

FORMATTER(
    TimeSignature, d,
    "TimeSignature(time={}, numerator={}, denominator={})",
    "TimeSignature({}, {}, {})",
    fix_float(d.time), d.numerator, d.denominator
)

FORMATTER(
    KeySignature, d,
    "KeySignature(time={}, key={}, tonality={}, degree={})",
    "KeySignature({}, {}, {}, {})",
    fix_float(d.time), d.key, d.tonality, d.degree()
)

FORMATTER(
    Tempo, d,
    "Tempo(time={}, qpm={}, mspq={})",
    "Tempo({}, {}, {})",
    fix_float(d.time), d.qpm(), d.mspq
)

FORMATTER(
    PitchBend, d,
    "PitchBend(time={}, value={})",
    "PitchBend({}, {})",
    fix_float(d.time), d.value
)

FORMATTER(
    TextMeta, d,
    "Text(time={}, text=\"{}\")",
    "Text({}, \"{}\")",
    fix_float(d.time), d.text
)
#undef FORMATTER
#undef INNER_FORMATTER
#undef HELPER

// #undef SUMMARY_FORMAT
}

// Declare ostream operator for all the classes and corresponding vectors

#define OSTREAMEABLE(__COUNT, STRUCT_NAME)                                              \
    template<symusic::TType T>                                                          \
    std::ostream &operator<<(std::ostream &os, const symusic::STRUCT_NAME<T> &data);

REPEAT_ON(
    OSTREAMEABLE,
    Note,
    Pedal,
    ControlChange,
    TimeSignature,
    KeySignature,
    Tempo,
    PitchBend,
    TextMeta,
    Track,
    Score
)
#undef OSTREAMEABLE

// #define OSTREAMEABLE(__COUNT, STRUCT_NAME)                                              \
//     template<symusic::TType T>                                                          \
//     std::ostream &operator<<(                                                           \
//         std::ostream &os, const std::vector<symusic::STRUCT_NAME<T>> &data              \
//     ) { return os << fmt::format("{}", data); }
//
//
// REPEAT_ON(
//     OSTREAMEABLE,
//     Note,
//     Pedal,
//     ControlChange,
//     TimeSignature,
//     KeySignature,
//     Tempo,
//     PitchBend,
//     TextMeta,
//     Track
// )
// #undef OSTREAMEABLE

// define a base formatter with parse that
#endif //LIBSYMUSIC_REPR_H

#pragma once

#include <string>
#include <ostream>

#include "fmt/core.h"
#include "fmt/ranges.h"

#include "symusic/event.h"
#include "symusic/detail/event_fields.h"
#include "symusic/track.h"
#include "symusic/score.h"

// define fmt::formatter for symusic::Note
// support different format for Note
// :d for details Note(time=0.22, duration=0.51, pitch=65, velocity=0.5)
// :s for short Note(0.22, 0.51, 65, 0.5)
// if nothing, use short format, i.e. "{}" is the same as "{:s}"

namespace symusic::detail {
struct BaseParser {};
}   // namespace symusic::detail

namespace fmt {

template<symusic::TType T>
struct formatter<T> {
    constexpr auto parse(const format_parse_context& ctx) { return ctx.begin(); }

    template<typename FmtCtx>
    auto format(T, FmtCtx& ctx) const {
        if constexpr (std::same_as<T, symusic::Tick>) return fmt::format_to(ctx.out(), "Tick");
        if constexpr (std::same_as<T, symusic::Quarter>)
            return fmt::format_to(ctx.out(), "Quarter");
        if constexpr (std::same_as<T, symusic::Second>) return fmt::format_to(ctx.out(), "Second");
    }
};

template<>
struct formatter<symusic::DataFormat> {
    static constexpr auto parse(const format_parse_context& ctx) { return ctx.begin(); }

    template<typename FmtCtx>
    constexpr auto format(const symusic::DataFormat& data_format, FmtCtx& ctx) const {
        using symusic::DataFormat;
        // TODO using x-macro to generate this verbose code
        switch (data_format) {
        case DataFormat::MIDI: return fmt::format_to(ctx.out(), "MIDI");
        case DataFormat::MusicXML: return fmt::format_to(ctx.out(), "MusicXML");
        case DataFormat::ABC: return fmt::format_to(ctx.out(), "ABC");
        case DataFormat::ZPP: return fmt::format_to(ctx.out(), "ZPP");
        case DataFormat::ALPACA: return fmt::format_to(ctx.out(), "ALPACA");
        case DataFormat::CEREAL: return fmt::format_to(ctx.out(), "CEREAL");
        }
    }
};

template<>
struct formatter<symusic::detail::BaseParser> {
    char presentation = 's';

    constexpr auto parse(const format_parse_context& ctx) {
        auto       it  = ctx.begin();
        auto const end = ctx.end();
        if (it != end && (*it == 'd' || *it == 's')) { presentation = *it++; }
        if (it != end && *it != '}') { throw format_error("invalid format"); }
        return it;
    }
};

template<symusic::TimeEvent E>
struct formatter<E> : formatter<symusic::detail::BaseParser> {
    template<typename FmtCtx>
    auto format(const E& event, FmtCtx& ctx) const {
        using T              = typename E::ttype;
        constexpr auto name  = symusic::detail::event_name<E>;
        auto           out   = fmt::format_to(ctx.out(), "{}(", name);
        bool           first = true;
        auto           field = [&](std::string_view label, const auto& value) {
            if (!first) out = fmt::format_to(out, ", ");
            first = false;
            if (this->presentation == 'd') out = fmt::format_to(out, "{}=", label);
            using V = std::remove_cvref_t<decltype(value)>;
            if constexpr (std::is_same_v<V, std::string>) {
                out = fmt::format_to(out, "'{}'", value);
            } else if constexpr (std::is_floating_point_v<V>) {
                if (label == "time" || (std::same_as<E, symusic::Note<T>> && label == "duration")) {
                    out = fmt::format_to(out, "{:.2f}", value);
                } else {
                    out = fmt::format_to(out, "{}", value);
                }
            } else {
                out = fmt::format_to(out, "{}", value);
            }
        };
        symusic::detail::for_event_field<E>([&](std::string_view label, auto member) {
            if constexpr (std::same_as<E, symusic::Tempo<T>>) {
                if (label == "mspq") field("qpm", event.qpm());
            }
            field(label, event.*member);
        });
        if constexpr (std::same_as<E, symusic::KeySignature<T>>) field("degree", event.degree());
        return this->presentation == 'd' ? fmt::format_to(out, ", ttype='{}')", T{}) :
                                           fmt::format_to(out, ", '{}')", T{});
    }
};

template<typename T>
struct formatter<std::shared_ptr<T>> {
    constexpr auto parse(format_parse_context& ctx) { return ctx.begin(); }

    template<typename FmtCtx>
    auto format(const std::shared_ptr<T>& data, FmtCtx& ctx) {
        if (data) { return fmt::format_to(ctx.out(), "{}", *data); }
        throw std::runtime_error("symusic: trying to format a nullptr");
    }
};

// #undef SUMMARY_FORMAT
}   // namespace fmt

template<symusic::TimeEvent E>
std::ostream& operator<<(std::ostream& out, const E& event) {
    return out << event.to_string();
}

template<symusic::TType T, typename S>
std::ostream& operator<<(std::ostream& out, const symusic::Track<T, S>& track) {
    return out << track.to_string();
}

template<symusic::TType T, typename S>
std::ostream& operator<<(std::ostream& out, const symusic::Score<T, S>& score) {
    return out << score.to_string();
}

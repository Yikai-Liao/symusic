#pragma once

#include <span>

#include "symusic/mtype.h"
#include "symusic/time_unit.h"
namespace symusic {

// define a enum for data formats
enum class DataFormat {
    MIDI,       // MIDI,            https://en.wikipedia.org/wiki/MIDI
    MusicXML,   // MusicXML,        https://en.wikipedia.org/wiki/MusicXML
    ABC,        // ABC Notation,    https://en.wikipedia.org/wiki/ABC_notation
    ZPP,        // zpp_bits, c++20, customised binary format, https://github.com/eyalz800/zpp_bits
    ALPACA,     // alpaca,   c++17, customised binary format, https://github.com/p-ranav/alpaca
    CEREAL,     // cereal,   c++11, customised binary format, https://github.com/USCiLab/cereal
};

template<TType T, typename S>
struct Score;

namespace detail {
template<typename T>
inline constexpr bool is_score = false;

template<TType T, typename S>
inline constexpr bool is_score<Score<T, S>> = true;

template<DataFormat F, typename T>
concept HasMemberParse = requires(std::span<const u8> bytes) { T::template parse<F>(bytes); };

template<DataFormat F, typename T>
concept HasMemberDumps = requires(const T& data) { data.template dumps<F>(); };
}   // namespace detail

template<DataFormat F, typename T>
    requires(detail::HasMemberParse<F, T> || !detail::is_score<T>)
[[nodiscard]] T parse(std::span<const u8> bytes);

template<DataFormat F, typename T>
    requires detail::HasMemberParse<F, T>
[[nodiscard]] T parse(std::span<const u8> bytes) {
    return T::template parse<F>(bytes);
}

/**
 * Parse raw input bytes, optionally sanitizing payload values to stay within the
 * MIDI value range enforced by minimidi.
 *
 * @param bytes Input MIDI bytes.
 * @param sanitize_data Clamp payload bytes to the 7-bit MIDI range before parsing.
 */
template<DataFormat F, typename T>
    requires(F == DataFormat::MIDI)
[[nodiscard]] T parse(std::span<const u8> bytes, bool sanitize_data) {
    return T::parse_midi_bytes(bytes, sanitize_data);
}

template<DataFormat F, typename T>
    requires(detail::HasMemberDumps<F, T> || !detail::is_score<T>)
[[nodiscard]] vec<u8> dumps(const T& data);

template<DataFormat F, typename T>
    requires detail::HasMemberDumps<F, T>
[[nodiscard]] vec<u8> dumps(const T& data) {
    return data.template dumps<F>();
}

}   // namespace symusic

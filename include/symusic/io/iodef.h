#pragma once

#include <span>

#include "symusic/mtype.h"
#include "symusic/time_unit.h"
namespace symusic {

// define a enum for data formats
enum class DataFormat {
    MIDI,       // MIDI,            https://en.wikipedia.org/wiki/MIDI
    ABC,        // ABC Notation,    https://en.wikipedia.org/wiki/ABC_notation
};

// Encode and decode the native binary representation used by pickle.
template<typename T>
[[nodiscard]] T deserialize(std::span<const u8> bytes);

template<typename T>
    requires requires(std::span<const u8> bytes) { T::deserialize(bytes); }
[[nodiscard]] T deserialize(std::span<const u8> bytes) { return T::deserialize(bytes); }

template<typename T>
[[nodiscard]] vec<u8> serialize(const T& data);

template<typename T>
    requires requires(const T& data) { data.serialize(); }
[[nodiscard]] vec<u8> serialize(const T& data) { return data.serialize(); }

}   // namespace symusic

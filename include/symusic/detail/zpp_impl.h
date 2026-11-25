#pragma once

#include <array>
#include <cstdint>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <span>

#include "zpp_bits.h"
#include "symusic/detail/event_fields.h"
#include "symusic/event.h"
#include "symusic/track.h"
#include "symusic/score.h"
#include "symusic/conversion.h"

// zpp_bits finds these overloads by ADL on the event type, so they live in
// namespace symusic. That also marks the types as having an explicit
// serializer, which keeps zpp_bits from decomposing them with structured
// bindings (ill-formed: the events inherit data members from TimeStamp).
namespace symusic {
// Events are written as their raw object bytes, so the wire format is
// `sizeof(event)` bytes per event. Padding bytes of a copied or constructed
// struct are indeterminate, so the output path first assembles the image in a
// zero-filled buffer and copies each member to its offset. Invariant: the bytes
// written depend only on member values, and padding is always zero. The input
// path reads the image directly and ignores padding.
template<class E, class... Members>
constexpr auto serialize_zeroed_padding(auto& archive, const E& data, const Members&... members) {
    alignas(E) std::array<std::byte, sizeof(E)> image{};
    const auto*                                 base = reinterpret_cast<const std::byte*>(&data);
    (std::memcpy(
         image.data() + (reinterpret_cast<const std::byte*>(&members) - base),
         &members,
         sizeof(Members)
     ),
     ...);
    return archive(zpp::bits::as_bytes(image));
}

template<TimeEvent E>
    requires(std::is_trivially_copyable_v<E> && !std::is_const_v<E>)
constexpr auto serialize(auto& archive, E& data) {
    return archive(zpp::bits::as_bytes(data));
}

template<TimeEvent E>
    requires std::is_trivially_copyable_v<E>
constexpr auto serialize(auto& archive, const E& data) {
    return std::apply(
        [&](const auto&... fields) {
            return serialize_zeroed_padding(archive, data, (data.*fields.second)...);
        },
        detail::event_fields<E>
    );
}

// Arithmetic event members occupy the leading bytes of each wire image.
template<typename E>
inline constexpr size_t padded_payload = 0;

template<TimeEvent E>
    requires std::is_trivially_copyable_v<E>
inline constexpr size_t padded_payload<E> = [] {
    const auto payload = std::apply(
        [](const auto&... fields) { return (sizeof(std::declval<E>().*fields.second) + ...); },
        detail::event_fields<E>
    );
    return payload == sizeof(E) ? 0 : payload;
}();

using ZppSize = zpp::bits::default_size_type;

inline ZppSize checked_zpp_size(size_t count) {
    if (count > std::numeric_limits<ZppSize>::max()) {
        throw std::overflow_error("ZPP container size exceeds the format limit");
    }
    return static_cast<ZppSize>(count);
}

// Minimum wire sizes bound allocations before zpp_bits reads container contents.
template<class E>
inline constexpr size_t min_zpp_size = sizeof(E);
template<TType T>
inline constexpr size_t min_zpp_size<TextMeta<T>> = sizeof(typename T::unit) + sizeof(ZppSize);
template<TType T>
inline constexpr size_t min_zpp_size<Track<T>> = 2 * sizeof(u8) + 6 * sizeof(ZppSize);
template<TType T>
inline constexpr size_t min_zpp_size<Score<T>> = sizeof(i32) + 5 * sizeof(ZppSize);

template<class Container>
struct ZppContainer {
    Container& values;
};

template<class Container>
constexpr zpp::bits::errc serialize(auto& archive, const ZppContainer<Container>& wrapped) {
    using E      = typename Container::value_type;
    auto& values = wrapped.values;
    if constexpr (std::is_const_v<Container>) {
        const auto count = checked_zpp_size(values.size());
        if constexpr (!std::ranges::contiguous_range<Container>) {
            auto result = archive(count);
            if (zpp::bits::failure(result)) return result;
            for (const auto& event : values) {
                result = archive(event);
                if (zpp::bits::failure(result)) return result;
            }
            return result;
        } else if constexpr (padded_payload<E> != 0) {
            constexpr size_t payload = padded_payload<E>;
            static_assert(sizeof(E) - payload == 2, "padding is a 2-byte tail");
            auto result = archive(count, zpp::bits::bytes(std::span<const E>(values)));
            if (zpp::bits::failure(result) || count == 0) return result;

            // A successful bulk write includes count complete event images at the end.
            auto images = archive.processed_data().last(std::span<const E>(values).size_bytes());
            for (size_t index = 0; index < count; ++index) {
                constexpr u16 kZero = 0;
                std::memcpy(images.data() + index * sizeof(E) + payload, &kZero, sizeof(kZero));
            }
            return result;
        } else {
            return archive(count, zpp::bits::unsized(values));
        }
    } else {
        ZppSize count{};
        if (auto result = archive(count); zpp::bits::failure(result)) return result;
        if (count > archive.remaining_data().size() / min_zpp_size<E>) {
            return std::errc::result_out_of_range;
        }
        values.resize(count);
        return archive(zpp::bits::unsized(values));
    }
}

template<typename Member>
constexpr decltype(auto) zpp_field(Member& value) {
    if constexpr (std::same_as<std::remove_const_t<Member>, std::string>) {
        return ZppContainer{value};
    } else {
        return (value);
    }
}

template<typename E>
    requires TimeEvent<std::remove_cv_t<E>> && (!std::is_trivially_copyable_v<E>)
constexpr auto serialize(auto& archive, E& data) {
    return std::apply(
        [&](const auto&... fields) { return archive(zpp_field(data.*fields.second)...); },
        detail::event_fields<std::remove_cv_t<E>>
    );
}

template<typename S, typename Track>
constexpr auto serialize_track(auto& archive, Track& data, auto& drum_flag) {
    return archive(
        ZppContainer{data.name},
        data.program,
        drum_flag,
        ZppContainer{S::get(data.notes)},
        ZppContainer{S::get(data.controls)},
        ZppContainer{S::get(data.pitch_bends)},
        ZppContainer{S::get(data.pedals)},
        ZppContainer{S::get(data.lyrics)}
    );
}

template<TType T, typename S>
constexpr auto serialize(auto& archive, const Track<T, S>& data) {
    const u8 drum_flag = data.is_drum;
    return serialize_track<S>(archive, data, drum_flag);
}

template<TType T>
constexpr zpp::bits::errc serialize(auto& archive, Track<T>& data) {
    u8   drum_flag{};
    auto result = serialize_track<ValueStorage>(archive, data, drum_flag);
    if (zpp::bits::failure(result)) return result;
    if (drum_flag > 1) return std::errc::invalid_argument;
    data.is_drum = drum_flag != 0;
    return result;
}

template<class S, class Range>
struct ZppTrackRange {
    const Range& tracks;
};

template<class S, class Range>
constexpr zpp::bits::errc serialize(auto& archive, const ZppTrackRange<S, Range>& data) {
    auto result = archive(checked_zpp_size(data.tracks.size()));
    if (zpp::bits::failure(result)) return result;
    for (const auto& track : data.tracks) {
        result = archive(S::get(track));
        if (zpp::bits::failure(result)) return result;
    }
    return result;
}

template<TType T, typename S>
constexpr auto serialize(auto& archive, const Score<T, S>& data) {
    using Range = std::remove_cvref_t<decltype(S::get(data.tracks))>;
    return archive(
        data.ticks_per_quarter,
        ZppTrackRange<S, Range>{S::get(data.tracks)},
        ZppContainer{S::get(data.time_signatures)},
        ZppContainer{S::get(data.key_signatures)},
        ZppContainer{S::get(data.tempos)},
        ZppContainer{S::get(data.markers)}
    );
}

template<TType T>
constexpr auto serialize(auto& archive, Score<T>& data) {
    return archive(
        data.ticks_per_quarter,
        ZppContainer{data.tracks},
        ZppContainer{data.time_signatures},
        ZppContainer{data.key_signatures},
        ZppContainer{data.tempos},
        ZppContainer{data.markers}
    );
}

}   // namespace symusic

namespace symusic {

namespace detail {

inline constexpr std::array<u8, 4> kZppMagic{'S', 'Y', 'M', 'Z'};
inline constexpr u8                kZppVersion = 1;

template<class T>
auto archive_zpp_state(auto& archive, T& data) {
    if constexpr (requires { typename T::value_type; }) {
        return archive(ZppContainer{data});
    } else {
        return archive(data);
    }
}

template<class T>
vec<u8> serialize_zpp(const T& data) {
    vec<u8> buffer;
    auto    out = zpp::bits::out(buffer);
    out(kZppMagic, kZppVersion).or_throw();
    archive_zpp_state(out, data).or_throw();
    return buffer;
}

template<class T>
T deserialize_zpp(std::span<const u8> buffer) {
    auto                             in = zpp::bits::in(buffer);
    std::array<u8, kZppMagic.size()> magic{};
    u8                               version{};
    if (zpp::bits::failure(in(magic, version)) || magic != kZppMagic) {
        throw std::invalid_argument(
            "Invalid ZPP pickle header (unversioned or malformed, expected version "
            + std::to_string(kZppVersion) + ")"
        );
    }
    if (version != kZppVersion) {
        throw std::invalid_argument(
            "ZPP pickle version " + std::to_string(version) + " does not match expected version "
            + std::to_string(kZppVersion)
        );
    }
    T data{};
    if (zpp::bits::failure(archive_zpp_state(in, data)) || in.position() != buffer.size()) {
        throw std::invalid_argument("Invalid ZPP pickle payload");
    }
    return data;
}

}   // namespace detail

template<TType T, typename S>
vec<u8> Track<T, S>::dump_zpp_bytes() const {
    return detail::serialize_zpp(*this);
}

template<TType T, typename S>
Track<T, S> Track<T, S>::parse_zpp_bytes(std::span<const u8> bytes) {
    return detail::adopt_storage<S>(detail::deserialize_zpp<Track<T>>(bytes));
}

template<TType T, typename S>
vec<u8> Score<T, S>::dump_zpp_bytes() const {
    return detail::serialize_zpp(*this);
}

template<TType T, typename S>
Score<T, S> Score<T, S>::parse_zpp_bytes(std::span<const u8> bytes) {
    return detail::adopt_storage<S>(detail::deserialize_zpp<Score<T>>(bytes));
}

namespace detail {

template<TType T, typename S>
vec<u8> dump_track_list(const typename S::template Tracks<Track<T, S>>& tracks) {
    using Range = std::remove_cvref_t<decltype(S::get(tracks))>;
    return detail::serialize_zpp(ZppTrackRange<S, Range>{S::get(tracks)});
}

}   // namespace detail

}   // namespace symusic

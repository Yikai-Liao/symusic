#include <array>
#include <cstdint>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <span>

#include "zpp_bits.h"
#include "MetaMacro.h"
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

#define SERIALIZE_NON_HEAP(__COUNT, NAME)                             \
    template<symusic::TType T>                                        \
    constexpr auto serialize(auto& archive, symusic::NAME<T>& data) { \
        return archive(zpp::bits::as_bytes(data));                    \
    }

REPEAT_ON(
    SERIALIZE_NON_HEAP, Note, Pedal, ControlChange, TimeSignature, KeySignature, Tempo, PitchBend
)

#undef SERIALIZE_NON_HEAP

// Output overloads. Pedal, Tempo and PitchBend have no padding (two 4-byte
// members), so their raw bytes are already deterministic.
#define SERIALIZE_PACKED_OUT(NAME)                                          \
    template<symusic::TType T>                                              \
    constexpr auto serialize(auto& archive, const symusic::NAME<T>& data) { \
        return archive(zpp::bits::as_bytes(data));                          \
    }
SERIALIZE_PACKED_OUT(Pedal)
SERIALIZE_PACKED_OUT(Tempo)
SERIALIZE_PACKED_OUT(PitchBend)
#undef SERIALIZE_PACKED_OUT

template<symusic::TType T>
constexpr auto serialize(auto& archive, const symusic::Note<T>& d) {
    return serialize_zeroed_padding(archive, d, d.time, d.duration, d.pitch, d.velocity);
}

template<symusic::TType T>
constexpr auto serialize(auto& archive, const symusic::ControlChange<T>& d) {
    return serialize_zeroed_padding(archive, d, d.time, d.number, d.value);
}

template<symusic::TType T>
constexpr auto serialize(auto& archive, const symusic::TimeSignature<T>& d) {
    return serialize_zeroed_padding(archive, d, d.time, d.numerator, d.denominator);
}

template<symusic::TType T>
constexpr auto serialize(auto& archive, const symusic::KeySignature<T>& d) {
    return serialize_zeroed_padding(archive, d, d.time, d.key, d.tonality);
}

// Per-element serialization of a whole `vec<E>` costs one archive call per
// event, several times slower than the bulk copy zpp_bits uses for plain
// trivially copyable elements. `zeroed(vec)` restores a bulk path for output:
// it writes the same `u32 size` prefix and element images as zpp_bits would,
// then zeroes the padding tail of each image inside the output buffer. No
// scratch copy is made, because allocating one per call dominated the cost.
// `payload` is the number of leading bytes that hold members; the tail is two
// bytes, which a single constant store clears (a memset call per event did not
// inline and cost more than the copy).
template<class E>
inline constexpr std::size_t padded_payload = 0;
template<class T>
inline constexpr std::size_t padded_payload<Note<T>> = 2 * sizeof(typename T::unit) + 2;
template<class T>
inline constexpr std::size_t padded_payload<ControlChange<T>> = sizeof(typename T::unit) + 2;
template<class T>
inline constexpr std::size_t padded_payload<TimeSignature<T>> = sizeof(typename T::unit) + 2;
template<class T>
inline constexpr std::size_t padded_payload<KeySignature<T>> = sizeof(typename T::unit) + 2;

using ZppSize = zpp::bits::default_size_type;

ZppSize checked_zpp_size(size_t count) {
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
inline constexpr size_t min_zpp_size<TrackNative<T>> = 2 * sizeof(u8) + 6 * sizeof(ZppSize);

template<class Container>
struct ZppContainer {
    Container& values;
};

template<class Container>
constexpr zpp::bits::errc serialize(auto& archive, const ZppContainer<Container>& wrapped) {
    using E = typename Container::value_type;
    auto& values = wrapped.values;
    if constexpr (std::is_const_v<Container>) {
        const auto count = checked_zpp_size(values.size());
        if constexpr (padded_payload<E> != 0) {
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

template<TType T>
constexpr auto serialize(auto& archive, const TextMeta<T>& data) {
    return archive(data.time, ZppContainer{data.text});
}

template<TType T>
constexpr auto serialize(auto& archive, TextMeta<T>& data) {
    return archive(data.time, ZppContainer{data.text});
}

template<class Track>
constexpr auto serialize_track(auto& archive, Track& data, auto& drum_flag) {
    return archive(
        ZppContainer{data.name}, data.program, drum_flag,
        ZppContainer{data.notes}, ZppContainer{data.controls}, ZppContainer{data.pitch_bends},
        ZppContainer{data.pedals}, ZppContainer{data.lyrics}
    );
}

template<TType T>
constexpr auto serialize(auto& archive, const TrackNative<T>& data) {
    const u8 drum_flag = data.is_drum;
    return serialize_track(archive, data, drum_flag);
}

template<TType T>
constexpr zpp::bits::errc serialize(auto& archive, TrackNative<T>& data) {
    u8 drum_flag{};
    auto result = serialize_track(archive, data, drum_flag);
    if (zpp::bits::failure(result)) return result;
    if (drum_flag > 1) return std::errc::invalid_argument;
    data.is_drum = drum_flag != 0;
    return result;
}

template<class Score>
constexpr auto serialize_score(auto& archive, Score& data) {
    return archive(
        data.ticks_per_quarter, ZppContainer{data.tracks}, ZppContainer{data.time_signatures},
        ZppContainer{data.key_signatures}, ZppContainer{data.tempos}, ZppContainer{data.markers}
    );
}

template<TType T>
constexpr auto serialize(auto& archive, const ScoreNative<T>& data) {
    return serialize_score(archive, data);
}

template<TType T>
constexpr auto serialize(auto& archive, ScoreNative<T>& data) {
    return serialize_score(archive, data);
}

}   // namespace symusic

namespace symusic {

namespace details {

constexpr std::array<u8, 4> kZppMagic{'S', 'Y', 'M', 'Z'};
constexpr u8 kZppVersion = 1;

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
    auto out = zpp::bits::out(buffer);
    out(kZppMagic, kZppVersion).or_throw();
    archive_zpp_state(out, data).or_throw();
    return buffer;
}

template<class T>
T deserialize_zpp(std::span<const u8> buffer) {
    auto in = zpp::bits::in(buffer);
    std::array<u8, kZppMagic.size()> magic{};
    u8 version{};
    if (zpp::bits::failure(in(magic, version)) || magic != kZppMagic) {
        throw std::invalid_argument("Invalid ZPP pickle header");
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

}   // namespace details

#define INSTANTIATE_ZPP_INNER(NAME, T)                                   \
    template<>                                                           \
    template<>                                                           \
    vec<u8> NAME<T>::dumps<DataFormat::ZPP>() const {                    \
        return details::serialize_zpp(*this);                            \
    }                                                                    \
    template<>                                                           \
    template<>                                                           \
    NAME<T> NAME<T>::parse<DataFormat::ZPP>(std::span<const u8> bytes) { \
        return details::deserialize_zpp<NAME<T>>(bytes);                 \
    }                                                                    \
    template<>                                                           \
    vec<u8> dumps<DataFormat::ZPP>(const NAME<T>& data) {                \
        return details::serialize_zpp(data);                             \
    }                                                                    \
    template<>                                                           \
    NAME<T> parse<DataFormat::ZPP>(std::span<const u8> bytes) {          \
        return details::deserialize_zpp<NAME<T>>(bytes);                 \
    }                                                                    \
    template<>                                                           \
    vec<u8> dumps<DataFormat::ZPP>(const vec<NAME<T>>& data) {           \
        return details::serialize_zpp(data);                             \
    }                                                                    \
    template<>                                                           \
    vec<NAME<T>> parse<DataFormat::ZPP>(std::span<const u8> bytes) {     \
        return details::deserialize_zpp<vec<NAME<T>>>(bytes);            \
    }

#define INSTANTIATE_ZPP(__COUNT, NAME)   \
    INSTANTIATE_ZPP_INNER(NAME, Tick)    \
    INSTANTIATE_ZPP_INNER(NAME, Quarter) \
    INSTANTIATE_ZPP_INNER(NAME, Second)

REPEAT_ON(
    INSTANTIATE_ZPP,
    Note,
    Pedal,
    ControlChange,
    TimeSignature,
    KeySignature,
    Tempo,
    PitchBend,
    TextMeta,
    TrackNative,
    ScoreNative
)

#undef INSTANTIATE_ZPP_INNER

// for Score and Track
#define INSTANTIATE_ZPP_INNER(NAME, T)                                                 \
    template<>                                                                         \
    template<>                                                                         \
    vec<u8> NAME<T>::dumps<DataFormat::ZPP>() const {                                  \
        return details::serialize_zpp(to_native(*this));                               \
    }                                                                                  \
    template<>                                                                         \
    template<>                                                                         \
    NAME<T> NAME<T>::parse<DataFormat::ZPP>(std::span<const u8> bytes) {               \
        return to_shared(std::move(details::deserialize_zpp<NAME##Native<T>>(bytes))); \
    }                                                                                  \
    template<>                                                                         \
    vec<u8> dumps<DataFormat::ZPP>(const NAME<T>& data) {                              \
        return data.dumps<DataFormat::ZPP>();                                          \
    }                                                                                  \
    template<>                                                                         \
    NAME<T> parse<DataFormat::ZPP>(std::span<const u8> bytes) {                        \
        return NAME<T>::parse<DataFormat::ZPP>(bytes);                                 \
    }

REPEAT_ON(INSTANTIATE_ZPP, Track, Score)

#undef INSTANTIATE_ZPP_INNER
#undef INSTANTIATE_ZPP

}   // namespace symusic

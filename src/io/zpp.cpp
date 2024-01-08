//
// Created by lyk on 23-12-25.
//
#include <span>

#include "zpp_bits.h"

#include "symusic/io/iodef.h"
#include "symusic/event.h"
#include "symusic/track.h"
#include "symusic/score.h"
// #include "symusic/note_arr.h"

namespace zpp::bits {
#define SERIALIZE_NON_HEAP(__COUNT, NAME)                                       \
    template<symusic::TType T>                                           \
    constexpr auto serialize(auto & archive, const symusic::NAME<T> & data) {   \
        return archive(zpp::bits::as_bytes(data));                              \
    }                                                                           \
    template<symusic::TType T>                                           \
    constexpr auto serialize(auto & archive, symusic::NAME<T> & data) {         \
        return archive(zpp::bits::as_bytes(data));                              \
    }

REPEAT_ON(
    SERIALIZE_NON_HEAP,
    Note,
    Pedal,
    ControlChange,
    TimeSignature,
    KeySignature,
    Tempo,
    PitchBend
)

#undef SERIALIZE_NON_HEAP

#define HELPER(X) X
#define SERIALIZE_COMMON(...) HELPER(SERIALIZE_COMMON_INNER(__VA_ARGS__))
#define SERIALIZE_COMMON_INNER(STRUCT, DATA, ...)                               \
    template<symusic::TType T>                                                  \
    constexpr auto serialize(auto & archive, const symusic::STRUCT<T> & DATA) { \
        return archive(__VA_ARGS__);                                            \
    }                                                                           \
    template<symusic::TType T>                                                  \
    constexpr auto serialize(auto & archive, symusic::STRUCT<T> & DATA) {       \
        return archive(__VA_ARGS__);                                            \
    }


SERIALIZE_COMMON(TextMeta, d, d.time, d.text)

// SERIALIZE_COMMON(
//     NoteArr, d,
//     d.name, d.program, d.is_drum,
//     d.time, d.duration, d.pitch, d.velocity
// )

SERIALIZE_COMMON(
    Track, d,
    d.name, d.program, d.is_drum,
    d.notes, d.controls, d.pitch_bends, d.pedals
)

SERIALIZE_COMMON(
    Score, d,
    d.ticks_per_quarter, d.tracks, d.time_signatures,
    d.key_signatures, d.tempos, d.lyrics, d.markers
)

#undef SERIALIZE_COMMON
#undef SERIALIZE_COMMON_INNER
#undef HELPER

}

namespace symusic {

namespace details {

template<class T>
vec<u8> serailize_zpp(const T & data) {
    vec<u8> buffer;
    auto out = zpp::bits::out(buffer);
    out(data).or_throw();
    return buffer;
}

template<class T>
T deserailize_zpp(std::span<const u8> buffer){
    auto in = zpp::bits::in(buffer);
    T data{};
    in(data).or_throw();
    return data;
}

} // namespace details

#define INSTANTIATE_ZPP_INNER(NAME, T)                                      \
    template<> template<>                                                   \
    vec<u8> NAME<T>::dumps<DataFormat::ZPP>() const {                       \
        return details::serailize_zpp(*this);                               \
    }                                                                       \
    template<> template<>                                                   \
    NAME<T> NAME<T>::parse<DataFormat::ZPP>(std::span<const u8> bytes) {    \
        return details::deserailize_zpp<NAME<T>>(bytes);                    \
    }                                                                       \
    template<>                                                              \
    vec<u8> dumps<DataFormat::ZPP>(const NAME<T> & data) {                  \
        return details::serailize_zpp(data);                                \
    }                                                                       \
    template<>                                                              \
    NAME<T> parse<DataFormat::ZPP>(std::span<const u8> bytes) {             \
        return details::deserailize_zpp<NAME<T>>(bytes);                    \
    }                                                                       \
    template<>                                                              \
    vec<u8> dumps<DataFormat::ZPP>(const vec<NAME<T>> & data) {             \
        return details::serailize_zpp(data);                                \
    }                                                                       \
    template<>                                                              \
    vec<NAME<T>> parse<DataFormat::ZPP>(std::span<const u8> bytes) {        \
        return details::deserailize_zpp<vec<NAME<T>>>(bytes);               \
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
    // NoteArr,
    Track,
    Score
)
} // namespace symusic

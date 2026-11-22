#include "symusic/detail/zpp_impl.h"
#include "MetaMacro.h"

namespace symusic {

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
    TextMeta
)

#undef INSTANTIATE_ZPP_INNER
#undef INSTANTIATE_ZPP

template vec<u8> Track<Tick, ValueStorage>::dump_zpp_bytes() const;
template Track<Tick, ValueStorage> Track<Tick, ValueStorage>::parse_zpp_bytes(std::span<const u8>);
template vec<u8> Track<Quarter, ValueStorage>::dump_zpp_bytes() const;
template Track<Quarter, ValueStorage> Track<Quarter, ValueStorage>::parse_zpp_bytes(std::span<const u8>);
template vec<u8> Track<Second, ValueStorage>::dump_zpp_bytes() const;
template Track<Second, ValueStorage> Track<Second, ValueStorage>::parse_zpp_bytes(std::span<const u8>);
template vec<u8> Score<Tick, ValueStorage>::dump_zpp_bytes() const;
template Score<Tick, ValueStorage> Score<Tick, ValueStorage>::parse_zpp_bytes(std::span<const u8>);
template vec<u8> Score<Quarter, ValueStorage>::dump_zpp_bytes() const;
template Score<Quarter, ValueStorage> Score<Quarter, ValueStorage>::parse_zpp_bytes(std::span<const u8>);
template vec<u8> Score<Second, ValueStorage>::dump_zpp_bytes() const;
template Score<Second, ValueStorage> Score<Second, ValueStorage>::parse_zpp_bytes(std::span<const u8>);
template<> vec<u8> dumps<DataFormat::ZPP>(const vec<Track<Tick>>& data) { return details::serialize_zpp(data); }
template<> vec<Track<Tick>> parse<DataFormat::ZPP>(std::span<const u8> data) { return details::deserialize_zpp<vec<Track<Tick>>>(data); }
template<> vec<u8> dumps<DataFormat::ZPP>(const vec<Score<Tick>>& data) { return details::serialize_zpp(data); }
template<> vec<Score<Tick>> parse<DataFormat::ZPP>(std::span<const u8> data) { return details::deserialize_zpp<vec<Score<Tick>>>(data); }
template<> vec<u8> dumps<DataFormat::ZPP>(const vec<Track<Quarter>>& data) { return details::serialize_zpp(data); }
template<> vec<Track<Quarter>> parse<DataFormat::ZPP>(std::span<const u8> data) { return details::deserialize_zpp<vec<Track<Quarter>>>(data); }
template<> vec<u8> dumps<DataFormat::ZPP>(const vec<Score<Quarter>>& data) { return details::serialize_zpp(data); }
template<> vec<Score<Quarter>> parse<DataFormat::ZPP>(std::span<const u8> data) { return details::deserialize_zpp<vec<Score<Quarter>>>(data); }
template<> vec<u8> dumps<DataFormat::ZPP>(const vec<Track<Second>>& data) { return details::serialize_zpp(data); }
template<> vec<Track<Second>> parse<DataFormat::ZPP>(std::span<const u8> data) { return details::deserialize_zpp<vec<Track<Second>>>(data); }
template<> vec<u8> dumps<DataFormat::ZPP>(const vec<Score<Second>>& data) { return details::serialize_zpp(data); }
template<> vec<Score<Second>> parse<DataFormat::ZPP>(std::span<const u8> data) { return details::deserialize_zpp<vec<Score<Second>>>(data); }

}  // namespace symusic

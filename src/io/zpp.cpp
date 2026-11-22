#include "symusic/detail/zpp_impl.h"

namespace symusic {

template<>
template<>
vec<u8> Note<Tick>::dumps<DataFormat::ZPP>() const {
    return detail::serialize_zpp(*this);
}

template<>
template<>
Note<Tick> Note<Tick>::parse<DataFormat::ZPP>(std::span<const u8> bytes) {
    return detail::deserialize_zpp<Note<Tick>>(bytes);
}

template<>
vec<u8> dumps<DataFormat::ZPP>(const vec<Note<Tick>>& data) {
    return detail::serialize_zpp(data);
}

template<>
vec<Note<Tick>> parse<DataFormat::ZPP>(std::span<const u8> bytes) {
    return detail::deserialize_zpp<vec<Note<Tick>>>(bytes);
}

template<>
template<>
vec<u8> Note<Quarter>::dumps<DataFormat::ZPP>() const {
    return detail::serialize_zpp(*this);
}

template<>
template<>
Note<Quarter> Note<Quarter>::parse<DataFormat::ZPP>(std::span<const u8> bytes) {
    return detail::deserialize_zpp<Note<Quarter>>(bytes);
}

template<>
vec<u8> dumps<DataFormat::ZPP>(const vec<Note<Quarter>>& data) {
    return detail::serialize_zpp(data);
}

template<>
vec<Note<Quarter>> parse<DataFormat::ZPP>(std::span<const u8> bytes) {
    return detail::deserialize_zpp<vec<Note<Quarter>>>(bytes);
}

template<>
template<>
vec<u8> Note<Second>::dumps<DataFormat::ZPP>() const {
    return detail::serialize_zpp(*this);
}

template<>
template<>
Note<Second> Note<Second>::parse<DataFormat::ZPP>(std::span<const u8> bytes) {
    return detail::deserialize_zpp<Note<Second>>(bytes);
}

template<>
vec<u8> dumps<DataFormat::ZPP>(const vec<Note<Second>>& data) {
    return detail::serialize_zpp(data);
}

template<>
vec<Note<Second>> parse<DataFormat::ZPP>(std::span<const u8> bytes) {
    return detail::deserialize_zpp<vec<Note<Second>>>(bytes);
}

template<>
template<>
vec<u8> Pedal<Tick>::dumps<DataFormat::ZPP>() const {
    return detail::serialize_zpp(*this);
}

template<>
template<>
Pedal<Tick> Pedal<Tick>::parse<DataFormat::ZPP>(std::span<const u8> bytes) {
    return detail::deserialize_zpp<Pedal<Tick>>(bytes);
}

template<>
vec<u8> dumps<DataFormat::ZPP>(const vec<Pedal<Tick>>& data) {
    return detail::serialize_zpp(data);
}

template<>
vec<Pedal<Tick>> parse<DataFormat::ZPP>(std::span<const u8> bytes) {
    return detail::deserialize_zpp<vec<Pedal<Tick>>>(bytes);
}

template<>
template<>
vec<u8> Pedal<Quarter>::dumps<DataFormat::ZPP>() const {
    return detail::serialize_zpp(*this);
}

template<>
template<>
Pedal<Quarter> Pedal<Quarter>::parse<DataFormat::ZPP>(std::span<const u8> bytes) {
    return detail::deserialize_zpp<Pedal<Quarter>>(bytes);
}

template<>
vec<u8> dumps<DataFormat::ZPP>(const vec<Pedal<Quarter>>& data) {
    return detail::serialize_zpp(data);
}

template<>
vec<Pedal<Quarter>> parse<DataFormat::ZPP>(std::span<const u8> bytes) {
    return detail::deserialize_zpp<vec<Pedal<Quarter>>>(bytes);
}

template<>
template<>
vec<u8> Pedal<Second>::dumps<DataFormat::ZPP>() const {
    return detail::serialize_zpp(*this);
}

template<>
template<>
Pedal<Second> Pedal<Second>::parse<DataFormat::ZPP>(std::span<const u8> bytes) {
    return detail::deserialize_zpp<Pedal<Second>>(bytes);
}

template<>
vec<u8> dumps<DataFormat::ZPP>(const vec<Pedal<Second>>& data) {
    return detail::serialize_zpp(data);
}

template<>
vec<Pedal<Second>> parse<DataFormat::ZPP>(std::span<const u8> bytes) {
    return detail::deserialize_zpp<vec<Pedal<Second>>>(bytes);
}

template<>
template<>
vec<u8> ControlChange<Tick>::dumps<DataFormat::ZPP>() const {
    return detail::serialize_zpp(*this);
}

template<>
template<>
ControlChange<Tick> ControlChange<Tick>::parse<DataFormat::ZPP>(std::span<const u8> bytes) {
    return detail::deserialize_zpp<ControlChange<Tick>>(bytes);
}

template<>
vec<u8> dumps<DataFormat::ZPP>(const vec<ControlChange<Tick>>& data) {
    return detail::serialize_zpp(data);
}

template<>
vec<ControlChange<Tick>> parse<DataFormat::ZPP>(std::span<const u8> bytes) {
    return detail::deserialize_zpp<vec<ControlChange<Tick>>>(bytes);
}

template<>
template<>
vec<u8> ControlChange<Quarter>::dumps<DataFormat::ZPP>() const {
    return detail::serialize_zpp(*this);
}

template<>
template<>
ControlChange<Quarter> ControlChange<Quarter>::parse<DataFormat::ZPP>(std::span<const u8> bytes) {
    return detail::deserialize_zpp<ControlChange<Quarter>>(bytes);
}

template<>
vec<u8> dumps<DataFormat::ZPP>(const vec<ControlChange<Quarter>>& data) {
    return detail::serialize_zpp(data);
}

template<>
vec<ControlChange<Quarter>> parse<DataFormat::ZPP>(std::span<const u8> bytes) {
    return detail::deserialize_zpp<vec<ControlChange<Quarter>>>(bytes);
}

template<>
template<>
vec<u8> ControlChange<Second>::dumps<DataFormat::ZPP>() const {
    return detail::serialize_zpp(*this);
}

template<>
template<>
ControlChange<Second> ControlChange<Second>::parse<DataFormat::ZPP>(std::span<const u8> bytes) {
    return detail::deserialize_zpp<ControlChange<Second>>(bytes);
}

template<>
vec<u8> dumps<DataFormat::ZPP>(const vec<ControlChange<Second>>& data) {
    return detail::serialize_zpp(data);
}

template<>
vec<ControlChange<Second>> parse<DataFormat::ZPP>(std::span<const u8> bytes) {
    return detail::deserialize_zpp<vec<ControlChange<Second>>>(bytes);
}

template<>
template<>
vec<u8> TimeSignature<Tick>::dumps<DataFormat::ZPP>() const {
    return detail::serialize_zpp(*this);
}

template<>
template<>
TimeSignature<Tick> TimeSignature<Tick>::parse<DataFormat::ZPP>(std::span<const u8> bytes) {
    return detail::deserialize_zpp<TimeSignature<Tick>>(bytes);
}

template<>
vec<u8> dumps<DataFormat::ZPP>(const vec<TimeSignature<Tick>>& data) {
    return detail::serialize_zpp(data);
}

template<>
vec<TimeSignature<Tick>> parse<DataFormat::ZPP>(std::span<const u8> bytes) {
    return detail::deserialize_zpp<vec<TimeSignature<Tick>>>(bytes);
}

template<>
template<>
vec<u8> TimeSignature<Quarter>::dumps<DataFormat::ZPP>() const {
    return detail::serialize_zpp(*this);
}

template<>
template<>
TimeSignature<Quarter> TimeSignature<Quarter>::parse<DataFormat::ZPP>(std::span<const u8> bytes) {
    return detail::deserialize_zpp<TimeSignature<Quarter>>(bytes);
}

template<>
vec<u8> dumps<DataFormat::ZPP>(const vec<TimeSignature<Quarter>>& data) {
    return detail::serialize_zpp(data);
}

template<>
vec<TimeSignature<Quarter>> parse<DataFormat::ZPP>(std::span<const u8> bytes) {
    return detail::deserialize_zpp<vec<TimeSignature<Quarter>>>(bytes);
}

template<>
template<>
vec<u8> TimeSignature<Second>::dumps<DataFormat::ZPP>() const {
    return detail::serialize_zpp(*this);
}

template<>
template<>
TimeSignature<Second> TimeSignature<Second>::parse<DataFormat::ZPP>(std::span<const u8> bytes) {
    return detail::deserialize_zpp<TimeSignature<Second>>(bytes);
}

template<>
vec<u8> dumps<DataFormat::ZPP>(const vec<TimeSignature<Second>>& data) {
    return detail::serialize_zpp(data);
}

template<>
vec<TimeSignature<Second>> parse<DataFormat::ZPP>(std::span<const u8> bytes) {
    return detail::deserialize_zpp<vec<TimeSignature<Second>>>(bytes);
}

template<>
template<>
vec<u8> KeySignature<Tick>::dumps<DataFormat::ZPP>() const {
    return detail::serialize_zpp(*this);
}

template<>
template<>
KeySignature<Tick> KeySignature<Tick>::parse<DataFormat::ZPP>(std::span<const u8> bytes) {
    return detail::deserialize_zpp<KeySignature<Tick>>(bytes);
}

template<>
vec<u8> dumps<DataFormat::ZPP>(const vec<KeySignature<Tick>>& data) {
    return detail::serialize_zpp(data);
}

template<>
vec<KeySignature<Tick>> parse<DataFormat::ZPP>(std::span<const u8> bytes) {
    return detail::deserialize_zpp<vec<KeySignature<Tick>>>(bytes);
}

template<>
template<>
vec<u8> KeySignature<Quarter>::dumps<DataFormat::ZPP>() const {
    return detail::serialize_zpp(*this);
}

template<>
template<>
KeySignature<Quarter> KeySignature<Quarter>::parse<DataFormat::ZPP>(std::span<const u8> bytes) {
    return detail::deserialize_zpp<KeySignature<Quarter>>(bytes);
}

template<>
vec<u8> dumps<DataFormat::ZPP>(const vec<KeySignature<Quarter>>& data) {
    return detail::serialize_zpp(data);
}

template<>
vec<KeySignature<Quarter>> parse<DataFormat::ZPP>(std::span<const u8> bytes) {
    return detail::deserialize_zpp<vec<KeySignature<Quarter>>>(bytes);
}

template<>
template<>
vec<u8> KeySignature<Second>::dumps<DataFormat::ZPP>() const {
    return detail::serialize_zpp(*this);
}

template<>
template<>
KeySignature<Second> KeySignature<Second>::parse<DataFormat::ZPP>(std::span<const u8> bytes) {
    return detail::deserialize_zpp<KeySignature<Second>>(bytes);
}

template<>
vec<u8> dumps<DataFormat::ZPP>(const vec<KeySignature<Second>>& data) {
    return detail::serialize_zpp(data);
}

template<>
vec<KeySignature<Second>> parse<DataFormat::ZPP>(std::span<const u8> bytes) {
    return detail::deserialize_zpp<vec<KeySignature<Second>>>(bytes);
}

template<>
template<>
vec<u8> Tempo<Tick>::dumps<DataFormat::ZPP>() const {
    return detail::serialize_zpp(*this);
}

template<>
template<>
Tempo<Tick> Tempo<Tick>::parse<DataFormat::ZPP>(std::span<const u8> bytes) {
    return detail::deserialize_zpp<Tempo<Tick>>(bytes);
}

template<>
vec<u8> dumps<DataFormat::ZPP>(const vec<Tempo<Tick>>& data) {
    return detail::serialize_zpp(data);
}

template<>
vec<Tempo<Tick>> parse<DataFormat::ZPP>(std::span<const u8> bytes) {
    return detail::deserialize_zpp<vec<Tempo<Tick>>>(bytes);
}

template<>
template<>
vec<u8> Tempo<Quarter>::dumps<DataFormat::ZPP>() const {
    return detail::serialize_zpp(*this);
}

template<>
template<>
Tempo<Quarter> Tempo<Quarter>::parse<DataFormat::ZPP>(std::span<const u8> bytes) {
    return detail::deserialize_zpp<Tempo<Quarter>>(bytes);
}

template<>
vec<u8> dumps<DataFormat::ZPP>(const vec<Tempo<Quarter>>& data) {
    return detail::serialize_zpp(data);
}

template<>
vec<Tempo<Quarter>> parse<DataFormat::ZPP>(std::span<const u8> bytes) {
    return detail::deserialize_zpp<vec<Tempo<Quarter>>>(bytes);
}

template<>
template<>
vec<u8> Tempo<Second>::dumps<DataFormat::ZPP>() const {
    return detail::serialize_zpp(*this);
}

template<>
template<>
Tempo<Second> Tempo<Second>::parse<DataFormat::ZPP>(std::span<const u8> bytes) {
    return detail::deserialize_zpp<Tempo<Second>>(bytes);
}

template<>
vec<u8> dumps<DataFormat::ZPP>(const vec<Tempo<Second>>& data) {
    return detail::serialize_zpp(data);
}

template<>
vec<Tempo<Second>> parse<DataFormat::ZPP>(std::span<const u8> bytes) {
    return detail::deserialize_zpp<vec<Tempo<Second>>>(bytes);
}

template<>
template<>
vec<u8> PitchBend<Tick>::dumps<DataFormat::ZPP>() const {
    return detail::serialize_zpp(*this);
}

template<>
template<>
PitchBend<Tick> PitchBend<Tick>::parse<DataFormat::ZPP>(std::span<const u8> bytes) {
    return detail::deserialize_zpp<PitchBend<Tick>>(bytes);
}

template<>
vec<u8> dumps<DataFormat::ZPP>(const vec<PitchBend<Tick>>& data) {
    return detail::serialize_zpp(data);
}

template<>
vec<PitchBend<Tick>> parse<DataFormat::ZPP>(std::span<const u8> bytes) {
    return detail::deserialize_zpp<vec<PitchBend<Tick>>>(bytes);
}

template<>
template<>
vec<u8> PitchBend<Quarter>::dumps<DataFormat::ZPP>() const {
    return detail::serialize_zpp(*this);
}

template<>
template<>
PitchBend<Quarter> PitchBend<Quarter>::parse<DataFormat::ZPP>(std::span<const u8> bytes) {
    return detail::deserialize_zpp<PitchBend<Quarter>>(bytes);
}

template<>
vec<u8> dumps<DataFormat::ZPP>(const vec<PitchBend<Quarter>>& data) {
    return detail::serialize_zpp(data);
}

template<>
vec<PitchBend<Quarter>> parse<DataFormat::ZPP>(std::span<const u8> bytes) {
    return detail::deserialize_zpp<vec<PitchBend<Quarter>>>(bytes);
}

template<>
template<>
vec<u8> PitchBend<Second>::dumps<DataFormat::ZPP>() const {
    return detail::serialize_zpp(*this);
}

template<>
template<>
PitchBend<Second> PitchBend<Second>::parse<DataFormat::ZPP>(std::span<const u8> bytes) {
    return detail::deserialize_zpp<PitchBend<Second>>(bytes);
}

template<>
vec<u8> dumps<DataFormat::ZPP>(const vec<PitchBend<Second>>& data) {
    return detail::serialize_zpp(data);
}

template<>
vec<PitchBend<Second>> parse<DataFormat::ZPP>(std::span<const u8> bytes) {
    return detail::deserialize_zpp<vec<PitchBend<Second>>>(bytes);
}

template<>
template<>
vec<u8> TextMeta<Tick>::dumps<DataFormat::ZPP>() const {
    return detail::serialize_zpp(*this);
}

template<>
template<>
TextMeta<Tick> TextMeta<Tick>::parse<DataFormat::ZPP>(std::span<const u8> bytes) {
    return detail::deserialize_zpp<TextMeta<Tick>>(bytes);
}

template<>
vec<u8> dumps<DataFormat::ZPP>(const vec<TextMeta<Tick>>& data) {
    return detail::serialize_zpp(data);
}

template<>
vec<TextMeta<Tick>> parse<DataFormat::ZPP>(std::span<const u8> bytes) {
    return detail::deserialize_zpp<vec<TextMeta<Tick>>>(bytes);
}

template<>
template<>
vec<u8> TextMeta<Quarter>::dumps<DataFormat::ZPP>() const {
    return detail::serialize_zpp(*this);
}

template<>
template<>
TextMeta<Quarter> TextMeta<Quarter>::parse<DataFormat::ZPP>(std::span<const u8> bytes) {
    return detail::deserialize_zpp<TextMeta<Quarter>>(bytes);
}

template<>
vec<u8> dumps<DataFormat::ZPP>(const vec<TextMeta<Quarter>>& data) {
    return detail::serialize_zpp(data);
}

template<>
vec<TextMeta<Quarter>> parse<DataFormat::ZPP>(std::span<const u8> bytes) {
    return detail::deserialize_zpp<vec<TextMeta<Quarter>>>(bytes);
}

template<>
template<>
vec<u8> TextMeta<Second>::dumps<DataFormat::ZPP>() const {
    return detail::serialize_zpp(*this);
}

template<>
template<>
TextMeta<Second> TextMeta<Second>::parse<DataFormat::ZPP>(std::span<const u8> bytes) {
    return detail::deserialize_zpp<TextMeta<Second>>(bytes);
}

template<>
vec<u8> dumps<DataFormat::ZPP>(const vec<TextMeta<Second>>& data) {
    return detail::serialize_zpp(data);
}

template<>
vec<TextMeta<Second>> parse<DataFormat::ZPP>(std::span<const u8> bytes) {
    return detail::deserialize_zpp<vec<TextMeta<Second>>>(bytes);
}

template vec<u8>                   Track<Tick, ValueStorage>::dump_zpp_bytes() const;
template Track<Tick, ValueStorage> Track<Tick, ValueStorage>::parse_zpp_bytes(std::span<const u8>);
template vec<u8>                   Track<Quarter, ValueStorage>::dump_zpp_bytes() const;
template Track<Quarter, ValueStorage> Track<Quarter, ValueStorage>::parse_zpp_bytes(
    std::span<const u8>
);
template vec<u8>                     Track<Second, ValueStorage>::dump_zpp_bytes() const;
template Track<Second, ValueStorage> Track<Second, ValueStorage>::parse_zpp_bytes(
    std::span<const u8>
);
template vec<u8>                   Score<Tick, ValueStorage>::dump_zpp_bytes() const;
template Score<Tick, ValueStorage> Score<Tick, ValueStorage>::parse_zpp_bytes(std::span<const u8>);
template vec<u8>                   Score<Quarter, ValueStorage>::dump_zpp_bytes() const;
template Score<Quarter, ValueStorage> Score<Quarter, ValueStorage>::parse_zpp_bytes(
    std::span<const u8>
);
template vec<u8>                     Score<Second, ValueStorage>::dump_zpp_bytes() const;
template Score<Second, ValueStorage> Score<Second, ValueStorage>::parse_zpp_bytes(
    std::span<const u8>
);
template<>
vec<u8> dumps<DataFormat::ZPP>(const vec<Track<Tick>>& data) {
    return detail::serialize_zpp(data);
}
template<>
vec<Track<Tick>> parse<DataFormat::ZPP>(std::span<const u8> data) {
    return detail::deserialize_zpp<vec<Track<Tick>>>(data);
}
template<>
vec<u8> dumps<DataFormat::ZPP>(const vec<Score<Tick>>& data) {
    return detail::serialize_zpp(data);
}
template<>
vec<Score<Tick>> parse<DataFormat::ZPP>(std::span<const u8> data) {
    return detail::deserialize_zpp<vec<Score<Tick>>>(data);
}
template<>
vec<u8> dumps<DataFormat::ZPP>(const vec<Track<Quarter>>& data) {
    return detail::serialize_zpp(data);
}
template<>
vec<Track<Quarter>> parse<DataFormat::ZPP>(std::span<const u8> data) {
    return detail::deserialize_zpp<vec<Track<Quarter>>>(data);
}
template<>
vec<u8> dumps<DataFormat::ZPP>(const vec<Score<Quarter>>& data) {
    return detail::serialize_zpp(data);
}
template<>
vec<Score<Quarter>> parse<DataFormat::ZPP>(std::span<const u8> data) {
    return detail::deserialize_zpp<vec<Score<Quarter>>>(data);
}
template<>
vec<u8> dumps<DataFormat::ZPP>(const vec<Track<Second>>& data) {
    return detail::serialize_zpp(data);
}
template<>
vec<Track<Second>> parse<DataFormat::ZPP>(std::span<const u8> data) {
    return detail::deserialize_zpp<vec<Track<Second>>>(data);
}
template<>
vec<u8> dumps<DataFormat::ZPP>(const vec<Score<Second>>& data) {
    return detail::serialize_zpp(data);
}
template<>
vec<Score<Second>> parse<DataFormat::ZPP>(std::span<const u8> data) {
    return detail::deserialize_zpp<vec<Score<Second>>>(data);
}

}   // namespace symusic

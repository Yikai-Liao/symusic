#include "symusic/detail/zpp_impl.h"

namespace symusic {

template<>
vec<u8> Note<Tick>::serialize() const {
    return detail::serialize_zpp(*this);
}

template<>
Note<Tick> Note<Tick>::deserialize(std::span<const u8> bytes) {
    return detail::deserialize_zpp<Note<Tick>>(bytes);
}

template<>
vec<u8> serialize(const vec<Note<Tick>>& data) {
    return detail::serialize_zpp(data);
}

template<>
vec<Note<Tick>> deserialize<vec<Note<Tick>>>(std::span<const u8> bytes) {
    return detail::deserialize_zpp<vec<Note<Tick>>>(bytes);
}

template<>
vec<u8> Note<Quarter>::serialize() const {
    return detail::serialize_zpp(*this);
}

template<>
Note<Quarter> Note<Quarter>::deserialize(std::span<const u8> bytes) {
    return detail::deserialize_zpp<Note<Quarter>>(bytes);
}

template<>
vec<u8> serialize(const vec<Note<Quarter>>& data) {
    return detail::serialize_zpp(data);
}

template<>
vec<Note<Quarter>> deserialize<vec<Note<Quarter>>>(std::span<const u8> bytes) {
    return detail::deserialize_zpp<vec<Note<Quarter>>>(bytes);
}

template<>
vec<u8> Note<Second>::serialize() const {
    return detail::serialize_zpp(*this);
}

template<>
Note<Second> Note<Second>::deserialize(std::span<const u8> bytes) {
    return detail::deserialize_zpp<Note<Second>>(bytes);
}

template<>
vec<u8> serialize(const vec<Note<Second>>& data) {
    return detail::serialize_zpp(data);
}

template<>
vec<Note<Second>> deserialize<vec<Note<Second>>>(std::span<const u8> bytes) {
    return detail::deserialize_zpp<vec<Note<Second>>>(bytes);
}

template<>
vec<u8> Pedal<Tick>::serialize() const {
    return detail::serialize_zpp(*this);
}

template<>
Pedal<Tick> Pedal<Tick>::deserialize(std::span<const u8> bytes) {
    return detail::deserialize_zpp<Pedal<Tick>>(bytes);
}

template<>
vec<u8> serialize(const vec<Pedal<Tick>>& data) {
    return detail::serialize_zpp(data);
}

template<>
vec<Pedal<Tick>> deserialize<vec<Pedal<Tick>>>(std::span<const u8> bytes) {
    return detail::deserialize_zpp<vec<Pedal<Tick>>>(bytes);
}

template<>
vec<u8> Pedal<Quarter>::serialize() const {
    return detail::serialize_zpp(*this);
}

template<>
Pedal<Quarter> Pedal<Quarter>::deserialize(std::span<const u8> bytes) {
    return detail::deserialize_zpp<Pedal<Quarter>>(bytes);
}

template<>
vec<u8> serialize(const vec<Pedal<Quarter>>& data) {
    return detail::serialize_zpp(data);
}

template<>
vec<Pedal<Quarter>> deserialize<vec<Pedal<Quarter>>>(std::span<const u8> bytes) {
    return detail::deserialize_zpp<vec<Pedal<Quarter>>>(bytes);
}

template<>
vec<u8> Pedal<Second>::serialize() const {
    return detail::serialize_zpp(*this);
}

template<>
Pedal<Second> Pedal<Second>::deserialize(std::span<const u8> bytes) {
    return detail::deserialize_zpp<Pedal<Second>>(bytes);
}

template<>
vec<u8> serialize(const vec<Pedal<Second>>& data) {
    return detail::serialize_zpp(data);
}

template<>
vec<Pedal<Second>> deserialize<vec<Pedal<Second>>>(std::span<const u8> bytes) {
    return detail::deserialize_zpp<vec<Pedal<Second>>>(bytes);
}

template<>
vec<u8> ControlChange<Tick>::serialize() const {
    return detail::serialize_zpp(*this);
}

template<>
ControlChange<Tick> ControlChange<Tick>::deserialize(std::span<const u8> bytes) {
    return detail::deserialize_zpp<ControlChange<Tick>>(bytes);
}

template<>
vec<u8> serialize(const vec<ControlChange<Tick>>& data) {
    return detail::serialize_zpp(data);
}

template<>
vec<ControlChange<Tick>> deserialize<vec<ControlChange<Tick>>>(std::span<const u8> bytes) {
    return detail::deserialize_zpp<vec<ControlChange<Tick>>>(bytes);
}

template<>
vec<u8> ControlChange<Quarter>::serialize() const {
    return detail::serialize_zpp(*this);
}

template<>
ControlChange<Quarter> ControlChange<Quarter>::deserialize(std::span<const u8> bytes) {
    return detail::deserialize_zpp<ControlChange<Quarter>>(bytes);
}

template<>
vec<u8> serialize(const vec<ControlChange<Quarter>>& data) {
    return detail::serialize_zpp(data);
}

template<>
vec<ControlChange<Quarter>> deserialize<vec<ControlChange<Quarter>>>(std::span<const u8> bytes) {
    return detail::deserialize_zpp<vec<ControlChange<Quarter>>>(bytes);
}

template<>
vec<u8> ControlChange<Second>::serialize() const {
    return detail::serialize_zpp(*this);
}

template<>
ControlChange<Second> ControlChange<Second>::deserialize(std::span<const u8> bytes) {
    return detail::deserialize_zpp<ControlChange<Second>>(bytes);
}

template<>
vec<u8> serialize(const vec<ControlChange<Second>>& data) {
    return detail::serialize_zpp(data);
}

template<>
vec<ControlChange<Second>> deserialize<vec<ControlChange<Second>>>(std::span<const u8> bytes) {
    return detail::deserialize_zpp<vec<ControlChange<Second>>>(bytes);
}

template<>
vec<u8> TimeSignature<Tick>::serialize() const {
    return detail::serialize_zpp(*this);
}

template<>
TimeSignature<Tick> TimeSignature<Tick>::deserialize(std::span<const u8> bytes) {
    return detail::deserialize_zpp<TimeSignature<Tick>>(bytes);
}

template<>
vec<u8> serialize(const vec<TimeSignature<Tick>>& data) {
    return detail::serialize_zpp(data);
}

template<>
vec<TimeSignature<Tick>> deserialize<vec<TimeSignature<Tick>>>(std::span<const u8> bytes) {
    return detail::deserialize_zpp<vec<TimeSignature<Tick>>>(bytes);
}

template<>
vec<u8> TimeSignature<Quarter>::serialize() const {
    return detail::serialize_zpp(*this);
}

template<>
TimeSignature<Quarter> TimeSignature<Quarter>::deserialize(std::span<const u8> bytes) {
    return detail::deserialize_zpp<TimeSignature<Quarter>>(bytes);
}

template<>
vec<u8> serialize(const vec<TimeSignature<Quarter>>& data) {
    return detail::serialize_zpp(data);
}

template<>
vec<TimeSignature<Quarter>> deserialize<vec<TimeSignature<Quarter>>>(std::span<const u8> bytes) {
    return detail::deserialize_zpp<vec<TimeSignature<Quarter>>>(bytes);
}

template<>
vec<u8> TimeSignature<Second>::serialize() const {
    return detail::serialize_zpp(*this);
}

template<>
TimeSignature<Second> TimeSignature<Second>::deserialize(std::span<const u8> bytes) {
    return detail::deserialize_zpp<TimeSignature<Second>>(bytes);
}

template<>
vec<u8> serialize(const vec<TimeSignature<Second>>& data) {
    return detail::serialize_zpp(data);
}

template<>
vec<TimeSignature<Second>> deserialize<vec<TimeSignature<Second>>>(std::span<const u8> bytes) {
    return detail::deserialize_zpp<vec<TimeSignature<Second>>>(bytes);
}

template<>
vec<u8> KeySignature<Tick>::serialize() const {
    return detail::serialize_zpp(*this);
}

template<>
KeySignature<Tick> KeySignature<Tick>::deserialize(std::span<const u8> bytes) {
    return detail::deserialize_zpp<KeySignature<Tick>>(bytes);
}

template<>
vec<u8> serialize(const vec<KeySignature<Tick>>& data) {
    return detail::serialize_zpp(data);
}

template<>
vec<KeySignature<Tick>> deserialize<vec<KeySignature<Tick>>>(std::span<const u8> bytes) {
    return detail::deserialize_zpp<vec<KeySignature<Tick>>>(bytes);
}

template<>
vec<u8> KeySignature<Quarter>::serialize() const {
    return detail::serialize_zpp(*this);
}

template<>
KeySignature<Quarter> KeySignature<Quarter>::deserialize(std::span<const u8> bytes) {
    return detail::deserialize_zpp<KeySignature<Quarter>>(bytes);
}

template<>
vec<u8> serialize(const vec<KeySignature<Quarter>>& data) {
    return detail::serialize_zpp(data);
}

template<>
vec<KeySignature<Quarter>> deserialize<vec<KeySignature<Quarter>>>(std::span<const u8> bytes) {
    return detail::deserialize_zpp<vec<KeySignature<Quarter>>>(bytes);
}

template<>
vec<u8> KeySignature<Second>::serialize() const {
    return detail::serialize_zpp(*this);
}

template<>
KeySignature<Second> KeySignature<Second>::deserialize(std::span<const u8> bytes) {
    return detail::deserialize_zpp<KeySignature<Second>>(bytes);
}

template<>
vec<u8> serialize(const vec<KeySignature<Second>>& data) {
    return detail::serialize_zpp(data);
}

template<>
vec<KeySignature<Second>> deserialize<vec<KeySignature<Second>>>(std::span<const u8> bytes) {
    return detail::deserialize_zpp<vec<KeySignature<Second>>>(bytes);
}

template<>
vec<u8> Tempo<Tick>::serialize() const {
    return detail::serialize_zpp(*this);
}

template<>
Tempo<Tick> Tempo<Tick>::deserialize(std::span<const u8> bytes) {
    return detail::deserialize_zpp<Tempo<Tick>>(bytes);
}

template<>
vec<u8> serialize(const vec<Tempo<Tick>>& data) {
    return detail::serialize_zpp(data);
}

template<>
vec<Tempo<Tick>> deserialize<vec<Tempo<Tick>>>(std::span<const u8> bytes) {
    return detail::deserialize_zpp<vec<Tempo<Tick>>>(bytes);
}

template<>
vec<u8> Tempo<Quarter>::serialize() const {
    return detail::serialize_zpp(*this);
}

template<>
Tempo<Quarter> Tempo<Quarter>::deserialize(std::span<const u8> bytes) {
    return detail::deserialize_zpp<Tempo<Quarter>>(bytes);
}

template<>
vec<u8> serialize(const vec<Tempo<Quarter>>& data) {
    return detail::serialize_zpp(data);
}

template<>
vec<Tempo<Quarter>> deserialize<vec<Tempo<Quarter>>>(std::span<const u8> bytes) {
    return detail::deserialize_zpp<vec<Tempo<Quarter>>>(bytes);
}

template<>
vec<u8> Tempo<Second>::serialize() const {
    return detail::serialize_zpp(*this);
}

template<>
Tempo<Second> Tempo<Second>::deserialize(std::span<const u8> bytes) {
    return detail::deserialize_zpp<Tempo<Second>>(bytes);
}

template<>
vec<u8> serialize(const vec<Tempo<Second>>& data) {
    return detail::serialize_zpp(data);
}

template<>
vec<Tempo<Second>> deserialize<vec<Tempo<Second>>>(std::span<const u8> bytes) {
    return detail::deserialize_zpp<vec<Tempo<Second>>>(bytes);
}

template<>
vec<u8> PitchBend<Tick>::serialize() const {
    return detail::serialize_zpp(*this);
}

template<>
PitchBend<Tick> PitchBend<Tick>::deserialize(std::span<const u8> bytes) {
    return detail::deserialize_zpp<PitchBend<Tick>>(bytes);
}

template<>
vec<u8> serialize(const vec<PitchBend<Tick>>& data) {
    return detail::serialize_zpp(data);
}

template<>
vec<PitchBend<Tick>> deserialize<vec<PitchBend<Tick>>>(std::span<const u8> bytes) {
    return detail::deserialize_zpp<vec<PitchBend<Tick>>>(bytes);
}

template<>
vec<u8> PitchBend<Quarter>::serialize() const {
    return detail::serialize_zpp(*this);
}

template<>
PitchBend<Quarter> PitchBend<Quarter>::deserialize(std::span<const u8> bytes) {
    return detail::deserialize_zpp<PitchBend<Quarter>>(bytes);
}

template<>
vec<u8> serialize(const vec<PitchBend<Quarter>>& data) {
    return detail::serialize_zpp(data);
}

template<>
vec<PitchBend<Quarter>> deserialize<vec<PitchBend<Quarter>>>(std::span<const u8> bytes) {
    return detail::deserialize_zpp<vec<PitchBend<Quarter>>>(bytes);
}

template<>
vec<u8> PitchBend<Second>::serialize() const {
    return detail::serialize_zpp(*this);
}

template<>
PitchBend<Second> PitchBend<Second>::deserialize(std::span<const u8> bytes) {
    return detail::deserialize_zpp<PitchBend<Second>>(bytes);
}

template<>
vec<u8> serialize(const vec<PitchBend<Second>>& data) {
    return detail::serialize_zpp(data);
}

template<>
vec<PitchBend<Second>> deserialize<vec<PitchBend<Second>>>(std::span<const u8> bytes) {
    return detail::deserialize_zpp<vec<PitchBend<Second>>>(bytes);
}

template<>
vec<u8> TextMeta<Tick>::serialize() const {
    return detail::serialize_zpp(*this);
}

template<>
TextMeta<Tick> TextMeta<Tick>::deserialize(std::span<const u8> bytes) {
    return detail::deserialize_zpp<TextMeta<Tick>>(bytes);
}

template<>
vec<u8> serialize(const vec<TextMeta<Tick>>& data) {
    return detail::serialize_zpp(data);
}

template<>
vec<TextMeta<Tick>> deserialize<vec<TextMeta<Tick>>>(std::span<const u8> bytes) {
    return detail::deserialize_zpp<vec<TextMeta<Tick>>>(bytes);
}

template<>
vec<u8> TextMeta<Quarter>::serialize() const {
    return detail::serialize_zpp(*this);
}

template<>
TextMeta<Quarter> TextMeta<Quarter>::deserialize(std::span<const u8> bytes) {
    return detail::deserialize_zpp<TextMeta<Quarter>>(bytes);
}

template<>
vec<u8> serialize(const vec<TextMeta<Quarter>>& data) {
    return detail::serialize_zpp(data);
}

template<>
vec<TextMeta<Quarter>> deserialize<vec<TextMeta<Quarter>>>(std::span<const u8> bytes) {
    return detail::deserialize_zpp<vec<TextMeta<Quarter>>>(bytes);
}

template<>
vec<u8> TextMeta<Second>::serialize() const {
    return detail::serialize_zpp(*this);
}

template<>
TextMeta<Second> TextMeta<Second>::deserialize(std::span<const u8> bytes) {
    return detail::deserialize_zpp<TextMeta<Second>>(bytes);
}

template<>
vec<u8> serialize(const vec<TextMeta<Second>>& data) {
    return detail::serialize_zpp(data);
}

template<>
vec<TextMeta<Second>> deserialize<vec<TextMeta<Second>>>(std::span<const u8> bytes) {
    return detail::deserialize_zpp<vec<TextMeta<Second>>>(bytes);
}

template vec<u8>                      Track<Tick, ValueStorage>::serialize() const;
template Track<Tick, ValueStorage>    Track<Tick, ValueStorage>::deserialize(std::span<const u8>);
template vec<u8>                      Track<Quarter, ValueStorage>::serialize() const;
template Track<Quarter, ValueStorage> Track<Quarter, ValueStorage>::deserialize(
    std::span<const u8>
);
template vec<u8>                      Track<Second, ValueStorage>::serialize() const;
template Track<Second, ValueStorage>  Track<Second, ValueStorage>::deserialize(std::span<const u8>);
template vec<u8>                      Score<Tick, ValueStorage>::serialize() const;
template Score<Tick, ValueStorage>    Score<Tick, ValueStorage>::deserialize(std::span<const u8>);
template vec<u8>                      Score<Quarter, ValueStorage>::serialize() const;
template Score<Quarter, ValueStorage> Score<Quarter, ValueStorage>::deserialize(
    std::span<const u8>
);
template vec<u8>                     Score<Second, ValueStorage>::serialize() const;
template Score<Second, ValueStorage> Score<Second, ValueStorage>::deserialize(std::span<const u8>);
template<>
vec<u8> serialize(const vec<Track<Tick>>& data) {
    return detail::serialize_zpp(data);
}
template<>
vec<Track<Tick>> deserialize(std::span<const u8> data) {
    return detail::deserialize_zpp<vec<Track<Tick>>>(data);
}
template<>
vec<u8> serialize(const vec<Score<Tick>>& data) {
    return detail::serialize_zpp(data);
}
template<>
vec<Score<Tick>> deserialize(std::span<const u8> data) {
    return detail::deserialize_zpp<vec<Score<Tick>>>(data);
}
template<>
vec<u8> serialize(const vec<Track<Quarter>>& data) {
    return detail::serialize_zpp(data);
}
template<>
vec<Track<Quarter>> deserialize(std::span<const u8> data) {
    return detail::deserialize_zpp<vec<Track<Quarter>>>(data);
}
template<>
vec<u8> serialize(const vec<Score<Quarter>>& data) {
    return detail::serialize_zpp(data);
}
template<>
vec<Score<Quarter>> deserialize(std::span<const u8> data) {
    return detail::deserialize_zpp<vec<Score<Quarter>>>(data);
}
template<>
vec<u8> serialize(const vec<Track<Second>>& data) {
    return detail::serialize_zpp(data);
}
template<>
vec<Track<Second>> deserialize(std::span<const u8> data) {
    return detail::deserialize_zpp<vec<Track<Second>>>(data);
}
template<>
vec<u8> serialize(const vec<Score<Second>>& data) {
    return detail::serialize_zpp(data);
}
template<>
vec<Score<Second>> deserialize(std::span<const u8> data) {
    return detail::deserialize_zpp<vec<Score<Second>>>(data);
}

}   // namespace symusic

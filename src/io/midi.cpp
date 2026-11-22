#include "symusic/detail/midi_impl.h"

namespace symusic {

template Score<Tick, ValueStorage> Score<Tick, ValueStorage>::parse_midi_bytes(std::span<const u8>, bool);
template vec<u8> Score<Tick, ValueStorage>::dump_midi_bytes() const;
template Score<Quarter, ValueStorage> Score<Quarter, ValueStorage>::parse_midi_bytes(std::span<const u8>, bool);
template vec<u8> Score<Quarter, ValueStorage>::dump_midi_bytes() const;
template Score<Second, ValueStorage> Score<Second, ValueStorage>::parse_midi_bytes(std::span<const u8>, bool);
template vec<u8> Score<Second, ValueStorage>::dump_midi_bytes() const;

}  // namespace symusic

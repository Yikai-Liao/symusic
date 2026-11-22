#include "symusic/detail/midi_impl.h"

namespace symusic {

template Score<Tick, ValueStorage> Score<Tick, ValueStorage>::from_midi(
    std::span<const u8>, MidiReadOptions
);
template vec<u8>                      Score<Tick, ValueStorage>::to_midi() const;
template Score<Quarter, ValueStorage> Score<Quarter, ValueStorage>::from_midi(
    std::span<const u8>, MidiReadOptions
);
template vec<u8>                     Score<Quarter, ValueStorage>::to_midi() const;
template Score<Second, ValueStorage> Score<Second, ValueStorage>::from_midi(
    std::span<const u8>, MidiReadOptions
);
template vec<u8> Score<Second, ValueStorage>::to_midi() const;

}   // namespace symusic

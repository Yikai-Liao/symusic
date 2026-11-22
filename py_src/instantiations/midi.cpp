#include "utils/storage.h"
#include "symusic/detail/midi_impl.h"

namespace symusic {

template Score<Tick, py::SharedStorage> Score<Tick, py::SharedStorage>::from_midi(
    std::span<const u8>, MidiReadOptions
);
template vec<u8>                           Score<Tick, py::SharedStorage>::to_midi() const;
template Score<Quarter, py::SharedStorage> Score<Quarter, py::SharedStorage>::from_midi(
    std::span<const u8>, MidiReadOptions
);
template vec<u8>                          Score<Quarter, py::SharedStorage>::to_midi() const;
template Score<Second, py::SharedStorage> Score<Second, py::SharedStorage>::from_midi(
    std::span<const u8>, MidiReadOptions
);
template vec<u8> Score<Second, py::SharedStorage>::to_midi() const;

}   // namespace symusic

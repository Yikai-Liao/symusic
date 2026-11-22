#include "utils/storage.h"
#include "symusic/detail/midi_impl.h"

namespace symusic {

template Score<Tick, py::SharedStorage> Score<Tick, py::SharedStorage>::parse_midi_bytes(std::span<const u8>, bool);
template vec<u8> Score<Tick, py::SharedStorage>::dump_midi_bytes() const;
template Score<Quarter, py::SharedStorage> Score<Quarter, py::SharedStorage>::parse_midi_bytes(std::span<const u8>, bool);
template vec<u8> Score<Quarter, py::SharedStorage>::dump_midi_bytes() const;
template Score<Second, py::SharedStorage> Score<Second, py::SharedStorage>::parse_midi_bytes(std::span<const u8>, bool);
template vec<u8> Score<Second, py::SharedStorage>::dump_midi_bytes() const;

}  // namespace symusic

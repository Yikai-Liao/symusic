// Declares the MIDI parse/dump specializations for users of the MIDI format.
// midi.cpp defines them, so it must not include this header (extern templates).
#pragma once

#ifndef LIBSYMUSIC_IO_MIDI_H
#define LIBSYMUSIC_IO_MIDI_H

#include "symusic/io/iodef.h"
#include "symusic/score.h"
#include "MetaMacro.h"

namespace symusic {

// explicit extern dumps and parse to overload the general implementation which always throws a compile time error
#define EXTERN_MIDI(__COUNT, T)                                         \
    extern template vec<u8> Score<T>::dumps<DataFormat::MIDI>() const;  \
    extern template Score<T> Score<T>::parse<DataFormat::MIDI>(std::span<const u8> bytes);

REPEAT_ON(EXTERN_MIDI, Tick, Quarter, Second) // Second is not supported yet

#undef EXTERN_MIDI

}

#endif //LIBSYMUSIC_IO_MIDI_H

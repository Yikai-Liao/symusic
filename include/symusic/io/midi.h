//
// Created by lyk on 2023/12/24.
// This file should be included by users if they need to use the MIDI format
// And it can't be included in midi.cpp becuase of extern template
//
#pragma once

#ifndef LIBSYMUSIC_IO_MIDI_H
#define LIBSYMUSIC_IO_MIDI_H

#include "symusic/io/iodef.h"
#include "symusic/score.h"

namespace symusic {

// explicit extern dumps and parse to overload the general implementation which always throws a compile time error
#define EXTERN_MIDI(__COUNT, T)                                         \
    extern template vec<u8> Score<T>::dumps<DataFormat::MIDI>() const;  \
    extern template Score<T> Score<T>::parse<DataFormat::MIDI>(std::span<const u8> bytes);

REPEAT_ON(EXTERN_MIDI, Tick, Quarter, Second) // Second is not supported yet

#undef EXTERN_MIDI

}

#endif //LIBSYMUSIC_IO_MIDI_H

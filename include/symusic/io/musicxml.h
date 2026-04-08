//
// MusicXML I/O explicit instantiations for Score.
//
#pragma once

#ifndef LIBSYMUSIC_IO_MUSICXML_H
#define LIBSYMUSIC_IO_MUSICXML_H

#include "MetaMacro.h"
#include "symusic/io/iodef.h"
#include "symusic/score.h"

namespace symusic {

#define EXTERN_MUSICXML(__COUNT, T)                                            \
    extern template vec<u8> Score<T>::dumps<DataFormat::MusicXML>() const;     \
    extern template Score<T> Score<T>::parse<DataFormat::MusicXML>(            \
        std::span<const u8> bytes                                              \
    );

REPEAT_ON(EXTERN_MUSICXML, Tick, Quarter, Second)

#undef EXTERN_MUSICXML

}   // namespace symusic

#endif   // LIBSYMUSIC_IO_MUSICXML_H

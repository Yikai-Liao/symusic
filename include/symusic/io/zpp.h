//
// Created by lyk on 2023/12/24.
//
#pragma once

#ifndef LIBSYMUSIC_IO_ZPP_H
#define LIBSYMUSIC_IO_ZPP_H

#include <span>

#include "zpp_bits.h"

#include "symusic/io/iodef.h"
#include "symusic/event.h"
#include "symusic/track.h"
#include "symusic/score.h"
#include "symusic/note_arr.h"

namespace symusic {

#define EXTERN_ZPP_INNER(NAME, T)                                                   \
    extern template NAME<T> NAME<T>::parse<DataFormat::ZPP>(std::span<const u8>);   \
    extern template vec<u8> Score<Tick>::dumps<DataFormat::ZPP>() const;

#define EXTERN_ZPP(__COUNT, NAME)   \
    EXTERN_ZPP_INNER(NAME, Tick)    \
    EXTERN_ZPP_INNER(NAME, Quarter) \
    EXTERN_ZPP_INNER(NAME, Second)

REPEAT_ON(
    EXTERN_ZPP,
    Note, Pedal, ControlChange, TimeSignature, KeySignature,
    Tempo, PitchBend, TextMeta, NoteArr, Track, Score
)

}
#endif //LIBSYMUSIC_IO_ZPP_H

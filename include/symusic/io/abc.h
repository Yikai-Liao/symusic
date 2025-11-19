//
// Created by auto-generated miniabc integration.
//
#pragma once

#ifndef LIBSYMUSIC_IO_ABC_H
#define LIBSYMUSIC_IO_ABC_H

#include "MetaMacro.h"
#include "symusic/io/iodef.h"
#include "symusic/score.h"

namespace symusic {

#define EXTERN_ABC(__COUNT, T)                                           \
    extern template Score<T> Score<T>::parse<DataFormat::ABC>(           \
        std::span<const u8> bytes                                        \
    );                                                                   \
    extern template vec<u8> Score<T>::dumps<DataFormat::ABC>() const;

REPEAT_ON(EXTERN_ABC, Tick, Quarter, Second)
#undef EXTERN_ABC

}  // namespace symusic

#endif  // LIBSYMUSIC_IO_ABC_H

//
// Created by lyk on 23-12-17.
//
#pragma once

#ifndef LIBSYMUSIC_CONVERSION_H
#define LIBSYMUSIC_CONVERSION_H

#include "symusic/time_unit.h"
#include "symusic/score.h"

namespace symusic {

template< TType To, TType From>
Score<To> convert(const Score<From> & score, typename To::unit min_dur = static_cast<typename To::unit>(0));

template<TType T>
Score<Tick> resample(const Score<T> & score, i32 tpq, Tick::unit min_dur = 0);

}
#endif //LIBSYMUSIC_CONVERSION_H

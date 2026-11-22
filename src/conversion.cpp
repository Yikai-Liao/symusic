#include "symusic/detail/conversion_impl.h"

namespace symusic {

template Score<Tick, ValueStorage> convert(const Score<Tick, ValueStorage>&, Tick::unit);
template Score<Tick, ValueStorage> convert(const Score<Quarter, ValueStorage>&, Tick::unit);
template Score<Tick, ValueStorage> convert(const Score<Second, ValueStorage>&, Tick::unit);
template Score<Quarter, ValueStorage> convert(const Score<Tick, ValueStorage>&, Quarter::unit);
template Score<Quarter, ValueStorage> convert(const Score<Quarter, ValueStorage>&, Quarter::unit);
template Score<Quarter, ValueStorage> convert(const Score<Second, ValueStorage>&, Quarter::unit);
template Score<Second, ValueStorage> convert(const Score<Tick, ValueStorage>&, Second::unit);
template Score<Second, ValueStorage> convert(const Score<Quarter, ValueStorage>&, Second::unit);
template Score<Second, ValueStorage> convert(const Score<Second, ValueStorage>&, Second::unit);
template Score<Tick, ValueStorage> resample(const Score<Tick, ValueStorage>&, i32, Tick::unit);
template Score<Tick, ValueStorage> resample(const Score<Quarter, ValueStorage>&, i32, Tick::unit);
template Score<Tick, ValueStorage> resample(const Score<Second, ValueStorage>&, i32, Tick::unit);

}  // namespace symusic

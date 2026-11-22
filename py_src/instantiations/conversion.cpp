#include "utils/storage.h"
#include "symusic/detail/conversion_impl.h"

namespace symusic {

template Score<Tick, py::SharedStorage> convert(const Score<Tick, py::SharedStorage>&, Tick::unit);
template Score<Tick, py::SharedStorage> convert(
    const Score<Quarter, py::SharedStorage>&, Tick::unit
);
template Score<Tick, py::SharedStorage> convert(
    const Score<Second, py::SharedStorage>&, Tick::unit
);
template Score<Quarter, py::SharedStorage> convert(
    const Score<Tick, py::SharedStorage>&, Quarter::unit
);
template Score<Quarter, py::SharedStorage> convert(
    const Score<Quarter, py::SharedStorage>&, Quarter::unit
);
template Score<Quarter, py::SharedStorage> convert(
    const Score<Second, py::SharedStorage>&, Quarter::unit
);
template Score<Second, py::SharedStorage> convert(
    const Score<Tick, py::SharedStorage>&, Second::unit
);
template Score<Second, py::SharedStorage> convert(
    const Score<Quarter, py::SharedStorage>&, Second::unit
);
template Score<Second, py::SharedStorage> convert(
    const Score<Second, py::SharedStorage>&, Second::unit
);
template Score<Tick, py::SharedStorage> resample(
    const Score<Tick, py::SharedStorage>&, i32, Tick::unit
);
template Score<Tick, py::SharedStorage> resample(
    const Score<Quarter, py::SharedStorage>&, i32, Tick::unit
);
template Score<Tick, py::SharedStorage> resample(
    const Score<Second, py::SharedStorage>&, i32, Tick::unit
);

}   // namespace symusic

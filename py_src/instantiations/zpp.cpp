#include "utils/storage.h"
#include "symusic/detail/zpp_impl.h"

namespace symusic {

template vec<u8>                        Track<Tick, py::SharedStorage>::dump_zpp_bytes() const;
template Track<Tick, py::SharedStorage> Track<Tick, py::SharedStorage>::parse_zpp_bytes(
    std::span<const u8>
);
template vec<u8> Track<Quarter, py::SharedStorage>::dump_zpp_bytes() const;
template Track<Quarter, py::SharedStorage> Track<Quarter, py::SharedStorage>::parse_zpp_bytes(
    std::span<const u8>
);
template vec<u8>                          Track<Second, py::SharedStorage>::dump_zpp_bytes() const;
template Track<Second, py::SharedStorage> Track<Second, py::SharedStorage>::parse_zpp_bytes(
    std::span<const u8>
);
template vec<u8>                        Score<Tick, py::SharedStorage>::dump_zpp_bytes() const;
template Score<Tick, py::SharedStorage> Score<Tick, py::SharedStorage>::parse_zpp_bytes(
    std::span<const u8>
);
template vec<u8> Score<Quarter, py::SharedStorage>::dump_zpp_bytes() const;
template Score<Quarter, py::SharedStorage> Score<Quarter, py::SharedStorage>::parse_zpp_bytes(
    std::span<const u8>
);
template vec<u8>                          Score<Second, py::SharedStorage>::dump_zpp_bytes() const;
template Score<Second, py::SharedStorage> Score<Second, py::SharedStorage>::parse_zpp_bytes(
    std::span<const u8>
);

namespace detail {
template vec<u8> dump_track_list<Tick, py::SharedStorage>(
    const shared<vec<shared<py::Track<Tick>>>>&
);
template vec<u8> dump_track_list<Quarter, py::SharedStorage>(
    const shared<vec<shared<py::Track<Quarter>>>>&
);
template vec<u8> dump_track_list<Second, py::SharedStorage>(
    const shared<vec<shared<py::Track<Second>>>>&
);

}   // namespace detail
}   // namespace symusic

#include "utils/storage.h"
#include "symusic/detail/track_impl.h"

namespace symusic {

template struct Track<Tick, py::SharedStorage>;
template struct Track<Quarter, py::SharedStorage>;
template struct Track<Second, py::SharedStorage>;

}   // namespace symusic

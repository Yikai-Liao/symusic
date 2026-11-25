#include "utils/storage.h"
#include "symusic/detail/score_impl.h"

namespace symusic {

template struct Score<Tick, py::SharedStorage>;
template struct Score<Quarter, py::SharedStorage>;
template struct Score<Second, py::SharedStorage>;

}   // namespace symusic

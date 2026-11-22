#include "symusic/detail/score_impl.h"

namespace symusic {

template struct Score<Tick, ValueStorage>;
template struct Score<Quarter, ValueStorage>;
template struct Score<Second, ValueStorage>;

}  // namespace symusic

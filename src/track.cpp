#include "symusic/detail/track_impl.h"

namespace symusic {

template struct Track<Tick, ValueStorage>;
template struct Track<Quarter, ValueStorage>;
template struct Track<Second, ValueStorage>;

}   // namespace symusic

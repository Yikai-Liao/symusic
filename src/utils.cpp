#include "symusic/utils.h"
#include "utf8.h"

#include <stdexcept>

namespace symusic::detail {

std::string strip_non_utf_8(const std::string& str) {
    std::string ans;
    utf8::replace_invalid(str.begin(), str.end(), std::back_inserter(ans));
    return ans;
}

void throw_i32_overflow(const i64 value) {
    throw std::overflow_error(
        "symusic: time value " + std::to_string(value) + " is out of the 32-bit tick range"
    );
}

void throw_f64_not_i32(const f64 value) {
    if (std::isnan(value)) { throw std::invalid_argument("symusic: time value is NaN"); }
    throw std::overflow_error(
        "symusic: time value " + std::to_string(value) + " is out of the 32-bit tick range"
    );
}

}   // namespace symusic::detail

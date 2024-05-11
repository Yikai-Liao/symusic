//
// Created by lyk on 23-12-16.
//
#include "symusic/utils.h"
#include "utf8.h"

namespace symusic::details {

std::string strip_non_utf_8(const std::string &str) {
  std::string ans;
  utf8::replace_invalid(str.begin(), str.end(), std::back_inserter(ans));
  return ans;
}

} // namespace symusic::details

//
// Created by lyk on 23-12-16.
//
#pragma once

#ifndef LIBSYMUSIC_UTILS_H
#define LIBSYMUSIC_UTILS_H

#include <string>

namespace symusic::details {
std::string strip_non_utf_8(const std::string & str);
}

#endif //LIBSYMUSIC_UTILS_H

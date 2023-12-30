//
// Created by lyk on 2023/12/24.
//
#pragma once

#ifndef LIBSYMUSIC_IO_COMMON_H
#define LIBSYMUSIC_IO_COMMON_H

#include <string>
#include <filesystem>
#include <span>

#include "symusic/mtype.h"

namespace symusic {

vec<u8> read_file(const std::filesystem::path & path);

vec<u8> read_file(const std::string & path);

void write_file(const std::filesystem::path & path, std::span<const u8> buffer);

void write_file(const std::string & path, std::span<const u8> buffer);

}

#endif //LIBSYMUSIC_IO_COMMON_H

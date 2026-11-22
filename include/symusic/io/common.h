#pragma once

#include <string>
#include <filesystem>
#include <span>

#include "symusic/mtype.h"

namespace symusic {

vec<u8> read_file(const std::filesystem::path& path);

vec<u8> read_file(const std::string& path);

void write_file(const std::filesystem::path& path, std::span<const u8> buffer);

void write_file(const std::string& path, std::span<const u8> buffer);

}   // namespace symusic

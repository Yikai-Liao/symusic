#pragma once

#include <string>
#include <filesystem>
#include <span>

#include "symusic/io/iodef.h"
#include "symusic/mtype.h"

namespace symusic {

// Choose the score format from an explicit name or a supported file extension.
DataFormat score_file_format(const std::filesystem::path& path, std::string format);

// Read a score from a regular file. Reject missing files and unseekable sources.
vec<u8> read_score_file(const std::filesystem::path& path);

vec<u8> read_file(const std::filesystem::path& path);

vec<u8> read_file(const std::string& path);

void write_file(const std::filesystem::path& path, std::span<const u8> buffer);

void write_file(const std::string& path, std::span<const u8> buffer);

}   // namespace symusic

//
// Created by lyk on 23-12-25.
//
#include <cstdio>
#include <stdexcept>

#include "symusic/io/common.h"

#ifdef _MSC_VER
#pragma warning(disable:4996)
#endif

namespace symusic {

vec<u8> read_file(const std::filesystem::path& path) {
    if(!exists(path)) { throw std::runtime_error("File not found"); }
    FILE * file = fopen(path.string().c_str(), "rb");
    if(!file) { throw std::runtime_error("File not found"); }
    fseek(file, 0, SEEK_END);
    const auto size = ftell(file);
    fseek(file, 0, SEEK_SET);
    vec<u8> buffer(size);
    fread(buffer.data(), 1, size, file);
    fclose(file);
    return buffer;
}

vec<u8> read_file(const std::string& path) {
    return read_file(std::filesystem::path(path));
}

void write_file(const std::filesystem::path& path, const std::span<const u8> buffer) {
    FILE * file = fopen(path.string().c_str(), "wb");
    if(!file) { throw std::runtime_error("File not found"); }
    fwrite(buffer.data(), 1, buffer.size(), file);
    fclose(file);
}

void write_file(const std::string& path, const std::span<const u8> buffer) {
    write_file(std::filesystem::path(path), buffer);
}

} // namespace symusic
#include <memory>
#include <span>
#include <stdexcept>
#include <utility>

#include "fmt/core.h"

#include "symusic/io/common.h"

#ifdef _MSC_VER
#pragma warning(disable : 4996)
#endif

#ifdef _WIN32
#include <Windows.h>

namespace {

std::string path_to_utf8(const std::filesystem::path& path) {
    const auto utf8 = path.u8string();
    return std::string(reinterpret_cast<const char*>(utf8.c_str()), utf8.size());
}

std::wstring to_utf16(const std::string& path) {
    const int wide_len = MultiByteToWideChar(CP_UTF8, 0, path.c_str(), -1, nullptr, 0);
    if (wide_len == 0) {
        throw std::runtime_error(fmt::format("Failed to convert UTF-8 path to UTF-16: {}", path));
    }

    // A nonzero API result is a positive length including the terminator.
    std::wstring wide_path(static_cast<size_t>(wide_len), L"\0"[0]);
    if (MultiByteToWideChar(CP_UTF8, 0, path.c_str(), -1, wide_path.data(), wide_len) == 0) {
        throw std::runtime_error(fmt::format("Failed to convert UTF-8 path to UTF-16: {}", path));
    }

    wide_path.pop_back();
    return wide_path;
}

}   // namespace
#endif

namespace symusic {

namespace {
vec<u8> read_file_contents(FILE* file) {
    const auto close_file = [](FILE* stream) { fclose(stream); };
    const std::unique_ptr<FILE, decltype(close_file)> handle(file, close_file);
    if (fseek(file, 0, SEEK_END) != 0) {
        throw std::runtime_error("Cannot seek to the end of the file");
    }
    const auto end = ftell(file);
    if (!std::in_range<size_t>(end) || fseek(file, 0, SEEK_SET) != 0) {
        throw std::runtime_error("Cannot determine the file size");
    }
    const auto size = static_cast<size_t>(end);
    vec<u8> buffer(size);
    if (size != 0 && fread(buffer.data(), 1, size, file) != size) {
        throw std::runtime_error("Cannot read the complete file");
    }
    return buffer;
}
}   // namespace

vec<u8> read_file(const std::string& path) {
#ifndef _WIN32
    FILE* fp = fopen(path.c_str(), "rb");
    if (fp == nullptr) { throw std::runtime_error(fmt::format("File not found file: {}", path)); }
#else   // deal with utf-8 path on windows
    FILE*         fp    = nullptr;
    const auto    wpath = to_utf16(path);
    const errno_t err   = _wfopen_s(&fp, wpath.c_str(), L"rb");
    if (err != 0 || fp == nullptr) {
        throw std::runtime_error(fmt::format("File not found file (error:{}): {}", err, path));
    }
#endif
    return read_file_contents(fp);
}

vec<u8> read_file(const std::filesystem::path& path) {
#ifndef _WIN32
    // On non-Windows, fallback to the string version for now.
    return read_file(path.string());
#else
    FILE*         fp  = nullptr;
    const errno_t err = _wfopen_s(&fp, path.c_str(), L"rb");
    if (err != 0 || fp == nullptr) {
        throw std::runtime_error(
            fmt::format("File not found file (error:{}): {}", err, path_to_utf8(path))
        );
    }
    return read_file_contents(fp);
#endif
}

void write_file(const std::string& path, const std::span<const u8> buffer) {
#ifndef _WIN32
    FILE* fp = fopen(path.c_str(), "wb");
#else   // deal with utf-8 path on windows
    FILE*         fp    = nullptr;
    const auto    wpath = to_utf16(path);
    const errno_t err   = _wfopen_s(&fp, wpath.c_str(), L"wb");
    if (err != 0 || fp == nullptr) {
        throw std::runtime_error(fmt::format("File not found file (error:{}): {}", err, path));
    }
#endif
    if (fp == nullptr) { throw std::runtime_error("File not found"); }
    fwrite(buffer.data(), 1, buffer.size(), fp);
    fclose(fp);
}

void write_file(const std::filesystem::path& path, const std::span<const u8> buffer) {
#ifndef _WIN32
    // On non-Windows, fallback to the string version for now.
    return write_file(path.string(), buffer);
#else
    FILE*         fp  = nullptr;
    const errno_t err = _wfopen_s(&fp, path.c_str(), L"wb");
    if (err != 0 || fp == nullptr) {
        throw std::runtime_error(
            fmt::format("File not found file (error:{}): {}", err, path_to_utf8(path))
        );
    }
    fwrite(buffer.data(), 1, buffer.size(), fp);
    fclose(fp);
#endif
}

}   // namespace symusic

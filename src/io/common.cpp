//
// Created by lyk on 23-12-25.
//

#include <stdexcept>

#include <span>

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

std::wstring ToUtf16(const std::string& str) {
    std::wstring ret;

    const int len = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), str.length(), NULL, 0);
    if (len > 0) {
        ret.resize(len);
        MultiByteToWideChar(CP_UTF8, 0, str.c_str(), str.length(), &ret[0], len);
    }
    return ret;
}

}   // namespace
#endif

namespace symusic {

vec<u8> read_file(const std::string& path) {
#ifndef _WIN32
    FILE* fp = fopen(path.c_str(), "rb");
    if (fp == nullptr) {
        throw std::runtime_error(fmt::format("File not found file: {}", path));
    }
#else   // deal with utf-8 path on windows
    FILE*         fp  = nullptr;
    const auto wpath = ToUtf16(path);
    const errno_t err = _wfopen_s(&fp, wpath.c_str(), L"rb");
    if (err != 0 || fp == nullptr) {
        throw std::runtime_error(fmt::format("File not found file (error:{}): {}", err, path));
    }
#endif
    fseek(fp, 0, SEEK_END);
    size_t size = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    std::vector<uint8_t> buffer(size);
    fread(buffer.data(), 1, size, fp);
    fclose(fp);
    return buffer;
}

vec<u8> read_file(const std::filesystem::path& path) {
#ifndef _WIN32
    // On non-Windows, fallback to the string version for now.
    return read_file(path.string());
#else
    FILE* fp = nullptr;
    const errno_t err = _wfopen_s(&fp, path.c_str(), L"rb");
    if (err != 0 || fp == nullptr) {
        throw std::runtime_error(
            fmt::format("File not found file (error:{}): {}", err, path_to_utf8(path))
        );
    }
    fseek(fp, 0, SEEK_END);
    size_t size = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    std::vector<uint8_t> buffer(size);
    fread(buffer.data(), 1, size, fp);
    fclose(fp);
    return buffer;
#endif
}

void write_file(const std::string& path, const std::span<const u8> buffer) {
#ifndef _WIN32
    FILE* fp = fopen(path.c_str(), "wb");
#else   // deal with utf-8 path on windows
    FILE*         fp  = nullptr;
    const auto wpath = ToUtf16(path);
    const errno_t err = _wfopen_s(&fp, wpath.c_str(), L"wb");
    if (err != 0 || fp == nullptr) {
        throw std::runtime_error(fmt::format("File not found file (error:{}): {}", err, path));
    }
#endif
    if (fp == nullptr) {
        throw std::runtime_error("File not found");
    }
    fwrite(buffer.data(), 1, buffer.size(), fp);
    fclose(fp);
}

void write_file(const std::filesystem::path& path, const std::span<const u8> buffer) {
#ifndef _WIN32
    // On non-Windows, fallback to the string version for now.
    return write_file(path.string(), buffer);
#else
    FILE* fp = nullptr;
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

//
// Created by lyk on 23-12-25.
//

#include <stdexcept>

#include "fmt/core.h"

#include "symusic/io/common.h"

#ifdef _MSC_VER
#pragma warning(disable:4996)
#endif

#ifdef _WIN32
#include <Windows.h>
std::wstring ToUtf16(const std::string & str)
{
    std::wstring ret;
    const int len = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), str.length(), NULL, 0);
    if (len > 0)
    {
        ret.resize(len);
        MultiByteToWideChar(CP_UTF8, 0, str.c_str(), str.length(), &ret[0], len);
    }
    return ret;
}
#endif

namespace symusic {

vec<u8> read_file(const std::string& path) {
#ifndef _WIN32
    FILE* fp = fopen(reinterpret_cast<const char*>(path.data()), "rb");
#else   // deal with utf-8 path on windows
    FILE* fp = _wfopen(ToUtf16(reinterpret_cast<const char*>(path.data())).c_str(), L"rb");
#endif
    if(fp == nullptr) {
        throw std::runtime_error(fmt::format("File not found: {}", path));
    }
    fseek(fp, 0, SEEK_END);
    size_t size = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    std::vector<uint8_t> buffer(size);
    fread(buffer.data(), 1, size, fp);
    return buffer;
}

vec<u8> read_file(const std::filesystem::path& path) {
    return read_file(path.string());
}

void write_file(const std::string& path, const std::span<const u8> buffer) {
#ifndef _WIN32
    FILE* fp = fopen(reinterpret_cast<const char*>(path.data()), "wb");
#else   // deal with utf-8 path on windows
    FILE* fp = _wfopen(ToUtf16(reinterpret_cast<const char*>(path.data())).c_str(), L"wb");
#endif
    if(fp == nullptr) {
        throw std::runtime_error("File not found");
    }
    fwrite(buffer.data(), 1, buffer.size(), fp);
    fclose(fp);
}

void write_file(const std::filesystem::path& path, const std::span<const u8> buffer) {
    return write_file(path.string(), buffer);
}

} // namespace symusic
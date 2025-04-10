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
std::wstring ToUtf16(const std::string& str) {
    std::wstring ret;

    printf("[ToUtf16] Input UTF-8 path: %s\n", str.c_str());

    const int len = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), str.length(), NULL, 0);
    printf("[ToUtf16] Calculated UTF-16 length: %d\n", len);
    if (len > 0) {
        ret.resize(len);
        MultiByteToWideChar(CP_UTF8, 0, str.c_str(), str.length(), &ret[0], len);
        printf("[ToUtf16] UTF-16 first 10 code units: ");
        for (int i = 0; i < len && i < 10; ++i) {
            printf("%04x ", static_cast<unsigned int>(ret[i]));
        }
        printf("\n");
    }
    return ret;
}
#endif

namespace symusic {

vec<u8> read_file(const std::string& path) {
#ifndef _WIN32
    FILE* fp = fopen(path.c_str(), "rb");
    if (fp == nullptr) {
        throw std::runtime_error(fmt::format("File not found file: {}", path));
    }
#else   // deal with utf-8 path on windows
    printf("[read_file] Input path: %s\n", path.c_str());
    FILE*         fp  = nullptr;
    const auto wpath = ToUtf16(path);
    const errno_t err = _wfopen_s(&fp, wpath.c_str(), L"rb");
    printf("[read_file] _wfopen_s returned: %d\n", static_cast<int>(err));
    if (err != 0 || fp == nullptr) {
        printf("[read_file] Failed to open file: %s\n", path.c_str());
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
    // On Windows, use the wide-character path directly with _wfopen_s
    printf("[read_file(fs::path)] Input path: %ls\n", path.c_str()); // Use %ls for wchar_t*
    FILE* fp = nullptr;
    const errno_t err = _wfopen_s(&fp, path.c_str(), L"rb");
    printf("[read_file(fs::path)] _wfopen_s returned: %d\n", static_cast<int>(err));
    if (err != 0 || fp == nullptr) {
        // Use path.string() for error message formatting if needed, but the primary issue is opening
        printf("[read_file(fs::path)] Failed to open file using wide path.\n");
        throw std::runtime_error(fmt::format("File not found file (error:{}): {}", err, path.string()));
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
    printf("[write_file] Input path: %s\n", path.c_str());
    FILE*         fp  = nullptr;
    const auto wpath = ToUtf16(path);
    const errno_t err = _wfopen_s(&fp, wpath.c_str(), L"wb");
    printf("[write_file] _wfopen_s returned: %d\n", static_cast<int>(err));
    if (err != 0 || fp == nullptr) {
        printf("[write_file] Failed to open file: %s\n", path.c_str());
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
    // On Windows, use the wide-character path directly with _wfopen_s
    printf("[write_file(fs::path)] Input path: %ls\n", path.c_str()); // Use %ls for wchar_t*
    FILE* fp = nullptr;
    const errno_t err = _wfopen_s(&fp, path.c_str(), L"wb");
    printf("[write_file(fs::path)] _wfopen_s returned: %d\n", static_cast<int>(err));
    if (err != 0 || fp == nullptr) {
        printf("[write_file(fs::path)] Failed to open file using wide path.\n");
        throw std::runtime_error(fmt::format("File not found file (error:{}): {}", err, path.string()));
    }
    fwrite(buffer.data(), 1, buffer.size(), fp);
    fclose(fp);
#endif
}

}   // namespace symusic

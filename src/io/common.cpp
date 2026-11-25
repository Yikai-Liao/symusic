#include <algorithm>
#include <cctype>
#include <cerrno>
#include <memory>
#include <span>
#include <stdexcept>
#include <system_error>
#include <utility>

#include "fmt/core.h"
#include "fmt/std.h"

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
    vec<u8>    buffer(size);
    if (size != 0 && fread(buffer.data(), 1, size, file) != size) {
        throw std::runtime_error("Cannot read the complete file");
    }
    return buffer;
}
}   // namespace

DataFormat score_file_format(const std::filesystem::path& path, std::string format) {
    if (format.empty()) {
        const auto extension = path.extension();
        if (extension == ".mid" || extension == ".midi" || extension == ".MID"
            || extension == ".MIDI")
            return DataFormat::MIDI;
        if (extension == ".abc") return DataFormat::ABC;
    } else {
        std::transform(format.begin(), format.end(), format.begin(), [](unsigned char ch) {
            return static_cast<char>(std::tolower(ch));
        });
        if (format == "midi" || format == "mid") return DataFormat::MIDI;
        if (format == "abc") return DataFormat::ABC;
    }
    throw std::invalid_argument("Unknown file format");
}

vec<u8> read_score_file(const std::filesystem::path& path) {
    std::error_code error;
    const auto      status = std::filesystem::status(path, error);
    if (error) { throw std::filesystem::filesystem_error("Cannot read score", path, error); }
    if (std::filesystem::is_directory(status)) {
        throw std::filesystem::filesystem_error(
            "Cannot read score", path, std::make_error_code(std::errc::is_a_directory)
        );
    }
    if (!std::filesystem::is_regular_file(status)) {
        throw std::runtime_error(
            fmt::format(
                "Cannot seek to the end of the file: path must refer to a regular file, got {}",
                path
            )
        );
    }
    return read_file(path);
}

vec<u8> read_file(const std::string& path) {
#ifdef _WIN32
    return read_file(std::filesystem::path(to_utf16(path)));
#else
    return read_file(std::filesystem::path(path));
#endif
}

vec<u8> read_file(const std::filesystem::path& path) {
#ifdef _WIN32
    FILE*      fp    = nullptr;
    const auto error = _wfopen_s(&fp, path.c_str(), L"rb");
#else
    FILE*      fp    = fopen(path.c_str(), "rb");
    const auto error = errno;
#endif
    if (fp == nullptr) {
        throw std::filesystem::filesystem_error(
            "Cannot open file for reading", path, std::error_code(error, std::generic_category())
        );
    }
    return read_file_contents(fp);
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

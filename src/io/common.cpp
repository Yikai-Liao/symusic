#include <algorithm>
#include <cctype>
#include <cerrno>
#include <memory>
#include <span>
#include <stdexcept>
#include <system_error>
#include <utility>

#include "fmt/core.h"

#include "symusic/detail/file_error.h"
#include "symusic/io/common.h"

#ifdef _MSC_VER
#pragma warning(disable : 4996)
#endif

#ifdef _WIN32
#include <Windows.h>

namespace {

std::wstring to_utf16(const std::string& path) {
    const int wide_len = MultiByteToWideChar(CP_UTF8, 0, path.c_str(), -1, nullptr, 0);
    if (wide_len == 0) {
        throw std::filesystem::filesystem_error(
            "Cannot convert UTF-8 path to UTF-16",
            std::error_code(static_cast<int>(GetLastError()), std::system_category())
        );
    }

    // A nonzero API result is a positive length including the terminator.
    std::wstring wide_path(static_cast<size_t>(wide_len), L"\0"[0]);
    if (MultiByteToWideChar(CP_UTF8, 0, path.c_str(), -1, wide_path.data(), wide_len) == 0) {
        throw std::filesystem::filesystem_error(
            "Cannot convert UTF-8 path to UTF-16",
            std::error_code(static_cast<int>(GetLastError()), std::system_category())
        );
    }

    wide_path.pop_back();
    return wide_path;
}

}   // namespace
#endif

namespace symusic {

namespace {
[[noreturn]] void throw_file_error(
    const char* operation, const std::filesystem::path& path, int error
) {
    throw std::filesystem::filesystem_error(
        operation, path, std::error_code(error ? error : EIO, std::generic_category())
    );
}

vec<u8> read_file_contents(FILE* file, const std::filesystem::path& path) {
    const auto close_file = [](FILE* stream) { fclose(stream); };
    const std::unique_ptr<FILE, decltype(close_file)> handle(file, close_file);
    if (fseek(file, 0, SEEK_END) != 0) {
        throw_file_error("Cannot seek to the end of the file", path, errno);
    }
    const auto end = ftell(file);
    if (!std::in_range<size_t>(end) || fseek(file, 0, SEEK_SET) != 0) {
        throw_file_error("Cannot determine the file size", path, errno);
    }
    const auto size = static_cast<size_t>(end);
    vec<u8>    buffer(size);
    if (size != 0 && fread(buffer.data(), 1, size, file) != size) {
        throw_file_error("Cannot read the complete file", path, ferror(file) ? errno : EIO);
    }
    return buffer;
}
}   // namespace

DataFormat score_file_format(const std::filesystem::path& path, std::string format) {
    if (format.empty()) {
        format = path.extension().string();
        std::transform(format.begin(), format.end(), format.begin(), [](unsigned char ch) {
            return static_cast<char>(std::tolower(ch));
        });
        if (format == ".mid" || format == ".midi") return DataFormat::MIDI;
        if (format == ".abc") return DataFormat::ABC;
    } else {
        if (format == "midi") return DataFormat::MIDI;
        if (format == "abc") return DataFormat::ABC;
    }
    throw std::invalid_argument("format must be 'midi' or 'abc', got " + format);
}

vec<u8> read_score_file(const std::filesystem::path& path) {
    const auto status = std::filesystem::status(path);
    if (!std::filesystem::exists(status)) {
        throw std::filesystem::filesystem_error(
            "Cannot read score", path, std::make_error_code(std::errc::no_such_file_or_directory)
        );
    }
    if (std::filesystem::is_directory(status)) {
        throw std::filesystem::filesystem_error(
            "Cannot read score", path, std::make_error_code(std::errc::is_a_directory)
        );
    }
    if (!std::filesystem::is_regular_file(status)) {
        throw_file_error("Cannot read a non-regular file", path, ESPIPE);
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
    return read_file_contents(fp, path);
}

void write_file(const std::string& path, std::span<const u8> buffer) {
#ifdef _WIN32
    write_file(std::filesystem::path(to_utf16(path)), buffer);
#else
    write_file(std::filesystem::path(path), buffer);
#endif
}

void write_file(const std::filesystem::path& path, std::span<const u8> buffer) {
#ifdef _WIN32
    FILE* fp = nullptr;
    const auto error = _wfopen_s(&fp, path.c_str(), L"wb");
#else
    FILE* fp = fopen(path.c_str(), "wb");
    const auto error = errno;
#endif
    if (!fp) {
        throw std::filesystem::filesystem_error(
            "Cannot open file for writing", path, detail::write_open_error(path, error)
        );
    }
    const bool complete = buffer.empty() || fwrite(buffer.data(), 1, buffer.size(), fp) == buffer.size();
    const auto write_error = errno;
    const auto close_result = fclose(fp);
    if (!complete) throw_file_error("Cannot write the complete file", path, write_error);
    if (close_result != 0) throw_file_error("Cannot close file after writing", path, errno);
}

}   // namespace symusic

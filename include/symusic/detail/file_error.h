#pragma once

#include <cerrno>
#include <filesystem>
#include <system_error>

namespace symusic::detail {

// Some platforms report EACCES when opening a directory for writing.
inline std::error_code write_open_error(const std::filesystem::path& path, int error) {
    std::error_code status_error;
    if (std::filesystem::is_directory(path, status_error)) {
        return std::make_error_code(std::errc::is_a_directory);
    }
    return std::error_code(error ? error : EIO, std::generic_category());
}

}   // namespace symusic::detail

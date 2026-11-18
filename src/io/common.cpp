#include "symusic/io/common.h"

#include <fstream>
#include <limits>
#include <stdexcept>
#include <string_view>

#include "fmt/core.h"

namespace symusic {
namespace {

std::string path_label(const std::filesystem::path& path) {
    const auto utf8 = path.u8string();
    return std::string(reinterpret_cast<const char*>(utf8.data()), utf8.size());
}

std::filesystem::path utf8_path(const std::string& path) {
#ifdef _WIN32
    return std::filesystem::path(std::u8string(path.begin(), path.end()));
#else
    return std::filesystem::path(path);
#endif
}

[[noreturn]] void file_error(std::string_view operation, const std::filesystem::path& path) {
    throw std::runtime_error(fmt::format("Could not {} file: {}", operation, path_label(path)));
}

}   // namespace

vec<u8> read_file(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary | std::ios::ate);
    if (!input) file_error("open input", path);
    const auto size = input.tellg();
    if (size < 0 || !input.seekg(0)) file_error("measure input", path);
    vec<u8> data;
    if (static_cast<uintmax_t>(size) > data.max_size()) file_error("measure input", path);
    data.resize(static_cast<size_t>(size));
    if (!data.empty() && !input.read(reinterpret_cast<char*>(data.data()), size))
        file_error("read input", path);
    return data;
}

vec<u8> read_file(const std::string& path) { return read_file(utf8_path(path)); }

void write_file(const std::filesystem::path& path, std::span<const u8> buffer) {
    std::ofstream output(path, std::ios::binary);
    if (!output) file_error("open output", path);
    if (buffer.size() > static_cast<uintmax_t>(std::numeric_limits<std::streamsize>::max()))
        file_error("write output", path);
    if (!buffer.empty())
        output.write(
            reinterpret_cast<const char*>(buffer.data()),
            static_cast<std::streamsize>(buffer.size())
        );
    output.close();
    if (!output) file_error("write output", path);
}

void write_file(const std::string& path, std::span<const u8> buffer) {
    write_file(utf8_path(path), buffer);
}

}   // namespace symusic

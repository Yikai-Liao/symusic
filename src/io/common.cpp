//
// Created by lyk on 23-12-25.
//

#include <stdexcept>
#include "fast_io_device.h"
#include "symusic/io/common.h"

#ifdef _MSC_VER
#pragma warning(disable:4996)
#endif

namespace symusic {

vec<u8> read_file(const std::u8string& path) {
    fast_io::native_file_loader loader(fast_io::mnp::os_c_str(path));
    vec<u8> buffer { loader.begin(), loader.end() };
    return buffer;
}

vec<u8> read_file(const std::filesystem::path& path) {
    return read_file(path.u8string());
}

void write_file(const std::u8string& path, const std::span<const u8> buffer) {
    fast_io::obuf_file obf(fast_io::mnp::os_c_str(path));
    fast_io::write(obf, buffer.begin(), buffer.end());
}

void write_file(const std::filesystem::path& path, const std::span<const u8> buffer) {
    return write_file(path.u8string(), buffer);
}

} // namespace symusic
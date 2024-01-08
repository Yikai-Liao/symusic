#include "cassert"
#include <vector>
#include "nanobench.h"
#include "fmt/core.h"
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

size_t read_file_win32(const std::string& path) {
    FILE* fp = _wfopen(ToUtf16(reinterpret_cast<const char*>(path.data())).c_str(), L"rb");
    if(fp == nullptr) {
        throw std::runtime_error("File not found");
    }
    fseek(fp, 0, SEEK_END);
    size_t size = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    std::vector<uint8_t> buffer(size);
    fread(buffer.data(), 1, size, fp);
    size_t ans = 0;
    for(auto const &byte : buffer) {
        ans += static_cast<size_t>(byte) % 4;
    }
    fclose(fp);
    return ans;
}
#endif

size_t read_file_fread(const std::string& path) {
    FILE* fp = fopen(path.c_str(), "rb");
    if(fp == nullptr) {
        throw std::runtime_error("File not found");
    }
    fseek(fp, 0, SEEK_END);
    size_t size = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    std::vector<uint8_t> buffer(size);
    fread(buffer.data(), 1, size, fp);
    size_t ans = 0;
    for(auto const &byte : buffer) {
        ans += static_cast<size_t>(byte) % 4;
    }
    fclose(fp);
    return ans;
}

// fstream
#include <fstream>
size_t read_file_fs_buf(const std::string& path) {
    std::ifstream ifs(path, std::ios::binary | std::ios::ate);
    if(!ifs) {
        throw std::runtime_error("File not found");
    }
    std::ifstream::pos_type pos = ifs.tellg();
    std::vector<uint8_t> buffer(pos);
    ifs.seekg(0, std::ios::beg);
    ifs.read(reinterpret_cast<char*>(buffer.data()), pos);
    size_t ans = 0;
    for(auto const &byte : buffer) {
        ans += static_cast<size_t>(byte) % 4;
    }
    return ans;
}

size_t read_file_fs_get(const std::string& path) {
    std::ifstream ifs(path, std::ios::binary | std::ios::ate);
    if(!ifs) {
        throw std::runtime_error("File not found");
    }
    std::ifstream::pos_type pos = ifs.tellg();
    ifs.seekg(0, std::ios::beg);
    size_t ans = 0;
    for(size_t i=0; i<pos; ++i) {
        ans += static_cast<size_t>(ifs.get()) % 4;
    }
    return ans;
}

// std::filebuf
#include <fstream>
size_t read_file_filebuf(const std::string& path) {
    std::filebuf fb;
    if (fb.open(path, std::ios::in | std::ios::binary)) {
        std::vector<uint8_t> buffer(fb.pubseekoff(0, std::ios::end, std::ios::in));
        fb.pubseekpos(0, std::ios::in);
        fb.sgetn(reinterpret_cast<char*>(buffer.data()), buffer.size());
        size_t ans = 0;
        for(auto const &byte : buffer) {
            ans += static_cast<size_t>(byte) % 4;
        }
        return ans;
    } else {
        throw std::runtime_error("File not found");
    }
}

int main(int argc, char *argv[]) {
    if(argc == 2) {
        const std::string filename {argv[1]};

        ankerl::nanobench::Bench().minEpochIterations(100)
        .run("fread raw", [&] {
            ankerl::nanobench::doNotOptimizeAway(read_file_fread(filename));
        })
#ifdef _WIN32
        .run("fread win32", [&] {
            ankerl::nanobench::doNotOptimizeAway(read_file_win32(filename));
        })
#endif
        .run("fstream read", [&] {
            ankerl::nanobench::doNotOptimizeAway(read_file_fs_buf(filename));
        })
        .run("fstream get", [&] {
            ankerl::nanobench::doNotOptimizeAway(read_file_fs_get(filename));
        })
        .run("filebuf", [&] {
            ankerl::nanobench::doNotOptimizeAway(read_file_filebuf(filename));
        });
    } else {
        // std::cout << "Usage: ./note_count <midi_file_name>" << std::endl;
        fmt::print("Usage: ./note_count <midi_file_name>/n");
    }

    return 0;
}

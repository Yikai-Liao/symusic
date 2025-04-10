/**
 * @file test_multilang_path.cpp
 * @brief Test compatibility of symusic::read_file and symusic::write_file with multilingual paths.
 *
 * This test case covers reading and writing binary files with paths containing
 * Chinese, Arabic, Japanese, Korean, and special characters.
 * It verifies the correct handling of UTF-8 multilingual paths on the Windows platform.
 */

#include <iostream>
#include <vector>
#include <string>
#include <cassert>

#include "symusic/io/common.h"

int main() {
    using namespace symusic;

    // Test filenames in multiple languages
    const std::vector<std::string> filenames = {
        "tests/testcases/测试文件.midi",
        "tests/testcases/ملف_اختبار.midi",
        "tests/testcases/テストファイル.midi",
        "tests/testcases/테스트파일.midi",
        "tests/testcases/文件_#@!.midi"
    };

    for (const auto& filename : filenames) {
        try {
            std::cout << "Reading file: " << filename << std::endl;
            auto data = read_file(filename);
            std::cout << "Bytes read: " << data.size() << std::endl;

            std::string copy_name = filename.substr(0, filename.find_last_of('.')) + "_copy.midi";
            std::cout << "Writing file: " << copy_name << std::endl;
            write_file(copy_name, data);

            auto data_copy = read_file(copy_name);
            std::cout << "Bytes in copied file: " << data_copy.size() << std::endl;

            assert(data == data_copy);
            std::cout << "File contents match. Test passed.\n" << std::endl;
        } catch (const std::exception& e) {
            std::cerr << "Exception occurred while processing file: " << filename << "\nError message: " << e.what() << "\n" << std::endl;
        }
    }

    std::cout << "Multilingual path read/write test completed." << std::endl;
    return 0;
}
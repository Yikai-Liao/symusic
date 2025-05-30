#pragma once
#ifndef SYMUSIC_TEST_COMMON_IO_HPP
#define SYMUSIC_TEST_COMMON_IO_HPP

#include <filesystem>
#include <fstream>
#include <string>
#include <vector>
#include "symusic/io/common.h"
#include "catch2/catch_test_macros.hpp"
using namespace symusic;
namespace fs = std::filesystem;

/**
 * Test suite for common I/O operations
 * These tests verify that reading and writing files works correctly
 */
TEST_CASE("Test Common File I/O Functions", "[symusic][io][common]") {
    // Create a temporary directory for test files
    fs::path temp_dir = fs::temp_directory_path() / "symusic_test_common_io";
    if (!fs::exists(temp_dir)) {
        fs::create_directory(temp_dir);
    }

    SECTION("Read and Write File with String Path") {
        // Create test data
        std::vector<uint8_t> test_data = {0x01, 0x02, 0x03, 0x04, 0x05};

        // Create a test file path as string
        std::string test_file_path = (temp_dir / "test_file.bin").string();

        // Write the test data to the file
        write_file(test_file_path, test_data);

        // Read the file back
        std::vector<uint8_t> read_data = read_file(test_file_path);

        // Verify the data
        REQUIRE(read_data.size() == test_data.size());
        for (size_t i = 0; i < test_data.size(); ++i) {
            REQUIRE(read_data[i] == test_data[i]);
        }
    }

    SECTION("Read and Write File with Filesystem Path") {
        // Create test data
        std::vector<uint8_t> test_data = {0x06, 0x07, 0x08, 0x09, 0x0A};

        // Create a test file path as filesystem path
        fs::path test_file_path = temp_dir / "test_file_fs.bin";

        // Write the test data to the file
        write_file(test_file_path, test_data);

        // Read the file back
        std::vector<uint8_t> read_data = read_file(test_file_path);

        // Verify the data
        REQUIRE(read_data.size() == test_data.size());
        for (size_t i = 0; i < test_data.size(); ++i) {
            REQUIRE(read_data[i] == test_data[i]);
        }
    }

    SECTION("Attempt to Read Non-existent File") {
        // Attempt to read a file that doesn't exist
        fs::path non_existent_file = temp_dir / "non_existent_file.bin";

        // Check that an exception is thrown
        REQUIRE_THROWS_AS(read_file(non_existent_file), std::runtime_error);
    }

    // Clean up the temporary directory
    fs::remove_all(temp_dir);
}

#endif // SYMUSIC_TEST_COMMON_IO_HPP

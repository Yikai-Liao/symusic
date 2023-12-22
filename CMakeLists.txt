cmake_minimum_required(VERSION 3.20)
project(symusic)

set(CMAKE_CXX_STANDARD 20)

option(BUILD_TEST "Build the test target" OFF)

include_directories(include)
include_directories(3rdparty/minimidi/include)
include_directories(3rdparty/pdqsort)
include_directories(3rdparty/zpp_bits)

find_package(Python REQUIRED COMPONENTS Interpreter Development.Module)
find_package(pybind11 CONFIG REQUIRED)

if(MSVC)
    add_compile_options(-bigobj)
endif()

python_add_library(core MODULE src/symusic.cpp WITH_SOABI)
target_link_libraries(core PRIVATE pybind11::headers)

if(BUILD_TEST)
    add_executable(note_count test/note_count.cpp)
    add_executable(dump test/dump.cpp)
endif()

if(NOT MSVC AND NOT ${CMAKE_BUILD_TYPE} MATCHES Debug|RelWithDebInfo)
    # Strip unnecessary sections of the binary on Linux/macOS
    pybind11_strip(core)
endif()

install(TARGETS core LIBRARY DESTINATION symusic)

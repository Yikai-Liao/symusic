cmake_minimum_required(VERSION 3.20)
project(pyscore)

set(CMAKE_CXX_STANDARD 17)

include_directories(include)
include_directories(3rdparty/minimidi/include)
include_directories(3rdparty/pdqsort)

# find_package(Python REQUIRED COMPONENTS Interpreter Development)
# find_package(pybind11 REQUIRED CONFIG)
add_subdirectory(3rdparty/eigen)
add_subdirectory(3rdparty/pybind11)

pybind11_add_module(pyscore src/pyscore.cpp)
add_executable(note_count test/note_count.cpp)

if(NOT MSVC AND NOT ${CMAKE_BUILD_TYPE} MATCHES Debug|RelWithDebInfo)
    # Strip unnecessary sections of the binary on Linux/macOS
    pybind11_strip(pyscore)
endif()
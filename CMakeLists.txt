cmake_minimum_required(VERSION 3.20)
project(symusic)

set(CMAKE_CXX_STANDARD 17)

option(BUILD_TEST "Build the test target" OFF)

include_directories(include)
include_directories(3rdparty/minimidi/include)
include_directories(3rdparty/pdqsort)

set(Python_LIBRARIES "${PYTHON_LIBRARIES}")
find_package(Python REQUIRED COMPONENTS Interpreter Development.Module)

add_subdirectory(3rdparty/eigen EXCLUDE_FROM_ALL)
add_subdirectory(3rdparty/pybind11 EXCLUDE_FROM_ALL)

pybind11_add_module(core src/symusic.cpp)
target_link_libraries(core PRIVATE eigen)

if(BUILD_TEST)
    add_executable(note_count test/note_count.cpp)
    target_link_libraries(note_count PRIVATE eigen)
endif()

if(NOT MSVC AND NOT ${CMAKE_BUILD_TYPE} MATCHES Debug|RelWithDebInfo)
    # Strip unnecessary sections of the binary on Linux/macOS
    pybind11_strip(core)
endif()

cmake_minimum_required(VERSION 3.20)
project(symusic)

set(CMAKE_CXX_STANDARD 20)

option(BUILD_TEST "Build the test target" OFF)
option(BUILD_SHARED_LIBS "Build libraries as shared as opposed to static" ON)

find_package(Python REQUIRED COMPONENTS Interpreter Development.Module)
find_package(pybind11 CONFIG REQUIRED)

if(MSVC)
    add_compile_options(-bigobj)
endif()

add_compile_options(-fno-lto)

add_subdirectory(3rdparty/libsymusic/3rdparty/fmt EXCLUDE_FROM_ALL)
# add_subdirectory(3rdparty/libsymusic EXCLUDE_FROM_ALL)
file(GLOB_RECURSE libsymusic_src 3rdparty/libsymusic/src/*.cpp 3rdparty/libsymusic/src/io/*.cpp)

python_add_library(core MODULE src/symusic.cpp ${libsymusic_src} WITH_SOABI )
target_include_directories(core PUBLIC 3rdparty/libsymusic/include)
target_link_libraries(core PUBLIC fmt::fmt-header-only)
target_include_directories(core PUBLIC  3rdparty/libsymusic/3rdparty/pdqsort)
target_include_directories(core PUBLIC  3rdparty/libsymusic/3rdparty/minimidi/include)
target_include_directories(core PUBLIC  3rdparty/libsymusic/3rdparty/zpp_bits)

target_link_libraries(core PRIVATE pybind11::headers)
set_target_properties(core PROPERTIES
    INTERPROCEDURAL_OPTIMIZATION ON
    VISIBILITY_INLINES_HIDDEN ON
)

if(BUILD_TEST)
    add_executable(note_count test/note_count.cpp)
    add_executable(dump test/dump.cpp)
endif()

if(NOT MSVC AND NOT ${CMAKE_BUILD_TYPE} MATCHES Debug|RelWithDebInfo)
    # Strip unnecessary sections of the binary on Linux/macOS
    pybind11_strip(core)
endif()

install(TARGETS core LIBRARY DESTINATION symusic)

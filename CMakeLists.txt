cmake_minimum_required(VERSION 3.20)
project(symusic)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

option(BUILD_PY "Build the python binding of symusic" OFF)
option(BUILD_TEST "Build the test target" OFF)
option(ENABLE_LTO "Enables link-time optimization, requires compiler support." ON)

add_subdirectory(3rdparty/fmt EXCLUDE_FROM_ALL)

include_directories(include)
include_directories(3rdparty/minimidi/include)
include_directories(3rdparty/pdqsort)
include_directories(3rdparty/zpp_bits)

file(GLOB_RECURSE symusic_src src/*.cpp src/io/*.cpp)

foreach(src_file ${symusic_src})
    message(STATUS "symusic_src: ${src_file}")
endforeach()

if(MSVC)
    add_compile_options(-bigobj)
endif()

add_library(symusic ${symusic_src})
target_link_libraries(symusic fmt::fmt-header-only)

if(BUILD_PY)
    message("Building python binding.")

    find_package(Python REQUIRED COMPONENTS Interpreter Development.Module)
    find_package(pybind11 CONFIG REQUIRED)

    python_add_library(core MODULE py_src/core.cpp WITH_SOABI)
    target_link_libraries(core PRIVATE pybind11::headers)
    target_link_libraries(core PRIVATE symusic)

    if(BUILD_TEST)
        add_executable(note_count tests/note_count.cpp)
        add_executable(dump tests/dump.cpp)
    endif()

    if(NOT MSVC AND NOT ${CMAKE_BUILD_TYPE} MATCHES Debug|RelWithDebInfo)
        # Strip unnecessary sections of the binary on Linux/macOS
        pybind11_strip(core)
    endif()

    install(TARGETS core LIBRARY DESTINATION symusic)
endif()

if (ENABLE_LTO)
    include(CheckIPOSupported)
    check_ipo_supported(RESULT has_lto OUTPUT lto_check_output)
    if(has_lto)
        if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
            message(STATUS "Clang detected, use -fuse-ld=lld")
            set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fuse-ld=lld")
        endif ()
        message(STATUS "Link-time optimization (LTO) enabled")
        set_property(TARGET symusic PROPERTY INTERPROCEDURAL_OPTIMIZATION TRUE)
    else()
        message(WARNING "Link-time optimization (LTO) is not supported: \n${lto_check_output}")
    endif()
endif()

# enable parallel compilation for MSVC
if(MSVC)
    message(STATUS "MSVC parallel compilation enabled")
    add_compile_options($<$<CXX_COMPILER_ID:MSVC>:/Gm->)
    cmake_host_system_information(RESULT CPU_NUMBER_OF_LOGICAL_CORES QUERY NUMBER_OF_LOGICAL_CORES)
    add_compile_options($<$<CXX_COMPILER_ID:MSVC>:/MP${CPU_NUMBER_OF_LOGICAL_CORES}>)
endif()
cmake_minimum_required(VERSION 3.20)
project(symusic)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_OSX_DEPLOYMENT_TARGET 10.15)

option(BUILD_PY "Build the python binding of symusic" OFF)
option(BUILD_TEST "Build the test target" OFF)
option(ENABLE_LTO "Enables link-time optimization, requires compiler support." ON)
option(VTUNE "Compile Options for Intel VTune" Off)


# set /utf-8 for MSVC
if(MSVC)
    add_compile_options(-bigobj)
    add_compile_options(/utf-8)
endif()


add_subdirectory(3rdparty/fmt EXCLUDE_FROM_ALL)
add_subdirectory(3rdparty/minimidi EXCLUDE_FROM_ALL)
add_subdirectory(3rdparty/prestosynth EXCLUDE_FROM_ALL)

include_directories(include)
include_directories(3rdparty/pdqsort)
include_directories(3rdparty/zpp_bits)
include_directories(3rdparty)

file(GLOB_RECURSE symusic_src src/*.cpp src/io/*.cpp)

foreach(src_file ${symusic_src})
    message(STATUS "symusic_src: ${src_file}")
endforeach()

add_library(symusic ${symusic_src})
target_link_libraries(symusic fmt::fmt-header-only)
target_link_libraries(symusic minimidi)
target_link_libraries(symusic prestosynth)

if(BUILD_PY)
    message("Building python binding.")
    find_package(Python REQUIRED COMPONENTS Interpreter Development.Module)
    # set symusic target to O3 and release mode because nanobind use minisize
    execute_process(
        COMMAND "${Python_EXECUTABLE}" -m nanobind --cmake_dir
        OUTPUT_STRIP_TRAILING_WHITESPACE OUTPUT_VARIABLE NB_DIR)
        list(APPEND CMAKE_PREFIX_PATH "${NB_DIR}")
        find_package(nanobind CONFIG REQUIRED)
    find_package(nanobind CONFIG REQUIRED)

    nanobind_add_module(core NOMINSIZE  py_src/core.cpp)
    target_link_libraries(core PRIVATE symusic)

    install(TARGETS core LIBRARY DESTINATION symusic)
    # install abc2midi and midi2abc
    add_subdirectory(3rdparty/abcmidi)
    install(TARGETS abc2midi midi2abc DESTINATION symusic/bin)
endif()

if(BUILD_TEST)
    add_subdirectory(./3rdparty/nanobench EXCLUDE_FROM_ALL)

    add_executable(note_count tests/note_count.cpp)
    add_executable(dump tests/dump.cpp)
    add_executable(io_bench tests/io_bench.cpp)
    add_executable(synth tests/synth.cpp)
    add_executable(sequence tests/sequence.cpp)
    add_executable(adjust_time tests/adjust_time.cpp)

    target_link_libraries(note_count PRIVATE symusic)
    target_link_libraries(dump PRIVATE symusic)
    target_link_libraries(io_bench PRIVATE symusic nanobench)
    target_link_libraries(synth PUBLIC symusic)
    target_link_libraries(sequence PUBLIC symusic)
    target_link_libraries(adjust_time PUBLIC symusic)

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
        if(BUILD_PY)
            set_property(TARGET core PROPERTY INTERPROCEDURAL_OPTIMIZATION TRUE)
        endif ()
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

if(VTUNE)
    message(STATUS "VTune enabled")
    if(MSVC)
        add_compile_options(/Zi)
        add_compile_options(/MDd)
        add_compile_options(/D "TBB_USE_THREADING_TOOLS")
    else()
        add_compile_options(-g)
    endif ()
endif ()

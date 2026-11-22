cmake_minimum_required(VERSION 3.20)
if(POLICY CMP0137)
    # LTO probes need the cross-toolchain variables in their test project.
    cmake_policy(SET CMP0137 NEW)
endif()
set(CMAKE_OSX_DEPLOYMENT_TARGET "13.0" CACHE STRING "Minimum supported macOS version")
project(symusic LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_RULE_MESSAGES OFF CACHE BOOL "Show individual Makefile compile and link steps")
set_property(GLOBAL PROPERTY RULE_MESSAGES ${CMAKE_RULE_MESSAGES})

include(cmake/BuildOptions.cmake)
include(cmake/Dependencies.cmake)

add_library(symusic
    src/conversion.cpp
    src/event.cpp
    src/io/common.cpp
    src/io/midi.cpp
    src/io/zpp.cpp
    src/pianoroll.cpp
    src/repr.cpp
    src/score.cpp
    src/synth.cpp
    src/track.cpp
    src/utils.cpp
)
target_include_directories(symusic
    PUBLIC include 3rdparty 3rdparty/pdqsort
    PRIVATE 3rdparty/zpp_bits
)
find_package(Threads REQUIRED)
target_link_libraries(symusic
    PUBLIC fmt::fmt-header-only minimidi pyvec
    PRIVATE Threads::Threads ${CMAKE_DL_LIBS}
)
set_property(TARGET symusic PROPERTY INTERPROCEDURAL_OPTIMIZATION ${SYMUSIC_CORE_LTO})
if(SYMUSIC_CORE_LTO)
    # Consumers must link the core's LTO objects, without enabling IPO on
    # their own compilation. CMake supplies the compiler-specific flags.
    if(MSVC)
        target_link_options(symusic INTERFACE ${CMAKE_CXX_LINK_OPTIONS_IPO})
    else()
        target_link_options(symusic INTERFACE
            ${CMAKE_CXX_COMPILE_OPTIONS_IPO} ${CMAKE_CXX_LINK_OPTIONS_IPO})
    endif()
endif()

if(SYMUSIC_BUILD_PYTHON)
    add_subdirectory(py_src)
endif()
if(SYMUSIC_BUILD_EXAMPLES)
    add_subdirectory(examples/cpp)
endif()
if(SYMUSIC_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests/cpp)
endif()

message(STATUS
    "symusic: Python=${SYMUSIC_BUILD_PYTHON}, tests=${SYMUSIC_BUILD_TESTS}, "
    "examples=${SYMUSIC_BUILD_EXAMPLES}, fast=${SYMUSIC_FAST_BUILD}, core LTO=${SYMUSIC_CORE_LTO}"
)

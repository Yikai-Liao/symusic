cmake_minimum_required(VERSION 3.20)
if(POLICY CMP0137)
    # LTO probes need the cross-toolchain variables in their test project.
    cmake_policy(SET CMP0137 NEW)
endif()
set(CMAKE_POLICY_VERSION_MINIMUM 3.5)
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
include(GNUInstallDirs)

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
    PUBLIC
        $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
        $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/3rdparty>
        $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/3rdparty/pdqsort>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/symusic/vendor>
    PRIVATE 3rdparty/zpp_bits
)
target_compile_features(symusic PUBLIC cxx_std_20)
find_package(Threads REQUIRED)
target_link_libraries(symusic PUBLIC
    $<BUILD_INTERFACE:fmt::fmt-header-only>
    $<BUILD_INTERFACE:minimidi>
    PRIVATE Threads::Threads ${CMAKE_DL_LIBS}
)
target_compile_definitions(symusic INTERFACE $<INSTALL_INTERFACE:FMT_HEADER_ONLY>)
add_library(symusic::symusic ALIAS symusic)
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
else()
    include(cmake/InstallCore.cmake)
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

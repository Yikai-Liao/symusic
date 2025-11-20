cmake_minimum_required(VERSION 3.20)
project(symusic)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_OSX_DEPLOYMENT_TARGET 11.0)

option(BUILD_SYMUSIC_PY "Build the python binding of symusic" OFF)
option(BUILD_SYMUSIC_EXAMPLE "Build the test target" OFF)
option(BUILD_SYMUSIC_TEST "Build the test target" OFF)
option(ENABLE_LTO "Enables link-time optimization, requires compiler support." ON)
option(SYMUSIC_FAST_BUILD "Use O1 and disable LTO for faster local builds" OFF)
option(VTUNE "Compile Options for Intel VTune" Off)
option(MEM_LEAK_WARNING "Enable memory leak warning" OFF)
option(ENABLE_COVERAGE "Enable code coverage" OFF)

# Enable code coverage if requested
if(ENABLE_COVERAGE)
    message(STATUS "Code coverage enabled")
    if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -fprofile-arcs -ftest-coverage")
        set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} --coverage")
    else()
        message(WARNING "Code coverage is only supported with GCC and Clang")
    endif()
endif()

# set /utf-8 for MSVC
if(MSVC)
    add_compile_options(-bigobj)
    add_compile_options(/utf-8)
endif()

# Add multilingual path test executable
add_executable(test_multilang_path tests/cpp/test_multilang_path.cpp src/io/common.cpp)
target_include_directories(test_multilang_path PRIVATE include)
target_link_libraries(test_multilang_path symusic)

if(MEM_LEAK_WARNING)
    message(STATUS "Nanobind memory leak warning ENABLED")
    add_definitions(-DMEM_LEAK_WARNING)
else ()
    message(STATUS "Nanobind memory leak warning DISABLED")
endif()

add_subdirectory(3rdparty/fmt EXCLUDE_FROM_ALL)
add_subdirectory(3rdparty/minimidi EXCLUDE_FROM_ALL)
add_subdirectory(3rdparty/prestosynth EXCLUDE_FROM_ALL)
add_subdirectory(3rdparty/pyvec EXCLUDE_FROM_ALL)

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
target_link_libraries(symusic pyvec)

if(SYMUSIC_FAST_BUILD)
    message(STATUS "SYMUSIC_FAST_BUILD enabled: disabling LTO and using O1 optimizations")
    set(ENABLE_LTO OFF)
    if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
        set(CMAKE_C_FLAGS_RELEASE "-O1 -DNDEBUG")
        set(CMAKE_CXX_FLAGS_RELEASE "-O1 -DNDEBUG")
        set(CMAKE_C_FLAGS_RELWITHDEBINFO "-O1 -g -DNDEBUG")
        set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "-O1 -g -DNDEBUG")
    elseif(MSVC)
        set(CMAKE_C_FLAGS_RELEASE "/O1 /DNDEBUG")
        set(CMAKE_CXX_FLAGS_RELEASE "/O1 /DNDEBUG")
        set(CMAKE_C_FLAGS_RELWITHDEBINFO "/O1 /Zi /DNDEBUG")
        set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "/O1 /Zi /DNDEBUG")
    endif()
endif()

if(BUILD_SYMUSIC_PY)
    message("Building python binding.")
    find_package(Python REQUIRED COMPONENTS Interpreter Development.Module)
    # set symusic target to O3 and release mode because nanobind use minisize
#   execute_process(
#        COMMAND "${Python_EXECUTABLE}" -m nanobind --cmake_dir
#        OUTPUT_STRIP_TRAILING_WHITESPACE OUTPUT_VARIABLE NB_DIR)
#        list(APPEND CMAKE_PREFIX_PATH "${NB_DIR}")
#   find_package(nanobind CONFIG REQUIRED)
    # show python executable path
    message(STATUS "Python_EXECUTABLE: ${Python_EXECUTABLE}")

    add_subdirectory(3rdparty/nanobind EXCLUDE_FROM_ALL)

    set(NB_FLAGS NB_STATIC STABLE_ABI PROTECT_STACK NOSTRIP)
    if(NOT SYMUSIC_FAST_BUILD)
        list(APPEND NB_FLAGS LTO)
    else()
        list(APPEND NB_FLAGS NOMINSIZE)
    endif()
    nanobind_add_module(
        core
        ${NB_FLAGS}
        NB_DOMAIN symusic
        py_src/bindings/core/module.cpp
        py_src/bindings/synth/synthesizer_bindings.cpp
        py_src/bindings/track/track_bindings.h
        py_src/bindings/score/score_bindings.h
        py_src/bindings/events/event_bindings.h
        py_src/bindings/core/binding_common.h
        py_src/utils/python_helpers.h
        py_src/utils/vector_bindings.h
    )
    target_link_libraries(core PRIVATE symusic)

    install(TARGETS core LIBRARY DESTINATION symusic)

    nanobind_add_stub(
        symusic_core_stub
        INSTALL_TIME
        MODULE symusic.core
        OUTPUT symusic/core.pyi
        MARKER_FILE symusic/py.typed
        PYTHON_PATH "."
    )
#   install abc2midi and midi2abc
    add_subdirectory(3rdparty/abcmidi)
    install(TARGETS abc2midi midi2abc DESTINATION symusic/bin)
endif()

if(BUILD_SYMUSIC_EXAMPLE)
    add_subdirectory(./3rdparty/nanobench EXCLUDE_FROM_ALL)

    add_executable(note_count examples/cpp/note_count.cpp)
    add_executable(dump examples/cpp/dump.cpp)
    add_executable(io_bench examples/cpp/io_bench.cpp)
    add_executable(synth examples/cpp/synth.cpp)
    add_executable(adjust_time examples/cpp/adjust_time.cpp)
    add_executable(process_midi_directory examples/cpp/process_midi_directory.cpp)

    target_link_libraries(note_count PRIVATE symusic)
    target_link_libraries(dump PRIVATE symusic)
    target_link_libraries(io_bench PRIVATE symusic nanobench)
    target_link_libraries(synth PUBLIC symusic)
    target_link_libraries(adjust_time PUBLIC symusic)
    target_link_libraries(process_midi_directory PRIVATE symusic)
endif()

if(BUILD_SYMUSIC_TEST)
    Include(FetchContent)
    FetchContent_Declare(
            Catch2
            GIT_REPOSITORY https://github.com/catchorg/Catch2.git
            GIT_TAG v3.4.0 # or a later release
    )
    FetchContent_MakeAvailable(Catch2)
    list(APPEND CMAKE_MODULE_PATH ${catch2_SOURCE_DIR}/extras)
    add_executable(symusic_test tests/cpp/test_main.cpp)
    target_include_directories(symusic_test PRIVATE tests/cpp)
    target_link_libraries(symusic_test PRIVATE Catch2::Catch2WithMain symusic)
    include(CTest)
    include(Catch)
    enable_testing()
    add_test(
        NAME symusic_ctest
        COMMAND $<TARGET_FILE:symusic_test> --success
    )
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
        if(BUILD_SYMUSIC_PY)
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

set_property(GLOBAL PROPERTY RULE_LAUNCH_COMPILE "${CMAKE_COMMAND} -E time")
set_property(GLOBAL PROPERTY RULE_LAUNCH_LINK "${CMAKE_COMMAND} -E time")

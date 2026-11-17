# Names used before 0.6.1. An old name applies only when the new one is unset;
# remove the aliases after the next minor release (see ROADMAP.md).
foreach(_symusic_alias IN ITEMS
        BUILD_SYMUSIC_PY:SYMUSIC_BUILD_PYTHON
        BUILD_SYMUSIC_EXAMPLE:SYMUSIC_BUILD_EXAMPLES
        BUILD_SYMUSIC_TEST:SYMUSIC_BUILD_TESTS
        ENABLE_LTO:SYMUSIC_ENABLE_LTO
        ENABLE_COVERAGE:SYMUSIC_ENABLE_COVERAGE
        MEM_LEAK_WARNING:SYMUSIC_WARN_ON_LEAKS)
    string(REPLACE ":" ";" _symusic_alias "${_symusic_alias}")
    list(GET _symusic_alias 0 _symusic_old)
    list(GET _symusic_alias 1 _symusic_new)
    if(DEFINED ${_symusic_old} AND NOT DEFINED ${_symusic_new})
        message(DEPRECATION "${_symusic_old} is deprecated; use ${_symusic_new}")
        set(${_symusic_new} "${${_symusic_old}}" CACHE BOOL "Set from deprecated ${_symusic_old}")
    endif()
endforeach()

option(SYMUSIC_BUILD_PYTHON "Build the Python extension and bundled ABC tools" OFF)
option(SYMUSIC_BUILD_EXAMPLES "Build the C++ examples" OFF)
option(SYMUSIC_BUILD_TESTS "Build the C++ tests" OFF)
option(SYMUSIC_ENABLE_LTO "Enable LTO in the C++ core, without LTO in Python bindings" ON)
option(SYMUSIC_FAST_BUILD "Use O1 and disable LTO for local iteration" OFF)
option(SYMUSIC_WARN_ON_LEAKS "Enable nanobind leak reports at Python shutdown" OFF)
option(SYMUSIC_ENABLE_COVERAGE "Enable C++ code coverage" OFF)
option(SYMUSIC_TIME_BUILD_RULES "Time individual compile and link rules" OFF)

if(MSVC)
    add_compile_options(/utf-8 "$<$<COMPILE_LANGUAGE:CXX>:/bigobj>" "$<$<COMPILE_LANGUAGE:CXX>:/Gm->")
    # Visual Studio generators need /MP in the language flags to compile in parallel.
    string(APPEND CMAKE_C_FLAGS " /MP")
    string(APPEND CMAKE_CXX_FLAGS " /MP")
endif()

if(SYMUSIC_FAST_BUILD)
    if(MSVC)
        set(CMAKE_C_FLAGS_RELEASE "/O1 /DNDEBUG")
        set(CMAKE_CXX_FLAGS_RELEASE "/O1 /DNDEBUG")
        set(CMAKE_C_FLAGS_RELWITHDEBINFO "/O1 /Zi /DNDEBUG")
        set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "/O1 /Zi /DNDEBUG")
    elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
        set(CMAKE_C_FLAGS_RELEASE "-O1 -DNDEBUG")
        set(CMAKE_CXX_FLAGS_RELEASE "-O1 -DNDEBUG")
        set(CMAKE_C_FLAGS_RELWITHDEBINFO "-O1 -g -DNDEBUG")
        set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "-O1 -g -DNDEBUG")
    endif()
endif()

if(SYMUSIC_ENABLE_COVERAGE)
    if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
        string(APPEND CMAKE_CXX_FLAGS " -g -fprofile-arcs -ftest-coverage")
        string(APPEND CMAKE_EXE_LINKER_FLAGS " --coverage")
    else()
        message(WARNING "Code coverage requires GCC or Clang")
    endif()
endif()

set(SYMUSIC_CORE_LTO OFF)
if(SYMUSIC_ENABLE_LTO AND NOT SYMUSIC_FAST_BUILD)
    include(CheckIPOSupported)
    check_ipo_supported(RESULT SYMUSIC_CORE_LTO OUTPUT _symusic_ipo_output LANGUAGES CXX)
    if(NOT SYMUSIC_CORE_LTO)
        message(WARNING "Core LTO is unavailable; continuing without it. Use --log-level=VERBOSE for probe details.")
        message(VERBOSE "${_symusic_ipo_output}")
    endif()
endif()

if(SYMUSIC_TIME_BUILD_RULES)
    set_property(GLOBAL PROPERTY RULE_LAUNCH_COMPILE "${CMAKE_COMMAND} -E time")
    set_property(GLOBAL PROPERTY RULE_LAUNCH_LINK "${CMAKE_COMMAND} -E time")
endif()

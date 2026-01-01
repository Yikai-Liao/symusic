cmake_minimum_required(VERSION 3.22)
project(miniabc VERSION 0.1.0 LANGUAGES C CXX)

set(MINIABC_INCLUDE_DIR "${PROJECT_SOURCE_DIR}/include")
set(MINIABC_SOURCE_DIR "${PROJECT_SOURCE_DIR}/src")

add_library(miniabc STATIC
    "${MINIABC_SOURCE_DIR}/parse.cpp"
    "${MINIABC_SOURCE_DIR}/render.cpp"
    "${MINIABC_SOURCE_DIR}/common.cpp"
)

target_include_directories(miniabc PUBLIC
    "${MINIABC_INCLUDE_DIR}"
)

target_compile_features(miniabc PUBLIC cxx_std_20)

option(MINIABC_USE_LEXY "Link Lexy for parsing helpers." ON)

if(MINIABC_USE_LEXY)
    add_subdirectory("${PROJECT_SOURCE_DIR}/3rdparty/lexy" "${CMAKE_CURRENT_BINARY_DIR}/lexy")
    target_link_libraries(miniabc PUBLIC lexy)
endif()

add_library(miniabc::miniabc ALIAS miniabc)

message(STATUS "miniabc project configured (Lexy ${MINIABC_USE_LEXY})")

cmake_minimum_required(VERSION 3.20)
project(symusic_install_consumer LANGUAGES CXX)

find_package(symusic CONFIG REQUIRED PATHS "${CMAKE_PREFIX_PATH}" NO_DEFAULT_PATH)

add_executable(install_consumer main.cpp)
target_link_libraries(install_consumer PRIVATE symusic::symusic)
target_link_options(install_consumer PRIVATE ${consumer_link_options})

enable_testing()
add_test(NAME install_consumer COMMAND install_consumer)

cmake_minimum_required(VERSION 3.20)
project(pyscore)

set(CMAKE_CXX_STANDARD 17)

find_package(Python REQUIRED COMPONENTS Interpreter Development)
find_package(pybind11 REQUIRED CONFIG)

include_directories(include)
include_directories(3rdparty/minimidi/include)
include_directories(3rdparty/pdqsort)

pybind11_add_module(pyscore src/pyscore.cpp)
add_executable(note_count src/note_count.cpp)

cmake_minimum_required(VERSION 3.15...3.30)

project(tms
    VERSION 1.0
    DESCRIPTION "Turing machine simulator"
    LANGUAGES CXX)

add_executable(tms main.cpp turing.cpp)

set_target_properties(tms PROPERTIES
    CXX_STANDARD 23
    CXX_EXTENSIONS OFF)

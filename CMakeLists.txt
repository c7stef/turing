cmake_minimum_required(VERSION 3.15...3.30)

project(tmsg
    VERSION 1.0
    DESCRIPTION "Turing machine simulator & generator"
    LANGUAGES CXX)

add_executable(tmsg main.cpp turing.cpp)

set_target_properties(tmsg PROPERTIES
    CXX_STANDARD 23
    CXX_EXTENSIONS OFF)

#!/bin/bash

cmake \
    -DCMAKE_TOOLCHAIN_FILE=$(pwd)/linux-clang.cmake \
    -S . -B build/

cmake --build build/ -j 4

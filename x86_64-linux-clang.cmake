set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR x86_64)

set(CMAKE_CXX_COMPILER clang++-18)
set(CMAKE_CXX_FLAGS "-stdlib=libc++ -Wall -Wextra -pedantic")
set(CMAKE_EXE_LINKER_FLAGS "-stdlib=libc++")
set(CMAKE_EXPORT_COMPILE_COMMANDS TRUE)

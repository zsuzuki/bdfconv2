cmake_minimum_required(VERSION 3.11)
enable_language(CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if (UNIX)
include_directories(/usr/local/include inc)
link_directories(/usr/local/lib)
endif()

project(bdfconv)
set(bdfconv_src main.cpp)
add_executable(bdfconv ${bdfconv_src})
